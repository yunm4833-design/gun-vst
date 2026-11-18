#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void set(BreachResonatorAudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

static juce::AudioBuffer<float> render(BreachResonatorAudioProcessor& processor, int size, bool automate = false, bool pitched = false)
{
    constexpr int total = 32768;
    juce::AudioBuffer<float> output(2, total), block(2, size);
    juce::MidiBuffer midi;
    processor.prepareToPlay(48000, size);
    for (int offset = 0; offset < total; offset += size)
    {
        if (automate && offset == 4096) set(processor, "outputEq1Freq", 730);
        if (automate && offset == 8192) set(processor, "outputEq1Type", 2);
        if (automate && offset == 16384) set(processor, "outputEqEnable", 0);
        for (int i = 0; i < size; ++i)
        {
            const double time = (offset + i) / 48000.0;
            const double low = .15 * std::sin(2 * breach::eq::pi * (pitched ? 130.813 : 40) * time);
            const double high = pitched ? 0 : .035 * std::sin(2 * breach::eq::pi * 1000 * time);
            const auto input = static_cast<float>(low + high);
            block.setSample(0, i, input);
            block.setSample(1, i, input * .65f);
        }
        processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel) output.copyFrom(channel, offset, block, channel, 0, size);
    }
    return output;
}

static double level(const juce::AudioBuffer<float>& audio, double hz, int channel)
{
    std::complex<double> bin = 0;
    constexpr int count = 24000;
    for (int index = 0; index < count; ++index)
    {
        const double window = .5 - .5 * std::cos(2 * breach::eq::pi * index / count);
        const auto sample = audio.getSample(channel, audio.getNumSamples() - count + index);
        require(std::isfinite(sample), "Non-finite plugin output");
        bin += static_cast<double>(sample) * window * std::polar(1.0, -2 * breach::eq::pi * hz * index / 48000);
    }
    return 20 * std::log10(std::max(1e-15, std::abs(bin) * 4 / count));
}

static void exact(const juce::AudioBuffer<float>& first, const juce::AudioBuffer<float>& second, const char* message)
{
    for (int c = 0; c < 2; ++c)
        require(std::memcmp(first.getReadPointer(c), second.getReadPointer(c), static_cast<std::size_t>(first.getNumSamples()) * sizeof(float)) == 0, message);
}

static void configure(BreachResonatorAudioProcessor& processor, float mix, int os)
{
    set(processor, "mix", mix);
    set(processor, "oversampling", static_cast<float>(os));
    set(processor, "gunMode", 0);
    set(processor, "chordPreset", 0);
    processor.applyChordPreset(0);
    set(processor, "softLimiter", 0);
    set(processor, "motionEnable", 0);
    set(processor, "space", 0);
    set(processor, "excHpf", 2000);
    set(processor, "outputEq1Freq", 150);
}

static void writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio)
{
    file.deleteFile();
    auto stream = file.createOutputStream();
    require(stream != nullptr, "Cannot create audition WAV");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "Cannot write audition WAV");
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        {
            BreachResonatorAudioProcessor processor;
            bool reachedEq = false;
            int newParameters = 0;
            for (const auto* parameter : processor.getParameters())
            {
                const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
                require(ranged != nullptr, "Unexpected non-APVTS parameter");
                if (ranged->getParameterID().startsWith("outputEq"))
                {
                    reachedEq = true;
                    ++newParameters;
                    require(parameter->getVersionHint() == 2, "New EQ IDs would reorder existing AUv2 automation");
                }
                else if (ranged->getParameterID().startsWith("voice") && parameter->getVersionHint() == 3)
                    require(newParameters == 41, "Voice macros precede earlier AU parameters");
                else require(!reachedEq && parameter->getVersionHint() == 1, "Legacy host parameter order changed");
            }
            require(newParameters == 41, "Incorrect EQ parameter count");
        }
        for (int os = 0; os < 3; ++os)
        {
            BreachResonatorAudioProcessor bypass;
            configure(bypass, 0, os);
            const auto reference = render(bypass, 256);
            require(level(reference, 40, 0) > -17, "Legacy excitation HPF unexpectedly changed fully dry bass");
            for (int type = 0; type < 3; ++type)
            {
                BreachResonatorAudioProcessor filtered;
                configure(filtered, 0, os);
                set(filtered, "outputEqEnable", 1);
                set(filtered, "outputEq1Type", static_cast<float>(type));
                const auto result = render(filtered, 256);
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto lowCut = level(result, 40, channel) - level(reference, 40, channel);
                    const auto highDelta = level(result, 1000, channel) - level(reference, 1000, channel);
                    const double expected = -10 * std::log10(1 + std::pow(150.0 / 40.0, 4 << type));
                    require(std::abs(lowCut - expected) < .2 && std::abs(highDelta) < .01, "Real processor low cut does not match its nominal slope");
                    if (channel == 0) std::cout << (1 << os) << "x, " << (12 << type) << " dB/oct: 40 Hz " << lowCut << ", 1 kHz " << highDelta << " dB\n";
                }
                require(filtered.getLatencySamples() == bypass.getLatencySamples(), "EQ changes reported latency");
                if (os == 0 && argc > 1)
                {
                    const juce::File folder(argv[1]);
                    require(folder.createDirectory().wasOk(), "Cannot create WAV folder");
                    if (type == 0) writeWav(folder.getChildFile("00-excitation-cut-only.wav"), reference);
                    writeWav(folder.getChildFile(juce::String("low-cut-") + juce::String(12 << type) + ".wav"), result);
                }
            }
        }
        {
            BreachResonatorAudioProcessor dryEq, wetEq;
            configure(dryEq, 1, 1);
            configure(wetEq, 1, 1);
            set(dryEq, "excHpf", 20);
            set(wetEq, "excHpf", 20);
            set(wetEq, "outputEqEnable", 1);
            set(wetEq, "outputEq1Freq", 400);
            set(wetEq, "outputEq1Type", 2);
            const auto unfiltered = level(render(dryEq, 256, false, true), 130.813, 0);
            const auto filtered = level(render(wetEq, 256, false, true), 130.813, 0);
            require(unfiltered > -40 && filtered - unfiltered < -70, "100% wet resonator bypasses output EQ");
            std::cout << "100% resonator wet, 130.813 Hz: " << filtered - unfiltered << " dB\n";
        }
        {
            BreachResonatorAudioProcessor initial, restored;
            configure(initial, .6f, 1);
            set(initial, "outputEqEnable", 1);
            set(initial, "outputEq8Enable", 1);
            set(initial, "outputEq8Type", 4);
            set(initial, "outputEq8Freq", 1200);
            set(initial, "outputEq8Gain", -7);
            juce::MemoryBlock state;
            initial.getStateInformation(state);
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            const auto a = render(initial, 64, true);
            const auto b = render(restored, 1024, true);
            exact(a, b, "EQ state or automation depends on block size");

            auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
            auto tree = juce::ValueTree::fromXml(*xml);
            for (int i = tree.getNumChildren() - 1; i >= 0; --i)
                if (tree.getChild(i).getProperty("id").toString().startsWith("outputEq")) tree.removeChild(i, nullptr);
            juce::MemoryBlock oldState;
            juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), oldState);
            set(restored, "outputEqEnable", 1);
            restored.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
            require(restored.apvts.getRawParameterValue("outputEqEnable")->load() < .5f && !restored.readOutputEqBand(7).enabled,
                    "Loading old state retained new EQ settings");
            BreachResonatorAudioProcessor expected, old;
            expected.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
            old.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
            set(expected, "outputEqEnable", 1);
            for (std::size_t i = 0; i < breach::eq::bandCount; ++i) set(expected, expected.outputEqBandId(i, "Enable"), 0);
            exact(render(expected, 256), render(old, 256), "All bands off does not bypass exactly");
        }
        std::cout << "PASS: real low cut at every slope and OS mode, wet/dry routing, latency, recall and automation\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
