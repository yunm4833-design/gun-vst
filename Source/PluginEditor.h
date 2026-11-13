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
    void timerCallback() override;
    void selectMode(int modeIdx);
    void drawFirearmSilhouette(juce::Graphics& g, int modeIdx, juce::Rectangle<float> area, juce::Colour inkCol);
    void drawImpactPanel(juce::Graphics& g, juce::Rectangle<int> bounds);

    BreachResonatorAudioProcessor& audioProcessor;
    MangaKnobLookAndFeel mangaLnf;
    bool isInverted = false;

    // 6 Symmetrical Main Knobs
    juce::Slider decayKnob, spaceKnob, driftKnob;
    juce::Slider toneKnob, shiftKnob, depthKnob;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> decayAtt, spaceAtt, driftAtt;
    std::unique_ptr<SliderAttachment> toneAtt, shiftAtt, depthAtt;

    // Top Chord / Arp Quick Controls
    juce::ComboBox rootBox, chordBox, voicesBox;
    juce::ToggleButton arpButton{ "ARP" };
    juce::TextButton invertButton{ "INVERT" };

    std::unique_ptr<ComboAttachment> rootAtt, chordAtt, voicesAtt;
    std::unique_ptr<ButtonAttachment> arpAtt;

    // Telemetry & Manga Animation State
    breach::VisualTelemetry::Snapshot lastSnapshot;
    uint32_t lastTransientCount = 0;
    int flashFramesRemaining = 0;
    float slashCutProgress = 0.0f;

    juce::Rectangle<int> centerPanelBounds;
    std::array<juce::Rectangle<int>, 5> modeSlotBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreachResonatorAudioProcessorEditor)
};
