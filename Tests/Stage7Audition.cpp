#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <stdexcept>
static void set(BreachResonatorAudioProcessor& p,const char* id,float value)
{auto* parameter=p.apvts.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
static void write(const juce::File& file,const juce::AudioBuffer<float>& audio)
{
    file.deleteFile();auto stream=file.createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,24,{},0));
    if(!writer || !writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()))throw std::runtime_error("WAV write failed");
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const juce::File folder(argc>1?argv[1]:"/tmp/breach-stage7-audition");folder.createDirectory();
        constexpr int size=256,total=48000*8;juce::AudioBuffer<float> source(2,total);source.clear();uint32_t seed=12345;
        for(int n=0;n<96000;++n)
        {
            seed=seed*1664525u+1013904223u;const double t=n/48000.0,beat=std::fmod(t,.5);
            const double level=static_cast<int>(t*2)%2?.025:.22;
            const double noise=static_cast<double>(seed>>8)/8388608.0-1;
            const double tone=std::sin(2*juce::MathConstants<double>::pi*130.813*t)+.7*std::sin(2*juce::MathConstants<double>::pi*164.814*t)+.5*std::sin(2*juce::MathConstants<double>::pi*195.998*t);
            const float x=static_cast<float>(level*(noise*std::exp(-beat*45)+tone*.3*std::exp(-beat*14)));
            source.setSample(0,n,x);source.setSample(1,n,x*.85f);
        }
        auto input=source;input.applyGain(3.0f);write(folder.getChildFile("00-input.wav"),input);
        const char* names[]={"01-smear-focused","02-smear-blurred","03-ott-input","04-hall","05-plate","06-space","07-shimmer"};
        for(int test=0;test<7;++test)
        {
            BreachResonatorAudioProcessor p;set(p,"gunMode",0);set(p,"oversampling",1);set(p,"mix",.8f);set(p,"drive",.4f);
            set(p,"decay",.6f);set(p,"motionEnable",0);set(p,"chordPreset",0);p.applyChordPreset(0);set(p,"space",test==1?1.0f:0.0f);
            if(test==2){set(p,"ottEnable",1);set(p,"ottDepth",.5f);set(p,"ottUp",.7f);set(p,"ottDown",.7f);}
            if(test>=3){set(p,"reverbEnable",1);set(p,"reverbType",static_cast<float>(test-3));set(p,"reverbMix",.65f);set(p,"reverbDecay",3.5f);set(p,"reverbDamping",.2f);set(p,"reverbShimmer",.9f);}
            p.prepareToPlay(48000,size);juce::MidiBuffer midi;juce::AudioBuffer<float> output(2,total),block(2,size);
            for(int offset=0;offset<total;offset+=size)
            {
                for(int c=0;c<2;++c)block.copyFrom(c,0,source,c,offset,size);
                p.processBlock(block,midi);for(int c=0;c<2;++c)output.copyFrom(c,offset,block,c,0,size);
            }
            output.applyGain(3.0f);write(folder.getChildFile(juce::String(names[test])+".wav"),output);
            std::cout<<"Rendered "<<names[test]<<'\n';
        }
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
