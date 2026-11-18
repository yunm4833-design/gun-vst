#pragma once

#include "MangaLookAndFeel.h"
#include "PluginProcessor.h"

class VoicePanel final : public juce::Component
{
public:
    VoicePanel(BreachResonatorAudioProcessor&, MangaLookAndFeel&);
    void resized() override;

private:
    void showDetails(bool show);
    void selectVoice();
    BreachResonatorAudioProcessor& processor;
    juce::ComboBox voice;
    juce::TextButton toggle {"VOICE EDIT >"};
    std::array<juce::Slider, 2> macros;
    std::array<juce::Label, 2> macroLabels;
    std::array<juce::Slider, 4> detail;
    std::array<juce::Label, 4> detailLabels;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<Attachment>, 2> macroAttachments;
    std::array<std::unique_ptr<Attachment>, 4> detailAttachments;
};

class ModRoutePanel final : public juce::Component, private juce::ListBoxModel
{
public:
    ModRoutePanel(BreachResonatorAudioProcessor&, MangaLookAndFeel&);
    ~ModRoutePanel() override;
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool highlighted) override;
    void selectedRowsChanged(int row) override;
    void selectRoute(int route);
    void updateAddButton();
    void setRouteAmount(int route, float value);
    int chosenRoute() const noexcept;
    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& style;
    juce::ListBox routes {"Active modulation routes"};
    juce::ComboBox source, destination;
    juce::TextButton add {"ADD"}, clear {"CLEAR"};
    juce::Slider amount;
    juce::Label amountLabel;
    std::array<juce::RangedAudioParameter*, 18> parameters {};
    std::array<const std::atomic<float>*, 18> raw {};
    std::array<float, 18> values {};
    std::array<int, 18> active {};
    int activeCount = 0, selected = -1;
    bool synchronizing = false;
    std::unique_ptr<juce::SliderParameterAttachment> amountAttachment;
};
