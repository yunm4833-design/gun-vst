#include "PluginEditor.h"
#include <cmath>

void MangaKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider&)
{
    const auto paper = getPaper();
    const auto ink   = getInk();

    const float diameter = static_cast<float>(std::min(width, height)) - 14.0f;
    const float radius   = std::max(10.0f, diameter * 0.5f);
    const float cx       = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy       = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    // 16 Radial manga calibration ticks (matching reference UI)
    constexpr int numTicks = 16;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
        const bool isMajor = (i == 0 || i == numTicks / 2 || i == numTicks - 1);
        const float rInner = radius + 2.5f;
        const float rOuter = radius + (isMajor ? 7.5f : 5.0f);

        g.setColour(ink);
        g.drawLine(cx + std::cos(angle) * rInner, cy + std::sin(angle) * rInner,
                   cx + std::cos(angle) * rOuter, cy + std::sin(angle) * rOuter,
                   isMajor ? 1.8f : 1.1f);
    }

    g.setColour(paper);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(ink);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.2f);
    g.drawEllipse(cx - radius * 0.80f, cy - radius * 0.80f, radius * 1.60f, radius * 1.60f, 0.9f);

    const float pointerAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle)
                             - juce::MathConstants<float>::halfPi;
    g.drawLine(cx, cy,
               cx + std::cos(pointerAngle) * (radius - 2.0f),
               cy + std::sin(pointerAngle) * (radius - 2.0f),
               2.8f);

    g.fillEllipse(cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
}

void MangaKnobLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style, juce::Slider&)
{
    const auto paper = getPaper();
    const auto ink   = getInk();

    if (style == juce::Slider::TwoValueHorizontal)
    {
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float leftX  = static_cast<float>(x) + 6.0f;
        const float rightX = static_cast<float>(x + width) - 6.0f;

        g.setColour(ink);
        g.drawRect(leftX, cy - 3.5f, rightX - leftX, 7.0f, 1.4f);

        const float lo = juce::jlimit(leftX, rightX, minSliderPos);
        const float hi = juce::jlimit(lo, rightX, maxSliderPos);
        g.fillRect(lo, cy - 3.5f, std::max(2.0f, hi - lo), 7.0f);

        auto drawThumb = [&](float tx)
        {
            g.setColour(paper);
            g.fillRect(tx - 4.5f, cy - 7.5f, 9.0f, 15.0f);
            g.setColour(ink);
            g.drawRect(tx - 4.5f, cy - 7.5f, 9.0f, 15.0f, 1.8f);
        };
        drawThumb(lo);
        drawThumb(hi);
    }
    else if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float leftX  = static_cast<float>(x) + 4.0f;
        const float rightX = static_cast<float>(x + width) - 4.0f;
        g.setColour(ink);
        g.drawRect(leftX, cy - 3.0f, rightX - leftX, 6.0f, 1.2f);
        const float pos = juce::jlimit(leftX, rightX, sliderPos);
        g.fillRect(leftX, cy - 3.0f, pos - leftX, 6.0f);
        g.setColour(paper);
        g.fillRect(pos - 3.5f, cy - 6.5f, 7.0f, 13.0f);
        g.setColour(ink);
        g.drawRect(pos - 3.5f, cy - 6.5f, 7.0f, 13.0f, 1.5f);
    }
}

void MangaKnobLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool, bool)
{
    const bool active = button.getToggleState() || button.isDown();
    const auto bg = active ? getInk() : getPaper();
    const auto fg = active ? getPaper() : getInk();

    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(fg);
    g.drawRoundedRectangle(bounds, 3.0f, 1.8f);
}

void MangaKnobLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool, bool)
{
    const bool active = button.getToggleState();
    const auto bg = active ? getInk() : getPaper();
    const auto fg = active ? getPaper() : getInk();

    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(getInk());
    g.drawRoundedRectangle(bounds, 3.0f, 1.6f);

    g.setColour(fg);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

// ============================================================================
// DualRangeSliderAttachment
// ============================================================================
DualRangeSliderAttachment::DualRangeSliderAttachment(juce::AudioProcessorValueTreeState& state,
                                                     const juce::String& lowParamId,
                                                     const juce::String& highParamId,
                                                     juce::Slider& sliderToControl)
    : apvts(state), lowId(lowParamId), highId(highParamId), slider(sliderToControl)
{
    slider.setSliderStyle(juce::Slider::TwoValueHorizontal);
    slider.setRange(20.0, 20000.0, 1.0);
    slider.setSkewFactorFromMidPoint(1000.0);

    apvts.addParameterListener(lowId, this);
    apvts.addParameterListener(highId, this);
    slider.addListener(this);

    syncFromParameters();
}

DualRangeSliderAttachment::~DualRangeSliderAttachment()
{
    slider.removeListener(this);
    apvts.removeParameterListener(lowId, this);
    apvts.removeParameterListener(highId, this);
}

void DualRangeSliderAttachment::syncFromParameters()
{
    if (isUpdating) return;
    isUpdating = true;
    const float lo = apvts.getRawParameterValue(lowId)->load(std::memory_order_relaxed);
    const float hi = apvts.getRawParameterValue(highId)->load(std::memory_order_relaxed);
    slider.setMinAndMaxValues(lo, std::max(lo + 40.0f, hi), juce::dontSendNotification);
    isUpdating = false;
}

void DualRangeSliderAttachment::sliderValueChanged(juce::Slider*)
{
    if (isUpdating) return;
    isUpdating = true;
    const float lo = static_cast<float>(slider.getMinValue());
    const float hi = static_cast<float>(std::max(slider.getMinValue() + 40.0, slider.getMaxValue()));
    if (auto* pLow = apvts.getParameter(lowId))
        pLow->setValueNotifyingHost(pLow->convertTo0to1(lo));
    if (auto* pHigh = apvts.getParameter(highId))
        pHigh->setValueNotifyingHost(pHigh->convertTo0to1(hi));
    isUpdating = false;
}

void DualRangeSliderAttachment::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([this] { syncFromParameters(); });
}

// ============================================================================
// BreachResonatorAudioProcessorEditor
// ============================================================================
BreachResonatorAudioProcessorEditor::BreachResonatorAudioProcessorEditor(BreachResonatorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&mangaLnf);

    // Restore Stage 2 Advanced Drawer State
    const bool startOpen = audioProcessor.isAdvancedPanelOpen();
    drawerAnimProgress = startOpen ? 1.0f : 0.0f;
    currentAdvTab = audioProcessor.getAdvancedActiveTab();
    advancedBtn.setToggleState(startOpen, juce::dontSendNotification);

    setSize(kBaseWidth, kBaseHeight + static_cast<int>(std::round(drawerAnimProgress * kAdvancedHeight)));

    auto configureKnob = [this](juce::Slider& s, std::unique_ptr<SliderAttachment>& att, const char* paramId, bool showText = true)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(showText ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 64, 14);
        addAndMakeVisible(s);
        att = std::make_unique<SliderAttachment>(audioProcessor.getAPVTS(), paramId, s);
    };

    // ---------------- TOP HEADER BAR ----------------
    presetPrevBtn.onClick = [this]
    {
        const int cur = chordPresetBox.getSelectedItemIndex();
        const int next = (cur <= 1) ? (chordPresetBox.getNumItems() - 1) : (cur - 1);
        chordPresetBox.setSelectedItemIndex(next, juce::sendNotification);
    };
    addAndMakeVisible(presetPrevBtn);

    chordPresetBox.addItemList({
        "CUSTOM", "MAJOR TRIAD", "MINOR TRIAD", "MAJOR 7TH", "MINOR 7TH",
        "DOMINANT 7TH", "SUS2", "SUS4", "DIM7", "AUG", "PENTATONIC"
    }, 1);
    addAndMakeVisible(chordPresetBox);
    chordPresetAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::chordPreset, chordPresetBox);

    presetNextBtn.onClick = [this]
    {
        const int cur = chordPresetBox.getSelectedItemIndex();
        const int next = (cur + 1 >= chordPresetBox.getNumItems()) ? 1 : (cur + 1);
        chordPresetBox.setSelectedItemIndex(next, juce::sendNotification);
    };
    addAndMakeVisible(presetNextBtn);

    triggerBtn.onClick = [this]
    {
        audioProcessor.triggerManualImpulse();
        bulletSurgeVel += 18.0f;
        sfxPunchScale = 1.32f;
        spawnImpactDebris(1.0f);
    };
    addAndMakeVisible(triggerBtn);

    advancedBtn.setClickingTogglesState(true);
    advancedBtn.onClick = [this]
    {
        const bool open = advancedBtn.getToggleState();
        audioProcessor.setAdvancedPanelOpen(open);
    };
    addAndMakeVisible(advancedBtn);

    invertBtn.onClick = [this]
    {
        isInverted = !isInverted;
        mangaLnf.inverted = isInverted;
        repaint();
    };
    addAndMakeVisible(invertBtn);

    // ---------------- SYMMETRICAL 6 MAIN KNOBS ----------------
    configureKnob(warpKnob,       warpAtt,       breach::ParamID::warp, false);
    configureKnob(driftKnob,      driftAtt,      breach::ParamID::drift, false);
    configureKnob(decayKnob,      decayAtt,      breach::ParamID::decay, false);
    configureKnob(filterTiltKnob, filterTiltAtt, breach::ParamID::filterTilt, false);
    configureKnob(filterQKnob,    filterQAtt,    breach::ParamID::filterQ, false);
    configureKnob(depthKnob,      depthAtt,      breach::ParamID::depth, false);

    // ---------------- COMPACT HARMONY / ARP / MOTION / RANGE STRIP ----------------
    static constexpr std::array<const char*, 12> kNoteNames{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (size_t i = 0; i < 12; ++i)
    {
        noteButtons[i].setButtonText(kNoteNames[i]);
        noteButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(noteButtons[i]);
        noteAtts[i] = std::make_unique<ButtonAttachment>(
            audioProcessor.getAPVTS(), breach::ParamID::noteToggles[i], noteButtons[i]);
    }

    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible(rootBox);
    rootAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::rootNote, rootBox);

    octaveBox.addItemList({ "OCT 1", "OCT 2", "OCT 3", "OCT 4", "OCT 5", "OCT 6" }, 1);
    addAndMakeVisible(octaveBox);
    octaveAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::octave, octaveBox);

    spreadBox.addItemList({ "1 OCT", "2 OCT", "3 OCT", "4 OCT" }, 1);
    addAndMakeVisible(spreadBox);
    spreadAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::spread, spreadBox);

    addAndMakeVisible(midiModeBtn);
    midiModeAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::midiMode, midiModeBtn);

    addAndMakeVisible(arpOnBtn);
    arpOnAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::arpOn, arpOnBtn);

    arpPatternBox.addItemList({ "UP", "DOWN", "UP-DN", "RAND", "CHORD" }, 1);
    addAndMakeVisible(arpPatternBox);
    arpPatternAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::arpPattern, arpPatternBox);

    addAndMakeVisible(motionOnBtn);
    motionOnAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::motionOn, motionOnBtn);

    filterRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filterRangeSlider);
    filterRangeAtt = std::make_unique<DualRangeSliderAttachment>(
        audioProcessor.getAPVTS(), breach::ParamID::filterLow, breach::ParamID::filterHigh, filterRangeSlider);

    // ---------------- STAGE 2: ADVANCED PANEL CONTROLS ----------------
    static constexpr std::array<const char*, 4> kTabTitles{
        "A // VOICES", "B // RESONATOR", "C // MODULATION", "D // GLOBAL"
    };
    for (int t = 0; t < 4; ++t)
    {
        auto& btn = advTabButtons[static_cast<size_t>(t)];
        btn.setButtonText(kTabTitles[static_cast<size_t>(t)]);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(404);
        btn.setToggleState(t == currentAdvTab, juce::dontSendNotification);
        btn.onClick = [this, t] { setAdvancedTab(t); };
        addAndMakeVisible(btn);
    }

    // Tab A: Voices
    voiceCapBox.addItemList({ "MAX 4 VOICES", "MAX 6 VOICES", "MAX 8 VOICES", "MAX 12 VOICES" }, 1);
    addAndMakeVisible(voiceCapBox);
    voiceCapAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::voiceCap, voiceCapBox);

    for (size_t v = 0; v < breach::kMaxVoices; ++v)
    {
        auto setupBar = [this](juce::Slider& s, std::unique_ptr<SliderAttachment>& att, const char* id)
        {
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(s);
            att = std::make_unique<SliderAttachment>(audioProcessor.getAPVTS(), id, s);
        };
        setupBar(vLevelSliders[v],    vLevelAtts[v],    breach::ParamID::voiceLevel[v]);
        setupBar(vPanSliders[v],      vPanAtts[v],      breach::ParamID::voicePan[v]);
        setupBar(vDetuneSliders[v],   vDetuneAtts[v],   breach::ParamID::voiceDetune[v]);
        setupBar(vDecayMulSliders[v], vDecayMulAtts[v], breach::ParamID::voiceDecayMul[v]);
    }

    // Tab B: Resonator
    resModelBox.addItemList({ "AUTO (GUN MODE)", "BANDPASS SVF", "COMB STRING", "DUAL FORMANT" }, 1);
    addAndMakeVisible(resModelBox);
    resModelAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::resModel, resModelBox);

    configureKnob(attackKnob,    attackAtt,    breach::ParamID::attack, true);
    configureKnob(fbDampingKnob, fbDampingAtt, breach::ParamID::fbDamping, true);
    configureKnob(stiffnessKnob, stiffnessAtt, breach::ParamID::stiffness, true);
    configureKnob(excHpfKnob,    excHpfAtt,    breach::ParamID::excHpfHz, true);
    configureKnob(excDriveKnob,  excDriveAtt,  breach::ParamID::excDrive, true);

    // Tab C: Modulation
    lfo1ShapeBox.addItemList({ "SINE", "TRI", "SQR", "RAND", "SAW" }, 1);
    addAndMakeVisible(lfo1ShapeBox);
    lfo1ShapeAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::advLfo1Shape, lfo1ShapeBox);

    lfo2ShapeBox.addItemList({ "SINE", "TRI", "SQR", "RAND", "SAW" }, 1);
    addAndMakeVisible(lfo2ShapeBox);
    lfo2ShapeAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::advLfo2Shape, lfo2ShapeBox);

    configureKnob(lfo1RateKnob,  lfo1RateAtt,  breach::ParamID::advLfo1RateHz, true);
    configureKnob(lfo1PhaseKnob, lfo1PhaseAtt, breach::ParamID::advLfo1Phase, true);
    addAndMakeVisible(lfo1SyncBtn);
    lfo1SyncAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::advLfo1Sync, lfo1SyncBtn);

    configureKnob(lfo2RateKnob,  lfo2RateAtt,  breach::ParamID::advLfo2RateHz, true);
    configureKnob(lfo2PhaseKnob, lfo2PhaseAtt, breach::ParamID::advLfo2Phase, true);
    addAndMakeVisible(lfo2SyncBtn);
    lfo2SyncAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::advLfo2Sync, lfo2SyncBtn);

    configureKnob(envAttKnob, envAttAtt, breach::ParamID::advEnvAttack, true);
    configureKnob(envRelKnob, envRelAtt, breach::ParamID::advEnvRelease, true);
    configureKnob(envAmtKnob, envAmtAtt, breach::ParamID::advEnvAmount, true);

    configureKnob(shiftKnob,          shiftAtt,          breach::ParamID::shiftHz, true);
    configureKnob(motionStrengthKnob, motionStrengthAtt, breach::ParamID::motionStrength, true);
    configureKnob(motionRateKnob,     motionRateAtt,     breach::ParamID::motionRateHz, true);
    configureKnob(unisonKnob,         unisonAtt,         breach::ParamID::unison, true);

    for (size_t s = 0; s < 3; ++s)
    {
        for (size_t d = 0; d < 6; ++d)
        {
            auto& ms = modMatrixSliders[s][d];
            ms.setSliderStyle(juce::Slider::LinearHorizontal);
            ms.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(ms);
            modMatrixAtts[s][d] = std::make_unique<SliderAttachment>(
                audioProcessor.getAPVTS(), breach::ParamID::modMatrix[s][d], ms);
        }
    }

    // Tab D: Global
    configureKnob(inGainKnob,        inGainAtt,        breach::ParamID::inputGainDb, true);
    configureKnob(outGainKnob,       outGainAtt,       breach::ParamID::outputGainDb, true);
    configureKnob(widthKnob,         widthAtt,         breach::ParamID::stereoWidth, true);
    configureKnob(smoothTimeKnob,    smoothTimeAtt,    breach::ParamID::smoothTimeMs, true);
    configureKnob(limiterThreshKnob, limiterThreshAtt, breach::ParamID::limiterThreshDb, true);

    dwCurveBox.addItemList({ "EQUAL POWER", "LINEAR", "S-CURVE" }, 1);
    addAndMakeVisible(dwCurveBox);
    dwCurveAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::dryWetCurve, dwCurveBox);

    oversamplingBox.addItemList({ "1X OVERSAMPLE", "2X OVERSAMPLE", "4X OVERSAMPLE" }, 1);
    addAndMakeVisible(oversamplingBox);
    oversamplingAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::oversampling, oversamplingBox);

    addAndMakeVisible(limiterOnBtn);
    limiterOnAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::limiterOn, limiterOnBtn);

    updateAdvancedControlVisibility();
    startTimerHz(60);
}

BreachResonatorAudioProcessorEditor::~BreachResonatorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BreachResonatorAudioProcessorEditor::setAdvancedTab(int tabIdx)
{
    currentAdvTab = juce::jlimit(0, 3, tabIdx);
    audioProcessor.setAdvancedActiveTab(currentAdvTab);
    updateAdvancedControlVisibility();
    repaint();
}

void BreachResonatorAudioProcessorEditor::updateAdvancedControlVisibility()
{
    const bool drawerVisible = (drawerAnimProgress > 0.02f);
    for (int t = 0; t < 4; ++t)
        advTabButtons[static_cast<size_t>(t)].setVisible(drawerVisible);

    const bool showA = drawerVisible && (currentAdvTab == 0);
    voiceCapBox.setVisible(showA);
    for (size_t v = 0; v < breach::kMaxVoices; ++v)
    {
        vLevelSliders[v].setVisible(showA);
        vPanSliders[v].setVisible(showA);
        vDetuneSliders[v].setVisible(showA);
        vDecayMulSliders[v].setVisible(showA);
    }

    const bool showB = drawerVisible && (currentAdvTab == 1);
    resModelBox.setVisible(showB);
    attackKnob.setVisible(showB);
    fbDampingKnob.setVisible(showB);
    stiffnessKnob.setVisible(showB);
    excHpfKnob.setVisible(showB);
    excDriveKnob.setVisible(showB);

    const bool showC = drawerVisible && (currentAdvTab == 2);
    lfo1ShapeBox.setVisible(showC);
    lfo1RateKnob.setVisible(showC);
    lfo1SyncBtn.setVisible(showC);
    lfo1PhaseKnob.setVisible(showC);
    lfo2ShapeBox.setVisible(showC);
    lfo2RateKnob.setVisible(showC);
    lfo2SyncBtn.setVisible(showC);
    lfo2PhaseKnob.setVisible(showC);
    envAttKnob.setVisible(showC);
    envRelKnob.setVisible(showC);
    envAmtKnob.setVisible(showC);
    shiftKnob.setVisible(showC);
    motionStrengthKnob.setVisible(showC);
    motionRateKnob.setVisible(showC);
    unisonKnob.setVisible(showC);
    for (size_t s = 0; s < 3; ++s)
        for (size_t d = 0; d < 6; ++d)
            modMatrixSliders[s][d].setVisible(showC);

    const bool showD = drawerVisible && (currentAdvTab == 3);
    inGainKnob.setVisible(showD);
    outGainKnob.setVisible(showD);
    widthKnob.setVisible(showD);
    dwCurveBox.setVisible(showD);
    oversamplingBox.setVisible(showD);
    limiterOnBtn.setVisible(showD);
    limiterThreshKnob.setVisible(showD);
    smoothTimeKnob.setVisible(showD);
}

void BreachResonatorAudioProcessorEditor::spawnImpactDebris(float intensity)
{
    const float cx = static_cast<float>(centerPanelBounds.getX()) + static_cast<float>(centerPanelBounds.getWidth()) * 0.60f;
    const float cy = static_cast<float>(centerPanelBounds.getCentreY());

    int spawned = 0;
    const int targetSpawn = static_cast<int>(6 + intensity * 12);
    for (auto& d : debrisPool)
    {
        if (!d.active && spawned < targetSpawn)
        {
            d.active = true;
            d.x = cx + (visualRng.nextFloat() - 0.5f) * 24.0f;
            d.y = cy + (visualRng.nextFloat() - 0.5f) * 28.0f;
            const float angle = (visualRng.nextFloat() - 0.5f) * 2.6f; // Burst forward & radially
            const float speed = 3.5f + visualRng.nextFloat() * 9.5f * (0.5f + 0.5f * intensity);
            d.vx = std::cos(angle) * speed * (visualRng.nextBool() ? 1.0f : -0.55f);
            d.vy = std::sin(angle) * speed - 1.8f;
            d.angle = visualRng.nextFloat() * 6.28f;
            d.vAngle = (visualRng.nextFloat() - 0.5f) * 0.28f;
            d.size = 4.0f + visualRng.nextFloat() * 7.5f;
            ++spawned;
        }
    }
}

void BreachResonatorAudioProcessorEditor::selectMode(int modeIdx)
{
    audioProcessor.setMode(modeIdx);
    slashCutProgress = 1.0f;
    bulletSurgeVel += 14.0f;
    sfxPunchScale = 1.25f;
    spawnImpactDebris(0.8f);
    repaint();
}

void BreachResonatorAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.getPosition();
    for (int i = 0; i < 5; ++i)
    {
        if (modeSlotBounds[static_cast<size_t>(i)].contains(pos))
        {
            selectMode(i);
            return;
        }
    }
}

void BreachResonatorAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    animClock += dt;

    // 1. 0.5-second smooth magazine-reload slide animation for Advanced Drawer
    const float targetDrawer = audioProcessor.isAdvancedPanelOpen() ? 1.0f : 0.0f;
    if (std::abs(drawerAnimProgress - targetDrawer) > 0.001f)
    {
        const float step = dt / 0.50f; // Exact 0.50s duration
        if (drawerAnimProgress < targetDrawer)
            drawerAnimProgress = std::min(1.0f, drawerAnimProgress + step);
        else
            drawerAnimProgress = std::max(0.0f, drawerAnimProgress - step);

        // Smooth cubic ease-in-out
        const float eased = drawerAnimProgress * drawerAnimProgress * (3.0f - 2.0f * drawerAnimProgress);
        const int targetH = kBaseHeight + static_cast<int>(std::round(eased * kAdvancedHeight));
        if (getHeight() != targetH)
            setSize(kBaseWidth, targetH);

        updateAdvancedControlVisibility();
    }

    // 2. Read Audio Telemetry & Update Realistic Bullet/Wall Physics
    lastSnapshot = audioProcessor.getVisualTelemetry().getSnapshot();

    if (lastSnapshot.transientPulseCount != lastTransientCount)
    {
        lastTransientCount = lastSnapshot.transientPulseCount;
        flashFramesRemaining = 2; // 1-frame manga impact invert flash
        bulletSurgeVel += 16.0f + lastSnapshot.inputPeak * 14.0f;
        sfxPunchScale = 1.30f;
        spawnImpactDebris(juce::jlimit(0.3f, 1.0f, lastSnapshot.inputPeak * 1.4f));
    }
    else if (flashFramesRemaining > 0)
    {
        --flashFramesRemaining;
    }

    // Spring-damper physics for bullet penetration surge
    const float targetSurge = juce::jlimit(0.0f, 32.0f, lastSnapshot.inputPeak * 34.0f);
    const float springForce = (targetSurge - bulletSurgePos) * 140.0f;
    bulletSurgeVel = (bulletSurgeVel + springForce * dt) * 0.78f;
    bulletSurgePos = juce::jlimit(-6.0f, 38.0f, bulletSurgePos + bulletSurgeVel * dt);

    sfxPunchScale = 1.0f + (sfxPunchScale - 1.0f) * 0.82f;

    // Update flying concrete debris shards
    const auto b = centerPanelBounds.toFloat();
    for (auto& d : debrisPool)
    {
        if (!d.active) continue;
        d.x += d.vx;
        d.y += d.vy;
        d.vy += 0.26f; // Gravity
        d.vx *= 0.985f;
        d.angle += d.vAngle;

        if (!b.expanded(24.0f).contains(d.x, d.y))
            d.active = false;
    }

    if (slashCutProgress > 0.0f)
        slashCutProgress = std::max(0.0f, slashCutProgress - 0.09f); // 0.18s diagonal cut

    repaint();
}

void BreachResonatorAudioProcessorEditor::resized()
{
    auto fullArea = getLocalBounds();
    auto mainArea = fullArea.removeFromTop(kBaseHeight);

    // 1. Top Header Bar (54px) - Exact Reference Layout
    auto header = mainArea.removeFromTop(54).reduced(14, 10);
    // Title takes left 280px
    header.removeFromLeft(280);

    // Center Preset Selector (< [ PRESET ] >)
    presetPrevBtn.setBounds(header.removeFromLeft(30));
    header.removeFromLeft(4);
    chordPresetBox.setBounds(header.removeFromLeft(160));
    header.removeFromLeft(4);
    presetNextBtn.setBounds(header.removeFromLeft(30));

    // Right Action Buttons: TRIGGER | ADVANCED | INVERT
    invertBtn.setBounds(header.removeFromRight(84));
    header.removeFromRight(8);
    advancedBtn.setBounds(header.removeFromRight(98));
    header.removeFromRight(8);
    triggerBtn.setBounds(header.removeFromRight(88));

    // 2. Bottom Firearm Mode Dock (116px) - Exact Reference Layout
    auto bottomDock = mainArea.removeFromBottom(116);
    const int slotW = bottomDock.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        modeSlotBounds[static_cast<size_t>(i)] = bottomDock.removeFromLeft(i == 4 ? bottomDock.getWidth() : slotW);

    // 3. Symmetrical Left & Right 3-Knob Wings (196px each)
    auto leftWing  = mainArea.removeFromLeft(196).reduced(22, 10);
    auto rightWing = mainArea.removeFromRight(196).reduced(22, 10);

    const int knobRowH = leftWing.getHeight() / 3;
    warpKnob.setBounds(leftWing.removeFromTop(knobRowH).reduced(16, 12));
    driftKnob.setBounds(leftWing.removeFromTop(knobRowH).reduced(16, 12));
    decayKnob.setBounds(leftWing.reduced(16, 12));

    filterTiltKnob.setBounds(rightWing.removeFromTop(knobRowH).reduced(16, 12));
    filterQKnob.setBounds(rightWing.removeFromTop(knobRowH).reduced(16, 12));
    depthKnob.setBounds(rightWing.reduced(16, 12));

    // 4. Center Manga Frame + Compact Bottom Harmony/Arp/Motion/Range Strip
    auto centerColumn = mainArea.reduced(6, 10);
    harmonyStripBounds = centerColumn.removeFromBottom(52);
    centerColumn.removeFromBottom(6);
    centerPanelBounds = centerColumn;

    auto hTop = harmonyStripBounds.removeFromTop(24);
    const int noteW = 28;
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t>(i)].setBounds(hTop.removeFromLeft(noteW).reduced(1, 0));

    hTop.removeFromLeft(6);
    rootBox.setBounds(hTop.removeFromLeft(54));
    hTop.removeFromLeft(4);
    octaveBox.setBounds(hTop.removeFromLeft(66));
    hTop.removeFromLeft(4);
    spreadBox.setBounds(hTop.removeFromLeft(62));
    hTop.removeFromLeft(4);
    midiModeBtn.setBounds(hTop);

    harmonyStripBounds.removeFromTop(4);
    auto hBot = harmonyStripBounds;
    arpOnBtn.setBounds(hBot.removeFromLeft(58));
    hBot.removeFromLeft(4);
    arpPatternBox.setBounds(hBot.removeFromLeft(74));
    hBot.removeFromLeft(6);
    motionOnBtn.setBounds(hBot.removeFromLeft(74));
    hBot.removeFromLeft(8);
    filterRangeSlider.setBounds(hBot);

    // 5. Stage 2: Slide-Out Advanced Magazine Drawer
    advancedDrawerBounds = fullArea.reduced(12, 8);
    if (advancedDrawerBounds.getHeight() > 20)
    {
        auto advArea = advancedDrawerBounds.reduced(10, 8);
        auto tabBar = advArea.removeFromTop(28);
        const int tabW = tabBar.getWidth() / 4;
        for (int t = 0; t < 4; ++t)
            advTabButtons[static_cast<size_t>(t)].setBounds(tabBar.removeFromLeft(t == 3 ? tabBar.getWidth() : tabW).reduced(2, 0));

        advArea.removeFromTop(10);

        // Layout Tab A: Voices (12 columns)
        auto tabAContent = advArea;
        auto topA = tabAContent.removeFromTop(26);
        voiceCapBox.setBounds(topA.removeFromLeft(150));
        tabAContent.removeFromTop(8);
        auto gridA = tabAContent;
        gridA.removeFromLeft(72); // Row label column
        const int colW = gridA.getWidth() / breach::kMaxVoices;
        for (int v = 0; v < breach::kMaxVoices; ++v)
        {
            auto col = gridA.removeFromLeft(colW).reduced(3, 0);
            const int rowH = col.getHeight() / 4;
            vLevelSliders[static_cast<size_t>(v)].setBounds(col.removeFromTop(rowH));
            vPanSliders[static_cast<size_t>(v)].setBounds(col.removeFromTop(rowH));
            vDetuneSliders[static_cast<size_t>(v)].setBounds(col.removeFromTop(rowH));
            vDecayMulSliders[static_cast<size_t>(v)].setBounds(col);
        }

        // Layout Tab B: Resonator
        auto tabBContent = advArea;
        auto topB = tabBContent.removeFromTop(28);
        resModelBox.setBounds(topB.removeFromLeft(190));
        tabBContent.removeFromTop(14);
        const int bColW = tabBContent.getWidth() / 5;
        attackKnob.setBounds(tabBContent.removeFromLeft(bColW).reduced(18, 12));
        fbDampingKnob.setBounds(tabBContent.removeFromLeft(bColW).reduced(18, 12));
        stiffnessKnob.setBounds(tabBContent.removeFromLeft(bColW).reduced(18, 12));
        excHpfKnob.setBounds(tabBContent.removeFromLeft(bColW).reduced(18, 12));
        excDriveKnob.setBounds(tabBContent.reduced(18, 12));

        // Layout Tab C: Modulation (Left = LFO1/LFO2/Env + Motion/Shift/Unison, Right = 3x6 Matrix)
        auto tabCContent = advArea;
        auto leftC = tabCContent.removeFromLeft(470);
        auto rowC1 = leftC.removeFromTop(leftC.getHeight() / 2);
        auto rowC2 = leftC;

        lfo1ShapeBox.setBounds(rowC1.removeFromLeft(72).reduced(2, 34));
        lfo1SyncBtn.setBounds(rowC1.removeFromLeft(52).reduced(2, 34));
        lfo1RateKnob.setBounds(rowC1.removeFromLeft(76).reduced(4, 6));
        lfo1PhaseKnob.setBounds(rowC1.removeFromLeft(76).reduced(4, 6));
        shiftKnob.setBounds(rowC1.removeFromLeft(90).reduced(6, 6));
        unisonKnob.setBounds(rowC1.reduced(6, 6));

        lfo2ShapeBox.setBounds(rowC2.removeFromLeft(72).reduced(2, 34));
        lfo2SyncBtn.setBounds(rowC2.removeFromLeft(52).reduced(2, 34));
        lfo2RateKnob.setBounds(rowC2.removeFromLeft(76).reduced(4, 6));
        lfo2PhaseKnob.setBounds(rowC2.removeFromLeft(76).reduced(4, 6));
        envAttKnob.setBounds(rowC2.removeFromLeft(64).reduced(2, 6));
        envRelKnob.setBounds(rowC2.removeFromLeft(64).reduced(2, 6));
        envAmtKnob.setBounds(rowC2.reduced(2, 6));

        tabCContent.removeFromLeft(12);
        auto matrixArea = tabCContent;
        matrixArea.removeFromTop(20);
        matrixArea.removeFromLeft(54);
        const int mColW = matrixArea.getWidth() / 6;
        const int mRowH = matrixArea.getHeight() / 3;
        for (int s = 0; s < 3; ++s)
        {
            auto rArea = matrixArea.removeFromTop(mRowH);
            for (int d = 0; d < 6; ++d)
                modMatrixSliders[static_cast<size_t>(s)][static_cast<size_t>(d)].setBounds(rArea.removeFromLeft(mColW).reduced(3, 14));
        }

        // Layout Tab D: Global
        auto tabDContent = advArea;
        const int dColW = tabDContent.getWidth() / 7;
        inGainKnob.setBounds(tabDContent.removeFromLeft(dColW).reduced(10, 22));
        outGainKnob.setBounds(tabDContent.removeFromLeft(dColW).reduced(10, 22));
        widthKnob.setBounds(tabDContent.removeFromLeft(dColW).reduced(10, 22));

        auto curveCol = tabDContent.removeFromLeft(dColW * 2).reduced(8, 28);
        dwCurveBox.setBounds(curveCol.removeFromTop(32));
        curveCol.removeFromTop(12);
        oversamplingBox.setBounds(curveCol.removeFromTop(32));
        curveCol.removeFromTop(12);
        limiterOnBtn.setBounds(curveCol.removeFromTop(30));

        limiterThreshKnob.setBounds(tabDContent.removeFromLeft(dColW).reduced(10, 22));
        smoothTimeKnob.setBounds(tabDContent.reduced(10, 22));
    }
}

void BreachResonatorAudioProcessorEditor::drawFirearmSilhouette(juce::Graphics& g, int modeIdx,
                                                                juce::Rectangle<float> area, juce::Colour inkCol)
{
    g.setColour(inkCol);
    juce::Path p;
    const float cx = area.getX() + area.getWidth() * 0.34f;
    const float cy = area.getCentreY();

    switch (modeIdx)
    {
        case 0: // PISTOL (Detailed slide, trigger guard, grip)
            p.startNewSubPath(cx - 34, cy - 12);
            p.lineTo(cx + 32, cy - 12);
            p.lineTo(cx + 32, cy - 1);
            p.lineTo(cx - 8,  cy - 1);
            p.lineTo(cx - 16, cy + 22);
            p.lineTo(cx - 30, cy + 22);
            p.lineTo(cx - 22, cy - 1);
            p.lineTo(cx - 34, cy - 1);
            p.closeSubPath();
            p.addRoundedRectangle(cx - 10, cy - 1, 12, 9, 2.0f);
            break;

        case 1: // SHOTGUN (Pump action + stock)
            p.startNewSubPath(cx - 54, cy + 12);
            p.lineTo(cx - 28, cy - 4);
            p.lineTo(cx + 52, cy - 4);
            p.lineTo(cx + 52, cy + 1);
            p.lineTo(cx + 18, cy + 1);
            p.lineTo(cx + 18, cy + 6);
            p.lineTo(cx - 18, cy + 6);
            p.lineTo(cx - 48, cy + 18);
            p.closeSubPath();
            break;

        case 2: // SMG (MP5-style curved mag + foregrip + stock)
            p.startNewSubPath(cx - 42, cy - 10);
            p.lineTo(cx + 36, cy - 10);
            p.lineTo(cx + 36, cy + 2);
            p.lineTo(cx + 12, cy + 2);
            p.lineTo(cx + 16, cy + 20);
            p.lineTo(cx + 8,  cy + 20);
            p.lineTo(cx + 4,  cy + 2);
            p.lineTo(cx - 12, cy + 2);
            p.lineTo(cx - 18, cy + 18);
            p.lineTo(cx - 26, cy + 18);
            p.lineTo(cx - 22, cy + 2);
            p.lineTo(cx - 42, cy + 2);
            p.closeSubPath();
            break;

        case 3: // SNIPER (Long barrel + scope + bipod)
            p.startNewSubPath(cx - 56, cy + 10);
            p.lineTo(cx - 28, cy - 2);
            p.lineTo(cx + 58, cy - 2);
            p.lineTo(cx + 58, cy + 2);
            p.lineTo(cx + 8,  cy + 2);
            p.lineTo(cx + 4,  cy + 12);
            p.lineTo(cx - 4,  cy + 12);
            p.lineTo(cx - 8,  cy + 2);
            p.lineTo(cx - 24, cy + 2);
            p.lineTo(cx - 50, cy + 16);
            p.closeSubPath();
            p.addRectangle(cx - 16, cy - 10, 32, 5);
            break;

        case 4: // REVOLVER (Cylinder + heavy barrel + grip)
        default:
            p.startNewSubPath(cx - 38, cy + 18);
            p.lineTo(cx - 24, cy + 1);
            p.lineTo(cx - 12, cy - 10);
            p.lineTo(cx + 38, cy - 10);
            p.lineTo(cx + 38, cy + 1);
            p.lineTo(cx - 6,  cy + 1);
            p.lineTo(cx - 22, cy + 20);
            p.closeSubPath();
            p.addEllipse(cx - 10, cy - 9, 14, 12);
            break;
    }

    g.strokePath(p, juce::PathStrokeType(1.8f));
}

void BreachResonatorAudioProcessorEditor::drawImpactPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const bool flashInvert = (flashFramesRemaining > 0);
    const bool effectiveInvert = isInverted ^ flashInvert;

    const auto paper = effectiveInvert ? juce::Colours::black : juce::Colours::white;
    const auto ink   = effectiveInvert ? juce::Colours::white : juce::Colours::black;

    g.setColour(paper);
    g.fillRect(bounds);

    g.setColour(ink);
    g.drawRect(bounds, 2.5f);

    const float bx = static_cast<float>(bounds.getX());
    const float by = static_cast<float>(bounds.getY());
    const float bw = static_cast<float>(bounds.getWidth());
    const float bh = static_cast<float>(bounds.getHeight());

    const float impactX = bx + bw * 0.59f;
    const float impactY = by + bh * 0.48f;

    // 1. Concrete Wall Slab + Halftone Screentone Shading (Exact Reference Style)
    const float wallEdgeX = bx + bw * 0.87f;
    g.drawLine(wallEdgeX, by, wallEdgeX, by + bh, 2.0f);

    for (float dy = by + 6.0f; dy < by + bh - 4.0f; dy += 6.5f)
    {
        for (float dx = wallEdgeX + 5.0f; dx < bx + bw - 4.0f; dx += 6.5f)
            g.fillEllipse(dx, dy, 2.0f, 2.0f);
    }

    // 2. Converging Manga Speed Lines (Left background -> Impact Crater)
    const float peak = juce::jlimit(0.0f, 1.0f, lastSnapshot.inputPeak * 1.6f);
    const float speedReach = bw * (0.34f + 0.16f * peak);
    for (int i = -11; i <= 11; ++i)
    {
        if (i == 0) continue;
        const float yLeft = impactY + static_cast<float>(i) * 14.5f;
        const float yRight = impactY + static_cast<float>(i) * 4.2f;
        const float jitter = std::sin(animClock * 18.0f + static_cast<float>(i) * 2.1f) * (2.0f + peak * 8.0f);
        const float endX = bx + speedReach + jitter;
        g.drawLine(bx + 2.0f, yLeft, endX, yRight, (i % 3 == 0) ? 2.1f : 1.0f);
    }

    // 3. Radial Fracture Web & Secondary Cracks (Driven by Active Voices & Decay)
    const int voices = juce::jlimit(1, breach::kMaxVoices, lastSnapshot.activeVoiceCount);
    const float baseDecay = static_cast<float>(decayKnob.getValue());
    for (int v = 0; v < voices; ++v)
    {
        const float energy = lastSnapshot.voiceEnergies[static_cast<size_t>(v)];
        const bool isArpLit = (lastSnapshot.activeArpStep == v);
        const float baseAngle = (static_cast<float>(v) / static_cast<float>(voices)) * juce::MathConstants<float>::twoPi + 0.28f;
        const float crackLen = 32.0f + (baseDecay * 75.0f) + (energy * 95.0f) + (isArpLit ? 20.0f : 0.0f);

        const float elbowX = impactX + std::cos(baseAngle + 0.15f) * (crackLen * 0.48f);
        const float elbowY = impactY + std::sin(baseAngle + 0.15f) * (crackLen * 0.48f);
        const float tipX   = impactX + std::cos(baseAngle - 0.06f) * crackLen;
        const float tipY   = impactY + std::sin(baseAngle - 0.06f) * crackLen;

        g.drawLine(impactX, impactY, elbowX, elbowY, isArpLit ? 3.0f : 1.8f);
        g.drawLine(elbowX, elbowY, tipX, tipY, isArpLit ? 2.0f : 1.2f);

        // Secondary forked fracture branch
        if (crackLen > 68.0f)
        {
            const float forkX = elbowX + std::cos(baseAngle - 0.48f) * (crackLen * 0.36f);
            const float forkY = elbowY + std::sin(baseAngle - 0.48f) * (crackLen * 0.36f);
            g.drawLine(elbowX, elbowY, forkX, forkY, 1.0f);
        }
    }

    // 4. Jagged Breached Crater + Halftone Rim
    const int mode = lastSnapshot.activeMode;
    auto drawCrater = [&](float cx, float cy, float rx, float ry)
    {
        juce::Path rim;
        constexpr int pts = 14;
        for (int k = 0; k < pts; ++k)
        {
            const float a = (static_cast<float>(k) / static_cast<float>(pts)) * juce::MathConstants<float>::twoPi;
            const float jag = (k % 2 == 0) ? 1.28f : 0.88f;
            const float px = cx + std::cos(a) * rx * 1.45f * jag;
            const float py = cy + std::sin(a) * ry * 1.45f * jag;
            if (k == 0) rim.startNewSubPath(px, py);
            else        rim.lineTo(px, py);
        }
        rim.closeSubPath();
        g.setColour(paper);
        g.fillPath(rim);
        g.setColour(ink);
        g.strokePath(rim, juce::PathStrokeType(1.6f));
        g.fillEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    };

    if (mode == 1) // Shotgun: multi-pellet scatter craters
    {
        drawCrater(impactX - 10, impactY - 22, 10, 10);
        drawCrater(impactX + 4,  impactY + 2,  14, 15);
        drawCrater(impactX - 14, impactY + 20, 9,  9);
    }
    else if (mode == 2) // SMG: rapid triple burst holes
    {
        drawCrater(impactX - 18, impactY - 10, 10, 11);
        drawCrater(impactX + 2,  impactY - 2,  12, 13);
        drawCrater(impactX + 18, impactY + 10, 10, 11);
    }
    else if (mode == 3) // Sniper: clean elongated armor-piercing tunnel
    {
        drawCrater(impactX, impactY, 26, 11);
    }
    else
    {
        drawCrater(impactX, impactY, 16, 19);
    }

    // 5. Realistic Bullet Projectile + Shockwave Wake + Motion Trails
    const float vibY = std::sin(animClock * 55.0f) * (0.6f + peak * 3.5f);
    const float bulletNoseX = impactX - 42.0f + bulletSurgePos;
    const float bulletY = impactY + vibY;

    // Aerodynamic wake lines trailing from bullet into crater
    g.setColour(ink);
    g.drawLine(bulletNoseX - 38.0f, bulletY - 6.0f, impactX + 6.0f, bulletY - 5.0f, 1.3f);
    g.drawLine(bulletNoseX - 34.0f, bulletY,        impactX + 8.0f, bulletY,        1.8f);
    g.drawLine(bulletNoseX - 38.0f, bulletY + 6.0f, impactX + 6.0f, bulletY + 5.0f, 1.3f);

    // Bullet jacket + ogive nose cone
    const float bLen = (mode == 3) ? 44.0f : 34.0f;
    const float bRad = (mode == 3) ? 5.5f  : 7.0f;
    juce::Path bullet;
    bullet.startNewSubPath(bulletNoseX - bLen, bulletY - bRad);
    bullet.lineTo(bulletNoseX - bLen * 0.35f, bulletY - bRad);
    bullet.quadraticTo(bulletNoseX + 4.0f, bulletY, bulletNoseX - bLen * 0.35f, bulletY + bRad);
    bullet.lineTo(bulletNoseX - bLen, bulletY + bRad);
    bullet.closeSubPath();

    g.setColour(paper);
    g.fillPath(bullet);
    g.setColour(ink);
    g.strokePath(bullet, juce::PathStrokeType(2.2f));
    // Cannelure groove rings on bullet casing
    g.drawLine(bulletNoseX - bLen + 5.0f, bulletY - bRad, bulletNoseX - bLen + 5.0f, bulletY + bRad, 1.5f);
    g.drawLine(bulletNoseX - bLen + 9.0f, bulletY - bRad, bulletNoseX - bLen + 9.0f, bulletY + bRad, 1.2f);

    // 6. Physics-Driven Flying Concrete Debris Shards (with halftone shading)
    for (const auto& d : debrisPool)
    {
        if (!d.active) continue;
        juce::Path shard;
        const float s = d.size;
        shard.startNewSubPath(d.x + std::cos(d.angle) * s, d.y + std::sin(d.angle) * s);
        shard.lineTo(d.x + std::cos(d.angle + 2.1f) * s * 0.9f, d.y + std::sin(d.angle + 2.1f) * s * 0.9f);
        shard.lineTo(d.x + std::cos(d.angle + 4.0f) * s * 1.1f, d.y + std::sin(d.angle + 4.0f) * s * 1.1f);
        shard.closeSubPath();

        g.setColour(paper);
        g.fillPath(shard);
        g.setColour(ink);
        g.strokePath(shard, juce::PathStrokeType(1.6f));
    }

    // 7. Bold Japanese Manga Katakana SFX (matching reference UI)
    static constexpr std::array<const char*, 5> kTopSfx{ "ドン", "バァン", "ダダダ", "ズドン", "ガキン" };
    static constexpr std::array<const char*, 5> kBotSfx{ "バキッ", "バリバリ", "ガガガ", "貫通ッ", "ドゴォ" };

    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(34.0f * sfxPunchScale).withStyle("Bold")));
    g.drawText(kTopSfx[static_cast<size_t>(mode % 5)],
               static_cast<int>(bx + bw * 0.16f), static_cast<int>(by + 12.0f), 160, 44, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(32.0f * sfxPunchScale).withStyle("Bold")));
    g.drawText(kBotSfx[static_cast<size_t>(mode % 5)],
               static_cast<int>(bx + bw * 0.56f), static_cast<int>(by + bh - 56.0f), 170, 44, juce::Justification::centred);

    // 8. 0.18s Diagonal Manga Blade Cut on Mode Switch
    if (slashCutProgress > 0.01f)
    {
        const float slashW = bw * 0.24f * slashCutProgress;
        juce::Path slash;
        slash.startNewSubPath(bx + bw * 0.35f - slashW, by);
        slash.lineTo(bx + bw * 0.35f + slashW, by);
        slash.lineTo(bx + bw * 0.65f + slashW, by + bh);
        slash.lineTo(bx + bw * 0.65f - slashW, by + bh);
        slash.closeSubPath();
        g.setColour(paper);
        g.fillPath(slash);
    }
}

void BreachResonatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto paper = isInverted ? juce::Colours::black : juce::Colours::white;
    const auto ink   = isInverted ? juce::Colours::white : juce::Colours::black;

    g.fillAll(paper);
    g.setColour(ink);
    g.drawRect(getLocalBounds(), 2);

    // 1. Top Header Emblem + Title (Crosshair + BREACH // 貫通)
    const float cx = 34.0f;
    const float cy = 27.0f;
    g.drawEllipse(cx - 11.0f, cy - 11.0f, 22.0f, 22.0f, 2.0f);
    g.drawLine(cx - 16.0f, cy, cx + 16.0f, cy, 1.8f);
    g.drawLine(cx, cy - 16.0f, cx, cy + 16.0f, 1.8f);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(23.0f).withStyle("Bold")));
    g.drawText("BREACH // 貫通", 58, 11, 230, 32, juce::Justification::centredLeft);

    // 2. Center Manga Impact Panel
    drawImpactPanel(g, centerPanelBounds);

    // 3. Symmetrical Left & Right Knob Labels (Reference Aesthetic)
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f).withStyle("Bold")));
    auto drawKnobLabel = [&](juce::Slider& s, const juce::String& label)
    {
        const auto b = s.getBounds();
        g.drawText(label, b.getX() - 10, b.getBottom() + 2, b.getWidth() + 20, 16, juce::Justification::centred);
    };

    drawKnobLabel(warpKnob,       "DRIVE");
    drawKnobLabel(driftKnob,      "SPACE");
    drawKnobLabel(decayKnob,      "DECAY");
    drawKnobLabel(filterTiltKnob, "TONE");
    drawKnobLabel(filterQKnob,    "DENSITY");
    drawKnobLabel(depthKnob,      "MIX");

    // 4. Bottom Firearm Mode Dock (Exact Reference 5-Cell Dock)
    const int dockTopY = kBaseHeight - 116;
    g.drawLine(0.0f, static_cast<float>(dockTopY), static_cast<float>(getWidth()), static_cast<float>(dockTopY), 2.5f);

    static constexpr std::array<const char*, 5> kGunNames{
        "PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER"
    };
    static constexpr std::array<const char*, 5> kGunSubs{
        "TRANSIENT", "SCATTER", "BURST", "PIERCE", "CRUSH"
    };

    const int activeMode = audioProcessor.getCurrentMode();
    for (int i = 0; i < 5; ++i)
    {
        const auto slot = modeSlotBounds[static_cast<size_t>(i)];
        const bool selected = (i == activeMode);

        g.setColour(selected ? ink : paper);
        g.fillRect(slot);

        g.setColour(ink);
        g.drawRect(slot, 1.5f);

        const auto fg = selected ? paper : ink;
        drawFirearmSilhouette(g, i, slot.toFloat().reduced(8.0f, 12.0f), fg);

        g.setColour(fg);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f).withStyle("Bold")));
        g.drawText(kGunNames[static_cast<size_t>(i)],
                   slot.getX() + slot.getWidth() / 2 - 4, slot.getCentreY() - 2,
                   slot.getWidth() / 2, 16, juce::Justification::centredLeft);
        g.drawText(kGunSubs[static_cast<size_t>(i)],
                   slot.getX() + slot.getWidth() / 2 - 4, slot.getCentreY() + 14,
                   slot.getWidth() / 2, 16, juce::Justification::centredLeft);
    }

    // 5. Stage 2: Slide-Out Advanced Magazine Drawer Frame & Captions
    if (drawerAnimProgress > 0.02f && advancedDrawerBounds.getHeight() > 20)
    {
        g.setColour(paper);
        g.fillRect(advancedDrawerBounds);
        g.setColour(ink);
        g.drawRect(advancedDrawerBounds, 2.2f);

        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
        if (currentAdvTab == 0) // Tab A: Voices
        {
            const int y0 = advancedDrawerBounds.getY() + 82;
            const int rowH = 44;
            g.drawText("LEVEL",   advancedDrawerBounds.getX() + 14, y0,            64, 20, juce::Justification::centredLeft);
            g.drawText("PAN",     advancedDrawerBounds.getX() + 14, y0 + rowH,     64, 20, juce::Justification::centredLeft);
            g.drawText("DETUNE",  advancedDrawerBounds.getX() + 14, y0 + rowH * 2, 64, 20, juce::Justification::centredLeft);
            g.drawText("DECAY x", advancedDrawerBounds.getX() + 14, y0 + rowH * 3, 64, 20, juce::Justification::centredLeft);
        }
        else if (currentAdvTab == 1) // Tab B: Resonator
        {
            auto cap = [&](juce::Slider& s, const char* txt)
            {
                const auto b = s.getBounds();
                g.drawText(txt, b.getX() - 10, b.getY() - 14, b.getWidth() + 20, 14, juce::Justification::centred);
            };
            cap(attackKnob,    "ATTACK (MS)");
            cap(fbDampingKnob, "FB DAMPING");
            cap(stiffnessKnob, "STIFFNESS");
            cap(excHpfKnob,    "EXC HPF (HZ)");
            cap(excDriveKnob,  "EXC DRIVE");
        }
        else if (currentAdvTab == 2) // Tab C: Modulation
        {
            auto cap = [&](juce::Component& c, const char* txt)
            {
                const auto b = c.getBounds();
                g.drawText(txt, b.getX() - 8, b.getY() - 13, b.getWidth() + 16, 13, juce::Justification::centred);
            };
            cap(lfo1RateKnob,  "LFO1 RATE");
            cap(lfo1PhaseKnob, "LFO1 PHASE");
            cap(shiftKnob,     "SHIFT (HZ)");
            cap(unisonKnob,    "UNISON");
            cap(lfo2RateKnob,  "LFO2 RATE");
            cap(lfo2PhaseKnob, "LFO2 PHASE");
            cap(envAttKnob,    "ENV ATT");
            cap(envRelKnob,    "ENV REL");
            cap(envAmtKnob,    "ENV AMT");

            static constexpr std::array<const char*, 6> kDstCols{ "WARP", "DRIFT", "SHIFT", "Q", "TILT", "DEPTH" };
            const int mStartX = advancedDrawerBounds.getX() + 546;
            const int mColW = (advancedDrawerBounds.getWidth() - 560) / 6;
            for (int d = 0; d < 6; ++d)
                g.drawText(kDstCols[static_cast<size_t>(d)], mStartX + d * mColW, advancedDrawerBounds.getY() + 46, mColW, 14, juce::Justification::centred);

            const int mStartY = advancedDrawerBounds.getY() + 76;
            const int mRowH = 62;
            g.drawText("LFO 1", advancedDrawerBounds.getX() + 492, mStartY,             50, 18, juce::Justification::centredLeft);
            g.drawText("LFO 2", advancedDrawerBounds.getX() + 492, mStartY + mRowH,     50, 18, juce::Justification::centredLeft);
            g.drawText("ENV",   advancedDrawerBounds.getX() + 492, mStartY + mRowH * 2, 50, 18, juce::Justification::centredLeft);
        }
        else if (currentAdvTab == 3) // Tab D: Global
        {
            auto cap = [&](juce::Slider& s, const char* txt)
            {
                const auto b = s.getBounds();
                g.drawText(txt, b.getX() - 10, b.getY() - 14, b.getWidth() + 20, 14, juce::Justification::centred);
            };
            cap(inGainKnob,        "INPUT GAIN");
            cap(outGainKnob,       "OUTPUT GAIN");
            cap(widthKnob,         "STEREO WIDTH");
            cap(limiterThreshKnob, "LIMIT THRESH");
            cap(smoothTimeKnob,    "SMOOTH (MS)");
        }
    }
}
