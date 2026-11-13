#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include <cmath>

struct VisualTelemetry
{
    std::atomic<float> inputRms { 0.0f };
    std::atomic<float> outputRms { 0.0f };
    std::atomic<float> transientImpulse { 0.0f };
    std::atomic<int>   activeArpVoice { 0 };
    std::atomic<float> lfo1Value { 0.0f };
    std::atomic<float> lfo2Value { 0.0f };
    std::atomic<float> envValue { 0.0f };
    std::array<std::atomic<float>, 12> voiceEnergies {};
};

class BreachResonatorAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxVoices = 12;

    BreachResonatorAudioProcessor();
    ~BreachResonatorAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void triggerPanicReset();
    void applyChordPreset(int presetIndex);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;
    VisualTelemetry telemetry;

private:
    struct VoiceDSP
    {
        // TPT State Variable Filter states (2 cascaded stages for formant / high-Q)
        float svf1Ic1 = 0.0f, svf1Ic2 = 0.0f;
        float svf2Ic1 = 0.0f, svf2Ic2 = 0.0f;

        // Waveguide fractional-delay comb line
        std::vector<float> combBuffer;
        int combWritePos = 0;
        float combLpState = 0.0f;
        float allpassState = 0.0f;

        // Per-voice envelope & smoothed frequency
        float attackEnv = 0.0f;
        float currentFreq = 220.0f;
        float energyMetric = 0.0f;

        void reset()
        {
            svf1Ic1 = svf1Ic2 = 0.0f;
            svf2Ic1 = svf2Ic2 = 0.0f;
            std::fill(combBuffer.begin(), combBuffer.end(), 0.0f);
            combWritePos = 0;
            combLpState = 0.0f;
            allpassState = 0.0f;
            attackEnv = 0.0f;
            energyMetric = 0.0f;
        }
    };

    struct SchroederAllpass
    {
        std::vector<float> buffer;
        int idx = 0;
        float feedback = 0.5f;

        void prepare(int size, float fb)
        {
            buffer.assign(static_cast<size_t>(juce::jmax(8, size)), 0.0f);
            idx = 0;
            feedback = fb;
        }

        void reset()
        {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            idx = 0;
        }

        inline float process(float x)
        {
            const float bufOut = buffer[static_cast<size_t>(idx)];
            const float y = -x + bufOut;
            buffer[static_cast<size_t>(idx)] = x + bufOut * feedback;
            if (++idx >= static_cast<int>(buffer.size()))
                idx = 0;
            return y;
        }
    };

    // Hilbert 8-stage allpass frequency shifter
    struct HilbertShifter
    {
        float a1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float a2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        double phase = 0.0;

        void reset()
        {
            for (int i = 0; i < 4; ++i) { a1[i] = 0.0f; a2[i] = 0.0f; }
            phase = 0.0;
        }

        inline float process(float x, float shiftHz, double sr)
        {
            if (std::abs(shiftHz) < 0.05f)
                return x;

            static constexpr float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static constexpr float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };

            float in1 = x;
            float in2 = x;
            for (int i = 0; i < 4; ++i)
            {
                const float y1 = c1[i] * (in1 - a1[i]) + a1[i];
                a1[i] = in1;
                in1 = y1;

                const float y2 = c2[i] * (in2 - a2[i]) + a2[i];
                a2[i] = in2;
                in2 = y2;
            }

            phase += (2.0 * juce::MathConstants<double>::pi * static_cast<double>(shiftHz)) / sr;
            if (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            else if (phase < -juce::MathConstants<double>::twoPi)
                phase += juce::MathConstants<double>::twoPi;

            return in1 * static_cast<float>(std::cos(phase)) - in2 * static_cast<float>(std::sin(phase));
        }
    };

    void rebuildTargetFrequencies(std::array<float, kMaxVoices>& freqs, int& activeCount);
    float evaluateLfo(int shape, float phase) const;

    double currentSampleRate = 44100.0;
    std::array<VoiceDSP, kMaxVoices> voicesL;
    std::array<VoiceDSP, kMaxVoices> voicesR;

    // Excitation HPF state
    float excHpfStateL = 0.0f, excHpfStateR = 0.0f;
    float excPrevInL = 0.0f, excPrevInR = 0.0f;

    // Stereo allpass diffusers & tilt EQ
    std::array<SchroederAllpass, 2> diffuserL;
    std::array<SchroederAllpass, 2> diffuserR;
    HilbertShifter hilbertL, hilbertR;
    float tiltLpL = 0.0f, tiltLpR = 0.0f;

    // Transient & envelope follower
    float envFast = 0.0f;
    float envSlow = 0.0f;
    float modEnvState = 0.0f;
    float transientHold = 0.0f;

    // LFO & Arpeggiator state
    float lfo1Phase = 0.0f;
    float lfo2Phase = 0.0f;
    double arpPhase = 0.0;
    int arpStep = 0;
    int arpDir = 1;
    uint32_t rngState = 0x12345678u;

    // Active MIDI notes bitmask
    std::array<bool, 128> midiNotesHeld {};

    // Gun topology crossfade state
    int lastGunMode = 0;
    int prevGunMode = 0;
    float gunCrossfade = 1.0f;

    // Smoothed macro parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smDrive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smSpace;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smDecay;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smTone;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smDensity;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smInGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smOutGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smWidth;

    std::atomic<bool> panicRequested { false };

    // Oversampling engines (2x and 4x)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;

    void processCoreBlock(juce::dsp::AudioBlock<float>& block, double effectiveSampleRate, double hostBpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessor)
};
