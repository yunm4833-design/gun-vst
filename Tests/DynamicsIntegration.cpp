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
static std::vector<float> automated(int mode,int os,int size)
{
    BreachResonatorAudioProcessor p;set(p,"gunMode",static_cast<float>(mode));set(p,"oversampling",static_cast<float>(os));
    set(p,"ottEnable",1);set(p,"ottDepth",.6f);set(p,"reverbEnable",1);set(p,"reverbMix",.4f);set(p,"reverbType",static_cast<float>(mode%4));
    p.prepareToPlay(48000,size);juce::MidiBuffer midi;std::vector<float> result;uint32_t seed=12345;
    for(int offset=0;offset<16384;offset+=size)
    {
        if(offset==4096){set(p,"ottLowSplit",260);set(p,"ottHighSplit",5000);set(p,"reverbPreDelay",80);set(p,"space",.85f);}
        if(offset==8192){set(p,"ottDepth",.25f);set(p,"reverbType",static_cast<float>((mode+1)%4));set(p,"reverbMix",.6f);}
        if(offset==12288){set(p,"ottEnable",0);set(p,"reverbEnable",0);}
        juce::AudioBuffer<float> b(2,size);
        for(int i=0;i<size;++i)
        {
            seed=seed*1664525u+1013904223u;
            const float x=.1f*static_cast<float>(std::sin((offset+i)*.07))+.03f*(static_cast<float>(seed>>8)/8388608.0f-1);
            b.setSample(0,i,x);b.setSample(1,i,x*.7f);
        }
        p.processBlock(b,midi);
        for(int i=0;i<size;++i)for(int c=0;c<2;++c){const float y=b.getSample(c,i);require(std::isfinite(y),"Non-finite automated output");result.push_back(y);}
    }
    return result;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        {
            BreachResonatorAudioProcessor emptyProcessor;set(emptyProcessor,"ottEnable",1);set(emptyProcessor,"reverbEnable",1);
            juce::AudioBuffer<float> noChannels(0,128),noSamples(2,0);juce::MidiBuffer midi;
            emptyProcessor.processBlock(noChannels,midi);emptyProcessor.processBlock(noSamples,midi);
            require(noChannels.getNumChannels()==0 && noSamples.getNumSamples()==0,"Empty buffer callback changed layout");
        }
        require(argc>1,"Supply pre-edit stage 6 fixture folder");const juce::File folder(argv[1]);
        for(int t=0;t<6;++t)
        {
            juce::MemoryBlock state;require(folder.getChildFile("state"+juce::String(t)+".bin").loadFileAsData(state),"Missing stage 6 state");
            BreachResonatorAudioProcessor p;
            // Loading an old project into a previously used processor must disable
            // modules that were not present when that project was saved.
            set(p,"ottEnable",1);set(p,"reverbEnable",1);
            p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            require(p.getSmearDspRevision()==1,"Old session did not choose legacy SMEAR");
            require(p.apvts.getRawParameterValue("ottEnable")->load()==0,"Missing OTT state retained prior settings");
            require(p.apvts.getRawParameterValue("reverbEnable")->load()==0,"Missing reverb state retained prior settings");
            const auto output=render(p,t%2?44100:48000);std::vector<float> expected(output.size());
            std::ifstream in(folder.getChildFile("audio"+juce::String(t)+".raw").getFullPathName().toStdString(),std::ios::binary);
            in.read(reinterpret_cast<char*>(expected.data()),static_cast<std::streamsize>(expected.size()*sizeof(float)));require(in.good(),"Missing stage 6 audio");
            float error=0;for(size_t n=0;n<output.size();++n)error=std::max(error,std::abs(output[n]-expected[n]));
            std::cout<<"Stage 6 case "<<t<<" sample error "<<error<<'\n';require(error<1e-6f,"Stage 6 audio changed");
            set(p,"ottEnable",1);set(p,"ottDepth",0);set(p,"reverbEnable",1);set(p,"reverbMix",0);
            juce::MemoryBlock zeroState;p.getStateInformation(zeroState);
            BreachResonatorAudioProcessor zero;zero.setStateInformation(zeroState.getData(),static_cast<int>(zeroState.getSize()));
            require(output==render(zero,t%2?44100:48000),"Zero controls do not bypass new modules");
            set(p,"ottDepth",.6f);set(p,"reverbMix",.4f);set(p,"reverbType",static_cast<float>(t%4));p.setSmearDspRevision(2);
            p.getStateInformation(state);BreachResonatorAudioProcessor a,b;
            a.setStateInformation(state.getData(),static_cast<int>(state.getSize()));b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            const auto modern=render(a,48000);require(a.getLatencySamples()==p.getLatencySamples(),"New modules changed reported latency");require(modern==render(b,48000),"New-module state recall is not deterministic");
            require(modern!=output,"New DSP is not in the audio path");require(a.getTailLengthSeconds()>=12,"Reverb tail is under-reported");
        }
        {
            BreachResonatorAudioProcessor previous;
            set(previous, "ottEnable", 1);
            set(previous, "ottDepth", .72f);
            set(previous, "ottTime", 165);
            set(previous, "ottUp", .81f);
            set(previous, "ottDown", .66f);
            set(previous, "ottLowSplit", 95);
            previous.setFaustOttEnabled(false);
            juce::MemoryBlock state;
            previous.getStateInformation(state);
            auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
            require(xml != nullptr, "Could not read OTT version fixture");
            xml->removeAttribute("ottDspRevision");
            juce::AudioProcessor::copyXmlToBinary(*xml, state);

            BreachResonatorAudioProcessor revived;
            require(revived.usesFaustOtt(), "New instance did not start with Faust OTT");
            revived.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(!revived.usesFaustOtt(), "Saved project predating Faust did not recall classic OTT");
            const auto classicAudio = render(previous, 48000);
            require(classicAudio == render(revived, 48000), "Enabled classic OTT did not recall sample exact");

            BreachResonatorAudioProcessor upgraded;
            upgraded.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            upgraded.setFaustOttEnabled(true);
            upgraded.getStateInformation(state);
            BreachResonatorAudioProcessor faust;
            faust.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            const auto upgradedAudio = render(upgraded, 48000);
            require(faust.usesFaustOtt() && render(faust, 48000) == upgradedAudio,
                    "Faust OTT did not roundtrip sample exact");
            require(upgradedAudio != classicAudio, "Faust switch did not change the audio engine");
        }
        for(int mode=0;mode<5;++mode)for(int os=0;os<3;++os)
        {
            const auto a=automated(mode,os,64),b=automated(mode,os,1024);float error=0;
            for(size_t n=0;n<a.size();++n)error=std::max(error,std::abs(a[n]-b[n]));
            std::cout<<"Automated mode "<<mode<<" OS "<<os<<" sample error "<<error<<'\n';require(error<2e-5f,"New DSP depends on buffer size");
        }
        std::cout<<"PASS: stage 6 recall, missing-state reset, exact zero bypass, new recall, latency-free controls and automation\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
