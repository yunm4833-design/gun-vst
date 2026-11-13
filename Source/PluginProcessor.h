#pragma once
#include "PluginParameters.h"
#include "VisualTelemetry.h"
#include "DspBuildingBlocks.h"
#include "ChordVoicingEngine.h"
#include "ResonatorBank.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class BreachResonatorAudioProcessor : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener,
                                      private juce::AsyncUpdater
{
public:
    BreachResonatorAudioProcessor();
    ~BreachResonatorAudioProcessor() override;

    void setMode(int index);
    int getCurrentMode() const noexcept { return currentModeIndex.load(std::memory_order_relaxed); }

    // Manual UI Trigger button fires a crisp internal impulse burst into the resonator bank
    void triggerManualImpulse() noexcept { manualTriggerRequested.store(true, std::memory_order_release); }

    // Advanced Panel UI State (persisted inside DAW project state)
    void setAdvancedPanelOpen(bool open) noexcept { advancedPanelOpen.store(open, std::memory_order_relaxed); }
    bool isAdvancedPanelOpen() const noexcept { return advancedPanelOpen.load(std::memory_order_relaxed); }
    void setAdvancedActiveTab(int tabIdx) noexcept { advancedActiveTab.store(juce::jlimit(0, 3, tabIdx), std::memory_order_relaxed); }
    int getAdvancedActiveTab() const noexcept { return advancedActiveTab.load(std::memory_order_relaxed); }

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
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    static double getDivisionInBeats(int rateIndex) noexcept;
    void processOversampledBlock(juce::dsp::AudioBlock<float>& block, double effectiveSampleRate, double bpm);

    juce::AudioProcessorValueTreeState apvts;
    breach::VisualTelemetry telemetry;

    // Stage 1 Atomic Parameter Pointers
    std::atomic<float>* modeParam = nullptr;
    std::array<std::atomic<float>*, 12> noteParams{};
    std::atomic<float>* chordPresetParam = nullptr;
    std::atomic<float>* rootNoteParam    = nullptr;
    std::atomic<float>* octaveParam      = nullptr;
    std::atomic<float>* spreadParam      = nullptr;
    std::atomic<float>* midiModeParam    = nullptr;
    std::atomic<float>* attackParam      = nullptr;
    std::atomic<float>* decayParam       = nullptr;

    std::atomic<float>* arpOnParam       = nullptr;
    std::atomic<float>* arpSyncParam     = nullptr;
    std::atomic<float>* arpRateSyncParam = nullptr;
    std::atomic<float>* arpRateHzParam   = nullptr;
    std::atomic<float>* arpPatternParam  = nullptr;

    std::atomic<float>* warpParam        = nullptr;
    std::atomic<float>* driftParam       = nullptr;
    std::atomic<float>* shiftHzParam     = nullptr;

    std::atomic<float>* motionOnParam       = nullptr;
    std::atomic<float>* motionStrengthParam = nullptr;
    std::atomic<float>* motionSyncParam     = nullptr;
    std::atomic<float>* motionRateSyncParam = nullptr;
    std::atomic<float>* motionRateHzParam   = nullptr;
    std::atomic<float>* motionShapeParam    = nullptr;
    std::atomic<float>* unisonParam         = nullptr;

    std::atomic<float>* filterTiltParam  = nullptr;
    std::atomic<float>* filterQParam     = nullptr;
    std::atomic<float>* filterLowParam   = nullptr;
    std::atomic<float>* filterHighParam  = nullptr;
    std::atomic<float>* depthParam       = nullptr;

    // Stage 2 Atomic Parameter Pointers
    std::atomic<float>* voiceCapParam = nullptr;
    std::array<std::atomic<float>*, breach::kMaxVoices> voiceLevelParams{};
    std::array<std::atomic<float>*, breach::kMaxVoices> voicePanParams{};
    std::array<std::atomic<float>*, breach::kMaxVoices> voiceDetuneParams{};
    std::array<std::atomic<float>*, breach::kMaxVoices> voiceDecayMulParams{};

    std::atomic<float>* resModelParam  = nullptr;
    std::atomic<float>* fbDampingParam = nullptr;
    std::atomic<float>* stiffnessParam = nullptr;
    std::atomic<float>* excHpfHzParam  = nullptr;
    std::atomic<float>* excDriveParam  = nullptr;

    std::atomic<float>* advLfo1ShapeParam    = nullptr;
    std::atomic<float>* advLfo1RateHzParam   = nullptr;
    std::atomic<float>* advLfo1RateSyncParam = nullptr;
    std::atomic<float>* advLfo1SyncParam     = nullptr;
    std::atomic<float>* advLfo1PhaseParam    = nullptr;

    std::atomic<float>* advLfo2ShapeParam    = nullptr;
    std::atomic<float>* advLfo2RateHzParam   = nullptr;
    std::atomic<float>* advLfo2RateSyncParam = nullptr;
    std::atomic<float>* advLfo2SyncParam     = nullptr;
    std::atomic<float>* advLfo2PhaseParam    = nullptr;

    std::atomic<float>* advEnvAttackParam    = nullptr;
    std::atomic<float>* advEnvReleaseParam   = nullptr;
    std::atomic<float>* advEnvAmountParam    = nullptr;

    std::array<std::array<std::atomic<float>*, 6>, 3> modMatrixParams{};

    std::atomic<float>* inputGainDbParam     = nullptr;
    std::atomic<float>* outputGainDbParam    = nullptr;
    std::atomic<float>* stereoWidthParam     = nullptr;
    std::atomic<float>* dryWetCurveParam     = nullptr;
    std::atomic<float>* oversamplingParam    = nullptr;
    std::atomic<float>* limiterOnParam       = nullptr;
    std::atomic<float>* limiterThreshDbParam = nullptr;
    std::atomic<float>* smoothTimeMsParam    = nullptr;

    // UI Persisted Drawer State
    std::atomic<bool> advancedPanelOpen{ false };
    std::atomic<int> advancedActiveTab{ 0 };
    std::atomic<bool> manualTriggerRequested{ false };
    int manualImpulseSamplesRemaining = 0;
    juce::Random impulseRng{ 0x199909 };

    // Chord preset sync state
    std::atomic<int> pendingPresetToApply{ -1 };
    std::atomic<bool> pendingSwitchToCustom{ false };
    std::atomic<bool> isSyncingChordSwitches{ false };

    // Parameter Smoothers
    float lastSmoothTimeMs = 15.0f;
    juce::SmoothedValue<float> smoothAttack;
    juce::SmoothedValue<float> smoothDecay;
    juce::SmoothedValue<float> smoothWarp;
    juce::SmoothedValue<float> smoothDrift;
    juce::SmoothedValue<float> smoothShiftHz;
    juce::SmoothedValue<float> smoothMotionStrength;
    juce::SmoothedValue<float> smoothUnison;
    juce::SmoothedValue<float> smoothFilterTilt;
    juce::SmoothedValue<float> smoothFilterQ;
    juce::SmoothedValue<float> smoothFilterLow;
    juce::SmoothedValue<float> smoothFilterHigh;
    juce::SmoothedValue<float> smoothDepth;
    juce::SmoothedValue<float> smoothInputGain;
    juce::SmoothedValue<float> smoothOutputGain;
    juce::SmoothedValue<float> smoothStereoWidth;

    // Pre-allocated Oversampling Engines (1x bypass, 2x, 4x)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    int lastOversamplingChoice = 0;

    // Dual Resonator Banks for 20ms crossfade
    std::atomic<int> currentModeIndex{ 0 };
    int activeMode = 0;
    int previousMode = 0;
    int modeCrossfadeSamplesRemaining = 0;
    int modeCrossfadeTotalSamples = 882;

    breach::dsp::ResonatorBank bankPrimary;
    breach::dsp::ResonatorBank bankSecondary;
    breach::dsp::ChordVoicingEngine voicingEngine;
    std::array<breach::dsp::VoicePitchSpec, breach::kMaxVoices> currentVoiceSpecs{};

    // Excitation Pre-Filter
    breach::dsp::TptSvf excHpf;

    // Drift Allpass Smear & Shift SSB Shifter
    breach::dsp::AllpassDiffuser smearDiffuserL1, smearDiffuserL2;
    breach::dsp::AllpassDiffuser smearDiffuserR1, smearDiffuserR2;
    breach::dsp::HilbertFrequencyShifter shifterL, shifterR;

    // Motion & Advanced Modulation Sources
    breach::dsp::MotionLfoBank motionLfo;
    breach::dsp::MotionLfoBank advLfo1;
    breach::dsp::MotionLfoBank advLfo2;
    breach::dsp::EnvelopeFollower advEnvFollower;

    // Wet Filter
    breach::dsp::SpectralTiltFilter tiltL, tiltR;
    breach::dsp::TptSvf wetHpfL, wetHpfR;
    breach::dsp::TptSvf wetLpfL, wetLpfR;

    // Arp Sequencer
    double arpPhaseAccumulator = 0.0;
    int arpStepCounter = 0;
    int arpPingPongDir = 1;
    int arpRandomVoice = 0;
    juce::Random arpRng{ 0xB4EAC4 };

    // Transient Detector
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    int transientCooldownSamples = 0;

    double baseSampleRate = 44100.0;
    double preparedEngineRate = 44100.0;
    int maxBlockSizePrepared = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessor)
};
