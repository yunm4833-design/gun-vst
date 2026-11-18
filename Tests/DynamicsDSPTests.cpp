#include "DynamicsDSP.h"
#include "SpatialDSP.h"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main()
{
    try
    {
        using namespace breach::dynamics;
        for(double sr:{8000.0,44100.0,48000.0,96000.0})
        {
            Engine engine;engine.prepare(sr);Controls controls;controls.enabled=true;controls.depth=0;engine.setControls(controls);
            for(int n=0;n<2000;++n){const float x=static_cast<float>(.1*std::sin(n*.1));const auto y=engine.process({x,x*.3f});require(y.left==x && y.right==x*.3f,"Zero depth is not exact bypass");}
            controls.enabled=false;controls.depth=1;engine.setControls(controls);
            for(int n=0;n<2000;++n)require(engine.process({.13f,-.2f}).left==.13f,"Disabled OTT changed input");
        }
        for(double hz:{30.0,120.0,700.0,2500.0,10000.0})
        {
            Crossover split;split.design(48000,120,2500);double energy=0;
            for(int n=0;n<96000;++n)
            {
                const auto bands=split.process(std::sin(2*pi*hz*n/48000));
                const double sum=bands[0]+bands[1]+bands[2];if(n>=48000)energy+=sum*sum;
            }
            require(std::abs(std::sqrt(energy/24000)-1)<.001,"Crossover sum magnitude is not flat");
        }
        for(size_t band=0;band<3;++band)
        {
            require(gainDb(-60,band,1,1,0)>5,"Upward compression does not lift detail");
            require(gainDb(-6,band,1,0,1)<-10,"Downward compression does not reduce peaks");
            require(gainDb(-120,band,1,1,1)==0,"Deep noise receives upward boost");
            require(gainDb(-60,band,0,1,1)==0,"Zero depth changes static gain");
        }
        for(float amplitude:{.001f,.5f})
        {
            Engine engine;engine.prepare(48000);Controls controls;controls.enabled=true;controls.depth=1;controls.upward=1;controls.downward=1;engine.setControls(controls);
            double energy=0,linkError=0;
            for(int n=0;n<96000;++n)
            {
                const float x=amplitude*static_cast<float>(std::sin(2*pi*700*n/48000));const auto y=engine.process({x,x*.25f});
                require(std::isfinite(y.left)&&std::isfinite(y.right),"Non-finite dynamics output");
                if(n>=48000){energy+=y.left*y.left;linkError=std::max(linkError,std::abs(static_cast<double>(y.right-y.left*.25f)));}
            }
            const double gain=std::sqrt(energy/24000)/amplitude;
            std::cout<<"Input amplitude "<<amplitude<<" output RMS gain "<<gain<<'\n';
            require(amplitude<.01f?gain>1.5:gain<.7,"Audible up/down behavior is wrong");
            require(linkError<1e-7,"Dynamics changes linked stereo ratio");
            controls.enabled=false;engine.setControls(controls);
            for(int n=0;n<2000;++n)engine.process({0,0});
            require(engine.idle(),"Bypass fade did not complete");
            require(engine.process({.17f,-.19f}).left==.17f,"Settled bypass is not exact");
        }
        for (std::size_t band = 0; band < 3; ++band)
        {
            const auto gain = [band](double db, double depth, double up, double down)
            {
                return breach::faustOtt::gainDb(db, band, depth, up, down);
            };
            require(gain(-60, .25, 1, 0) > 2 && gain(-60, 1, 1, 0) > gain(-60, .25, 1, 0),
                    "Faust DEPTH did not progressively lift quiet details");
            require(gain(-8, .25, 0, 1) < -1 && gain(-8, 1, 0, 1) < gain(-8, .25, 0, 1),
                    "Faust DEPTH did not progressively tame peaks");
            require(gain(-60, 1, 0, 1) > -.001 && gain(-8, 1, 1, 0) < .001,
                    "Faust upward and downward controls are not independent");
            require(std::abs(gain(-120, 1, 2, 2)) < .001 && gain(-60, 0, 2, 2) == 0,
                    "Faust silence guard or zero depth failed");
            require(breach::faustOtt::releaseSeconds(band, .5) < breach::faustOtt::releaseSeconds(band, 1)
                    && breach::faustOtt::releaseSeconds(band, 3) > breach::faustOtt::releaseSeconds(band, 1),
                    "Faust TIME did not scale release");
        }
        for (double sr : { 44100.0, 96000.0 })
        {
            Engine ott;
            ott.prepare(sr);
            Controls controls;
            controls.useFaust = true;
            controls.enabled = true;
            controls.depth = 1;
            ott.setControls(controls);
            double quietEnergy = 0, loudEnergy = 0;
            const int window = static_cast<int>(sr * .16);
            for (int n = 0; n < 4 * window; ++n)
            {
                const float x = (n < 2 * window ? .001f : .4f)
                    * static_cast<float>(std::sin(2 * pi * 800 * n / sr));
                const auto y = ott.process({ x, x * .5f });
                require(std::isfinite(y.left) && std::abs(y.right - y.left * .5f) < 1e-7f,
                        "Faust detector broke stereo linking");
                if (n >= window && n < 2 * window) quietEnergy += y.left * y.left;
                if (n >= 3 * window) loudEnergy += y.left * y.left;
            }
            require(std::sqrt(2 * quietEnergy / window) > .002 && std::sqrt(2 * loudEnergy / window) < .23,
                    "Faust model gains were not applied in the engine");
            controls.depth = 0;
            ott.setControls(controls);
            for (int n = 0; n < window; ++n) ott.process({ 0, 0 });
            require(ott.idle() && ott.process({ .7f, -.4f }).left == .7f, "Faust zero DEPTH did not null with bypass");
        }
        for(double sr:{44100.0,96000.0})
        {
            breach::spatial::ShortSmear shortDiff;breach::spatial::StereoDiffuser longDiff;
            shortDiff.prepare(sr);longDiff.prepare(sr);
            double earlyShort=0,lateShort=0,lateLong=0;
            for(int n=0;n<static_cast<int>(sr*.6);++n)
            {
                const float x=n==0?1.0f:0.0f;
                const auto a=shortDiff.process({x,x},1),b=longDiff.process({x,x},1);
                if(n<sr*.15)earlyShort+=a.left*a.left+a.right*a.right;
                else{lateShort+=a.left*a.left+a.right*a.right;lateLong+=b.left*b.left+b.right*b.right;}
            }
            require(std::abs(earlyShort+lateShort-2)<2e-5,"Full SMEAR lost stereo impulse energy");
            require(lateShort<earlyShort*1e-6 && lateShort<lateLong*.01,"SMEAR produces a long reverb-like tail");
            shortDiff.reset();for(int n=0;n<1000;++n){const auto y=shortDiff.process({.1f,-.2f},0);require(y.left==.1f&&y.right==-.2f,"Zero SMEAR is not dry");}
        }
        std::cout<<"PASS: crossover phase alignment, dynamics curves, linked stereo, both OTT models, exact bypass and short SMEAR\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
