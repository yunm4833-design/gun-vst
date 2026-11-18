#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr const char* kSemitoneLabels[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

}

//==============================================================================
// GunSelectorStrip
//==============================================================================
GunSelectorStrip::GunSelectorStrip(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l)
    : processor(p), laf(l)
{
}

void GunSelectorStrip::drawGunIcon(juce::Graphics& g, int index, juce::Rectangle<float> area,
                                    juce::Colour foreground, juce::Colour background)
{
    const void* data[] = {BinaryData::Gun0_svg, BinaryData::Gun1_svg, BinaryData::Gun2_svg, BinaryData::Gun3_svg, BinaryData::Gun4_svg};
    const int sizes[] = {BinaryData::Gun0_svgSize, BinaryData::Gun1_svgSize, BinaryData::Gun2_svgSize, BinaryData::Gun3_svgSize, BinaryData::Gun4_svgSize};
    const auto slot = static_cast<size_t>(index);
    if (!icons[slot])
        icons[slot] = juce::Drawable::createFromImageData(data[index], static_cast<size_t>(sizes[index]));
    if (!icons[slot]) return;

    if (!tintedIcons[slot] || iconColours[slot] != foreground)
    {
        auto icon = icons[slot]->createCopy();
        // Swap the two ink colours without recolouring a replacement twice.
        icon->replaceColour(juce::Colours::black, juce::Colours::magenta);
        icon->replaceColour(juce::Colours::white, background);
        icon->replaceColour(juce::Colours::magenta, foreground);
        tintedIcons[slot] = std::move(icon);
        iconColours[slot] = foreground;
    }

    tintedIcons[slot]->drawWithin(g, area, juce::RectanglePlacement::centred, 1.0f);
}

void GunSelectorStrip::paint(juce::Graphics& g)
{
    static constexpr const char* titles[] {"PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER"};
    static constexpr const char* subtitles[] {"TRANSIENT", "SCATTER", "BURST", "PIERCE", "CRUSH"};
    const int currentMode = juce::jlimit(0, 4, static_cast<int>(processor.apvts.getRawParameterValue("gunMode")->load()));
    const auto bounds = getLocalBounds().toFloat();
    const float cellWidth = bounds.getWidth() / 5.0f;
    const auto paper = laf.getPaperColour();
    const auto ink = laf.getInkColour();
    const auto titleFont = laf.getMangaFont(19.5f, true);
    const auto subtitleFont = laf.getMangaFont(10.2f, true).withExtraKerningFactor(.08f);

    auto clippedCorners = [](juce::Rectangle<float> r, float corner)
    {
        juce::Path path;
        path.startNewSubPath(r.getX() + corner, r.getY());
        path.lineTo(r.getRight() - corner, r.getY());
        path.lineTo(r.getRight(), r.getY() + corner);
        path.lineTo(r.getRight(), r.getBottom() - corner);
        path.lineTo(r.getRight() - corner, r.getBottom());
        path.lineTo(r.getX() + corner, r.getBottom());
        path.lineTo(r.getX(), r.getBottom() - corner);
        path.lineTo(r.getX(), r.getY() + corner);
        path.closeSubPath();
        return path;
    };

    g.fillAll(paper);
    for (int i = 0; i < 5; ++i)
    {
        const auto cell = juce::Rectangle<float>(bounds.getX() + static_cast<float>(i) * cellWidth,
                                                  bounds.getY(), cellWidth, bounds.getHeight()).reduced(2.1f, 1.15f);
        const bool selected = i == currentMode;
        const auto foreground = selected ? paper : ink;
        const auto background = selected ? ink : paper;
        const auto frame = clippedCorners(cell, 5.4f);
        g.setColour(background);
        g.fillPath(frame);
        g.setColour(ink);
        g.strokePath(frame, juce::PathStrokeType(1.8f));
        if (selected)
        {
            g.setColour(foreground);
            g.strokePath(clippedCorners(cell.reduced(3.6f), 3.6f), juce::PathStrokeType(1.0f));
        }

        auto content = cell.reduced(13.0f, 6.0f);
        const auto subtitle = content.removeFromBottom(14.0f);
        const auto title = content.removeFromBottom(23.0f);
        drawGunIcon(g, i, content.reduced(0, 2), foreground, background);
        g.setColour(foreground);
        g.setFont(titleFont);
        g.drawFittedText(titles[i], title.toNearestInt(), juce::Justification::centred, 1);
        g.setFont(subtitleFont);
        g.drawFittedText(subtitles[i], subtitle.toNearestInt(), juce::Justification::centred, 1);
    }
}

void GunSelectorStrip::mouseDown(const juce::MouseEvent& e)
{
    const int idx = juce::jlimit(0, 4, static_cast<int>((static_cast<float>(e.x) / static_cast<float>(getWidth())) * 5.0f));
    if (auto* param = processor.apvts.getParameter("gunMode"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(idx)));
        param->endChangeGesture();
        repaint();
    }
}

//==============================================================================
// BreachResonatorAudioProcessorEditor
//==============================================================================
BreachResonatorAudioProcessorEditor::BreachResonatorAudioProcessorEditor(BreachResonatorAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      viewport(p, mangaLaf),
      keyboard(p, mangaLaf),
      settingsBackdrop(mangaLaf),
      voicePanel(p, mangaLaf),
      modRoutePanel(p, mangaLaf),
      gunStrip(p, mangaLaf),
      eqPanel(p, mangaLaf),
      ottMeters(p.telemetry, mangaLaf)
{
    setLookAndFeel(&mangaLaf);

    // Top bar controls
    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(chordPresetBox);
    addAndMakeVisible(nextPresetBtn);
    addAndMakeVisible(panicBtn);
    addAndMakeVisible(advancedBtn);
    addAndMakeVisible(invertBtn);

    chordPresetBox.addItemList({ "MAJOR TRIAD", "MINOR TRIAD", "MAJ7", "MIN7", "DOM7", "SUS2", "SUS4", "DIM7", "AUG", "PENTATONIC" }, 1);
    presetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "chordPreset", chordPresetBox);

    chordPresetBox.onChange = [this]
    {
        audioProcessor.applyChordPreset(chordPresetBox.getSelectedItemIndex());
    };

    prevPresetBtn.onClick = [this]
    {
        const int idx = (chordPresetBox.getSelectedItemIndex() + 9) % 10;
        chordPresetBox.setSelectedItemIndex(idx, juce::sendNotification);
    };

    nextPresetBtn.onClick = [this]
    {
        const int idx = (chordPresetBox.getSelectedItemIndex() + 1) % 10;
        chordPresetBox.setSelectedItemIndex(idx, juce::sendNotification);
    };

    panicBtn.onClick = [this]
    {
        viewport.triggerPreview();
    };

    advancedBtn.setClickingTogglesState(true);
    advancedBtn.setToggleState(drawerExpanded, juce::dontSendNotification);
    advancedBtn.onClick = [this]
    {
        drawerExpanded = advancedBtn.getToggleState();
        updateDrawerVisibility();
        setSize(1024, drawerExpanded ? 765 : 562);
    };

    invertBtn.setClickingTogglesState(true);
    invertBtn.onClick = [this]
    {
        mangaLaf.setInverted(invertBtn.getToggleState());
        sendLookAndFeelChange();
        repaint();
    };

    panicBtn.setTooltip("Preview the comic impact animation. Use PANIC in keyboard settings to reset audio tails.");
    addAndMakeVisible(keyboard);
    addAndMakeVisible(voicePanel);
    addAndMakeVisible(modRoutePanel);
    keyboard.onSettings = [this]
    {
        performanceSettings = !performanceSettings;
        updateDrawerVisibility(); resized(); repaint();
    };
    addChildComponent(panicResetBtn);
    panicResetBtn.onClick = [this] { audioProcessor.triggerPanicReset(); };
    addChildComponent(closeSettingsBtn);
    closeSettingsBtn.onClick = [this]
    {
        performanceSettings = false;
        updateDrawerVisibility(); repaint();
    };

    // Viewport & Gun Strip
    addAndMakeVisible(viewport);
    addChildComponent(settingsBackdrop);
    addAndMakeVisible(gunStrip);

    // 6 Macro Knobs
    configureRotaryKnob(driveSlider,   driveLabel,   "DRIVE",   false);
    configureRotaryKnob(spaceSlider,   spaceLabel,   "SMEAR",   false);
    configureRotaryKnob(decaySlider,   decayLabel,   "RING TIME", false);
    configureRotaryKnob(toneSlider,    toneLabel,    "TONE",    false);
    configureRotaryKnob(densitySlider, densityLabel, "DENSITY", false);
    configureRotaryKnob(mixSlider,     mixLabel,     "MIX",     false);

    spaceSlider.setTooltip("SMEAR: short transient diffusion, from focused to blurred. No reverb tail. Legacy automation ID: space.");
    decaySlider.setTooltip("Sets the ring time of every resonator voice. The readout shows the nominal time before voice trim and damping.");

    driveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "drive",   driveSlider);
    spaceAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "space",   spaceSlider);
    decayAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "decay",   decaySlider);
    decaySlider.textFromValueFunction = [this](double value)
    {
        if (!audioProcessor.usesRingTime()) return juce::String((value - .05) / .95 * 100, 0) + "%";
        const auto seconds = audioProcessor.nominalRingSeconds(static_cast<float>(value));
        return seconds < 1 ? juce::String(seconds * 1000, 0) + " ms" : juce::String(seconds, 2) + " s";
    };
    toneAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "tone",    toneSlider);
    densityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "density", densitySlider);
    mixAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "mix",     mixSlider);

    // 12 Semitone Buttons
    for (int i = 0; i < 12; ++i)
    {
        auto& btn = noteButtons[static_cast<size_t>(i)];
        btn.setButtonText(kSemitoneLabels[i]);
        btn.setClickingTogglesState(true);
        addAndMakeVisible(btn);
        noteAtts[static_cast<size_t>(i)] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, "note_" + juce::String(i), btn);
    }

    // Root, Octave, Voice Cap, MIDI, Arp, Motion
    rootNoteBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    octaveBox.addItemList({ "1 OCT", "2 OCT", "3 OCT", "4 OCT" }, 1);
    voiceCapBox.addItemList({ "4 VOX", "6 VOX", "8 VOX", "12 VOX" }, 1);
    arpDirBox.addItemList({ "UP", "DOWN", "UP-DN", "RAND", "SYNC" }, 1);

    addAndMakeVisible(rootNoteBox);
    addAndMakeVisible(octaveBox);
    addAndMakeVisible(voiceCapBox);
    addAndMakeVisible(midiBtn);
    addAndMakeVisible(arpBtn);
    addAndMakeVisible(arpDirBox);
    addAndMakeVisible(motionBtn);
    addAndMakeVisible(arpRateSlider);

    midiBtn.setClickingTogglesState(true);
    arpBtn.setClickingTogglesState(true);
    motionBtn.setClickingTogglesState(true);

    arpRateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    arpRateSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    rootAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "rootNote",     rootNoteBox);
    octaveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "octaveSpread", octaveBox);
    voiceCapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "voiceCap",     voiceCapBox);
    midiAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "midiMode",     midiBtn);
    arpAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "arpEnable",    arpBtn);
    arpDirAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "arpDirection", arpDirBox);
    motionAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "motionEnable", motionBtn);
    arpRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "arpRate",      arpRateSlider);

    addAndMakeVisible(eqPanel);
    juce::TextButton* tabs[] = { &tabABtn, &tabBBtn, &tabCBtn, &tabDBtn, &tabEBtn, &tabFBtn, &tabGBtn };
    for (int t = 0; t < static_cast<int>(std::size(tabs)); ++t)
    {
        tabs[t]->setClickingTogglesState(true);
        tabs[t]->setRadioGroupId(4001);
        tabs[t]->setToggleState(t == activeDrawerTab, juce::dontSendNotification);
        tabs[t]->onClick = [this, t]
        {
            activeDrawerTab = t;
            updateDrawerVisibility();
            resized();
            repaint();
        };
        addAndMakeVisible(tabs[t]);
    }

    // Tab B // RESONATOR
    resTopologyBox.addItemList({ "AUTO (GUN MODE)", "BANDPASS SVF", "COMB STRING", "DUAL FORMANT" }, 1);
    addAndMakeVisible(resTopologyBox);
    resTopoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "resTopology", resTopologyBox);

    configureRotaryKnob(attackSlider,    attackLabel,    "ATTACK (MS)",  true);
    configureRotaryKnob(fbDampSlider,    fbDampLabel,    "FB DAMPING",   true);
    configureRotaryKnob(stiffnessSlider, stiffnessLabel, "STIFFNESS",    true);
    configureRotaryKnob(excHpfSlider,    excHpfLabel,    "EXC INPUT CUT", true);
    excHpfSlider.setTooltip("One-pole low cut for the resonator input, in Hz. Use G // EQ for full signal low cut.");
    configureRotaryKnob(excDriveSlider,  excDriveLabel,  "EXC DRIVE",    true);

    attackAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "resAttack", attackSlider);
    fbDampAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "fbDamping", fbDampSlider);
    stiffnessAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "stiffness", stiffnessSlider);
    excHpfAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "excHpf",    excHpfSlider);
    excDriveAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "excDrive",  excDriveSlider);

    // Tab C // MODULATION
    lfo1ShapeBox.addItemList({ "SINE", "TRI", "SAW", "SQR", "S&H" }, 1);
    lfo2ShapeBox.addItemList({ "SINE", "TRI", "SAW", "SQR", "S&H" }, 1);
    lfo1SyncBtn.setClickingTogglesState(true);
    lfo2SyncBtn.setClickingTogglesState(true);

    addAndMakeVisible(lfo1ShapeBox);
    addAndMakeVisible(lfo2ShapeBox);
    addAndMakeVisible(lfo1SyncBtn);
    addAndMakeVisible(lfo2SyncBtn);

    configureRotaryKnob(lfo1RateSlider,  lfo1RateLbl,  "LFO1 RATE",  true);
    configureRotaryKnob(lfo1PhaseSlider, lfo1PhaseLbl, "LFO1 PHASE", true);
    configureRotaryKnob(shiftSlider,     shiftLbl,     "SHIFT (HZ)", true);
    configureRotaryKnob(unisonSlider,    unisonLbl,    "UNISON",     true);

    configureRotaryKnob(lfo2RateSlider,  lfo2RateLbl,  "LFO2 RATE",  true);
    configureRotaryKnob(lfo2PhaseSlider, lfo2PhaseLbl, "LFO2 PHASE", true);
    configureRotaryKnob(envAttSlider,    envAttLbl,    "ENV ATT",    true);
    configureRotaryKnob(envRelSlider,    envRelLbl,    "ENV REL",    true);
    configureRotaryKnob(envAmtSlider,    envAmtLbl,    "ENV AMT",    true);

    lfo1ShapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "lfo1Shape",    lfo1ShapeBox);
    lfo2ShapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "lfo2Shape",    lfo2ShapeBox);
    lfo1SyncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "lfo1Sync",     lfo1SyncBtn);
    lfo2SyncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "lfo2Sync",     lfo2SyncBtn);
    lfo1RateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "lfo1Rate",     lfo1RateSlider);
    lfo1PhaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "lfo1Phase",    lfo1PhaseSlider);
    shiftAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "freqShift",    shiftSlider);
    unisonAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "stereoUnison", unisonSlider);
    lfo2RateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "lfo2Rate",     lfo2RateSlider);
    lfo2PhaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "lfo2Phase",    lfo2PhaseSlider);
    envAttAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "envAttack",    envAttSlider);
    envRelAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "envRelease",   envRelSlider);
    envAmtAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "envAmount",    envAmtSlider);

    balanceRevisionBtn.setTooltip("Fixed wet-level calibration and progressive controls for the upgraded engines. No adaptive auto-gain. Change while stopped.");
    addAndMakeVisible(balanceRevisionBtn);
    balanceRevisionBtn.onClick = [this]
    {
        audioProcessor.setBalanceRevision(audioProcessor.getBalanceRevision() >= 2 ? 1 : 2);
        balanceRevisionBtn.setButtonText(audioProcessor.getBalanceRevision() >= 2 ? "LEVELS: BALANCED" : "LEVELS: LEGACY");
    };
    balanceRevisionBtn.setButtonText(audioProcessor.getBalanceRevision() >= 2 ? "LEVELS: BALANCED" : "LEVELS: LEGACY");

    modalRevisionBtn.setTooltip("Rings modal body for PISTOL / REVOLVER in AUTO or SVF mode. PISTOL: six shorter modes; REVOLVER: twelve longer modes. Change while stopped.");
    addAndMakeVisible(modalRevisionBtn);
    modalRevisionBtn.onClick = [this]
    {
        audioProcessor.setModalDspRevision(audioProcessor.getModalDspRevision() >= 2 ? 1 : 2);
        modalRevisionBtn.setButtonText(audioProcessor.getModalDspRevision() >= 2 ? "MODAL: RINGS" : "MODAL: LEGACY");
    };
    modalRevisionBtn.setButtonText(audioProcessor.getModalDspRevision() >= 2 ? "MODAL: RINGS" : "MODAL: LEGACY");

    formantRevisionBtn.setTooltip("STK four-formant body plus pitched resonance. Applies to SMG / DUAL FORMANT. TONE morphs A-E-I-O-U; FB DAMPING broadens the formants. Change while stopped.");
    addAndMakeVisible(formantRevisionBtn);
    formantRevisionBtn.onClick = [this]
    {
        audioProcessor.setFormantDspRevision(audioProcessor.getFormantDspRevision() >= 2 ? 1 : 2);
        formantRevisionBtn.setButtonText(audioProcessor.getFormantDspRevision() >= 2 ? "FORMANT: STK" : "FORMANT: LEGACY");
    };
    formantRevisionBtn.setButtonText(audioProcessor.getFormantDspRevision() >= 2 ? "FORMANT: STK" : "FORMANT: LEGACY");
    toneSlider.setTooltip("TONE. In STK formant mode, also morphs vowel A-E-I-O-U.");

    stringRevisionBtn.setTooltip("Adapted Mutable Instruments Rings string model. Applies to SHOTGUN or the COMB STRING override. Change while stopped.");
    addAndMakeVisible(stringRevisionBtn);
    stringRevisionBtn.onClick = [this]
    {
        audioProcessor.setStringDspRevision(audioProcessor.getStringDspRevision() >= 2 ? 1 : 2);
        stringRevisionBtn.setButtonText(audioProcessor.getStringDspRevision() >= 2 ? "STRING: RINGS" : "STRING: LEGACY");
    };
    stringRevisionBtn.setButtonText(audioProcessor.getStringDspRevision() >= 2 ? "STRING: RINGS" : "STRING: LEGACY");

    nonlinearRevisionBtn.setTooltip("DRIVE/SNIPER anti-aliasing. Change while transport is stopped so the host can update latency compensation.");
    addAndMakeVisible(nonlinearRevisionBtn);
    nonlinearRevisionBtn.onClick = [this]
    {
        audioProcessor.setNonlinearDspRevision(audioProcessor.getNonlinearDspRevision() >= 2 ? 1 : 2);
        nonlinearRevisionBtn.setButtonText(audioProcessor.getNonlinearDspRevision() >= 2 ? "DRIVE DSP: NEW" : "DRIVE DSP: LEGACY");
    };
    nonlinearRevisionBtn.setButtonText(audioProcessor.getNonlinearDspRevision() >= 2 ? "DRIVE DSP: NEW" : "DRIVE DSP: LEGACY");

    spatialRevisionBtn.setTooltip("Switch spatial DSP. LEGACY preserves v1.1.2 sessions; NEW enables corrected diffusion and frequency shifting.");
    addAndMakeVisible(spatialRevisionBtn);
    spatialRevisionBtn.onClick = [this]
    {
        audioProcessor.setSpatialDspRevision(audioProcessor.getSpatialDspRevision() >= 2 ? 1 : 2);
        spatialRevisionBtn.setButtonText(audioProcessor.getSpatialDspRevision() >= 2 ? "SPATIAL: NEW" : "SPATIAL: LEGACY");
    };
    spatialRevisionBtn.setButtonText(audioProcessor.getSpatialDspRevision() >= 2 ? "SPATIAL: NEW" : "SPATIAL: LEGACY");

    // Tab D // GLOBAL
    configureRotaryKnob(inGainSlider,  inGainLbl,  "IN GAIN (DB)",  true);
    configureRotaryKnob(outGainSlider, outGainLbl, "OUT GAIN (DB)", true);
    configureRotaryKnob(widthSlider,   widthLbl,   "WIDTH (%)",     true);
    configureRotaryKnob(smoothSlider,  smoothLbl,  "SMOOTH (MS)",   true);

    mixCurveBox.addItemList({ "EQUAL POWER", "LINEAR", "S-CURVE" }, 1);
    oversampleBox.addItemList({ "1X OFF", "2X IIR", "4X IIR" }, 1);
    limiterBtn.setClickingTogglesState(true);
    limiterBtn.onClick = [this]
    {
        limiterBtn.setButtonText(limiterBtn.getToggleState() ? "LIMITER: ON" : "LIMITER: OFF");
    };

    mixCurveLbl.setText("MIX CURVE", juce::dontSendNotification);
    mixCurveLbl.setFont(mangaLaf.getMangaFont(10.0f, true));
    mixCurveLbl.setJustificationType(juce::Justification::centred);

    oversampleLbl.setText("OVERSAMPLING", juce::dontSendNotification);
    oversampleLbl.setFont(mangaLaf.getMangaFont(10.0f, true));
    oversampleLbl.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(mixCurveLbl);
    addAndMakeVisible(mixCurveBox);
    addAndMakeVisible(oversampleLbl);
    addAndMakeVisible(oversampleBox);
    addAndMakeVisible(limiterBtn);

    inGainAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "inGain",        inGainSlider);
    outGainAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "outGain",       outGainSlider);
    widthAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "stereoWidth",   widthSlider);
    smoothAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (audioProcessor.apvts, "paramSmoothMs", smoothSlider);
    mixCurveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "mixCurve",      mixCurveBox);
    oversampleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "oversampling",  oversampleBox);
    limiterAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>  (audioProcessor.apvts, "softLimiter",   limiterBtn);

    limiterBtn.setButtonText(limiterBtn.getToggleState() ? "LIMITER: ON" : "LIMITER: OFF");

    addAndMakeVisible(smearRevisionBtn);
    addAndMakeVisible(ringTimeBtn);
    ringTimeBtn.setTooltip("Choose physical ring time or the original decay mapping. Switch with playback stopped.");
    ringTimeBtn.setButtonText(audioProcessor.usesRingTime() ? "RING: NEW" : "RING: LEGACY");
    ringTimeBtn.onClick = [this]
    {
        audioProcessor.setRingTimeEnabled(!audioProcessor.usesRingTime());
        ringTimeBtn.setButtonText(audioProcessor.usesRingTime() ? "RING: NEW" : "RING: LEGACY");
    };
    smearRevisionBtn.setTooltip("SHORT uses compact diffusion with a true zero point in every gun mode. LEGACY recalls earlier SPACE behavior. Change while stopped.");
    smearRevisionBtn.onClick = [this]
    {
        const bool shortMode = !audioProcessor.isShortSmearEnabled();
        if (shortMode) audioProcessor.setSpatialDspRevision(2);
        audioProcessor.setSmearDspRevision(shortMode ? 2 : 1);
        smearRevisionBtn.setButtonText(shortMode ? "SMEAR: SHORT" : "SMEAR: LEGACY");
    };
    smearRevisionBtn.setButtonText(audioProcessor.isShortSmearEnabled() ? "SMEAR: SHORT" : "SMEAR: LEGACY");
    smearInfo.setText("SHORT DIFFUSION // REVERB IS IN TAB F", juce::dontSendNotification);
    smearInfo.setFont(mangaLaf.getMangaFont(14)); addAndMakeVisible(smearInfo);

    const char* ottIds[] = {"ottDepth", "ottTime", "ottUp", "ottDown", "ottInGain", "ottOutGain", "ottLowSplit", "ottHighSplit"};
    const char* ottNames[] = {"DEPTH", "TIME", "UPWARD", "DOWNWARD", "IN GAIN", "OUT GAIN", "LOW SPLIT", "HIGH SPLIT"};
    const char* ottTips[] = {
        "Amount of compression across all three bands. 0% bypasses OTT.",
        "Release speed. 100% uses the model's measured timings.",
        "Lift quieter details in each band.", "Tame louder peaks in each band.",
        "Input trim before the detector.", "Output trim before the resonator.",
        "Low/mid crossover frequency.", "Mid/high crossover frequency."
    };
    for (std::size_t i = 0; i < ottSliders.size(); ++i)
    {
        auto& slider = ottSliders[i];
        configureRotaryKnob(slider, ottLabels[i], ottNames[i], true);
        slider.setTooltip(ottTips[i]);
        ottAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, ottIds[i], slider);
    }
    auto percentDisplay = [](juce::Slider& slider)
    {
        slider.textFromValueFunction = [](double value) { return juce::String(value * 100, 1) + "%"; };
        slider.valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue() * .01; };
        slider.updateText();
    };
    ottSliders[0].setName("OTT DEPTH");
    percentDisplay(ottSliders[0]);
    ottSliders[1].setTextValueSuffix("%");
    for (std::size_t i : { 2u, 3u })
    {
        auto& slider = ottSliders[i];
        slider.textFromValueFunction = [this](double value)
        {
            return juce::String(value * (audioProcessor.usesFaustOtt() ? 200 : 100), 1) + "%";
        };
        slider.valueFromTextFunction = [this](const juce::String& text)
        {
            return text.getDoubleValue() / (audioProcessor.usesFaustOtt() ? 200 : 100);
        };
        slider.updateText();
    }
    for (std::size_t i : { 4u, 5u }) ottSliders[i].setTextValueSuffix(" dB");
    for (std::size_t i : { 6u, 7u }) ottSliders[i].setTextValueSuffix(" Hz");
    ottLabels[0].setFont(mangaLaf.getMangaFont(17, true));
    ottEnableBtn.setClickingTogglesState(true);
    ottEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "ottEnable", ottEnableBtn);
    ottEnableBtn.onClick = [this] { refreshOttPanel(); };
    ottSetupBtn.onClick = [this]
    {
        ottSetupOpen = !ottSetupOpen;
        ottSetupBtn.setButtonText(ottSetupOpen ? "< BACK" : "SETUP >");
        updateDrawerVisibility();
    };
    ottModelBtn.setTooltip("FAUST uses the measured model. CLASSIC preserves the earlier BREACH response.");
    ottModelBtn.onClick = [this]
    {
        audioProcessor.setFaustOttEnabled(!audioProcessor.usesFaustOtt());
        refreshOttPanel();
    };
    ottHeader.setText("INPUT DYNAMICS  /  BEFORE RESONATOR", juce::dontSendNotification);
    ottHeader.setFont(mangaLaf.getMangaFont(16, true));
    for (auto* component : std::initializer_list<juce::Component*> {
             &ottEnableBtn, &ottSetupBtn, &ottModelBtn, &ottHeader, &ottMeters })
        addAndMakeVisible(*component);
    ottFaustDisplayed = !audioProcessor.usesFaustOtt();
    refreshOttPanel();

    const char* reverbIds[] = {"reverbMix","reverbDecay","reverbPreDelay","reverbDamping","reverbWidth","reverbShimmer"};
    const char* reverbNames[] = {"BLEND","DECAY (S)","PREDELAY (MS)","DAMPING","WIDTH","SHIMMER"};
    const char* reverbTips[] = {"Separate post-resonator reverb mix. Zero is dry.","Reverb decay target in seconds, separate from resonator DECAY.",
        "Delay before the reverb starts.","Darkens the feedback and shortens high-frequency ringing.","Stereo width of the reverb only.","Octave-up layer amount, active only in SHIMMER mode."};
    for(size_t i=0;i<6;++i)
    {
        configureRotaryKnob(reverbSliders[i],reverbLabels[i],reverbNames[i],true);
        reverbSliders[i].setTooltip(reverbTips[i]);
        reverbAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts,reverbIds[i],reverbSliders[i]);
    }
    for(size_t i:{0u,3u,4u,5u})percentDisplay(reverbSliders[i]);
    reverbEnableBtn.setClickingTogglesState(true);addAndMakeVisible(reverbEnableBtn);
    reverbEnableAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts,"reverbEnable",reverbEnableBtn);
    reverbEnableBtn.onClick=[this]{reverbEnableBtn.setButtonText(reverbEnableBtn.getToggleState()?"REVERB: ON":"REVERB: OFF");};
    reverbEnableBtn.onClick();
    reverbTypeBox.addItemList({"HALL","PLATE","SPACE","SHIMMER"},1);addAndMakeVisible(reverbTypeBox);
    reverbTypeBox.setTooltip("HALL: open room. PLATE: compact and dense. SPACE: large and sparse. SHIMMER: octave-up late field.");
    reverbTypeBox.setName("REVERB TYPE");
    reverbTypeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts,"reverbType",reverbTypeBox);
    reverbTypeBox.onChange=[this]
    {
        const bool shimmerMode=reverbTypeBox.getSelectedId()==4;
        reverbSliders[5].setEnabled(shimmerMode);
        reverbSliders[5].setAlpha(shimmerMode?1.0f:.35f);
        reverbLabels[5].setAlpha(shimmerMode?1.0f:.35f);
    };
    reverbTypeBox.onChange();
    reverbHeader.setText("BREACH MIX -> REVERB -> OUTPUT",juce::dontSendNotification);
    reverbHeader.setFont(mangaLaf.getMangaFont(17));addAndMakeVisible(reverbHeader);
    reverbInfo.setText("SMEAR BLURS TRANSIENTS. REVERB ADDS A SEPARATE TAIL.",juce::dontSendNotification);
    reverbInfo.setFont(mangaLaf.getMangaFont(13));addAndMakeVisible(reverbInfo);

    updateDrawerVisibility();
    setSize(1024, drawerExpanded ? 765 : 562);
    startTimerHz(60);
}

BreachResonatorAudioProcessorEditor::~BreachResonatorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BreachResonatorAudioProcessorEditor::configureRotaryKnob(juce::Slider& slider,
                                                              juce::Label& label,
                                                              const juce::String& text,
                                                              bool showTextBox)
{
    slider.setName(text);
    slider.setTooltip(text);
    slider.setPopupDisplayEnabled(!showTextBox, !showTextBox, this);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    if (showTextBox)
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 17);
    else
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    label.setText(text, juce::dontSendNotification);
    label.setFont(mangaLaf.getMangaFont(showTextBox ? 15.0f : 24.0f, false));
    label.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void BreachResonatorAudioProcessorEditor::updateDrawerVisibility()
{
    settingsBackdrop.setVisible(performanceSettings);
    if (performanceSettings) settingsBackdrop.toFront(false);
    const bool showA = drawerExpanded && (activeDrawerTab == 0);
    const bool showB = drawerExpanded && (activeDrawerTab == 1);
    const bool showC = drawerExpanded && (activeDrawerTab == 2);
    const bool showD = drawerExpanded && (activeDrawerTab == 3);
    const bool showE = drawerExpanded && (activeDrawerTab == 4);
    const bool showF = drawerExpanded && (activeDrawerTab == 5);

    tabABtn.setVisible(drawerExpanded);
    tabBBtn.setVisible(drawerExpanded);
    tabCBtn.setVisible(drawerExpanded);
    tabDBtn.setVisible(drawerExpanded);
    tabEBtn.setVisible(drawerExpanded);tabFBtn.setVisible(drawerExpanded);tabGBtn.setVisible(drawerExpanded);
    eqPanel.setVisible(drawerExpanded && activeDrawerTab == 6);
    if (eqPanel.isVisible()) eqPanel.refresh();

    voicePanel.setVisible(showA);
    modRoutePanel.setVisible(showC);
    if (showC) modRoutePanel.refresh();

    for (auto* component : std::initializer_list<juce::Component*>{&rootNoteBox, &octaveBox, &voiceCapBox, &midiBtn,
             &arpBtn, &arpDirBox, &motionBtn, &arpRateSlider, &closeSettingsBtn, &panicResetBtn})
    {
        component->setVisible(performanceSettings);
        if (performanceSettings) component->toFront(false);
    }
    for (auto& note : noteButtons) note.setVisible(false);

    // Tab B
    resTopologyBox.setVisible(showB);
    ringTimeBtn.setVisible(showB);smearRevisionBtn.setVisible(showB);smearInfo.setVisible(showB);
    for (auto* c : std::initializer_list<juce::Component*>{
             &attackSlider, &attackLabel, &fbDampSlider, &fbDampLabel,
             &stiffnessSlider, &stiffnessLabel, &excHpfSlider, &excHpfLabel,
             &excDriveSlider, &excDriveLabel })
    {
        c->setVisible(showB);
    }

    // Tab C
    for (auto* c : std::initializer_list<juce::Component*>{
             &lfo1ShapeBox, &lfo1SyncBtn, &lfo1RateSlider, &lfo1RateLbl, &lfo1PhaseSlider, &lfo1PhaseLbl,
             &shiftSlider, &shiftLbl, &unisonSlider, &unisonLbl,
             &lfo2ShapeBox, &lfo2SyncBtn, &lfo2RateSlider, &lfo2RateLbl, &lfo2PhaseSlider, &lfo2PhaseLbl,
             &envAttSlider, &envAttLbl, &envRelSlider, &envRelLbl, &envAmtSlider, &envAmtLbl })
    {
        c->setVisible(showC);
    }
    // Tab D
    for (auto* c : std::initializer_list<juce::Component*>{
             &inGainSlider, &inGainLbl, &outGainSlider, &outGainLbl,
             &widthSlider, &widthLbl, &mixCurveLbl, &mixCurveBox,
             &oversampleLbl, &oversampleBox, &limiterBtn, &spatialRevisionBtn, &nonlinearRevisionBtn, &stringRevisionBtn, &formantRevisionBtn, &modalRevisionBtn, &balanceRevisionBtn, &smoothSlider, &smoothLbl })
    {
        c->setVisible(showD);
    }
    ottEnableBtn.setVisible(showE);
    ottSetupBtn.setVisible(showE);
    ottHeader.setVisible(showE);
    ottModelBtn.setVisible(showE && ottSetupOpen);
    ottMeters.setVisible(showE && !ottSetupOpen);
    for (std::size_t i = 0; i < ottSliders.size(); ++i)
    {
        const bool visible = showE && (i < 4 || i == 5 || ottSetupOpen);
        ottSliders[i].setVisible(visible);
        ottLabels[i].setVisible(visible);
    }
    if (showE) refreshOttPanel();
    reverbEnableBtn.setVisible(showF);reverbTypeBox.setVisible(showF);reverbHeader.setVisible(showF);reverbInfo.setVisible(showF);
    for(auto& c:reverbSliders)c.setVisible(showF);
    for(auto& c:reverbLabels)c.setVisible(showF);
}

void BreachResonatorAudioProcessorEditor::refreshOttPanel()
{
    ottEnableBtn.setButtonText(ottEnableBtn.getToggleState() ? "OTT: ON" : "OTT: OFF");
    const bool useFaust = audioProcessor.usesFaustOtt();
    if (useFaust != ottFaustDisplayed)
    {
        ottFaustDisplayed = useFaust;
        ottModelBtn.setButtonText(useFaust ? "MODEL: FAUST" : "MODEL: CLASSIC");
        ottSliders[1].setTooltip(useFaust ? "Release speed. 100% uses the model's measured timings."
                                          : "Scales attack and release together. 100% uses classic timing.");
        ottSliders[2].updateText();
        ottSliders[3].updateText();
    }
    if (ottMeters.isVisible()) ottMeters.repaint();
}

void BreachResonatorAudioProcessorEditor::OttGainMeters::paint(juce::Graphics& g)
{
    const auto ink = laf.getInkColour();
    const auto paper = laf.getPaperColour();
    auto area = getLocalBounds().reduced(1);
    g.setColour(ink.withAlpha(.52f));
    g.drawRect(area);
    area.reduce(9, 4);
    auto header = area.removeFromTop(17);
    header.removeFromLeft(49);
    header.removeFromRight(83);
    g.setColour(ink);
    g.setFont(laf.getMangaFont(12, true));
    g.drawText("CUT", header.removeFromLeft(header.getWidth() / 2), juce::Justification::centred);
    g.drawText("LIFT", header, juce::Justification::centred);

    constexpr const char* bandNames[] { "LOW", "MID", "HIGH" };
    for (std::size_t band = 0; band < telemetry.ottGainDb.size(); ++band)
    {
        auto row = area.removeFromTop(24);
        auto name = row.removeFromLeft(49);
        auto value = row.removeFromRight(83);
        auto track = row.reduced(0, 5);
        const int centre = track.getCentreX();
        const float db = telemetry.ottGainDb[band].load(std::memory_order_relaxed);
        const int end = centre + juce::roundToInt(juce::jlimit(-36.0f, 36.0f, db) * static_cast<float>(track.getWidth()) / 72);
        g.setColour(ink);
        g.setFont(laf.getMangaFont(14, true));
        g.drawText(bandNames[band], name, juce::Justification::centredLeft);
        const float displayDb = std::abs(db) < .05f ? 0.0f : db;
        const auto signedValue = (displayDb > 0 ? juce::String("+") : juce::String()) + juce::String(displayDb, 1) + " dB";
        g.drawText(signedValue, value.reduced(5, 0), juce::Justification::centredRight);
        g.setColour(ink.withAlpha(.18f));
        g.fillRect(track);
        if (centre != end)
        {
            g.setColour(ink);
            g.fillRect(juce::jmin(centre, end), track.getY(), std::abs(end - centre), track.getHeight());
        }
        g.setColour(paper);
        g.drawVerticalLine(centre, static_cast<float>(track.getY() - 1), static_cast<float>(track.getBottom() + 1));
    }
}

void BreachResonatorAudioProcessorEditor::timerCallback()
{
    if (modRoutePanel.isVisible()) modRoutePanel.refresh();
    ringTimeBtn.setButtonText(audioProcessor.usesRingTime() ? "RING: NEW" : "RING: LEGACY");
    if (eqPanel.isVisible()) eqPanel.refresh();
    smearRevisionBtn.setButtonText(audioProcessor.isShortSmearEnabled()?"SMEAR: SHORT":"SMEAR: LEGACY");
    if (ottEnableBtn.isVisible()) refreshOttPanel();
    reverbEnableBtn.setButtonText(reverbEnableBtn.getToggleState()?"REVERB: ON":"REVERB: OFF");
    nonlinearRevisionBtn.setButtonText(audioProcessor.getNonlinearDspRevision() >= 2 ? "DRIVE DSP: NEW" : "DRIVE DSP: LEGACY");
    stringRevisionBtn.setButtonText(audioProcessor.getStringDspRevision() >= 2 ? "STRING: RINGS" : "STRING: LEGACY");
    formantRevisionBtn.setButtonText(audioProcessor.getFormantDspRevision() >= 2 ? "FORMANT: STK" : "FORMANT: LEGACY");
    modalRevisionBtn.setButtonText(audioProcessor.getModalDspRevision() >= 2 ? "MODAL: RINGS" : "MODAL: LEGACY");
    balanceRevisionBtn.setButtonText(audioProcessor.getBalanceRevision() >= 2 ? "LEVELS: BALANCED" : "LEVELS: LEGACY");
    viewport.tickAnimation(juce::Time::getMillisecondCounterHiRes());
    keyboard.repaint();
    gunStrip.repaint();
    spatialRevisionBtn.setButtonText(audioProcessor.getSpatialDspRevision() >= 2 ? "SPATIAL: NEW" : "SPATIAL: LEGACY");
    limiterBtn.setButtonText(limiterBtn.getToggleState() ? "LIMITER: ON" : "LIMITER: OFF");
}

void BreachResonatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto paper = mangaLaf.getPaperColour();
    const auto ink = mangaLaf.getInkColour();
    g.fillAll(paper); g.setColour(ink);
    g.drawEllipse(24.0f, 18.0f, 44.0f, 44.0f, 2.0f);
    g.drawLine(17.0f, 40.0f, 39.0f, 40.0f, 2.0f);
    g.drawLine(52.0f, 40.0f, 75.0f, 40.0f, 2.0f);
    g.drawLine(46.0f, 10.0f, 46.0f, 32.0f, 2.0f);
    g.drawLine(46.0f, 47.0f, 46.0f, 69.0f, 2.0f);
    g.fillEllipse(44.0f, 38.0f, 4.0f, 4.0f);
    g.setFont(juce::Font(juce::FontOptions(40.0f, juce::Font::bold)));
    g.drawText("BREACH //", 90, 17, 200, 43, juce::Justification::centredLeft, false);
    g.setFont(mangaLaf.getMangaFont(24.0f, true));
    g.drawText("RESONATOR", 285, 23, 99, 33, juce::Justification::centredLeft, false);
    if (drawerExpanded)
    {
        g.drawRoundedRectangle(14.0f, 565.0f, 996.0f, 187.0f, 3.0f, 2.0f);
        g.drawLine(14.0f, 603.0f, 1010.0f, 603.0f, 1.7f);
    }

}

void BreachResonatorAudioProcessorEditor::resized()
{
    prevPresetBtn.setBounds(399, 21, 37, 37);
    chordPresetBox.setBounds(446, 21, 138, 37);
    nextPresetBtn.setBounds(594, 21, 37, 37);
    panicBtn.setBounds(662, 21, 102, 37);
    advancedBtn.setBounds(774, 21, 118, 37);
    invertBtn.setBounds(903, 21, 103, 37);
    auto placeMacro = [](juce::Slider& s, juce::Label& l, int x, int y)
    {
        s.setBounds(x, y, 100, 94); l.setBounds(x - 6, y + 89, 112, 28);
    };
    placeMacro(driveSlider, driveLabel, 38, 89);
    placeMacro(spaceSlider, spaceLabel, 38, 209);
    placeMacro(decaySlider, decayLabel, 38, 329);
    placeMacro(toneSlider, toneLabel, 885, 89);
    placeMacro(densitySlider, densityLabel, 885, 209);
    placeMacro(mixSlider, mixLabel, 885, 329);
    viewport.setBounds(175, 90, 674, 316);
    keyboard.setBounds(175, 405, 674, 39);
    gunStrip.setBounds(13, 455, 998, 95);
    rootNoteBox.setBounds(328, 320, 65, 28);
    octaveBox.setBounds(399, 320, 82, 28);
    voiceCapBox.setBounds(487, 320, 86, 28);
    midiBtn.setBounds(579, 320, 65, 28);
    panicResetBtn.setBounds(651, 320, 91, 28);
    closeSettingsBtn.setBounds(754, 320, 80, 28);
    arpBtn.setBounds(192, 360, 65, 28);
    arpDirBox.setBounds(264, 360, 88, 28);
    motionBtn.setBounds(360, 360, 88, 28);
    arpRateSlider.setBounds(462, 360, 369, 28);
    settingsBackdrop.setBounds(180, 312, 664, 89);
    if (!drawerExpanded) return;
    juce::TextButton* tabs[] = {&tabABtn, &tabBBtn, &tabCBtn, &tabDBtn, &tabEBtn, &tabFBtn, &tabGBtn};
    constexpr std::array<int, 7> widths {{121, 156, 156, 134, 144, 140, 115}};
    int tabX = 14;
    for (std::size_t t = 0; t < widths.size(); ++t)
    {
        tabs[t]->setBounds(tabX, 565, widths[t], 38);
        tabX += widths[t] + 5;
    }
    eqPanel.setBounds(26, 612, 972, 132);
    voicePanel.setBounds(30, 612, 964, 134);
    modRoutePanel.setBounds(532, 611, 462, 135);

    // Tab B // RESONATOR Layout
    if (activeDrawerTab == 1)
    {
        resTopologyBox.setBounds(18, 588, 165, 24);
        smearRevisionBtn.setBounds(200, 588, 150, 24);
        ringTimeBtn.setBounds(365, 588, 148, 24);
        smearInfo.setBounds(528, 588, 298, 24);

        auto placeDrawerKnob = [](juce::Slider& s, juce::Label& l, int x, int y)
        {
            l.setBounds(x, y, 120, 15);
            s.setBounds(x + 12, y + 16, 96, 92);
        };

        placeDrawerKnob(attackSlider,    attackLabel,     44, 616);
        placeDrawerKnob(fbDampSlider,    fbDampLabel,    208, 616);
        placeDrawerKnob(stiffnessSlider, stiffnessLabel, 372, 616);
        placeDrawerKnob(excHpfSlider,    excHpfLabel,    536, 616);
        placeDrawerKnob(excDriveSlider,  excDriveLabel,  700, 616);
    }

    // Tab C // MODULATION Layout
    if (activeDrawerTab == 2)
    {
        lfo1ShapeBox.setBounds(18, 608, 62, 24);
        lfo1SyncBtn.setBounds(84,  608, 44, 24);

        lfo2ShapeBox.setBounds(18, 678, 62, 24);
        lfo2SyncBtn.setBounds(84,  678, 44, 24);

        auto placeModKnob = [](juce::Slider& s, juce::Label& l, int x, int y)
        {
            l.setBounds(x - 4, y, 66, 13);
            s.setBounds(x, y + 13, 58, 54);
        };

        placeModKnob(lfo1RateSlider,  lfo1RateLbl,  134, 586);
        placeModKnob(lfo1PhaseSlider, lfo1PhaseLbl, 198, 586);
        placeModKnob(shiftSlider,     shiftLbl,     266, 586);
        placeModKnob(unisonSlider,    unisonLbl,    342, 586);

        placeModKnob(lfo2RateSlider,  lfo2RateLbl,  134, 658);
        placeModKnob(lfo2PhaseSlider, lfo2PhaseLbl, 198, 658);
        placeModKnob(envAttSlider,    envAttLbl,    262, 658);
        placeModKnob(envRelSlider,    envRelLbl,    318, 658);
        placeModKnob(envAmtSlider,    envAmtLbl,    374, 658);

    }

    // Tab D // GLOBAL Layout
    if (activeDrawerTab == 3)
    {
        auto placeGlobalKnob = [](juce::Slider& s, juce::Label& l, int x, int y)
        {
            l.setBounds(x, y, 110, 16);
            s.setBounds(x + 7, y + 18, 96, 94);
        };

        placeGlobalKnob(inGainSlider,  inGainLbl,  28,  604);
        placeGlobalKnob(outGainSlider, outGainLbl, 154, 604);
        placeGlobalKnob(widthSlider,   widthLbl,   280, 604);

        balanceRevisionBtn.setBounds(410, 584, 256, 22);
        mixCurveLbl.setBounds(410, 608, 126, 16);
        mixCurveBox.setBounds(410, 628, 126, 26);

        oversampleLbl.setBounds(550, 608, 116, 16);
        oversampleBox.setBounds(550, 628, 116, 26);

        limiterBtn.setBounds(410, 663, 124, 24);
        modalRevisionBtn.setBounds(542, 663, 124, 24);
        spatialRevisionBtn.setBounds(410, 693, 124, 24);
        nonlinearRevisionBtn.setBounds(542, 693, 124, 24);
        stringRevisionBtn.setBounds(410, 720, 124, 18);
        formantRevisionBtn.setBounds(542, 720, 124, 18);

        placeGlobalKnob(smoothSlider, smoothLbl, 696, 604);
    }
    if (activeDrawerTab == 4)
    {
        ottEnableBtn.setBounds(28, 611, 108, 25);
        ottHeader.setBounds(150, 611, 380, 25);
        ottSetupBtn.setBounds(882, 611, 111, 25);
        ottModelBtn.setBounds(706, 611, 160, 25);
        ottMeters.setBounds(550, 638, 443, 103);
        const int knobX[] = { 28, 138, 237, 336, 552, 435, 700, 848 };
        const int knobW[] = { 104, 92, 92, 92, 140, 101, 140, 140 };
        for (std::size_t i = 0; i < ottSliders.size(); ++i)
        {
            ottLabels[i].setBounds(knobX[i], 642, knobW[i], 17);
            ottSliders[i].setBounds(knobX[i], 660, knobW[i], i == 0 ? 86 : 78);
        }
    }
    if(activeDrawerTab==5)
    {
        reverbEnableBtn.setBounds(30,610,110,25);reverbTypeBox.setBounds(158,610,158,25);
        reverbHeader.setBounds(340,610,650,25);
        for(size_t i=0;i<6;++i)
        {
            const int x=34+static_cast<int>(i)*162;
            reverbLabels[i].setBounds(x,639,130,16);reverbSliders[i].setBounds(x+22,656,86,78);
        }
        reverbInfo.setBounds(205,736,760,15);
    }
    if (activeDrawerTab > 0 && activeDrawerTab < 4)
    {
        for (auto* child : getChildren())
        {
            if (!child->isVisible() || child == &modRoutePanel) continue;
            auto bounds = child->getBounds();
            if (bounds.getY() >= 584)
            {
                bounds.setX(14 + juce::roundToInt((bounds.getX() - 10) * 1.19f));
                bounds.setWidth(juce::roundToInt(bounds.getWidth() * 1.19f));
                bounds.setY(608 + juce::roundToInt((bounds.getY() - 584) * 0.89f));
                bounds.setHeight(juce::roundToInt(bounds.getHeight() * 0.89f));
                child->setBounds(bounds);
            }
        }
    }
}

