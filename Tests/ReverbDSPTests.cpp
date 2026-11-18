#include "ReverbDSP.h"
#include <complex>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static std::vector<float> impulse(int mode,float decay,float predelay=0,bool anti=false)
{
    breach::reverb::Bank bank;bank.prepare(48000);breach::reverb::Controls c;c.enabled=true;c.type=mode;c.mix=1;c.decay=decay;c.predelayMs=predelay;c.damping=.3f;c.shimmer=1;
    bank.setControls(c);for(int n=0;n<4800;++n)bank.process({0,0});
    std::vector<float> output;output.reserve(192000);
    for(int n=0;n<192000;++n)
    {
        const float x=n==0?.5f:0.0f;const auto y=bank.process({x,anti?-x:x});
        require(std::isfinite(y.left)&&std::isfinite(y.right),"Reverb became non-finite");output.push_back(y.left);
    }
    return output;
}
int main()
{
    try
    {
        using namespace breach::reverb;
        std::array<std::vector<float>,4> signature;
        for(int mode=0;mode<4;++mode)
        {
            signature[static_cast<size_t>(mode)]=impulse(mode,2);
            const auto shortTail=impulse(mode,.3f),longTail=impulse(mode,3),anti=impulse(mode,2,0,true);
            double shortEnergy=0,longEnergy=0,stereoEnergy=0;
            for(size_t i=48000;i<144000;++i){shortEnergy+=shortTail[i]*shortTail[i];longEnergy+=longTail[i]*longTail[i];}
            for(float y:anti)stereoEnergy+=y*y;
            std::cout<<"Mode "<<mode<<" late short/long "<<shortEnergy<<" / "<<longEnergy<<'\n';
            require(longEnergy>shortEnergy*100 && longEnergy>1e-10,"DECAY does not create a longer reverb tail");
            require(stereoEnergy>1e-6,"Anti-phase stereo excitation is lost");
            const auto delayed=impulse(mode,2,100);
            double difference=0;for(size_t i=4800;i<delayed.size();++i)difference=std::max(difference,std::abs(static_cast<double>(delayed[i]-signature[static_cast<size_t>(mode)][i-4800])));
            if(mode<3)require(difference<1e-6,"Predelay time is wrong");
            else
            {
                // The two-head shifter is time-varying, so compare onset timing
                // rather than asserting time-invariant shifted waveforms.
                auto onset=[](const auto& signal){for(size_t i=0;i<signal.size();++i)if(std::abs(signal[i])>1e-8f)return i;return signal.size();};
                require(std::abs(static_cast<int>(onset(delayed))-static_cast<int>(onset(signature[3]))-4800)<=1,"Shimmer predelay onset is wrong");
            }
        }
        for(size_t a=0;a<4;++a)for(size_t b=a+1;b<4;++b)require(signature[a]!=signature[b],"Reverb modes are identical");
        for(double sr:{8000.0,44100.0,96000.0})
        {
            Bank bank;bank.prepare(sr);Controls c;c.enabled=false;bank.setControls(c);
            require(bank.process({.23f,-.4f}).left==.23f,"Disabled reverb changes signal");
            c.enabled=true;c.mix=0;bank.setControls(c);require(bank.process({-.33f,.1f}).left==-.33f,"Zero reverb blend changes signal");
            c.mix=1;c.decay=8;c.damping=0;c.width=2;c.type=3;bank.setControls(c);
            for(int n=0;n<static_cast<int>(sr);++n){const auto y=bank.process({n==100?.2f:0.0f,0});require(std::isfinite(y.left)&&std::abs(y.left)<10,"Extreme reverb is unstable");}
        }
        breach::stkverb::OctaveShift shifter;shifter.prepare(48000);
        std::vector<double> pitch;
        for(int n=0;n<96000;++n)
        {
            const double y=shifter.process(std::sin(2*breach::stkverb::pi*440*n/48000));
            if(n>=48000)pitch.push_back(y);
        }
        auto magnitude=[&](double hz)
        {
            std::complex<double> bin=0;
            for(size_t n=0;n<pitch.size();++n)bin+=pitch[n]*std::polar(1.0,-2*breach::stkverb::pi*hz*static_cast<double>(n)/48000);
            return std::abs(bin);
        };
        double low=0,high=0,peakHz=0;
        for(int f=400;f<=480;f+=5)low=std::max(low,magnitude(f));
        for(int f=820;f<=940;f+=5){const double bin=magnitude(f);if(bin>high){high=bin;peakHz=f;}}
        std::cout<<"Granular octave dominant bin "<<peakHz<<" Hz for 440 Hz input\n";
        require(high>low*10 && std::abs(peakHz-880)<=25,"SHIMMER lacks a nominal octave-up band");
        std::cout<<"PASS: four distinct tails, decay/predelay, anti-phase input, bypass, extreme settings and octave-up\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
