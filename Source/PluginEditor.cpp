#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr const char* kSemitoneLabels[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Pure English comic-book SFX replacing all Japanese Katakana strings
    constexpr const char* kImpactSfx[5][3] = {
        { "BANG!!",   "CRACK!!",   "SNAP!!"   }, // 0: PISTOL
        { "BOOM!!",   "BLAM!!",    "BLAST!!"  }, // 1: SHOTGUN
        { "BRRRRT!!", "RATATAT!!", "BURST!!"  }, // 2: SMG
        { "PIERCE!!", "KRAKOW!!",  "SHATTER!!"}, // 3: SNIPER
        { "THOOM!!",  "CRUSH!!",   "SLAM!!"   }  // 4: REVOLVER
    };

    constexpr const char* kTailSfx[5] = {
        "RING...",    // 0: PISTOL
        "RUMBLE...",  // 1: SHOTGUN
        "BUZZZ...",   // 2: SMG
        "ZINGGG...",  // 3: SNIPER
        "HUMMM..."    // 4: REVOLVER
    };
}

//==============================================================================
// MangaSplitViewport
//==============================================================================
MangaSplitViewport::MangaSplitViewport(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l)
    : processor(p), laf(l)
{
    particles.resize(24);
}

void MangaSplitViewport::tickAnimation()
{
    const float tr = processor.telemetry.transientImpulse.load(std::memory_order_relaxed);
    if (tr > 0.35f && lastTransient <= 0.35f)
    {
        recoilVel = 14.0f * tr;
        flashAlpha = juce::jmin(0.85f, tr * 0.75f);
        sfxIndex = (sfxIndex + 1) % 3;

        // Spawn angular manga debris shards
        for (size_t i = 0; i < particles.size(); ++i)
        {
            const float angle = (static_cast<float>(i) / static_cast<float>(particles.size())) * juce::MathConstants<float>::twoPi;
            const float speed = 2.0f + static_cast<float>((i * 7) % 5) * 1.1f;
            particles[i].x = 0.0f;
            particles[i].y = 0.0f;
            particles[i].vx = std::cos(angle) * speed;
            particles[i].vy = std::sin(angle) * speed;
            particles[i].size = 2.5f + static_cast<float>(i % 4);
            particles[i].life = 1.0f;
        }
    }
    lastTransient = tr;

    // Spring recoil integration
    const float kSpring = 0.24f;
    const float kDamp = 0.76f;
    recoilVel += -kSpring * recoilOffset;
    recoilVel *= kDamp;
    recoilOffset += recoilVel;

    flashAlpha *= 0.82f;

    for (auto& pt : particles)
    {
        if (pt.life > 0.01f)
        {
            pt.x += pt.vx;
            pt.y += pt.vy;
            pt.life *= 0.91f;
        }
    }
}

void MangaSplitViewport::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    auto paper = laf.getPaperColour();
    auto ink   = laf.getInkColour();

    // Subtle transient flash inversion
    if (flashAlpha > 0.12f)
    {
        paper = paper.interpolatedWith(ink, flashAlpha * 0.28f);
    }

    g.setColour(paper);
    g.fillRect(bounds);

    // Right-hand manga screentone meter strip (12% width)
    const float stripW = juce::jmax(34.0f, bounds.getWidth() * 0.12f);
    const auto mainArea = bounds.withTrimmedRight(stripW);
    const auto meterArea = bounds.removeFromRight(stripW);

    // Draw halftone dots in meter strip
    g.setColour(ink.withAlpha(0.75f));
    const float outRms = juce::jlimit(0.0f, 1.0f, processor.telemetry.outputRms.load(std::memory_order_relaxed) * 2.4f);
    const float activeTopY = meterArea.getBottom() - meterArea.getHeight() * outRms;

    for (float y = meterArea.getY() + 4.0f; y < meterArea.getBottom() - 3.0f; y += 5.0f)
    {
        const bool isHot = (y >= activeTopY);
        const float dotR = isHot ? 1.6f : 0.85f;
        for (float x = meterArea.getX() + 4.0f; x < meterArea.getRight() - 3.0f; x += 5.0f)
        {
            g.fillEllipse(x - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    // Left-to-center radial harmonic voice rays
    const int arpVoice = processor.telemetry.activeArpVoice.load(std::memory_order_relaxed);
    const float focalX = mainArea.getX() + mainArea.getWidth() * 0.42f;
    const float focalY = mainArea.getCentreY();

    for (int v = 0; v < 12; ++v)
    {
        const float energy = juce::jlimit(0.0f, 1.0f,
            processor.telemetry.voiceEnergies[static_cast<size_t>(v)].load(std::memory_order_relaxed) * 3.5f);
        const float frac = (static_cast<float>(v) - 5.5f) / 5.5f;
        const float startY = mainArea.getCentreY() + frac * (mainArea.getHeight() * 0.44f);
        const float endY   = focalY + frac * (mainArea.getHeight() * 0.14f);

        const bool isArp = (v == arpVoice);
        const float strokeW = isArp ? 2.4f : (1.0f + energy * 1.6f);
        g.setColour(ink.withAlpha(isArp ? 1.0f : (0.55f + 0.45f * energy)));
        g.drawLine(mainArea.getX() + 2.0f, startY, focalX - 8.0f, endY, strokeW);
    }

    // Center-right kinetic bullet impact & fracture star
    const float cx = mainArea.getX() + mainArea.getWidth() * 0.68f + recoilOffset;
    const float cy = mainArea.getCentreY();

    // Fracture radial cracks
    g.setColour(ink);
    for (int c = 0; c < 10; ++c)
    {
        const float baseA = (static_cast<float>(c) / 10.0f) * juce::MathConstants<float>::twoPi + 0.25f;
        const float r1 = 18.0f;
        const float r2 = 34.0f + static_cast<float>((c * 13) % 18) + recoilOffset * 0.7f;
        const float mx = cx + std::cos(baseA + 0.08f) * ((r1 + r2) * 0.52f);
        const float my = cy + std::sin(baseA + 0.08f) * ((r1 + r2) * 0.52f);

        juce::Path crack;
        crack.startNewSubPath(cx + std::cos(baseA) * r1, cy + std::sin(baseA) * r1);
        crack.lineTo(mx, my);
        crack.lineTo(cx + std::cos(baseA - 0.05f) * r2, cy + std::sin(baseA - 0.05f) * r2);
        g.strokePath(crack, juce::PathStrokeType(1.3f));
    }

    // Jagged starburst crater
    juce::Path star;
    constexpr int points = 16;
    for (int i = 0; i < points; ++i)
    {
        const float a = (static_cast<float>(i) / static_cast<float>(points)) * juce::MathConstants<float>::twoPi;
        const float r = (i % 2 == 0) ? (22.0f + recoilOffset * 0.5f) : 11.0f;
        const float px = cx + std::cos(a) * r;
        const float py = cy + std::sin(a) * r;
        if (i == 0) star.startNewSubPath(px, py);
        else        star.lineTo(px, py);
    }
    star.closeSubPath();
    g.setColour(paper);
    g.fillPath(star);
    g.setColour(ink);
    g.strokePath(star, juce::PathStrokeType(1.8f));

    // Solid black core & secondary impact dots
    g.fillEllipse(cx - 8.5f, cy - 8.5f, 17.0f, 17.0f);
    g.fillEllipse(cx - 16.0f, cy - 15.0f, 8.0f, 8.0f);
    g.fillEllipse(cx - 15.0f, cy + 8.0f, 7.5f, 7.5f);

    // Incoming projectile slug silhouette
    juce::Path bullet;
    bullet.startNewSubPath(cx - 44.0f, cy - 4.0f);
    bullet.lineTo(cx - 26.0f, cy - 4.0f);
    bullet.quadraticTo(cx - 19.0f, cy, cx - 26.0f, cy + 4.0f);
    bullet.lineTo(cx - 44.0f, cy + 4.0f);
    bullet.closeSubPath();
    g.setColour(paper);
    g.fillPath(bullet);
    g.setColour(ink);
    g.strokePath(bullet, juce::PathStrokeType(1.6f));
    g.drawLine(cx - 39.0f, cy - 4.0f, cx - 39.0f, cy + 4.0f, 1.4f);

    // Debris particles
    for (const auto& pt : particles)
    {
        if (pt.life > 0.05f)
        {
            g.setColour(ink.withAlpha(pt.life));
            g.fillRect(cx + pt.x - pt.size * 0.5f, cy + pt.y - pt.size * 0.5f, pt.size, pt.size);
        }
    }

    // English Comic-Book SFX Typography (100% ASCII, zero mojibake)
    const int gunMode = juce::jlimit(0, 4, static_cast<int>(processor.apvts.getRawParameterValue("gunMode")->load()));
    const juce::String topSfx = kImpactSfx[gunMode][sfxIndex % 3];
    const juce::String botSfx = kTailSfx[gunMode];

    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText(topSfx,
               static_cast<int>(mainArea.getX() + 26.0f - recoilOffset * 0.3f),
               static_cast<int>(mainArea.getY() + 10.0f),
               220, 32, juce::Justification::centredLeft, false);

    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText(botSfx,
               static_cast<int>(mainArea.getRight() - 185.0f),
               static_cast<int>(mainArea.getBottom() - 36.0f),
               175, 28, juce::Justification::centredRight, false);

    // Panel borders
    g.setColour(ink);
    g.drawRect(bounds, 2.0f);
    g.drawLine(meterArea.getX(), bounds.getY(), meterArea.getX(), bounds.getBottom(), 1.8f);
}

//==============================================================================
// GunSelectorStrip
//==============================================================================
GunSelectorStrip::GunSelectorStrip(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l)
    : processor(p), laf(l)
{
}

void GunSelectorStrip::drawGunIcon(juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour strokeCol)
{
    g.setColour(strokeCol);
    const float x = area.getX();
    const float y = area.getCentreY();
    const float w = area.getWidth();

    juce::Path p;
    switch (index)
    {
        case 0: // PISTOL
            p.startNewSubPath(x + w * 0.15f, y - 8.0f);
            p.lineTo(x + w * 0.85f, y - 8.0f);
            p.lineTo(x + w * 0.85f, y - 1.0f);
            p.lineTo(x + w * 0.42f, y - 1.0f);
            p.lineTo(x + w * 0.34f, y + 12.0f);
            p.lineTo(x + w * 0.18f, y + 12.0f);
            p.lineTo(x + w * 0.25f, y - 1.0f);
            p.lineTo(x + w * 0.15f, y - 1.0f);
            p.closeSubPath();
            break;

        case 1: // SHOTGUN
            p.startNewSubPath(x + w * 0.08f, y + 6.0f);
            p.lineTo(x + w * 0.34f, y - 5.0f);
            p.lineTo(x + w * 0.92f, y - 5.0f);
            p.lineTo(x + w * 0.92f, y + 1.0f);
            p.lineTo(x + w * 0.40f, y + 1.0f);
            p.lineTo(x + w * 0.18f, y + 11.0f);
            p.closeSubPath();
            break;

        case 2: // SMG
            p.startNewSubPath(x + w * 0.12f, y - 6.0f);
            p.lineTo(x + w * 0.86f, y - 6.0f);
            p.lineTo(x + w * 0.86f, y + 1.0f);
            p.lineTo(x + w * 0.62f, y + 1.0f);
            p.lineTo(x + w * 0.66f, y + 12.0f);
            p.lineTo(x + w * 0.54f, y + 12.0f);
            p.lineTo(x + w * 0.50f, y + 1.0f);
            p.lineTo(x + w * 0.38f, y + 1.0f);
            p.lineTo(x + w * 0.32f, y + 11.0f);
            p.lineTo(x + w * 0.20f, y + 11.0f);
            p.lineTo(x + w * 0.25f, y + 1.0f);
            p.lineTo(x + w * 0.12f, y + 1.0f);
            p.closeSubPath();
            break;

        case 3: // SNIPER
            p.startNewSubPath(x + w * 0.06f, y + 6.0f);
            p.lineTo(x + w * 0.34f, y - 3.0f);
            p.lineTo(x + w * 0.95f, y - 3.0f);
            p.lineTo(x + w * 0.95f, y + 1.0f);
            p.lineTo(x + w * 0.56f, y + 1.0f);
            p.lineTo(x + w * 0.52f, y + 7.0f);
            p.lineTo(x + w * 0.43f, y + 7.0f);
            p.lineTo(x + w * 0.45f, y + 1.0f);
            p.lineTo(x + w * 0.34f, y + 1.0f);
            p.lineTo(x + w * 0.14f, y + 9.0f);
            p.closeSubPath();
            g.drawRect(x + w * 0.38f, y - 8.5f, w * 0.24f, 4.0f, 1.4f);
            break;

        case 4: // REVOLVER
        default:
            p.startNewSubPath(x + w * 0.16f, y + 9.0f);
            p.lineTo(x + w * 0.36f, y - 7.0f);
            p.lineTo(x + w * 0.86f, y - 7.0f);
            p.lineTo(x + w * 0.86f, y - 1.0f);
            p.lineTo(x + w * 0.44f, y - 1.0f);
            p.lineTo(x + w * 0.30f, y + 11.0f);
            p.closeSubPath();
            g.drawEllipse(x + w * 0.43f, y - 6.5f, 9.0f, 6.0f, 1.4f);
            break;
    }

    g.strokePath(p, juce::PathStrokeType(1.8f));
}

void GunSelectorStrip::paint(juce::Graphics& g)
{
    static constexpr const char* kTitles[5]    = { "PISTOL",    "SHOTGUN", "SMG",   "SNIPER", "REVOLVER" };
    static constexpr const char* kSubtitles[5] = { "TRANSIENT", "SCATTER", "BURST", "PIERCE", "CRUSH" };

    const int currentMode = juce::jlimit(0, 4, static_cast<int>(processor.apvts.getRawParameterValue("gunMode")->load()));
    const auto bounds = getLocalBounds().toFloat();
    const float cellW = bounds.getWidth() / 5.0f;

    const auto paper = laf.getPaperColour();
    const auto ink   = laf.getInkColour();

    for (int i = 0; i < 5; ++i)
    {
        const auto cell = juce::Rectangle<float>(bounds.getX() + static_cast<float>(i) * cellW, bounds.getY(), cellW, bounds.getHeight());
        const bool selected = (i == currentMode);

        g.setColour(selected ? ink : paper);
        g.fillRect(cell);

        const auto fg = selected ? paper : ink;
        drawGunIcon(g, i, cell.withWidth(cell.getWidth() * 0.52f).reduced(6.0f, 10.0f), fg);

        g.setColour(fg);
        const auto textRect = cell.withTrimmedLeft(cell.getWidth() * 0.48f).reduced(2.0f, 12.0f);
        g.setFont(laf.getMangaFont(11.5f, true));
        g.drawText(kTitles[i], textRect.removeFromTop(textRect.getHeight() * 0.5f), juce::Justification::centredLeft, false);
        g.setFont(laf.getMangaFont(10.0f, true));
        g.drawText(kSubtitles[i], textRect, juce::Justification::centredLeft, false);

        g.setColour(ink);
        g.drawRect(cell, 1.5f);
    }
}

void GunSelectorStrip::mouseDown(const juce::MouseEvent& e)
{
    const int idx = juce::jlimit(0, 4, static_cast<int>((static_cast<float>(e.x) / static_cast<float>(getWidth())) * 5.0f));
    if (auto* param = processor.apvts.getParameter("gunMode"))
    {
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(idx)));
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
      gunStrip(p, mangaLaf)
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
        audioProcessor.triggerPanicReset();
    };

    advancedBtn.setClickingTogglesState(true);
    advancedBtn.setToggleState(drawerExpanded, juce::dontSendNotification);
    advancedBtn.onClick = [this]
    {
        drawerExpanded = advancedBtn.getToggleState();
        updateDrawerVisibility();
        setSize(860, drawerExpanded ? 740 : 540);
    };

    invertBtn.setClickingTogglesState(true);
    invertBtn.onClick = [this]
    {
        mangaLaf.setInverted(invertBtn.getToggleState());
        sendLookAndFeelChange();
        repaint();
    };

    // Viewport & Gun Strip
    addAndMakeVisible(viewport);
    addAndMakeVisible(gunStrip);

    // 6 Macro Knobs
    configureRotaryKnob(driveSlider,   driveLabel,   "DRIVE",   false);
    configureRotaryKnob(spaceSlider,   spaceLabel,   "SPACE",   false);
    configureRotaryKnob(decaySlider,   decayLabel,   "DECAY",   false);
    configureRotaryKnob(toneSlider,    toneLabel,    "TONE",    false);
    configureRotaryKnob(densitySlider, densityLabel, "DENSITY", false);
    configureRotaryKnob(mixSlider,     mixLabel,     "MIX",     false);

    driveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "drive",   driveSlider);
    spaceAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "space",   spaceSlider);
    decayAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "decay",   decaySlider);
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

    // Drawer Tabs (4 buttons)
    juce::TextButton* tabs[4] = { &tabABtn, &tabBBtn, &tabCBtn, &tabDBtn };
    for (int t = 0; t < 4; ++t)
    {
        tabs[t]->setClickingTogglesState(true);
        tabs[t]->setRadioGroupId(4001);
        tabs[t]->setToggleState(t == activeDrawerTab, juce::dontSendNotification);
        tabs[t]->onClick = [this, t]
        {
            activeDrawerTab = t;
            updateDrawerVisibility();
            resized();
        };
        addAndMakeVisible(tabs[t]);
    }

    // Tab A // VOICES (12 voice strips)
    tabAHeaderLabel.setText("PER-VOICE MIXER  //  LEVEL  |  PAN  |  DETUNE (CT)  |  DECAY MUL", juce::dontSendNotification);
    tabAHeaderLabel.setFont(mangaLaf.getMangaFont(10.5f, true));
    addAndMakeVisible(tabAHeaderLabel);

    for (int v = 0; v < BreachResonatorAudioProcessor::kMaxVoices; ++v)
    {
        auto& row = voiceRows[static_cast<size_t>(v)];
        row.title.setText("V" + juce::String(v + 1), juce::dontSendNotification);
        row.title.setFont(mangaLaf.getMangaFont(10.5f, true));
        row.title.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(row.title);

        for (auto* s : { &row.level, &row.pan, &row.detune, &row.decay })
        {
            s->setSliderStyle(juce::Slider::LinearHorizontal);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(s);
        }

        row.levelAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "vLevel_"  + juce::String(v), row.level);
        row.panAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "vPan_"    + juce::String(v), row.pan);
        row.detuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "vDetune_" + juce::String(v), row.detune);
        row.decayAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "vDecay_"  + juce::String(v), row.decay);
    }

    // Tab B // RESONATOR
    resTopologyBox.addItemList({ "AUTO (GUN MODE)", "BANDPASS SVF", "COMB STRING", "DUAL FORMANT" }, 1);
    addAndMakeVisible(resTopologyBox);
    resTopoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "resTopology", resTopologyBox);

    configureRotaryKnob(attackSlider,    attackLabel,    "ATTACK (MS)",  true);
    configureRotaryKnob(fbDampSlider,    fbDampLabel,    "FB DAMPING",   true);
    configureRotaryKnob(stiffnessSlider, stiffnessLabel, "STIFFNESS",    true);
    configureRotaryKnob(excHpfSlider,    excHpfLabel,    "EXC HPF (HZ)", true);
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

    static constexpr const char* kRowNames[3] = { "LFO 1", "LFO 2", "ENV" };
    static constexpr const char* kColNames[6] = { "WARP", "DRIFT", "SHIFT", "Q", "TILT", "DEPTH" };
    static constexpr const char* kSrcKeys[3]  = { "lfo1", "lfo2", "env" };
    static constexpr const char* kDstKeys[6]  = { "warp", "drift", "shift", "q", "tilt", "depth" };

    for (int r = 0; r < 3; ++r)
    {
        matrixRowLabels[static_cast<size_t>(r)].setText(kRowNames[r], juce::dontSendNotification);
        matrixRowLabels[static_cast<size_t>(r)].setFont(mangaLaf.getMangaFont(9.5f, true));
        matrixRowLabels[static_cast<size_t>(r)].setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(matrixRowLabels[static_cast<size_t>(r)]);
    }

    for (int c = 0; c < 6; ++c)
    {
        matrixColLabels[static_cast<size_t>(c)].setText(kColNames[c], juce::dontSendNotification);
        matrixColLabels[static_cast<size_t>(c)].setFont(mangaLaf.getMangaFont(9.5f, true));
        matrixColLabels[static_cast<size_t>(c)].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(matrixColLabels[static_cast<size_t>(c)]);
    }

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 6; ++c)
        {
            const size_t idx = static_cast<size_t>(r * 6 + c);
            auto& sl = matrixSliders[idx];
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(sl);
            matrixAtts[idx] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, juce::String("mod_") + kSrcKeys[r] + "_" + kDstKeys[c], sl);
        }
    }

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

    updateDrawerVisibility();
    setSize(860, drawerExpanded ? 740 : 540);
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
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    if (showTextBox)
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 17);
    else
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    label.setText(text, juce::dontSendNotification);
    label.setFont(mangaLaf.getMangaFont(10.0f, true));
    label.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void BreachResonatorAudioProcessorEditor::updateDrawerVisibility()
{
    const bool showA = drawerExpanded && (activeDrawerTab == 0);
    const bool showB = drawerExpanded && (activeDrawerTab == 1);
    const bool showC = drawerExpanded && (activeDrawerTab == 2);
    const bool showD = drawerExpanded && (activeDrawerTab == 3);

    tabABtn.setVisible(drawerExpanded);
    tabBBtn.setVisible(drawerExpanded);
    tabCBtn.setVisible(drawerExpanded);
    tabDBtn.setVisible(drawerExpanded);

    // Tab A
    tabAHeaderLabel.setVisible(showA);
    for (auto& row : voiceRows)
    {
        row.title.setVisible(showA);
        row.level.setVisible(showA);
        row.pan.setVisible(showA);
        row.detune.setVisible(showA);
        row.decay.setVisible(showA);
    }

    // Tab B
    resTopologyBox.setVisible(showB);
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
    for (auto& l : matrixRowLabels) l.setVisible(showC);
    for (auto& l : matrixColLabels) l.setVisible(showC);
    for (auto& s : matrixSliders)   s.setVisible(showC);

    // Tab D
    for (auto* c : std::initializer_list<juce::Component*>{
             &inGainSlider, &inGainLbl, &outGainSlider, &outGainLbl,
             &widthSlider, &widthLbl, &mixCurveLbl, &mixCurveBox,
             &oversampleLbl, &oversampleBox, &limiterBtn, &smoothSlider, &smoothLbl })
    {
        c->setVisible(showD);
    }
}

void BreachResonatorAudioProcessorEditor::timerCallback()
{
    viewport.tickAnimation();
    viewport.repaint();
}

void BreachResonatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto paper = mangaLaf.getPaperColour();
    const auto ink   = mangaLaf.getInkColour();

    g.fillAll(paper);

    // Outer manga page frame
    g.setColour(ink);
    g.drawRect(bounds.reduced(1.0f), 2.5f);

    // Top-left reticle icon + "BREACH // RESONATOR" ASCII title
    const float cx = 28.0f;
    const float cy = 24.0f;
    g.drawEllipse(cx - 10.0f, cy - 10.0f, 20.0f, 20.0f, 1.8f);
    g.drawLine(cx - 14.0f, cy, cx + 14.0f, cy, 1.8f);
    g.drawLine(cx, cy - 14.0f, cx, cy + 14.0f, 1.8f);

    g.setFont(juce::Font(juce::FontOptions(17.5f, juce::Font::bold)));
    g.drawText("BREACH // RESONATOR", 48, 10, 215, 28, juce::Justification::centredLeft, false);

    g.setFont(mangaLaf.getMangaFont(9.5f, true));
    g.setColour(mangaLaf.getMutedInkColour());
    g.drawText("v1.1.1", 575, 14, 48, 22, juce::Justification::centredRight, false);

    // Horizontal divider below main panel / gun strip
    g.setColour(ink);
    g.drawLine(0.0f, 444.0f, bounds.getWidth(), 444.0f, 2.0f);
    g.drawLine(0.0f, 540.0f, bounds.getWidth(), 540.0f, 2.0f);

    if (drawerExpanded)
    {
        const auto drawerFrame = juce::Rectangle<float>(10.0f, 548.0f, bounds.getWidth() - 20.0f, bounds.getHeight() - 558.0f);
        g.drawRect(drawerFrame, 1.8f);
    }
}

void BreachResonatorAudioProcessorEditor::resized()
{
    // Top Header Bar (y: 10..38)
    prevPresetBtn.setBounds(268, 11, 26, 26);
    chordPresetBox.setBounds(298, 11, 134, 26);
    nextPresetBtn.setBounds(436, 11, 26, 26);

    panicBtn.setBounds(630, 11, 66, 26);
    advancedBtn.setBounds(702, 11, 80, 26);
    invertBtn.setBounds(788, 11, 60, 26);

    // Left Macro Knobs (x: 16..156)
    auto placeMacro = [](juce::Slider& s, juce::Label& l, int x, int y)
    {
        s.setBounds(x, y, 104, 96);
        l.setBounds(x, y + 96, 104, 18);
    };

    placeMacro(driveSlider,   driveLabel,   32,  54);
    placeMacro(spaceSlider,   spaceLabel,   32, 180);
    placeMacro(decaySlider,   decayLabel,   32, 306);

    // Right Macro Knobs (x: 704..844)
    placeMacro(toneSlider,    toneLabel,    724,  54);
    placeMacro(densitySlider, densityLabel, 724, 180);
    placeMacro(mixSlider,     mixLabel,     724, 306);

    // Center Manga Viewport
    viewport.setBounds(168, 52, 524, 326);

    // Chord & Arp Control Strip (y: 384..436)
    int nx = 168;
    for (int i = 0; i < 12; ++i)
    {
        noteButtons[static_cast<size_t>(i)].setBounds(nx, 385, 22, 22);
        nx += 24;
    }

    // Widened Root, Octave, Voice Cap, MIDI boxes so text never clips to "..."
    rootNoteBox.setBounds(458, 385, 48, 22);
    octaveBox.setBounds(510, 385, 58, 22);
    voiceCapBox.setBounds(572, 385, 62, 22);
    midiBtn.setBounds(638, 385, 54, 22);

    // Second sub-row: ARP, DIR, MOTION, Rate Slider
    arpBtn.setBounds(168, 412, 48, 22);
    arpDirBox.setBounds(220, 412, 68, 22);
    motionBtn.setBounds(292, 412, 66, 22);
    arpRateSlider.setBounds(364, 412, 328, 22);

    // 5 Gun Topology Strip (y: 444..540)
    gunStrip.setBounds(0, 444, 860, 96);

    if (!drawerExpanded)
        return;

    // Advanced Drawer Tabs (y: 556..580)
    const int tabW = 204;
    tabABtn.setBounds(18,            556, tabW, 24);
    tabBBtn.setBounds(18 + tabW + 4, 556, tabW, 24);
    tabCBtn.setBounds(18 + (tabW + 4) * 2, 556, tabW, 24);
    tabDBtn.setBounds(18 + (tabW + 4) * 3, 556, tabW, 24);

    // Tab A // VOICES Layout (2 columns x 6 rows = 12 voices)
    if (activeDrawerTab == 0)
    {
        tabAHeaderLabel.setBounds(22, 584, 816, 16);
        for (int v = 0; v < BreachResonatorAudioProcessor::kMaxVoices; ++v)
        {
            const int col = v / 6;
            const int r   = v % 6;
            const int bx  = 20 + col * 414;
            const int by  = 603 + r * 20;

            auto& row = voiceRows[static_cast<size_t>(v)];
            row.title.setBounds(bx, by, 28, 18);
            row.level.setBounds(bx + 32,  by, 90, 18);
            row.pan.setBounds(bx + 126,   by, 90, 18);
            row.detune.setBounds(bx + 220, by, 90, 18);
            row.decay.setBounds(bx + 314,  by, 90, 18);
        }
    }

    // Tab B // RESONATOR Layout
    if (activeDrawerTab == 1)
    {
        resTopologyBox.setBounds(18, 588, 165, 24);

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

        // 3x6 Modulation Matrix on the right (x: 438..840)
        const int matStartX = 476;
        const int colW = 60;
        for (int c = 0; c < 6; ++c)
            matrixColLabels[static_cast<size_t>(c)].setBounds(matStartX + c * colW, 588, colW - 4, 15);

        for (int r = 0; r < 3; ++r)
        {
            const int ry = 612 + r * 38;
            matrixRowLabels[static_cast<size_t>(r)].setBounds(434, ry + 2, 38, 18);
            for (int c = 0; c < 6; ++c)
            {
                matrixSliders[static_cast<size_t>(r * 6 + c)].setBounds(matStartX + c * colW, ry, colW - 6, 22);
            }
        }
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

        mixCurveLbl.setBounds(410, 608, 126, 16);
        mixCurveBox.setBounds(410, 628, 126, 26);

        oversampleLbl.setBounds(550, 608, 116, 16);
        oversampleBox.setBounds(550, 628, 116, 26);

        limiterBtn.setBounds(410, 668, 256, 28);

        placeGlobalKnob(smoothSlider, smoothLbl, 696, 604);
    }
}
