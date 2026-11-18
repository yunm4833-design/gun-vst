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
        const juce::File folder(argc > 2 ? argv[2] : "/tmp/breach-spatial-reference");
        require(write || folder.isDirectory(), "Reference folder missing"); folder.createDirectory();
        for (int t = 0; t < 6; ++t)
        {
            BreachResonatorAudioProcessor processor;
            require(processor.getSpatialDspRevision() == 2, "New instance did not select upgraded spatial DSP");
            const auto stateFile = folder.getChildFile("state" + juce::String(t) + ".bin");
            const auto audioFile = folder.getChildFile("audio" + juce::String(t) + ".raw");
            if (write)
            {
                processor.setSpatialDspRevision(1);
                configure(processor, t); juce::MemoryBlock state; processor.getStateInformation(state);
                require(stateFile.replaceWithData(state.getData(), state.getSize()), "Cannot save state fixture");
            }
            else
            {
                juce::MemoryBlock state; require(stateFile.loadFileAsData(state), "Cannot read old state");
                processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                require(processor.getSpatialDspRevision() == 1, "Unversioned session did not select legacy DSP");
            }
            const auto output = render(processor, t % 2 == 0 ? 48000.0 : 44100.0);
            if (write) require(audioFile.replaceWithData(output.data(), output.size() * sizeof(float)), "Cannot save reference audio");
            else
            {
                std::vector<float> reference(output.size());
                std::ifstream in(audioFile.getFullPathName().toStdString(), std::ios::binary);
                in.read(reinterpret_cast<char*>(reference.data()), static_cast<std::streamsize>(reference.size() * sizeof(float)));
                require(in.good(), "Cannot read reference audio");
                float error = 0.0f;
                for (size_t i = 0; i < output.size(); ++i) error = std::max(error, std::abs(output[i] - reference[i]));
                std::cout << "Legacy case " << t << " max error " << error << '\n';
                require(error < 1.0e-6f, "Old session audio changed");
                juce::MemoryBlock oldState; processor.getStateInformation(oldState);
                BreachResonatorAudioProcessor modern;
                modern.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
                modern.setSpatialDspRevision(2);
                modern.setNonlinearDspRevision(1);
                juce::MemoryBlock newState; modern.getStateInformation(newState);
                BreachResonatorAudioProcessor restored;
                restored.setStateInformation(newState.getData(), static_cast<int>(newState.getSize()));
                require(restored.getSpatialDspRevision() == 2, "Upgraded engine selection not restored");
                auto upgraded = render(modern, t % 2 == 0 ? 48000.0 : 44100.0);
                auto recalled = render(restored, t % 2 == 0 ? 48000.0 : 44100.0);
                require(upgraded == recalled, "New session recall changed audio");
                if (t == 0) require(upgraded == output, "SPACE=0 SHIFT=0 changed unaffected audio");
                else require(upgraded != output, "New spatial engine had no effect");
                juce::MemoryBlock saved; restored.getStateInformation(saved);
                require(saved == newState, "Render changed serialized parameters");

            }
        }
        if (!write)
        {
            for (double sr : {44100.0, 48000.0, 96000.0})
                for (int factor = 0; factor < 3; ++factor)
                {
                    BreachResonatorAudioProcessor full;
                    set(full, "voiceCap", 3.0f); set(full, "density", 1.0f);
                    set(full, "oversampling", static_cast<float>(factor)); set(full, "space", 1.0f);
                    set(full, "freqShift", factor == 1 ? -500.0f : 500.0f);
                    for (int n = 0; n < 12; ++n)
                    {
                        const auto id = "note_" + juce::String(n);
                        set(full, id.toRawUTF8(), 1.0f);
                    }
                    const auto rendered = render(full, sr, factor == 0 ? 32 : (factor == 1 ? 128 : 1024));
                    require(!rendered.empty(), "12-voice render failed");
                }
            std::cout << "PASS: full processor 12-voice stress at 44.1/48/96 kHz, 1X/2X/4X and +/-500 Hz\n";
        }
        std::cout << (write ? "Reference fixtures saved\n" : "PASS: legacy audio recall, new engine roundtrip, zero-effect bypass and version migration\n");
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
