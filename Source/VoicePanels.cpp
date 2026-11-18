#include "VoicePanels.h"

namespace
{
void configureKnob(juce::Component& owner, MangaLookAndFeel& look, juce::Slider& slider,
                   juce::Label& label, const char* name, float labelSize = 16)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    label.setText(name, juce::dontSendNotification);
    label.setFont(look.getMangaFont(labelSize, false));
    label.setJustificationType(juce::Justification::centred);
    owner.addAndMakeVisible(slider);
    owner.addAndMakeVisible(label);
}

constexpr const char* sourceNames[] {"LFO 1", "LFO 2", "ENVELOPE"};
constexpr const char* destinationNames[] {"PITCH WARP", "SMEAR", "FREQ SHIFT", "RESONANCE Q", "TONE", "DENSITY"};
constexpr const char* sourceKeys[] {"lfo1", "lfo2", "env"};
constexpr const char* destinationKeys[] {"warp", "drift", "shift", "q", "tilt", "depth"};
}

VoicePanel::VoicePanel(BreachResonatorAudioProcessor& p, MangaLookAndFeel& look) : processor(p)
{
    const char* macroNames[] {"TUNE SPREAD", "PAN SPREAD"};
    const char* macroIds[] {"voiceTuneSpread", "voicePanSpread"};
    const char* macroUnits[] {" ct", "%"};
    for (std::size_t i = 0; i < macros.size(); ++i)
    {
        configureKnob(*this, look, macros[i], macroLabels[i], macroNames[i], 21);
        macroAttachments[i] = std::make_unique<Attachment>(p.apvts, macroIds[i], macros[i]);
        macros[i].setTextValueSuffix(macroUnits[i]);
    }
    macros[0].setTooltip("Detune voices around the lowest voice. Zero adds no detuning. Single voices keep their pitch.");
    macros[1].setTooltip("Spread voices across the stereo field. Zero keeps each voice's original pan.");

    addAndMakeVisible(toggle);
    toggle.onClick = [this] { showDetails(!voice.isVisible()); };
    addAndMakeVisible(voice);
    voice.setName("VOICE SLOT");
    voice.setTooltip("Edit a voice slot, including stored settings for currently inactive slots.");
    for (int slot = 1; slot <= BreachResonatorAudioProcessor::kMaxVoices; ++slot)
        voice.addItem("V" + juce::String(slot), slot);
    voice.setSelectedId(1, juce::dontSendNotification);
    voice.onChange = [this] { selectVoice(); };
    const char* labels[] {"LEVEL", "PAN", "TUNE", "RING TRIM"};
    const char* tips[] {"Level of this voice.", "Pan of this voice before PAN SPREAD.",
        "Tune this voice in cents before TUNE SPREAD.", "Multiply the main RING TIME for this voice."};
    for (std::size_t i = 0; i < detail.size(); ++i)
    {
        configureKnob(*this, look, detail[i], detailLabels[i], labels[i]);
        detail[i].setTooltip(tips[i]);
    }
    selectVoice();
    showDetails(false);
}

void VoicePanel::selectVoice()
{
    const int slot = juce::jlimit(0, BreachResonatorAudioProcessor::kMaxVoices - 1, voice.getSelectedId() - 1);
    const char* prefixes[] {"vLevel_", "vPan_", "vDetune_", "vDecay_"};
    for (std::size_t i = 0; i < detail.size(); ++i)
    {
        detailAttachments[i].reset();
        detailAttachments[i] = std::make_unique<Attachment>(processor.apvts, juce::String(prefixes[i]) + juce::String(slot), detail[i]);
    }
    detail[2].setTextValueSuffix(" ct");
    detail[3].setTextValueSuffix("x");
}

void VoicePanel::showDetails(bool show)
{
    toggle.setButtonText(show ? "< VOICES" : "VOICE EDIT >");
    voice.setVisible(show);
    for (auto& slider : macros) slider.setVisible(!show);
    for (auto& label : macroLabels) label.setVisible(!show);
    for (auto& slider : detail) slider.setVisible(show);
    for (auto& label : detailLabels) label.setVisible(show);
}

void VoicePanel::resized()
{
    toggle.setBounds(getWidth() - 138, 6, 136, 26);
    voice.setBounds(getWidth() - 236, 6, 88, 26);
    for (std::size_t i = 0; i < macros.size(); ++i)
    {
        const int x = 145 + static_cast<int>(i) * 370;
        macroLabels[i].setBounds(x, 0, 230, 26);
        macros[i].setBounds(x + 66, 29, 98, 101);
    }
    for (std::size_t i = 0; i < detail.size(); ++i)
    {
        const int x = 26 + static_cast<int>(i) * 179;
        detailLabels[i].setBounds(x, 1, 130, 24);
        detail[i].setBounds(x + 21, 28, 88, 102);
    }
}

ModRoutePanel::ModRoutePanel(BreachResonatorAudioProcessor& p, MangaLookAndFeel& look)
    : processor(p), style(look)
{
    routes.setModel(this);
    routes.setRowHeight(22);
    routes.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(routes);
    for (int s = 0; s < 3; ++s)
    {
        source.addItem(sourceNames[s], s + 1);
        for (int d = 0; d < 6; ++d)
        {
            const auto id = "mod_" + juce::String(sourceKeys[s]) + "_" + destinationKeys[d];
            const auto route = static_cast<std::size_t>(s * 6 + d);
            parameters[route] = processor.apvts.getParameter(id);
            raw[route] = processor.apvts.getRawParameterValue(id);
        }
    }
    for (int d = 0; d < 6; ++d) destination.addItem(destinationNames[d], d + 1);
    source.setName("ROUTE SOURCE");
    destination.setName("ROUTE DESTINATION");
    source.setTextWhenNothingSelected("SOURCE");
    destination.setTextWhenNothingSelected("DESTINATION");
    source.onChange = destination.onChange = [this] { updateAddButton(); };
    addAndMakeVisible(source);
    addAndMakeVisible(destination);
    addAndMakeVisible(add);
    add.setTooltip("Add a route at 25% or select it if it already exists.");
    add.onClick = [this]
    {
        const int route = chosenRoute();
        if (route < 0) return;
        if (std::abs(raw[static_cast<std::size_t>(route)]->load()) < .005f) setRouteAmount(route, .25f);
        selectRoute(route);
        refresh();
        for (int row = 0; row < activeCount; ++row)
            if (active[static_cast<std::size_t>(row)] == route) routes.selectRow(row);
    };
    addAndMakeVisible(clear);
    clear.setTooltip("Set the selected route's amount to zero.");
    clear.onClick = [this]
    {
        if (selected < 0) return;
        setRouteAmount(selected, 0);
        selectRoute(-1);
        refresh();
    };
    configureKnob(*this, look, amount, amountLabel, "AMOUNT", 14);
    amount.setTooltip("Signed amount of the selected route.");
    amount.setDoubleClickReturnValue(true, 0);
    amount.setEnabled(false);
    clear.setEnabled(false);
    updateAddButton();
    refresh();
}

ModRoutePanel::~ModRoutePanel() { routes.setModel(nullptr); }

int ModRoutePanel::chosenRoute() const noexcept
{
    if (source.getSelectedId() <= 0 || destination.getSelectedId() <= 0) return -1;
    return (source.getSelectedId() - 1) * 6 + destination.getSelectedId() - 1;
}

void ModRoutePanel::updateAddButton() { add.setEnabled(chosenRoute() >= 0); }

void ModRoutePanel::setRouteAmount(int route, float value)
{
    const auto index = static_cast<std::size_t>(route);
    // End an existing slider binding before creating a separate host gesture.
    if (selected == route) selectRoute(-1);
    juce::ParameterAttachment attachment(*parameters[index], [](float) {}, nullptr);
    attachment.setValueAsCompleteGesture(value);
}

void ModRoutePanel::refresh()
{
    std::array<int, 18> next {};
    int count = 0;
    bool changed = false;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        const float value = raw[i]->load(std::memory_order_relaxed);
        changed = changed || std::abs(value - values[i]) > .0001f;
        values[i] = value;
        if (std::abs(value) >= .005f) next[static_cast<std::size_t>(count++)] = static_cast<int>(i);
    }
    if (count != activeCount || next != active)
    {
        juce::ScopedValueSetter<bool> guard(synchronizing, true);
        active = next;
        activeCount = count;
        routes.updateContent();
        int selectedRow = -1;
        for (int row = 0; row < count; ++row)
            if (active[static_cast<std::size_t>(row)] == selected) selectedRow = row;
        if (selectedRow >= 0) routes.selectRow(selectedRow, true);
        else routes.deselectAllRows();
        repaint();
    }
    if (changed) routes.repaint();
}

void ModRoutePanel::selectRoute(int route)
{
    if (route == selected) return;
    amountAttachment.reset();
    selected = route;
    const bool valid = route >= 0;
    amount.setEnabled(valid);
    amount.setAlpha(valid ? 1.0f : .4f);
    clear.setEnabled(valid);
    if (!valid)
    {
        amount.setValue(0, juce::dontSendNotification);
        return;
    }
    amountAttachment = std::make_unique<juce::SliderParameterAttachment>(*parameters[static_cast<std::size_t>(route)], amount, nullptr);
    amount.textFromValueFunction = [](double value) { return juce::String(value * 100, 0) + "%"; };
    amount.valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue() * .01; };
    amount.updateText();
}

int ModRoutePanel::getNumRows() { return activeCount; }

void ModRoutePanel::selectedRowsChanged(int row)
{
    if (!synchronizing && row >= 0 && row < activeCount) selectRoute(active[static_cast<std::size_t>(row)]);
}

void ModRoutePanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool highlighted)
{
    if (row < 0 || row >= activeCount) return;
    const auto route = static_cast<std::size_t>(active[static_cast<std::size_t>(row)]);
    const auto ink = style.getInkColour();
    const auto paper = style.getPaperColour();
    g.setColour(highlighted ? ink : paper);
    g.fillRect(0, 0, width, height);
    g.setColour(highlighted ? paper : ink);
    g.setFont(style.getMangaFont(13, false));
    const auto label = juce::String(sourceNames[route / 6]) + " > " + destinationNames[route % 6];
    g.drawText(label, 7, 0, width - 64, height, juce::Justification::centredLeft);
    const auto value = values[route] * 100;
    g.drawText((value > 0 ? "+" : "") + juce::String(value, 0) + "%", width - 60, 0, 52, height, juce::Justification::centredRight);
}

void ModRoutePanel::paint(juce::Graphics& g)
{
    const auto frame = routes.getBounds().expanded(1);
    g.setColour(style.getInkColour());
    g.drawRect(frame);
    if (activeCount == 0)
    {
        g.setFont(style.getMangaFont(16, false));
        g.drawText("NO ACTIVE ROUTES", frame, juce::Justification::centred);
    }
}

void ModRoutePanel::resized()
{
    const int listWidth = getWidth() - 111;
    routes.setBounds(3, 3, listWidth - 4, 94);
    source.setBounds(3, 106, 95, 25);
    destination.setBounds(104, 106, listWidth - 177, 25);
    add.setBounds(listWidth - 68, 106, 65, 25);
    amountLabel.setBounds(listWidth + 4, 0, 98, 19);
    amount.setBounds(listWidth + 15, 20, 76, 77);
    clear.setBounds(listWidth + 12, 106, 82, 25);
}
