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
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const bool write=argc>1 && juce::String(argv[1])=="--write-reference";
        const juce::File folder(argc>2?argv[2]:"/tmp/breach-stage2-reference");folder.createDirectory();
        for(int t=0;t<6;++t)
        {
            BreachResonatorAudioProcessor processor;
            const auto stateFile=folder.getChildFile("state"+juce::String(t)+".bin");
            const auto audioFile=folder.getChildFile("audio"+juce::String(t)+".raw");
            if(write)
            {
                configure(processor,t);set(processor,"gunMode",1.0f);
                set(processor,"resTopology",t>=3?2.0f:0.0f);
                set(processor,"stiffness",t%2?0.7f:0.0f);
                juce::MemoryBlock state;processor.getStateInformation(state);
                require(stateFile.replaceWithData(state.getData(),state.getSize()),"Cannot write state");
            }
            else
            {
                juce::MemoryBlock state;require(stateFile.loadFileAsData(state),"Missing reference");
                processor.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            }
            const auto output=render(processor,t%2?44100.0:48000.0);
            if(write)require(audioFile.replaceWithData(output.data(),output.size()*sizeof(float)),"Cannot write audio");
            else
            {
                std::vector<float> reference(output.size());
                std::ifstream in(audioFile.getFullPathName().toStdString(),std::ios::binary);
                in.read(reinterpret_cast<char*>(reference.data()),static_cast<std::streamsize>(reference.size()*sizeof(float)));
                require(in.good(),"Cannot read reference");float error=0;
                for(size_t n=0;n<output.size();++n)error=std::max(error,std::abs(output[n]-reference[n]));
                std::cout<<"Stage 2 case "<<t<<" max error "<<error<<'\n';
                require(error<1e-6f,"Stage 2 audio recall changed");
                require(processor.getStringDspRevision()==1,"Unversioned string state did not choose legacy");
                processor.setStringDspRevision(2);
                juce::MemoryBlock upgradedState;processor.getStateInformation(upgradedState);
                BreachResonatorAudioProcessor modern,restored;
                modern.setStateInformation(upgradedState.getData(),static_cast<int>(upgradedState.getSize()));
                restored.setStateInformation(upgradedState.getData(),static_cast<int>(upgradedState.getSize()));
                require(restored.getStringDspRevision()==2,"Rings selection did not restore");
                const auto changed=render(modern,t%2?44100.0:48000.0);
                const auto recalled=render(restored,t%2?44100.0:48000.0);
                require(changed==recalled,"Rings state recall changed audio");
                require(changed!=output,"Rings model did not reach actual audio output");

            }
        }
        if(!write)
        {
            for(int mode:{0,2,3,4})
            {
                BreachResonatorAudioProcessor a;set(a,"gunMode",static_cast<float>(mode));
                a.setStringDspRevision(1);juce::MemoryBlock state;a.getStateInformation(state);
                BreachResonatorAudioProcessor b;b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
                b.setStringDspRevision(2);
                require(render(a,48000.0)==render(b,48000.0),"Non-string topology changed with Rings selection");
            }
            for(double sr:{44100.0,48000.0,96000.0})
                for(int os=0;os<3;++os)
                {
                    BreachResonatorAudioProcessor full;set(full,"gunMode",1.0f);
                    set(full,"drive",1.0f);set(full,"excDrive",1.0f);set(full,"stiffness",1.0f);
                    set(full,"density",1.0f);set(full,"voiceCap",3.0f);set(full,"oversampling",static_cast<float>(os));
                    for(int note=0;note<12;++note){const auto id="note_"+juce::String(note);set(full,id.toRawUTF8(),1.0f);}
                    render(full,sr,os==0?32:(os==1?128:1024));
                }
            std::cout<<"PASS: Rings audio/state roundtrip, topology isolation, 12-voice stress and old session recall\n";
        }
        std::cout<<"PASS: stage 2 session fixtures\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
