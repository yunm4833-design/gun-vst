#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class MangaKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    bool inverted = false;

    juce::Colour getPaper() const noexcept { return inverted ? juce::Colours::black : juce::Colours::white; }
    juce::Colour getInk()   const noexcept { return inverted ? juce::Colours::white : juce::Colours::black; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

class DualRangeSliderAttachment : private juce::Slider::Listener,
                                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    DualRangeSliderAttachment(juce::AudioProcessorValueTreeState& state,
                              const juce::String& lowParamId,
                              const juce::String& highParamId,
                              juce::Slider& sliderToControl);
    ~DualRangeSliderAttachment() override;

private:
    void sliderValueChanged(juce::Slider* s) override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void syncFromParameters();

    juce::AudioProcessorValueTreeState& apvts;
    juce::String lowId;
    juce::String highId;
    juce::Slider& slider;
    bool isUpdating = false;
};

class BreachResonatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit BreachResonatorAudioProcessorEditor(BreachResonatorAudioProcessor&);
    ~BreachResonatorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    static constexpr int kBaseWidth      = 1040;
    static constexpr int kBaseHeight     = 640;
    static constexpr int kAdvancedHeight = 290;

    struct DebrisParticle
    {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float angle = 0.0f;
        float vAngle = 0.0f;
        float size = 6.0f;
        bool active = false;
    };

    void timerCallback() override;
    void selectMode(int modeIdx);
    void spawnImpactDebris(float intensity);
    void setAdvancedTab(int tabIdx);
    void updateAdvancedControlVisibility();
    void drawFirearmSilhouette(juce::Graphics& g, int modeIdx, juce::Rectangle<float> area, juce::Colour inkCol);
    void drawImpactPanel(juce::Graphics& g, juce::Rectangle<int> bounds);

    BreachResonatorAudioProcessor& audioProcessor;
    MangaKnobLookAndFeel mangaLnf;
    bool isInverted = false;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // ---------------- HEADER CONTROLS (Reference UI Alignment) ----------------
    juce::TextButton presetPrevBtn{ "<" };
    juce::ComboBox chordPresetBox;
    juce::TextButton presetNextBtn{ ">" };
    juce::TextButton triggerBtn{ "TRIGGER" };
    juce::TextButton advancedBtn{ "ADVANCED" };
    juce::TextButton invertBtn{ "INVERT" };

    std::unique_ptr<ComboAttachment> chordPresetAtt;

    // ---------------- SYMMETRICAL 6 MAIN KNOBS (Reference UI) ----------------
    // Left Wing: WARP (DRIVE), DRIFT (SPACE), DECAY
    juce::Slider warpKnob, driftKnob, decayKnob;
    // Right Wing: TILT (TONE), Q (DENSITY), DEPTH (MIX)
    juce::Slider filterTiltKnob, filterQKnob, depthKnob;

    std::unique_ptr<SliderAttachment> warpAtt, driftAtt, decayAtt;
    std::unique_ptr<SliderAttachment> filterTiltAtt, filterQAtt, depthAtt;

    // ---------------- COMPACT HARMONY / ARP / MOTION / RANGE STRIP ----------------
    std::array<juce::TextButton, 12> noteButtons;
    std::array<std::unique_ptr<ButtonAttachment>, 12> noteAtts;
    juce::ComboBox rootBox, octaveBox, spreadBox;
    juce::ToggleButton midiModeBtn{ "MIDI" };
    juce::ToggleButton arpOnBtn{ "ARP" };
    juce::ComboBox arpPatternBox;
    juce::ToggleButton motionOnBtn{ "MOTION" };
    juce::Slider filterRangeSlider;

    std::unique_ptr<ComboAttachment> rootAtt, octaveAtt, spreadAtt, arpPatternAtt;
    std::unique_ptr<ButtonAttachment> midiModeAtt, arpOnAtt, motionOnAtt;
    std::unique_ptr<DualRangeSliderAttachment> filterRangeAtt;

    // ---------------- STAGE 2: SLIDE-OUT ADVANCED DRAWER (4 TABS) ----------------
    float drawerAnimProgress = 0.0f; // 0.0 = closed, 1.0 = open (0.5s magazine slide)
    int currentAdvTab = 0;
    std::array<juce::TextButton, 4> advTabButtons;

    // Tab A: Voices
    juce::ComboBox voiceCapBox;
    std::unique_ptr<ComboAttachment> voiceCapAtt;
    std::array<juce::Slider, breach::kMaxVoices> vLevelSliders;
    std::array<juce::Slider, breach::kMaxVoices> vPanSliders;
    std::array<juce::Slider, breach::kMaxVoices> vDetuneSliders;
    std::array<juce::Slider, breach::kMaxVoices> vDecayMulSliders;
    std::array<std::unique_ptr<SliderAttachment>, breach::kMaxVoices> vLevelAtts;
    std::array<std::unique_ptr<SliderAttachment>, breach::kMaxVoices> vPanAtts;
    std::array<std::unique_ptr<SliderAttachment>, breach::kMaxVoices> vDetuneAtts;
    std::array<std::unique_ptr<SliderAttachment>, breach::kMaxVoices> vDecayMulAtts;

    // Tab B: Resonator
    juce::ComboBox resModelBox;
    juce::Slider attackKnob, fbDampingKnob, stiffnessKnob, excHpfKnob, excDriveKnob;
    std::unique_ptr<ComboAttachment> resModelAtt;
    std::unique_ptr<SliderAttachment> attackAtt, fbDampingAtt, stiffnessAtt, excHpfAtt, excDriveAtt;

    // Tab C: Modulation (2 LFOs + 1 Env Follower + 18-Route Matrix)
    juce::ComboBox lfo1ShapeBox, lfo2ShapeBox;
    juce::Slider lfo1RateKnob, lfo1PhaseKnob, lfo2RateKnob, lfo2PhaseKnob;
    juce::ToggleButton lfo1SyncBtn{ "SYNC" }, lfo2SyncBtn{ "SYNC" };
    juce::Slider envAttKnob, envRelKnob, envAmtKnob;
    juce::Slider shiftKnob, motionStrengthKnob, motionRateKnob, unisonKnob;
    std::array<std::array<juce::Slider, 6>, 3> modMatrixSliders;

    std::unique_ptr<ComboAttachment> lfo1ShapeAtt, lfo2ShapeAtt;
    std::unique_ptr<SliderAttachment> lfo1RateAtt, lfo1PhaseAtt, lfo2RateAtt, lfo2PhaseAtt;
    std::unique_ptr<ButtonAttachment> lfo1SyncAtt, lfo2SyncAtt;
    std::unique_ptr<SliderAttachment> envAttAtt, envRelAtt, envAmtAtt;
    std::unique_ptr<SliderAttachment> shiftAtt, motionStrengthAtt, motionRateAtt, unisonAtt;
    std::array<std::array<std::unique_ptr<SliderAttachment>, 6>, 3> modMatrixAtts;

    // Tab D: Global
    juce::Slider inGainKnob, outGainKnob, widthKnob, smoothTimeKnob, limiterThreshKnob;
    juce::ComboBox dwCurveBox, oversamplingBox;
    juce::ToggleButton limiterOnBtn{ "SOFT LIMITER" };

    std::unique_ptr<SliderAttachment> inGainAtt, outGainAtt, widthAtt, smoothTimeAtt, limiterThreshAtt;
    std::unique_ptr<ComboAttachment> dwCurveAtt, oversamplingAtt;
    std::unique_ptr<ButtonAttachment> limiterOnAtt;

    // ---------------- REALISTIC BULLET-WALL PHYSICS & ANIMATION ----------------
    breach::VisualTelemetry::Snapshot lastSnapshot;
    uint32_t lastTransientCount = 0;
    int flashFramesRemaining = 0;
    float slashCutProgress = 0.0f;
    float bulletSurgePos = 0.0f;      // Spring-damped penetration surge
    float bulletSurgeVel = 0.0f;
    float sfxPunchScale = 1.0f;
    float animClock = 0.0f;
    std::array<DebrisParticle, 28> debrisPool{};
    juce::Random visualRng{ 0x777 };

    juce::Rectangle<int> centerPanelBounds;
    juce::Rectangle<int> harmonyStripBounds;
    std::array<juce::Rectangle<int>, 5> modeSlotBounds;
    juce::Rectangle<int> advancedDrawerBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessorEditor)
};
