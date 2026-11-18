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
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const bool write=argc>1 && juce::String(argv[1])=="--write-reference";
        const juce::File folder(argc>2?argv[2]:"/tmp/breach-stage4-reference");folder.createDirectory();
        for(int t=0;t<6;++t)
        {
            BreachResonatorAudioProcessor p;
            const auto stateFile=folder.getChildFile("state"+juce::String(t)+".bin");
            const auto audioFile=folder.getChildFile("audio"+juce::String(t)+".raw");
            if(write)
            {
                configure(p,t);set(p,"gunMode",t%2?4.0f:0.0f);set(p,"resTopology",t>=3?1.0f:0.0f);
                set(p,"stiffness",t%2?.6f:.1f);set(p,"fbDamping",.4f);
                juce::MemoryBlock state;p.getStateInformation(state);
                require(stateFile.replaceWithData(state.getData(),state.getSize()),"Cannot write state");
            }
            else
            {
                juce::MemoryBlock state;require(stateFile.loadFileAsData(state),"Missing reference");
                p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            }
            const auto output=render(p,t%2?44100.0:48000.0);
            if(write)require(audioFile.replaceWithData(output.data(),output.size()*sizeof(float)),"Cannot write audio");
            else
            {
                std::vector<float> ref(output.size());std::ifstream in(audioFile.getFullPathName().toStdString(),std::ios::binary);
                in.read(reinterpret_cast<char*>(ref.data()),static_cast<std::streamsize>(ref.size()*sizeof(float)));
                require(in.good(),"Cannot read reference");float error=0;
                for(size_t n=0;n<output.size();++n)error=std::max(error,std::abs(output[n]-ref[n]));
                std::cout<<"Stage 4 case "<<t<<" max error "<<error<<'\n';require(error<1e-6f,"Stage 4 recall changed");
                require(p.getModalDspRevision()==1,"Missing modal revision did not choose legacy");
                p.setModalDspRevision(2);juce::MemoryBlock state;p.getStateInformation(state);
                BreachResonatorAudioProcessor a,b;
                a.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
                b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
                require(b.getModalDspRevision()==2,"Modal selection not restored");
                const auto modern=render(a,t%2?44100.0:48000.0);
                require(modern==render(b,t%2?44100.0:48000.0),"Modal audio recall changed");
                require(modern!=output,"Modal bank is not in the audio path");

            }
        }
        if(!write)
        {
            for(int mode:{1,2,3})
            {
                BreachResonatorAudioProcessor a;set(a,"gunMode",static_cast<float>(mode));a.setModalDspRevision(1);
                juce::MemoryBlock state;a.getStateInformation(state);
                BreachResonatorAudioProcessor b;b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));b.setModalDspRevision(2);
                require(render(a,48000.0)==render(b,48000.0),"Other gun mode changed");
            }
            for(int topology:{2,3})
            {
                BreachResonatorAudioProcessor a;set(a,"gunMode",4.0f);set(a,"resTopology",static_cast<float>(topology));a.setModalDspRevision(1);
                juce::MemoryBlock state;a.getStateInformation(state);
                BreachResonatorAudioProcessor b;b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));b.setModalDspRevision(2);
                require(render(a,48000.0)==render(b,48000.0),"Manual topology override ignored");
            }
            for(double sr:{44100.0,48000.0,96000.0})
                for(int os=0;os<3;++os)
                    for(int mode:{0,4})
                    {
                        BreachResonatorAudioProcessor p;set(p,"gunMode",static_cast<float>(mode));set(p,"voiceCap",3.0f);set(p,"density",1.0f);
                        set(p,"drive",1.0f);set(p,"excDrive",1.0f);set(p,"oversampling",static_cast<float>(os));
                        set(p,"mod_lfo1_warp",1.0f);set(p,"mod_lfo2_q",1.0f);set(p,"lfo1Rate",20.0f);
                        for(int note=0;note<12;++note){const auto id="note_"+juce::String(note);set(p,id.toRawUTF8(),1.0f);}
                        render(p,sr,os==0?32:(os==1?128:1024));
                    }
            std::cout<<"PASS: modal state/audio roundtrip, gun/topology isolation and two-profile 12-voice stress\n";
        }
        std::cout<<"PASS: stage 4 session fixtures\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
