#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MangaLookAndFeel.h"
#include <array>
#include <vector>

class MangaSplitViewport : public juce::Component
{
public:
    MangaSplitViewport(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l);

    void paint(juce::Graphics& g) override;
    void tickAnimation();

private:
    struct DebrisParticle
    {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float size = 3.0f;
        float life = 0.0f;
    };

    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& laf;

    float recoilOffset = 0.0f;
    float recoilVel = 0.0f;
    float flashAlpha = 0.0f;
    float lastTransient = 0.0f;
    int   sfxIndex = 0;
    std::vector<DebrisParticle> particles;
};

class GunSelectorStrip : public juce::Component
{
public:
    GunSelectorStrip(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void drawGunIcon(juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour strokeCol);

    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& laf;
};

class BreachResonatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit BreachResonatorAudioProcessorEditor(BreachResonatorAudioProcessor&);
    ~BreachResonatorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateDrawerVisibility();
    void configureRotaryKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, bool showTextBox);

    BreachResonatorAudioProcessor& audioProcessor;
    MangaLookAndFeel mangaLaf;

    // Top bar controls
    juce::TextButton prevPresetBtn { "<" };
    juce::ComboBox   chordPresetBox;
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton panicBtn      { "PANIC" };
    juce::TextButton advancedBtn   { "ADVANCED" };
    juce::TextButton invertBtn     { "INVERT" };

    // Center Manga Split Viewport
    MangaSplitViewport viewport;

    // 6 Macro Knobs
    juce::Slider driveSlider, spaceSlider, decaySlider, toneSlider, densitySlider, mixSlider;
    juce::Label  driveLabel, spaceLabel, decayLabel, toneLabel, densityLabel, mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAtt, spaceAtt, decayAtt, toneAtt, densityAtt, mixAtt;

    // Chord & Arp Bar
    std::array<juce::TextButton, 12> noteButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> noteAtts;

    juce::ComboBox rootNoteBox, octaveBox, voiceCapBox;
    juce::TextButton midiBtn { "MIDI" };
    juce::TextButton arpBtn  { "ARP" };
    juce::ComboBox   arpDirBox;
    juce::TextButton motionBtn { "MOTION" };
    juce::Slider     arpRateSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAtt, rootAtt, octaveAtt, voiceCapAtt, arpDirAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   midiAtt, arpAtt, motionAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   arpRateAtt;

    // 5 Gun Topology Strip
    GunSelectorStrip gunStrip;

    // Advanced 4-Tab Drawer
    bool drawerExpanded = true;
    int  activeDrawerTab = 1; // Default to Tab B // RESONATOR

    juce::TextButton tabABtn { "A // VOICES" };
    juce::TextButton tabBBtn { "B // RESONATOR" };
    juce::TextButton tabCBtn { "C // MODULATION" };
    juce::TextButton tabDBtn { "D // GLOBAL" };

    // Tab A // VOICES controls
    struct VoiceRowControls
    {
        juce::Label  title;
        juce::Slider level, pan, detune, decay;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt, panAtt, detuneAtt, decayAtt;
    };
    std::array<VoiceRowControls, BreachResonatorAudioProcessor::kMaxVoices> voiceRows;
    juce::Label tabAHeaderLabel;

    // Tab B // RESONATOR controls
    juce::ComboBox resTopologyBox;
    juce::Slider   attackSlider, fbDampSlider, stiffnessSlider, excHpfSlider, excDriveSlider;
    juce::Label    attackLabel, fbDampLabel, stiffnessLabel, excHpfLabel, excDriveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> resTopoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   attackAtt, fbDampAtt, stiffnessAtt, excHpfAtt, excDriveAtt;

    // Tab C // MODULATION controls
    juce::ComboBox   lfo1ShapeBox, lfo2ShapeBox;
    juce::TextButton lfo1SyncBtn { "SYNC" }, lfo2SyncBtn { "SYNC" };
    juce::Slider     lfo1RateSlider, lfo1PhaseSlider, shiftSlider, unisonSlider;
    juce::Slider     lfo2RateSlider, lfo2PhaseSlider, envAttSlider, envRelSlider, envAmtSlider;
    juce::Label      lfo1RateLbl, lfo1PhaseLbl, shiftLbl, unisonLbl;
    juce::Label      lfo2RateLbl, lfo2PhaseLbl, envAttLbl, envRelLbl, envAmtLbl;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfo1ShapeAtt, lfo2ShapeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   lfo1SyncAtt, lfo2SyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   lfo1RateAtt, lfo1PhaseAtt, shiftAtt, unisonAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   lfo2RateAtt, lfo2PhaseAtt, envAttAtt, envRelAtt, envAmtAtt;

    std::array<juce::Label, 3> matrixRowLabels;
    std::array<juce::Label, 6> matrixColLabels;
    std::array<juce::Slider, 18> matrixSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 18> matrixAtts;

    // Tab D // GLOBAL controls
    juce::Slider     inGainSlider, outGainSlider, widthSlider, smoothSlider;
    juce::Label      inGainLbl, outGainLbl, widthLbl, mixCurveLbl, oversampleLbl, smoothLbl;
    juce::ComboBox   mixCurveBox, oversampleBox;
    juce::TextButton limiterBtn { "LIMITER: ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   inGainAtt, outGainAtt, widthAtt, smoothAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixCurveAtt, oversampleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   limiterAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessorEditor)
};
