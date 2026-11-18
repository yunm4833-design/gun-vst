#include "OutputEqPanel.h"

namespace
{
constexpr double topDb = 18, bottomDb = -54;
}

OutputEqGraph::OutputEqGraph(BreachResonatorAudioProcessor& p, MangaLookAndFeel& look)
    : processor(p), style(look)
{
    setName("OUTPUT EQ RESPONSE");
    setTooltip("Drag a band for frequency and gain. Alt-drag for Q. Cut and notch nodes use vertical drag for Q. Double-click to toggle the band.");
    refresh();
}

OutputEqGraph::~OutputEqGraph() { endDrag(); }

void OutputEqGraph::refresh()
{
    const auto nextRates = processor.getOutputEqRates();
    const bool nextEnabled = processor.apvts.getRawParameterValue("outputEqEnable")->load() > .5f;
    bool changed = std::abs(nextRates.base - rates.base) > 0 || std::abs(nextRates.core - rates.core) > 0 || nextEnabled != enabled;
    rates = nextRates;
    enabled = nextEnabled;
    for (std::size_t i = 0; i < settings.size(); ++i)
    {
        const auto next = processor.readOutputEqBand(i);
        const auto& old = settings[i];
        changed = changed || next.enabled != old.enabled || next.shape != old.shape
            || std::abs(next.hz - old.hz) + std::abs(next.q - old.q) + std::abs(next.gain - old.gain) > 0;
        settings[i] = next;
    }
    if (changed) repaint();
}

void OutputEqGraph::setSelectedBand(std::size_t band)
{
    selected = band;
    repaint();
}

juce::Rectangle<float> OutputEqGraph::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft(36).withTrimmedRight(14).withTrimmedTop(11).withTrimmedBottom(20);
}

float OutputEqGraph::frequencyX(double hz) const
{
    const auto area = plot();
    const auto proportion = std::log(std::clamp(hz, breach::eq::minHz, rates.upperFrequency()) / breach::eq::minHz)
                            / std::log(rates.upperFrequency() / breach::eq::minHz);
    return area.getX() + area.getWidth() * static_cast<float>(proportion);
}

float OutputEqGraph::dbY(double db) const
{
    const auto area = plot();
    return area.getY() + area.getHeight() * static_cast<float>((topDb - std::clamp(db, bottomDb, topDb)) / (topDb - bottomDb));
}

juce::Point<float> OutputEqGraph::node(std::size_t band) const
{
    const auto& s = settings[band];
    return {frequencyX(s.hz), dbY(breach::eq::hasGain(s.shape) ? s.gain : 0)};
}

int OutputEqGraph::hitTestNode(juce::Point<float> point) const
{
    int hit = -1;
    float closest = 13;
    for (std::size_t i = 0; i < settings.size(); ++i)
    {
        if (!settings[i].enabled && i != selected) continue;
        const float distance = point.getDistanceFrom(node(i));
        if (distance < closest || (distance <= closest && i == selected))
        {
            closest = distance;
            hit = static_cast<int>(i);
        }
    }
    return hit;
}

void OutputEqGraph::paint(juce::Graphics& g)
{
    const auto ink = style.getInkColour(), paper = style.getPaperColour();
    const auto area = plot();
    if (area.isEmpty()) return;
    g.setColour(style.getPanelTintColour());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(.75f), 3);
    g.setColour(ink);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.75f), 3, 1.25f);
    g.setFont(style.getMangaFont(9, false));
    for (const int db : {18, 0, -18, -36, -54})
    {
        const float y = dbY(db);
        g.setColour(ink.withAlpha(db == 0 ? .46f : .15f));
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
        g.setColour(ink.withAlpha(.8f));
        g.drawText(juce::String(db), 4, juce::roundToInt(y) - 7, 25, 14, juce::Justification::centredRight);
    }
    for (const int hz : {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000})
    {
        if (hz > rates.upperFrequency()) break;
        const float x = frequencyX(hz);
        g.setColour(ink.withAlpha(.15f));
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
        g.setColour(ink.withAlpha(.8f));
        const auto label = hz < 1000 ? juce::String(hz) : juce::String(hz / 1000) + "K";
        g.drawText(label, juce::roundToInt(x) - 15, getHeight() - 18, 30, 14, juce::Justification::centred);
    }

    std::array<breach::eq::Filter, breach::eq::bandCount> filters;
    for (std::size_t i = 0; i < filters.size(); ++i)
        if (settings[i].enabled) filters[i] = breach::eq::design(settings[i], rates);
    juce::Path response;
    constexpr int points = 400;
    const double ratio = rates.upperFrequency() / breach::eq::minHz;
    for (int i = 0; i <= points; ++i)
    {
        const double proportion = static_cast<double>(i) / points;
        const double hz = breach::eq::minHz * std::pow(ratio, proportion);
        const auto z = std::polar(1.0, -2 * breach::eq::pi * hz / rates.core);
        double db = 0;
        for (const auto& filter : filters) db += filter.magnitudeDb(z, z * z);
        const float x = area.getX() + area.getWidth() * static_cast<float>(proportion);
        if (i == 0) response.startNewSubPath(x, dbY(db));
        else response.lineTo(x, dbY(db));
    }
    if (enabled)
    {
        auto fill = response;
        fill.lineTo(area.getRight(), dbY(0));
        fill.lineTo(area.getX(), dbY(0));
        fill.closeSubPath();
        g.setColour(ink.withAlpha(.10f));
        g.fillPath(fill);
        g.setColour(ink);
        g.strokePath(response, juce::PathStrokeType(2, juce::PathStrokeType::curved));
    }
    else
    {
        juce::Path dashed;
        constexpr float dashes[] {4, 3};
        juce::PathStrokeType(1.3f).createDashedStroke(dashed, response, dashes, 2);
        g.setColour(ink.withAlpha(.49f));
        g.fillPath(dashed);
        g.setColour(ink);
        g.drawHorizontalLine(juce::roundToInt(dbY(0)), area.getX(), area.getRight());
        g.drawText("BYPASSED", getWidth() - 83, 7, 69, 16, juce::Justification::centredRight);
    }

    auto drawNode = [&](std::size_t index)
    {
        const bool picked = index == selected;
        const auto p = node(index);
        const float radius = picked ? 9 : 7;
        const juce::Rectangle<float> bounds(p.x - radius, p.y - radius, 2 * radius, 2 * radius);
        g.setColour(picked ? ink : paper);
        g.fillEllipse(bounds);
        g.setColour(ink);
        g.drawEllipse(bounds, 1.3f);
        g.setColour(picked ? paper : ink);
        g.setFont(style.getMangaFont(10));
        g.drawText(juce::String(static_cast<int>(index + 1)), bounds.toNearestInt(), juce::Justification::centred);
    };
    for (std::size_t i = 0; i < settings.size(); ++i)
        if (i != selected && settings[i].enabled) drawNode(i);
    drawNode(selected);
}

void OutputEqGraph::mouseDown(const juce::MouseEvent& event)
{
    endDrag();
    if (!event.mods.isLeftButtonDown()) return;
    const int hit = hitTestNode(event.position);
    if (hit < 0) return;
    const auto band = static_cast<std::size_t>(hit);
    if (onSelect) onSelect(band);
    dragOrigin = event.position;
    frequencyMoved = false;
    dragSettings = settings[band];
    draggingQ = breach::eq::hasQ(dragSettings.shape) && (event.mods.isAltDown() || !breach::eq::hasGain(dragSettings.shape));
    auto param = [&](const char* suffix) { return processor.apvts.getParameter(processor.outputEqBandId(band, suffix)); };
    dragFrequency = param("Freq");
    if (draggingQ) dragVertical = param("Q");
    else if (breach::eq::hasGain(dragSettings.shape)) dragVertical = param("Gain");
    dragFrequency->beginChangeGesture();
    if (dragVertical != nullptr) dragVertical->beginChangeGesture();
}

void OutputEqGraph::mouseDrag(const juce::MouseEvent& event)
{
    if (dragFrequency == nullptr || plot().isEmpty()) return;
    const auto delta = event.position - dragOrigin;
    auto write = [](auto* parameter, double value)
    {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
    };
    frequencyMoved = frequencyMoved || std::abs(delta.x) > 0;
    if (frequencyMoved)
    {
        const double extent = rates.upperFrequency();
        const double hz = std::min(dragSettings.hz, extent) * std::pow(extent / breach::eq::minHz, delta.x / plot().getWidth());
        write(dragFrequency, std::clamp(hz, breach::eq::minHz, extent));
    }
    if (dragVertical != nullptr)
    {
        const double value = draggingQ ? std::clamp(dragSettings.q * std::exp(-delta.y * .025), .25, 8.0)
                                     : std::clamp(dragSettings.gain - delta.y * (topDb - bottomDb) / plot().getHeight(), -18.0, 18.0);
        write(dragVertical, value);
    }
    refresh();
}

void OutputEqGraph::mouseUp(const juce::MouseEvent&) { endDrag(); }

void OutputEqGraph::endDrag()
{
    if (dragFrequency != nullptr) dragFrequency->endChangeGesture();
    if (dragVertical != nullptr) dragVertical->endChangeGesture();
    dragFrequency = dragVertical = nullptr;
}

void OutputEqGraph::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) return;
    const int hit = hitTestNode(event.position);
    if (hit < 0) return;
    const auto band = static_cast<std::size_t>(hit);
    if (onSelect) onSelect(band);
    auto* parameter = processor.apvts.getParameter(processor.outputEqBandId(band, "Enable"));
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(settings[band].enabled ? 0 : 1);
    parameter->endChangeGesture();
    refresh();
}

OutputEqPanel::OutputEqPanel(BreachResonatorAudioProcessor& p, MangaLookAndFeel& style)
    : processor(p), graph(p, style)
{
    setName("OUTPUT EQ");
    graph.onSelect = [this](std::size_t band) { select(band); };
    addAndMakeVisible(graph);
    enable.setClickingTogglesState(true);
    enable.setTooltip("Equalize the full BREACH mix before reverb and the output limiter.");
    addAndMakeVisible(enable);
    enableAttachment = std::make_unique<Attachment::ButtonAttachment>(processor.apvts, "outputEqEnable", enable);
    enable.onClick = [this] { refresh(); };
    bandEnable.setClickingTogglesState(true);
    bandEnable.setTooltip("Enable the selected filter band.");
    addAndMakeVisible(bandEnable);
    bandEnable.onClick = [this] { refresh(); };
    addAndMakeVisible(type);
    type.setName("EQ FILTER TYPE");
    for (std::size_t i = 0; i < breach::eq::shapeNames.size(); ++i)
        type.addItem(breach::eq::shapeNames[i], static_cast<int>(i + 1));
    type.onChange = [this] { refresh(); };

    for (std::size_t i = 0; i < bandButtons.size(); ++i)
    {
        auto& button = bandButtons[i];
        button.setName("EQ BAND " + juce::String(static_cast<int>(i + 1)));
        button.setTooltip("Select band " + juce::String(static_cast<int>(i + 1)) + ". A dot marks an enabled band.");
        button.onClick = [this, i] { select(i); };
        addAndMakeVisible(button);
    }
    const char* const names[] {"FREQ (HZ)", "GAIN (DB)", "Q"};
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        slider.setName("EQ " + juce::String(names[i]));
        slider.setTooltip(names[i]);
        slider.onValueChange = [this] { graph.refresh(); };
        addAndMakeVisible(slider);
        auto& label = labels[i];
        label.setText(names[i], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(style.getMangaFont(11));
        addAndMakeVisible(label);
    }
    select(0);
}

void OutputEqPanel::select(std::size_t band)
{
    if (band != selected || bandAttachment == nullptr)
    {
        selected = band;
        bandAttachment.reset();
        typeAttachment.reset();
        for (auto& attachment : sliderAttachments) attachment.reset();
        auto id = [&](const char* suffix) { return processor.outputEqBandId(band, suffix); };
        bandAttachment = std::make_unique<Attachment::ButtonAttachment>(processor.apvts, id("Enable"), bandEnable);
        typeAttachment = std::make_unique<Attachment::ComboBoxAttachment>(processor.apvts, id("Type"), type);
        const char* const suffixes[] {"Freq", "Gain", "Q"};
        for (std::size_t i = 0; i < sliders.size(); ++i)
            sliderAttachments[i] = std::make_unique<Attachment::SliderAttachment>(processor.apvts, id(suffixes[i]), sliders[i]);
    }
    graph.setSelectedBand(band);
    refresh();
}

void OutputEqPanel::refresh()
{
    graph.refresh();
    refreshControls();
}

void OutputEqPanel::refreshControls()
{
    enable.setButtonText(enable.getToggleState() ? "EQ ON" : "EQ OFF");
    bandEnable.setButtonText(bandEnable.getToggleState() ? "BAND ON" : "BAND OFF");
    const auto shape = processor.readOutputEqBand(selected).shape;
    auto enableField = [this](std::size_t index, bool active)
    {
        sliders[index].setEnabled(active);
        sliders[index].setAlpha(active ? 1.0f : .4f);
        labels[index].setEnabled(active);
    };
    enableField(1, breach::eq::hasGain(shape));
    enableField(2, breach::eq::hasQ(shape));
    for (std::size_t i = 0; i < bandButtons.size(); ++i)
    {
        auto& button = bandButtons[i];
        const bool active = processor.readOutputEqBand(i).enabled;
        button.setButtonText(juce::String(static_cast<int>(i + 1)) + (active ? " ." : ""));
        button.setToggleState(i == selected, juce::dontSendNotification);
    }
}

void OutputEqPanel::resized()
{
    auto area = getLocalBounds();
    auto controls = area.removeFromRight(274);
    area.removeFromRight(12);
    graph.setBounds(area);
    auto bands = controls.removeFromTop(25);
    for (auto& button : bandButtons) button.setBounds(bands.removeFromLeft(34).reduced(2, 0));
    controls.removeFromTop(5);
    auto options = controls.removeFromTop(25);
    enable.setBounds(options.removeFromLeft(65).reduced(1, 0));
    bandEnable.setBounds(options.removeFromLeft(74).reduced(1, 0));
    type.setBounds(options.reduced(1, 0));
    controls.removeFromTop(4);
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        auto column = controls.removeFromLeft(91).reduced(4, 0);
        labels[i].setBounds(column.removeFromTop(16));
        sliders[i].setBounds(column);
    }
}
