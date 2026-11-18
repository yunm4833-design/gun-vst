#include "PluginProcessor.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void set(BreachResonatorAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(value));
}
static void configure(BreachResonatorAudioProcessor& p, int test)
{
    set(p, "gunMode", static_cast<float>(test % 5));
    set(p, "oversampling", static_cast<float>(test % 3));
    set(p, "freqShift", test % 3 == 0 ? 0.0f : (test % 3 == 1 ? -73.0f : 137.0f));
    set(p, "space", test == 0 ? 0.0f : 0.7f);
    set(p, "voiceCap", 3.0f); set(p, "density", 1.0f);
}
static std::vector<float> render(BreachResonatorAudioProcessor& p, double sr, int blockSize = 128)
{
    p.prepareToPlay(sr, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> output;
    uint32_t noise = 12345;
    const int blocks = static_cast<int>(sr * 0.4) / blockSize;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> block(2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            noise = noise * 1664525u + 1013904223u;
            const int n = b * blockSize + i;
            const float input = 0.12f * static_cast<float>(std::sin(n * 0.07))
                + 0.05f * (static_cast<float>(noise >> 8) / 8388608.0f - 1.0f)
                + (n % 4000 == 0 ? 0.3f : 0.0f);
            block.setSample(0, i, input); block.setSample(1, i, input * 0.7f);
        }
        p.processBlock(block, midi);
        for (int i = 0; i < blockSize; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float sample = block.getSample(c, i);
                require(std::isfinite(sample), "Non-finite output"); output.push_back(sample);
            }
    }
    p.releaseResources(); return output;
}
// Identical automation times with different host block sizes must produce the
// same audio. This catches per-block resets of a partially completed ramp.
static std::vector<float> automated(int mode,int os,int blockSize,float density=.55f,bool silence=false,bool dry=false)
{
    BreachResonatorAudioProcessor p;
    set(p,"gunMode",static_cast<float>(mode));set(p,"oversampling",static_cast<float>(os));
    set(p,"density",density);set(p,"motionEnable",0);set(p,"softLimiter",0);set(p,"mix",dry?0.0f:.6f);
    set(p,"paramSmoothMs",25);set(p,"space",.35f);
    p.prepareToPlay(48000,blockSize);juce::MidiBuffer midi;std::vector<float> out;
    uint32_t seed=12345;
    for(int offset=0;offset<16384;offset+=blockSize)
    {
        if(offset==4096 && !dry){set(p,"drive",.85f);set(p,"excDrive",.7f);set(p,"density",.8f);set(p,"mix",.95f);}
        if(offset==8192 && !dry){set(p,"decay",.2f);set(p,"tone",.15f);set(p,"mix",.35f);set(p,"inGain",-3);}
        if(offset==12288 && !dry){set(p,"paramSmoothMs",70);set(p,"drive",.1f);set(p,"density",.2f);}
        juce::AudioBuffer<float> b(2,blockSize);
        for(int i=0;i<blockSize;++i)
        {
            seed=seed*1664525u+1013904223u;
            const float x=silence?0.0f:.05f*(static_cast<float>(seed>>8)/8388608.0f-1)+.1f*static_cast<float>(std::sin((offset+i)*.017));
            b.setSample(0,i,x);b.setSample(1,i,x*.8f);
        }
        p.processBlock(b,midi);
        for(int i=0;i<blockSize;++i)for(int c=0;c<2;++c)
        {
            const float y=b.getSample(c,i);require(std::isfinite(y),"Automation became non-finite");out.push_back(y);
        }
    }
    return out;
}
static void currentTests()
{
    for(int mode=0;mode<5;++mode)
        for(int os=0;os<3;++os)
        {
            const auto reference=automated(mode,os,64);
            for(int size:{256,1024})
            {
                const auto other=automated(mode,os,size);float error=0;
                for(size_t n=0;n<other.size();++n)error=std::max(error,std::abs(other[n]-reference[n]));
                std::cout<<"Automation mode "<<mode<<" OS "<<os<<" block "<<size<<" error "<<error<<'\n';
                require(error<2e-5f,"Macro automation depends on host buffer size");
            }
            const auto zero=automated(mode,os,256,.55f,true);
            for(float sample:zero)require(sample==0,"Balance layer creates noise from silence");
        }
    for(int mode=0;mode<5;++mode)
    {
        BreachResonatorAudioProcessor p;configure(p,mode);p.setBalanceRevision(2);
        juce::MemoryBlock state;p.getStateInformation(state);
        BreachResonatorAudioProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(restored.getBalanceRevision()==2,"Balanced revision did not roundtrip");
        require(render(p,48000)==render(restored,48000),"Balanced audio did not roundtrip");
        const auto a=automated(mode,0,256,.499f),b=automated(mode,0,256,.501f);
        double error=0,signal=0;
        for(size_t i=0;i<4096*2;++i){error+=(a[i]-b[i])*(a[i]-b[i]);signal+=a[i]*a[i];}
        require(std::sqrt(error/std::max(signal,1e-20))<.02,"Density boundary clicks");
    }
    // The wet-only trims cannot alter a fully dry path (ADAA reports 2 samples).
    const auto dry=automated(0,0,256,.55f,false,true);uint32_t seed=12345;
    std::vector<float> input(dry.size(),0);
    for(size_t i=0;i<dry.size()/2;++i)
    {
        seed=seed*1664525u+1013904223u;
        const float x=.05f*(static_cast<float>(seed>>8)/8388608.0f-1)+.1f*static_cast<float>(std::sin(i*.017));
        if(i+2<dry.size()/2){input[(i+2)*2]=x;input[(i+2)*2+1]=x*.8f;}
    }
    float dryError=0;for(size_t i=0;i<dry.size();++i)dryError=std::max(dryError,std::abs(dry[i]-input[i]));
    require(dryError<1e-6f,"Wet calibration modified dry gain or latency");
    std::cout<<"PASS: buffer-independent automation, density continuity, silence, dry alignment and balanced recall\n";
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const bool stageEight = argc > 1 && juce::String(argv[1]) == "--verify-stage8";
        const bool write=argc>1 && juce::String(argv[1])=="--write-reference";
        const juce::File folder(argc>2?argv[2]:"/tmp/breach-stage5-reference");folder.createDirectory();
        for(int t=0;t<6;++t)
        {
            BreachResonatorAudioProcessor p;
            const auto sf=folder.getChildFile("state"+juce::String(t)+".bin");
            const auto af=folder.getChildFile("audio"+juce::String(t)+".raw");
            if(write)
            {
                configure(p,t);set(p,"gunMode",static_cast<float>(t%5));
                set(p,"drive",.45f);set(p,"mix",t==5?.35f:1.0f);
                juce::MemoryBlock state;p.getStateInformation(state);
                require(sf.replaceWithData(state.getData(),state.getSize()),"Cannot write state");
            }
            else
            {
                juce::MemoryBlock state;require(sf.loadFileAsData(state),"Missing reference");
                if (stageEight) { set(p, "voiceTuneSpread", 20); set(p, "voicePanSpread", 100); }
                p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
                if (stageEight)
                {
                    require(!p.usesRingTime(), "Older session did not choose original decay");
                    require(p.apvts.getRawParameterValue("voiceTuneSpread")->load() < .01f
                            && p.apvts.getRawParameterValue("voicePanSpread")->load() < .01f, "Older session left voice macros active");
                }
            }
            const auto output=render(p,t%2?44100.0:48000.0);
            if(write)require(af.replaceWithData(output.data(),output.size()*sizeof(float)),"Cannot write audio");
            else
            {
                std::vector<float> ref(output.size());std::ifstream in(af.getFullPathName().toStdString(),std::ios::binary);
                in.read(reinterpret_cast<char*>(ref.data()),static_cast<std::streamsize>(ref.size()*sizeof(float)));
                require(in.good(),"Cannot read reference");float error=0;
                for(size_t n=0;n<output.size();++n)error=std::max(error,std::abs(output[n]-ref[n]));
                require(p.getBalanceRevision() == (stageEight ? 2 : 1), "Balance revision was not restored");
                std::cout << (stageEight ? "Stage 8 case " : "Stage 5 case ")<<t<<" max error "<<error<<'\n';require(error<1e-6f,"Earlier session audio changed");
            }
        }
        std::cout << (stageEight ? "PASS: stage 8 session fixtures\n" : "PASS: stage 5 session fixtures\n");
        if(!write && !stageEight)currentTests();
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
