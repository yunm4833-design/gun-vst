#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MangaLookAndFeel.h"
#include "MangaControls.h"
#include "OutputEqPanel.h"
#include "VoicePanels.h"
#include "ImpactPanel.h"
#include <array>

class GunSelectorStrip : public juce::Component
{
public:
    GunSelectorStrip(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void drawGunIcon(juce::Graphics& g, int index, juce::Rectangle<float> area,
                     juce::Colour foreground, juce::Colour background);

    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& laf;
    std::array<std::unique_ptr<juce::Drawable>, 5> icons;
    std::array<std::unique_ptr<juce::Drawable>, 5> tintedIcons;
    std::array<juce::Colour, 5> iconColours;
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
    class OttGainMeters : public juce::Component
    {
    public:
        OttGainMeters(const VisualTelemetry& source, const MangaLookAndFeel& theme)
            : telemetry(source), laf(theme) {}
        void paint(juce::Graphics&) override;
    private:
        const VisualTelemetry& telemetry;
        const MangaLookAndFeel& laf;
    };

    void updateDrawerVisibility();
    void refreshOttPanel();
    void configureRotaryKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, bool showTextBox);

    BreachResonatorAudioProcessor& audioProcessor;
    MangaLookAndFeel mangaLaf;

    // Top bar controls
    juce::TextButton prevPresetBtn { "<" };
    juce::ComboBox   chordPresetBox;
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton panicBtn      { "TRIGGER" };
    juce::TextButton advancedBtn   { "ADVANCED" };
    juce::TextButton invertBtn     { "INVERT" };

    // Center Manga Split Viewport
    MangaSplitViewport viewport;
    MangaKeyboard keyboard;
    MangaSettingsBackdrop settingsBackdrop;
    VoicePanel voicePanel;
    ModRoutePanel modRoutePanel;
    juce::TextButton panicResetBtn { "PANIC" };
    juce::TextButton closeSettingsBtn { "CLOSE" };
    juce::TooltipWindow tooltipWindow { this, 650 };
    bool performanceSettings = false;

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

    // Advanced Drawer
    bool drawerExpanded = true;
    int  activeDrawerTab = 0; // Reference opens the voice macros.

    juce::TextButton tabABtn { "A // VOICES" };
    juce::TextButton tabBBtn { "B // RESONATOR" };
    juce::TextButton tabCBtn { "C // MODULATION" };
    juce::TextButton tabDBtn { "D // GLOBAL" };
    juce::TextButton tabEBtn { "E // DYNAMICS" }, tabFBtn { "F // REVERB" }, tabGBtn { "G // EQ" };
    OutputEqPanel eqPanel;

    // Tab B // RESONATOR controls
    juce::ComboBox resTopologyBox;
    juce::TextButton smearRevisionBtn { "SMEAR: SHORT" };
    juce::TextButton ringTimeBtn { "RING: NEW" };
    juce::Label smearInfo;
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

    // Tab D // GLOBAL controls
    juce::Slider     inGainSlider, outGainSlider, widthSlider, smoothSlider;
    juce::Label      inGainLbl, outGainLbl, widthLbl, mixCurveLbl, oversampleLbl, smoothLbl;
    juce::ComboBox   mixCurveBox, oversampleBox;
    juce::TextButton balanceRevisionBtn { "LEVELS: BALANCED" };
    juce::TextButton modalRevisionBtn { "MODAL: RINGS" };
    juce::TextButton formantRevisionBtn { "FORMANT: STK" };
    juce::TextButton stringRevisionBtn { "STRING: RINGS" };
    juce::TextButton nonlinearRevisionBtn { "DRIVE DSP: NEW" };
    juce::TextButton spatialRevisionBtn { "SPATIAL: NEW" };
    juce::TextButton limiterBtn { "LIMITER: ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   inGainAtt, outGainAtt, widthAtt, smoothAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixCurveAtt, oversampleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   limiterAtt;

    // Tab E // DYNAMICS and Tab F // REVERB
    std::array<juce::Slider,8> ottSliders;
    std::array<juce::Label,8> ottLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,8> ottAttachments;
    juce::TextButton ottEnableBtn { "OTT: OFF" };
    juce::TextButton ottSetupBtn { "SETUP >" };
    juce::TextButton ottModelBtn { "MODEL: FAUST" };
    juce::Label ottHeader;
    OttGainMeters ottMeters;
    bool ottSetupOpen = false;
    bool ottFaustDisplayed = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ottEnableAttachment;
    std::array<juce::Slider,6> reverbSliders;
    std::array<juce::Label,6> reverbLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,6> reverbAttachments;
    juce::TextButton reverbEnableBtn { "REVERB: OFF" };
    juce::ComboBox reverbTypeBox;
    juce::Label reverbHeader,reverbInfo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverbEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbTypeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessorEditor)
};
