#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <chrono>

static void set(BreachResonatorAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value));
}
static bool writeWave(const juce::File& file, const juce::AudioBuffer<float>& buffer)
{
    auto stream = file.createOutputStream();
    if (!stream) return false;
    stream->setPosition(0); stream->truncate();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), 48000.0, 2, 24, {}, 0));
    if (!writer) return false;
    stream.release(); return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder(argc > 1 ? argv[1] : "/tmp/breach-nonlinear-audition"); folder.createDirectory();
    constexpr int total = 48000 * 8, blockSize = 128;
    juce::AudioBuffer<float> dry(2, total); dry.clear();
    uint32_t seed = 8849;
    for (int n = 0; n < 48000 * 6; ++n)
    {
        seed = seed * 1664525u + 1013904223u;
        const double t = static_cast<double>(n % 12000) / 48000.0;
        const double noise = static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
        const float x = static_cast<float>(0.2 * noise * std::exp(-t*80)
            + 0.2 * std::sin(2*juce::MathConstants<double>::pi*110*t) * std::exp(-t*25));
        dry.setSample(0,n,x); dry.setSample(1,n,x * 0.85f);
    }
    if (!writeWave(folder.getChildFile("00-input-percussion.wav"), dry)) return 1;
    for (int example = 0; example < 2; ++example)
        for (int revision = 1; revision <= 2; ++revision)
        {
            BreachResonatorAudioProcessor processor;
            processor.setSpatialDspRevision(2);
            processor.setNonlinearDspRevision(revision);
            set(processor,"gunMode",example == 0 ? 0.0f : 3.0f);
            set(processor,"drive",0.8f);set(processor,"excDrive",0.6f); set(processor,"mix",1.0f);
            set(processor,"space",0.0f);
            set(processor,"freqShift",0.0f);
            set(processor,"oversampling",1.0f);
            processor.prepareToPlay(48000, blockSize);
            juce::AudioBuffer<float> output(2,total), block(2,blockSize); juce::MidiBuffer midi;
            const auto start = std::chrono::steady_clock::now();
            for (int offset = 0; offset < total; offset += blockSize)
            {
                for (int c=0;c<2;++c) block.copyFrom(c,0,dry,c,offset,blockSize);
                processor.processBlock(block,midi);
                for (int c=0;c<2;++c) output.copyFrom(c,offset,block,c,0,blockSize);
            }
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            const double rmsL=output.getRMSLevel(0,0,total), rmsR=output.getRMSLevel(1,0,total);
            const double rms=std::sqrt((rmsL*rmsL+rmsR*rmsR)*0.5);
            const float gain=static_cast<float>(0.07/std::max(rms,1.0e-9));
            output.applyGain(gain);
            const auto name = juce::String(example == 0 ? "drive-" : "sniper-") + (revision == 1 ? "legacy" : "new");
            if (!writeWave(folder.getChildFile(name+".wav"),output)) return 1;
            std::cout << name << ": 8 seconds rendered in " << elapsed << " seconds; RMS matching gain " << gain << '\n';
        }
}
