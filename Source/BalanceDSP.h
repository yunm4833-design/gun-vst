#pragma once
#include "ThirdParty/Rings/RingsModalPrimitives.h"
#include "RingTimeDSP.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace breach::balance
{
constexpr double pi=3.14159265358979323846;
// Endpoints retained; the lower DRIVE range becomes more progressive.
inline float driveCurve(float x) noexcept
{
    x=std::clamp(x,0.0f,1.0f);
    return std::expm1(x*1.3815510558f)/std::expm1(1.3815510558f);
}
inline float decayCurve(float x) noexcept {return std::clamp((x-.05f)/.95f,0.0f,1.0f);}
inline float voiceWeight(float density,int voice,int count) noexcept
{
    const float target=1.0f+std::clamp(density,0.0f,1.0f)*static_cast<float>(std::max(0,count-1));
    return std::clamp(target-static_cast<float>(voice),0.0f,1.0f);
}
inline float densityNormalization(float density,int count) noexcept
{
    float power=0;
    for(int v=0;v<count;++v){const float weight=voiceWeight(density,v,count);power+=weight*weight;}
    return 1.0f/std::sqrt(std::max(power,1.0f));
}
// Fixed offline-calibrated wet trims. They never inspect the incoming envelope.
// PISTOL, SHOTGUN, SMG, SNIPER, REVOLVER; legacy/manual fallback remains unity.
inline constexpr std::array<float,5> trimDb {{-2.3f, 2.5f, 0.0f, 12.0f, -4.1f}};
inline float trim(int profile) noexcept
{
    return profile>=0 && profile<5 ? std::pow(10.0f,trimDb[static_cast<size_t>(profile)]/20.0f) : 1.0f;
}

// A consistent DECAY response for the remaining SVF-based SNIPER core. The
// imported bandpass is peak-normalized before saturation to retain drive range.
class SniperVoice
{
public:
    void prepare(double sr) noexcept {sampleRate=sr;interval=std::max(1,static_cast<int>(std::lround(sr/2000.0)));reset();}
    void reset() noexcept {filter.Reset();countdown=0;lastFrequency=-1;lastDecay=-1;lastQ=-2;lastRing=-1;}
    float process(float input,float frequency,float decay,float qMod,double ringSeconds = 0) noexcept
    {
        if(countdown--<=0)
        {
            countdown=interval-1;
            if (std::abs(frequency-lastFrequency)>1e-8f || std::abs(decay-lastDecay)>1e-8f || std::abs(qMod-lastQ)>1e-8f || std::abs(ringSeconds-lastRing)>1e-8)
            {
            lastFrequency=frequency;lastDecay=decay;lastQ=qMod;lastRing=ringSeconds;
            const double f=std::clamp(static_cast<double>(frequency),25.0,sampleRate*.4);
            const double qScale = std::exp2(.35*std::clamp(static_cast<double>(qMod),-1.0,1.0));
            const double seconds = ringSeconds > 0 ? std::clamp(ringSeconds * qScale, .006, ring::maxSeconds)
                : std::clamp(.04*std::pow(30.0,std::clamp(static_cast<double>(decay),0.0,1.0))*qScale,.025,1.6);
            const double loss=std::log(1000.0)/(seconds*sampleRate),rho=std::exp(-loss),omega=2*pi*f/sampleRate;
            const double oneMinus=-std::expm1(-loss);
            const double plus=oneMinus*oneMinus+4*rho*std::pow(std::sin(omega*.5),2);
            const double minus=oneMinus*oneMinus+4*rho*std::pow(std::cos(omega*.5),2);
            const double g=std::sqrt(plus/minus);
            r=2*(-std::expm1(-2*loss))/std::sqrt(plus*minus);filter.set_g_r(g,r);
            onset = ringSeconds > 0 ? ring::onsetGain(ringSeconds, .23) : 1;
            }
        }
        return static_cast<float>(filter.Process(input)*r*onset);
    }
private:
    ringsmodal::Svf filter;
    double sampleRate=48000,r=.1,lastRing=-1,onset=1;
    float lastFrequency=-1,lastDecay=-1,lastQ=-2;
    int interval=24,countdown=0;
};
class SniperBank
{
public:
    void selectRate(double sr) noexcept
    {if(sr!=rate){rate=sr;for(auto& voice:voices)for(auto& channel:voice)channel.prepare(sr);}}
    void reset() noexcept {for(auto& voice:voices)for(auto& channel:voice)channel.reset();}
    float process(size_t v,size_t c,float x,float f,float d,float q,double ringSeconds = 0) noexcept {return voices[v][c].process(x,f,d,q,ringSeconds);}
private:
    std::array<std::array<SniperVoice,2>,12> voices;
    double rate=0;
};
} // namespace breach::balance
