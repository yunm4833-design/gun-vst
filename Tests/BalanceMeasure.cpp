#include "PluginProcessor.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdint>
static void set(BreachResonatorAudioProcessor& p,const char* id,float value)
{auto* param=p.apvts.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));}
static juce::AudioBuffer<float> source(int type,int total)
{
    constexpr double sr=48000,pi=3.14159265358979323846;
    juce::AudioBuffer<float> result(2,total);result.clear();uint32_t seed=456789;
    double phase=0;
    for(int n=0;n<std::min(total,144000);++n)
    {
        seed=seed*1664525u+1013904223u;const double noise=static_cast<double>(seed>>8)/8388608.0-1;
        const double t=n/sr,beat=std::fmod(t,.25);double x=0;
        if(type==0 || type==4)
        {
            const double s=type==0?1.0:1.3;
            x=.7*noise*std::exp(-beat*55*s)+.6*std::sin(2*pi*(48*beat+38*(1-std::exp(-beat*30))/30))*std::exp(-beat*24*s);
        }
        else if(type==1)
        {
            const double notes[]={65.4064,87.3071,98.0,130.813};const double f=notes[static_cast<int>(t*2)%4];phase+=2*pi*f/sr;
            for(int k=1;k<=12;++k)x+=std::sin(k*phase)/std::pow(k,1.4);
            x*=.5+.5*std::exp(-std::fmod(t,.5)*8);
        }
        else if(type==2 || type==5)
        {
            const double pitches[]={130.813,164.814,195.998,261.626,329.628,391.996};
            for(int k=0;k<6;++k)x+=std::sin(2*pi*pitches[k]*(type==5?1.122462:1.0)*t+std::sin(t*(k+1)*.2)*.08)/6;
            x+=.05*noise;
        }
        else
        {
            const double f=130.813+4*std::sin(2*pi*4.8*t);phase+=2*pi*f/sr;
            for(int k=1;k<=28;++k)
            {
                const double hz=f*k;
                const double envelope=.2+2*std::exp(-std::pow((hz-650)/250,2))+std::exp(-std::pow((hz-1300)/350,2))+.4*std::exp(-std::pow((hz-2600)/500,2));
                x+=std::sin(k*phase)*envelope/k;
            }
            x*=.5+.5*std::sin(2*pi*2.2*t)*std::sin(2*pi*2.2*t);x+=.03*noise;
        }
        result.setSample(0,n,static_cast<float>(x));result.setSample(1,n,static_cast<float>(x*.9));
    }
    const double rms=std::sqrt((std::pow(result.getRMSLevel(0,0,144000),2)+std::pow(result.getRMSLevel(1,0,144000),2))*.5);
    result.applyGain(static_cast<float>(.1/rms));return result;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder(argc>1?argv[1]:"/tmp/breach-balance-measure");folder.createDirectory();
    const bool legacy=argc>2 && juce::String(argv[2])=="--legacy";
    constexpr int total=192000,blockSize=256;
    for(int type=0;type<6;++type)
    {
        const auto input=source(type,total);
        {
            std::ofstream f(folder.getChildFile("input-"+juce::String(type)+".f32").getFullPathName().toStdString(),std::ios::binary);
            for(int n=0;n<total;++n)for(int c=0;c<2;++c){const float x=input.getSample(c,n);f.write(reinterpret_cast<const char*>(&x),sizeof(x));}
        }
        for(int mode=0;mode<5;++mode)
        {
            BreachResonatorAudioProcessor p;
            if(legacy)p.setBalanceRevision(1);
            set(p,"gunMode",static_cast<float>(mode));set(p,"mix",1);set(p,"softLimiter",0);set(p,"motionEnable",0);
            set(p,"oversampling",1);set(p,"drive",.45f);set(p,"excDrive",.3f);set(p,"space",0);set(p,"freqShift",0);
            set(p,"chordPreset",0);p.applyChordPreset(0);
            p.prepareToPlay(48000,blockSize);juce::MidiBuffer midi;juce::AudioBuffer<float> block(2,blockSize);
            std::ofstream file(folder.getChildFile("mode-"+juce::String(mode)+"-input-"+juce::String(type)+".f32").getFullPathName().toStdString(),std::ios::binary);
            for(int offset=0;offset<total;offset+=blockSize)
            {
                for(int c=0;c<2;++c)block.copyFrom(c,0,input,c,offset,blockSize);
                p.processBlock(block,midi);
                for(int n=0;n<blockSize;++n)for(int c=0;c<2;++c){const float x=block.getSample(c,n);file.write(reinterpret_cast<const char*>(&x),sizeof(x));}
            }
        }
        std::cout<<"Measured source "<<type<<'\n';
    }
}
