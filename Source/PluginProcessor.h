#pragma once
#include "PluginParameters.h"
#include "VisualTelemetry.h"
#include "DspBuildingBlocks.h"
#include "ChordVoicingEngine.h"
#include "ResonatorBank.h"
#include <juce_audio_processors/juce_audio_processors.h>

class BreachResonatorAudioProcessor : public juce::AudioProcessor
{
public:
    BreachResonatorAudioProcessor();
    ~BreachResonatorAudioProcessor() override = default;

    void setMode(int index);
    int getCurrentMode() const noexcept { return currentModeIndex.load(std::memory_order_relaxed); }

    const breach::VisualTelemetry& getVisualTelemetry() const noexcept { return telemetry; }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BREACH Resonator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    static double getArpDivisionInBeats(int rateIndex) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    breach::VisualTelemetry telemetry;

    std::atomic<float>* modeParam         = nullptr;
    std::atomic<float>* rootNoteParam     = nullptr;
    std::atomic<float>* scaleTypeParam    = nullptr;
    std::atomic<float>* chordTypeParam    = nullptr;
    std::atomic<float>* voiceCountParam   = nullptr;
    std::atomic<float>* octaveSpanParam   = nullptr;
    std::atomic<float>* midiInputParam    = nullptr;

    std::atomic<float>* decayParam        = nullptr;
    std::atomic<float>* dampingParam      = nullptr;
    std::atomic<float>* toneParam         = nullptr;

    std::atomic<float>* octaveSpreadParam = nullptr;
    std::atomic<float>* detuneParam       = nullptr;
    std::atomic<float>* stereoWidthParam  = nullptr;

    std::atomic<float>* driftParam        = nullptr;
    std::atomic<float>* shiftHzParam      = nullptr;

    std::atomic<float>* arpEnabledParam   = nullptr;
    std::atomic<float>* arpRateParam      = nullptr;
    std::atomic<float>* arpPatternParam   = nullptr;
    std::atomic<float>* arpGateParam      = nullptr;

    std::atomic<float>* lfo1RateParam     = nullptr;
    std::atomic<float>* lfo1ShapeParam    = nullptr;
    std::atomic<float>* lfo2RateParam     = nullptr;
    std::atomic<float>* lfo2ShapeParam    = nullptr;
    std::atomic<float>* envAttackParam    = nullptr;
    std::atomic<float>* envReleaseParam   = nullptr;

    std::atomic<float>* lfo1ToDecayParam  = nullptr;
    std::atomic<float>* lfo1ToFilterParam = nullptr;
    std::atomic<float>* lfo2ToDriftParam  = nullptr;
    std::atomic<float>* lfo2ToShiftParam  = nullptr;
    std::atomic<float>* envToDecayParam   = nullptr;
    std::atomic<float>* envToFilterParam  = nullptr;

    std::atomic<float>* hpfFreqParam      = nullptr;
    std::atomic<float>* lpfFreqParam      = nullptr;
    std::atomic<float>* depthParam        = nullptr;

    juce::SmoothedValue<float> smoothDecay;
    juce::SmoothedValue<float> smoothDamping;
    juce::SmoothedValue<float> smoothTone;
    juce::SmoothedValue<float> smoothDrift;
    juce::SmoothedValue<float> smoothShiftHz;
    juce::SmoothedValue<float> smoothHpf;
    juce::SmoothedValue<float> smoothLpf;
    juce::SmoothedValue<float> smoothDepth;

    std::atomic<int> currentModeIndex{ 0 };
    int activeMode = 0;
    int previousMode = 0;
    int modeCrossfadeSamplesRemaining = 0;
    int modeCrossfadeTotalSamples = 882;

    breach::dsp::ResonatorBank bankPrimary;
    breach::dsp::ResonatorBank bankSecondary;
    breach::dsp::ChordVoicingEngine voicingEngine;
    std::array<breach::dsp::VoicePitchSpec, breach::VisualTelemetry::kMaxVoices> currentVoiceSpecs{};

    breach::dsp::AllpassDiffuser smearDiffuserL1, smearDiffuserL2;
    breach::dsp::AllpassDiffuser smearDiffuserR1, smearDiffuserR2;

    breach::dsp::HilbertFrequencyShifter shifterL, shifterR;

    breach::dsp::TptSvf wetHpfL, wetHpfR;
    breach::dsp::TptSvf wetLpfL, wetLpfR;

    breach::dsp::ModulationLfo lfo1, lfo2;
    breach::dsp::EnvelopeFollower envFollower;

    double arpBeatAccumulator = 0.0;
    int arpStepCounter = 0;
    int arpPingPongDir = 1;
    int arpRandomVoice = 0;
    juce::Random arpRng{ 0xB4EAC4 };

    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    int transientCooldownSamples = 0;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessor)
};
