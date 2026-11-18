#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr double rate = 48000;
constexpr int excitationSamples = 960;
constexpr const char* names[] {"PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER"};

void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

void set(BreachResonatorAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void configure(BreachResonatorAudioProcessor& p, int mode, float position, float trim, bool singleNote, int oversampling)
{
    set(p, "gunMode", static_cast<float>(mode));
    set(p, "mix", 1);
    set(p, "space", 0);
    set(p, "drive", .25f);
    set(p, "softLimiter", 0);
    set(p, "motionEnable", 0);
    set(p, "arpEnable", 0);
    set(p, "stiffness", 0);
    set(p, "density", 1);
    set(p, "stereoUnison", 0);
    set(p, "decay", .05f + .95f * position);
    set(p, "vDecay_0", trim);
    set(p, "oversampling", static_cast<float>(oversampling));
    if (singleNote)
        for (int i = 0; i < 12; ++i) set(p, "note_" + juce::String(i), i == 0 ? 1.0f : 0.0f);
    else
        p.applyChordPreset(0);
}

juce::AudioBuffer<float> render(BreachResonatorAudioProcessor& p, int size, int total, bool automate = false)
{
    p.prepareToPlay(rate, size);
    juce::AudioBuffer<float> output(2, total);
    juce::AudioBuffer<float> block(2, size);
    juce::MidiBuffer midi;
    for (int offset = 0; offset < total; offset += size)
    {
        if (automate && offset == 4096) { set(p, "voiceTuneSpread", 15); set(p, "voicePanSpread", 72); set(p, "decay", .7f); }
        if (automate && offset == 8192) { set(p, "voiceTuneSpread", 0); set(p, "voicePanSpread", 0); }
        const int count = std::min(size, total - offset);
        block.setSize(2, count, false, false, true);
        for (int i = 0; i < count; ++i)
        {
            const int n = offset + i;
            const float input = automate ? static_cast<float>(.06 * std::sin(n * .04)) : (n < excitationSamples
                ? static_cast<float>(.10 * std::sin(2 * breach::balance::pi * 130.813 * n / rate)
                    * std::sin(breach::balance::pi * n / excitationSamples)) : 0.0f);
            block.setSample(0, i, input);
            block.setSample(1, i, input);
        }
        p.processBlock(block, midi);
        for (int c = 0; c < 2; ++c) output.copyFrom(c, offset, block, c, 0, count);
    }
    return output;
}

struct Measurement { double decay, peak; };

Measurement measure(const juce::AudioBuffer<float>& audio)
{
    double peak = 0, energy = 0;
    std::vector<double> reverseEnergy(static_cast<std::size_t>(audio.getNumSamples()));
    for (int i = audio.getNumSamples() - 1; i >= 0; --i)
    {
        for (int c = 0; c < 2; ++c)
        {
            const double sample = audio.getSample(c, i);
            require(std::isfinite(sample), "Non-finite ring output");
            energy += sample * sample;
            if (i < excitationSamples + 480) peak = std::max(peak, std::abs(sample));
        }
        reverseEnergy[static_cast<std::size_t>(i)] = energy;
    }
    const double tailEnergy = reverseEnergy[excitationSamples];
    require(peak > 1e-6 && tailEnergy > 1e-9, "Missing resonator response");
    auto crossing = [&](double db)
    {
        const double threshold = tailEnergy * std::pow(10, db / 10);
        for (std::size_t i = excitationSamples; i < reverseEnergy.size(); ++i)
            if (reverseEnergy[i] < threshold) return static_cast<double>(i) / rate;
        return static_cast<double>(reverseEnergy.size()) / rate;
    };
    // T20 extrapolation gives a repeatable result for the composite tail.
    return {(crossing(-25) - crossing(-5)) * 3, 20 * std::log10(peak)};
}

void writeAudio(const juce::File& path, const juce::AudioBuffer<float>& audio)
{
    path.deleteFile();
    auto stream = path.createOutputStream();
    require(stream != nullptr, "Cannot create WAV");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(), rate, 2, 24, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "Cannot write WAV");
}

float maxDifference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float error = 0;
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < a.getNumSamples(); ++n)
            error = std::max(error, std::abs(a.getSample(c, n) - b.getSample(c, n)));
    return error;
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const int clipSamples = 115200;
        bool onsetWithinRange = true;
        for (int os : {0, 2})
        {
            for (int mode = 0; mode < 5; ++mode)
            {
                juce::AudioBuffer<float> montage(2, clipSamples * 3);
                double previous = 0, minimumPeak = 100, maximumPeak = -100;
                int clip = 0;
                for (float position : {.2f, .5f, .8f})
                {
                    BreachResonatorAudioProcessor p;
                    configure(p, mode, position, 1, true, os);
                    const auto audio = render(p, 256, clipSamples);
                    const auto result = measure(audio);
                    std::cout << (1 << os) << "x " << names[mode] << ' ' << position << " target "
                              << p.nominalRingSeconds(.05f + .95f * position) << " s, measured " << result.decay
                              << " s, onset " << result.peak << " dBFS" << std::endl;
                    require(result.decay > previous * 1.4, "RING TIME does not lengthen the measured tail");
                    previous = result.decay;
                    minimumPeak = std::min(minimumPeak, result.peak);
                    maximumPeak = std::max(maximumPeak, result.peak);
                    for (int c = 0; c < 2; ++c) montage.copyFrom(c, clip * clipSamples, audio, c, 0, clipSamples);
                    ++clip;
                }
                onsetWithinRange = onsetWithinRange && maximumPeak - minimumPeak < 4;
                std::cout << names[mode] << " onset span " << maximumPeak - minimumPeak << " dB" << std::endl;
                if (os == 0 && argc > 1)
                {
                    const juce::File folder(argv[1]);
                    require(folder.createDirectory().wasOk(), "Cannot create audition folder");
                    montage.applyGain(3);
                    require(montage.getMagnitude(0, montage.getNumSamples()) < 1, "Audition clips at common gain");
                    writeAudio(folder.getChildFile(juce::String(names[mode]) + "-20-50-80.wav"), montage);
                }
            }
        }
        for (int mode = 0; mode < 5; ++mode)
        {
            BreachResonatorAudioProcessor below, above, neutral;
            configure(below, mode, .75f, 2, true, 0);
            configure(above, mode, .92f, 2, true, 0);
            configure(neutral, mode, .75f, 1, true, 0);
            const double lower = measure(render(below, 512, 230400)).decay;
            const double higher = measure(render(above, 512, 230400)).decay;
            const double normal = measure(render(neutral, 512, 230400)).decay;
            std::cout << names[mode] << " 2x trim at 75% / 92%: " << lower << " / " << higher
                      << " s, neutral at 75%: " << normal << " s" << std::endl;
            require(higher > lower * 1.25 && lower > normal * 1.4, "Voice trim or upper RING TIME range stalls");
        }
        {
            BreachResonatorAudioProcessor p, restored, baseline;
            configure(p, 0, .5f, 1, false, 1);
            configure(baseline, 0, .5f, 1, false, 1);
            set(p, "voiceTuneSpread", 18);
            set(p, "voicePanSpread", 42);
            juce::MemoryBlock state;
            p.getStateInformation(state);
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(restored.usesRingTime(), "New ring time did not round-trip");
            const auto first = render(p, 64, 16384, true);
            const auto second = render(restored, 512, 16384, true);
            const auto dryMacros = render(baseline, 512, 16384, true);
            require(maxDifference(first, second) < 2e-5f, "Voice macros or ring automation depend on block size");
            require(maxDifference(first, dryMacros) > .001f, "Voice macros do not affect audio");
        }
        require(onsetWithinRange, "RING TIME changes onset more than 4 dB across the test positions");
        std::cout << "PASS: measured five-mode ring contours, onset, high trim, macro sound and restore/automation\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
