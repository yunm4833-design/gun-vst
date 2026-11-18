#include "BalanceDSP.h"
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

int main()
{
    try
    {
        using namespace breach::balance;
        require(driveCurve(0)==0 && std::abs(driveCurve(1)-1)<1e-6f,"DRIVE endpoints changed");
        float previous=0;
        for(int i=1;i<=1000;++i)
        {
            const float x=static_cast<float>(i)/1000, y=driveCurve(x);
            require(y>previous && y<=x+1e-6f,"DRIVE is not progressive and monotonic"); previous=y;
        }
        require(decayCurve(.05f)==0 && decayCurve(1)==1,"DECAY range not normalized");
        for(int count:{1,4,6,8,12})
            for(int i=0;i<=1000;++i)
            {
                const float d=static_cast<float>(i)/1000;
                float power=0;
                for(int v=0;v<count;++v)
                {
                    const float w=voiceWeight(d,v,count);
                    const float delta=std::abs(w-voiceWeight(std::max(0.0f,d-1e-5f),v,count));
                    require(delta<.00013f,"DENSITY weight jumps at a voice boundary"); power+=w*w;
                }
                const float norm=densityNormalization(d,count);
                require(std::abs(power*norm*norm-1)<2e-6f,"DENSITY power compensation changed");
            }
        require(trim(-1)==1 && trim(5)==1,"Legacy trim is not unity");
        for(int mode=0;mode<5;++mode)
            require(std::isfinite(trim(mode)) && std::abs(trimDb[static_cast<size_t>(mode)])<=12,"Unbounded mode trim");
        for(double sr:{32000.0,44100.0,48000.0,96000.0})
            for(float decay:{0.0f,.5f,1.0f})
            {
                SniperVoice voice; voice.prepare(sr);
                const double expected=.04*std::pow(30.0,decay);
                double early=0,late=0; int earlyCount=0,lateCount=0;
                for(int n=0;n<static_cast<int>(expected*sr*.8);++n)
                {
                    const double t=n/sr;
                    const float y=voice.process(n==0?1.0f:0.0f,1000,decay,0);
                    require(std::isfinite(y),"SNIPER became non-finite");
                    if(t>=expected*.2 && t<expected*.3){early+=y*y;++earlyCount;}
                    if(t>=expected*.6 && t<expected*.7){late+=y*y;++lateCount;}
                }
                const double measured=-6.0*std::log(10.0)*(expected*.4)/std::log((late/lateCount)/(early/earlyCount));
                require(std::abs(measured/expected-1)<.05,"SNIPER decay is not rate-correct");
            }
        SniperVoice voice;voice.prepare(48000);
        double energy=0;
        for(int n=0;n<144000;++n)
        {
            const float y=voice.process(static_cast<float>(std::sin(2*pi*1000*n/48000)),1000,1,0);
            if(n>=96000)energy+=y*y;
        }
        require(std::abs(std::sqrt(energy/24000)-1)<.01,"SNIPER peak gain is not normalized");
        voice.reset();
        for(int n=0;n<48000;++n)require(voice.process(0,1000,1,0)==0,"SNIPER creates sound from silence");
        std::cout<<"PASS: progressive curves, continuous density, fixed trims, SNIPER normalized gain/RT60/silence\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
