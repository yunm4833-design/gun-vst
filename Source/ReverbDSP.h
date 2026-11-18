#pragma once
#include "ThirdParty/STK/StkReverb.h"
#include "DynamicsDSP.h"

namespace breach::reverb
{
struct Controls
{
    bool enabled=false;
    int type=0;
    float mix=.18f, decay=2, predelayMs=20, damping=.5f, width=1, shimmer=.5f;
};
struct Stereo { float left, right; };
class Bank
{
public:
    void prepare(double sr)
    {
        sampleRate=sr; step=1/(.020*sr); smooth=1-std::exp(-1/(.020*sr));
        controlInterval=std::max(1,static_cast<int>(std::lround(sr/1000)));
        for(size_t p=0;p<4;++p)for(size_t c=0;c<2;++c)
        {
            const double size=p==0?1.6:(p==1?.42:(p==2?1.5:1.1));
            models[p][c].prepare(sr,p==0,size*(c==0?1:1.013));
        }
        for(auto& s:shift)s.prepare(sr);
        for(auto& channel:antiAlias)for(auto& f:channel)f.design(sr,sr*.16,false);
        pre[0].assign(static_cast<size_t>(sr*.21)+4,0);pre[1]=pre[0];
        initialized=false;reset();
    }
    void reset() noexcept
    {
        for(auto& p:models)for(auto& c:p)c.reset();
        resetAux();blend=0;activeType=0;countdown=0;lastDecay=lastDamping=-1;initialized=false;
    }
    void setControls(Controls next) noexcept
    {
        next.type=std::clamp(next.type,0,3);next.mix=std::clamp(next.mix,0.0f,1.0f);
        next.decay=std::clamp(next.decay,.2f,8.0f);next.predelayMs=std::clamp(next.predelayMs,0.0f,200.0f);
        next.damping=std::clamp(next.damping,0.0f,1.0f);next.width=std::clamp(next.width,0.0f,2.0f);
        next.shimmer=std::clamp(next.shimmer,0.0f,1.0f);target=next;
        if(!initialized){current=next;activeType=next.type;initialized=true;}
    }
    bool idle() const noexcept {return blend<=0 && (!target.enabled || target.mix<=0);}
    Stereo process(Stereo input) noexcept
    {
        if(idle())return input;
        auto slew=[&](float& value,float destination){value+=static_cast<float>(smooth*(destination-value));};
        slew(current.mix,target.mix);slew(current.decay,target.decay);slew(current.predelayMs,target.predelayMs);
        slew(current.damping,target.damping);slew(current.width,target.width);slew(current.shimmer,target.shimmer);
        const bool active=target.enabled && target.mix>0 && activeType==target.type;
        blend+=std::clamp((active?1.0:0.0)-blend,-step,step);
        if(blend<=0)
        {
            for(auto& c:models[static_cast<size_t>(activeType)])c.reset();
            if(activeType!=target.type)
            {activeType=target.type;for(auto& c:models[static_cast<size_t>(activeType)])c.reset();}
            resetAux();lastDecay=lastDamping=-1;countdown=0;return input;
        }
        if(countdown--<=0)
        {
            countdown=controlInterval-1;
            if(std::abs(current.decay-lastDecay)>1e-5f || std::abs(current.damping-lastDamping)>1e-5f)
            {
                for(auto& c:models[static_cast<size_t>(activeType)])c.setDecay(current.decay,current.damping);
                lastDecay=current.decay;lastDamping=current.damping;
            }
        }
        const auto left=models[static_cast<size_t>(activeType)][0].process(predelay(input.left,0));
        const auto right=models[static_cast<size_t>(activeType)][1].process(predelay(input.right,1));
        if(++preIndex==pre[0].size())preIndex=0;
        double wetL=.75*left.left+.25*right.left,wetR=.25*left.right+.75*right.right;
        if(activeType==3)
        {
            double l=wetL,r=wetR;
            for(auto& f:antiAlias[0])l=f.process(l);
            for(auto& f:antiAlias[1])r=f.process(r);
            const double octaveL=shift[0].process(l),octaveR=shift[1].process(r);
            wetL+=(octaveL-wetL)*current.shimmer*.7;wetR+=(octaveR-wetR)*current.shimmer*.7;
        }
        const double mid=(wetL+wetR)*.5,side=(wetL-wetR)*.5*current.width;
        wetL=mid+side;wetR=mid-side;
        const double mix=current.mix*blend;
        return {static_cast<float>(input.left*(1-mix)+wetL*mix),static_cast<float>(input.right*(1-mix)+wetR*mix)};
    }
private:
    void resetAux() noexcept
    {
        for(auto& p:pre)std::fill(p.begin(),p.end(),0);
        preIndex=0;
        for(auto& s:shift)s.reset();
        for(auto& c:antiAlias)for(auto& f:c)f.reset();
    }
    double predelay(double input,size_t channel) noexcept
    {
        auto& buffer=pre[channel];buffer[preIndex]=input;
        double position=static_cast<double>(preIndex)-current.predelayMs*.001*sampleRate;
        if(position<0)position+=buffer.size();
        if(position>=static_cast<double>(buffer.size()))position=0;
        const auto a=static_cast<size_t>(position),b=(a+1)%buffer.size();
        return buffer[a]+(position-static_cast<double>(a))*(buffer[b]-buffer[a]);
    }
    std::array<std::array<stkverb::Network,2>,4> models;
    std::array<stkverb::OctaveShift,2> shift;
    std::array<std::array<dynamics::Biquad,4>,2> antiAlias;
    std::array<std::vector<double>,2> pre;
    Controls target,current;
    size_t preIndex=0;
    double sampleRate=48000,step=.001,smooth=.001,blend=0;
    float lastDecay=-1,lastDamping=-1;
    int activeType=0,countdown=0,controlInterval=48;
    bool initialized=false;
};
class Engine
{
public:
    void prepare(double baseRate){for(size_t i=0;i<3;++i)banks[i].prepare(baseRate*static_cast<double>(1u<<i));active=-1;}
    void selectRate(int index) noexcept
    {index=std::clamp(index,0,2);if(index!=active){active=index;banks[static_cast<size_t>(active)].reset();}}
    void reset() noexcept {if(active>=0)banks[static_cast<size_t>(active)].reset();}
    void setControls(Controls next) noexcept {banks[static_cast<size_t>(active)].setControls(next);}
    bool idle() const noexcept {return banks[static_cast<size_t>(active)].idle();}
    Stereo process(Stereo input) noexcept {return banks[static_cast<size_t>(active)].process(input);}
private:
    std::array<Bank,3> banks;
    int active=-1;
};
} // namespace breach::reverb
