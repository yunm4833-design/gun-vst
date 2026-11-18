#pragma once

#include "MangaLookAndFeel.h"
#include "PluginProcessor.h"

class OutputEqGraph final : public juce::Component, public juce::SettableTooltipClient
{
public:
    OutputEqGraph(BreachResonatorAudioProcessor&, MangaLookAndFeel&);
    ~OutputEqGraph() override;
    void refresh();
    void setSelectedBand(std::size_t band);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    std::function<void(std::size_t)> onSelect;

private:
    juce::Rectangle<float> plot() const;
    float frequencyX(double hz) const;
    float dbY(double db) const;
    juce::Point<float> node(std::size_t band) const;
    int hitTestNode(juce::Point<float>) const;
    void endDrag();

    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& style;
    std::array<breach::eq::BandSettings, breach::eq::bandCount> settings;
    breach::eq::Rates rates;
    std::size_t selected = 0;
    bool enabled = false;
    juce::RangedAudioParameter* dragFrequency = nullptr;
    juce::RangedAudioParameter* dragVertical = nullptr;
    juce::Point<float> dragOrigin;
    breach::eq::BandSettings dragSettings;
    bool draggingQ = false, frequencyMoved = false;
};

class OutputEqPanel final : public juce::Component
{
public:
    OutputEqPanel(BreachResonatorAudioProcessor&, MangaLookAndFeel&);
    void refresh();
    void resized() override;

private:
    void select(std::size_t band);
    void refreshControls();

    using Attachment = juce::AudioProcessorValueTreeState;
    BreachResonatorAudioProcessor& processor;
    OutputEqGraph graph;
    juce::TextButton enable {"EQ OFF"};
    juce::TextButton bandEnable {"BAND ON"};
    std::array<juce::TextButton, breach::eq::bandCount> bandButtons;
    std::array<juce::Slider, 3> sliders;
    std::array<juce::Label, 3> labels;
    juce::ComboBox type;
    std::unique_ptr<Attachment::ButtonAttachment> enableAttachment, bandAttachment;
    std::unique_ptr<Attachment::ComboBoxAttachment> typeAttachment;
    std::array<std::unique_ptr<Attachment::SliderAttachment>, 3> sliderAttachments;
    std::size_t selected = 0;
};
