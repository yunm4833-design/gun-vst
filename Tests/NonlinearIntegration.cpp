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
        const bool write = argc > 1 && juce::String(argv[1]) == "--write-reference";
        const juce::File folder(argc > 2 ? argv[2] : "/tmp/breach-stage1-reference");
        folder.createDirectory();
        for (int t = 0; t < 6; ++t)
        {
            BreachResonatorAudioProcessor processor;
            const auto stateFile = folder.getChildFile("state" + juce::String(t) + ".bin");
            const auto audioFile = folder.getChildFile("audio" + juce::String(t) + ".raw");
            if (write)
            {
                processor.setNonlinearDspRevision(1);
                configure(processor,t);
                set(processor,"gunMode",t % 2 ? 3.0f : static_cast<float>(t % 5));
                set(processor,"drive",0.8f); set(processor,"excDrive",0.6f);
                juce::MemoryBlock state; processor.getStateInformation(state);
                require(stateFile.replaceWithData(state.getData(),state.getSize()),"Cannot write state");
            }
            else
            {
                juce::MemoryBlock state; require(stateFile.loadFileAsData(state),"Missing reference");
                processor.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            }
            const auto output = render(processor,t % 2 ? 44100.0 : 48000.0);
            if (write) require(audioFile.replaceWithData(output.data(),output.size()*sizeof(float)),"Cannot write audio");
            else
            {
                std::vector<float> reference(output.size());
                std::ifstream in(audioFile.getFullPathName().toStdString(),std::ios::binary);
                in.read(reinterpret_cast<char*>(reference.data()),static_cast<std::streamsize>(reference.size()*sizeof(float)));
                require(in.good(),"Cannot read reference");
                float error=0;
                for(size_t n=0;n<output.size();++n) error=std::max(error,std::abs(reference[n]-output[n]));
                std::cout << "Stage 1 case " << t << " max error " << error << '\n';
                require(error<1.0e-6f,"Stage 1 session audio changed");
                require(processor.getNonlinearDspRevision()==1,"Missing nonlinear revision did not select legacy");
                processor.setNonlinearDspRevision(2);
                juce::MemoryBlock modernState; processor.getStateInformation(modernState);
                BreachResonatorAudioProcessor upgradedProcessor;
                upgradedProcessor.setStateInformation(modernState.getData(),static_cast<int>(modernState.getSize()));
                const auto upgraded=render(upgradedProcessor,t % 2 ? 44100.0 : 48000.0);
                BreachResonatorAudioProcessor restored;
                restored.setStateInformation(modernState.getData(),static_cast<int>(modernState.getSize()));
                require(restored.getNonlinearDspRevision()==2,"Nonlinear revision not restored");
                const auto recalled=render(restored,t % 2 ? 44100.0 : 48000.0);
                require(upgraded==recalled,"Modern nonlinear state recall changed audio");
                require(upgraded!=output,"Modern nonlinear processing had no effect");
                require(restored.getLatencySamples()>=2,"Modern processing latency not reported");

            }
        }
        if(!write)
        {
            BreachResonatorAudioProcessor dry;
            set(dry,"mix",0.0f); set(dry,"softLimiter",0.0f);
            set(dry,"inGain",0.0f); set(dry,"outGain",0.0f); set(dry,"oversampling",0.0f);
            dry.prepareToPlay(48000.0,128);
            require(dry.getNonlinearDspRevision()==2 && dry.getLatencySamples()==2,"New instance latency incorrect");
            juce::AudioBuffer<float> block(2,128);juce::MidiBuffer midi;
            for(int n=0;n<8;++n) {block.clear();dry.processBlock(block,midi);}
            block.clear();block.setSample(0,0,0.25f);block.setSample(1,0,-0.25f);
            dry.processBlock(block,midi);
            for(int n=0;n<128;++n)
            {
                require(block.getSample(0,n)==(n==2?0.25f:0.0f),"Dry path not delayed exactly two samples");
                require(block.getSample(1,n)==(n==2?-0.25f:0.0f),"Dry stereo compensation incorrect");
            }
            for(int pass=0;pass<12;++pass)
            {
                set(dry,"oversampling",static_cast<float>(pass%3));
                dry.setNonlinearDspRevision(pass%2?1:2);
                require(pass%2 ? dry.getLatencySamples()==0 : dry.getLatencySamples()>=2,"Revision setter did not notify host latency immediately");
                block.clear();dry.processBlock(block,midi);
                require(pass%2 ? dry.getLatencySamples()==0 : dry.getLatencySamples()>=2,"Latency failed to update at engine/OS switch");
                for(int n=0;n<128;++n) require(std::isfinite(block.getSample(0,n)),"Nonfinite live OS/revision switch");
                std::cout << "Switch " << pass << " latency " << dry.getLatencySamples() << '\n';
            }
            for(double sr:{44100.0,48000.0,96000.0})
                for(int os=0;os<3;++os)
                {
                    BreachResonatorAudioProcessor full;
                    set(full,"gunMode",3.0f);set(full,"drive",1.0f);set(full,"excDrive",1.0f);
                    set(full,"density",1.0f);set(full,"voiceCap",3.0f);set(full,"oversampling",static_cast<float>(os));
                    for(int note=0;note<12;++note) {const auto id="note_"+juce::String(note);set(full,id.toRawUTF8(),1.0f);}
                    render(full,sr,os==0?32:(os==1?128:1024));
                }
            std::cout << "PASS: new-engine recall, dry/host latency, live switches and max-drive 12-voice stress\n";
        }
        std::cout << "PASS: stage 1 session fixtures\n";
    }
    catch(const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
