#include "PluginEditor.h"
#include <cmath>

void MangaKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider&)
{
    const auto paper = getPaper();
    const auto ink   = getInk();

    const float diameter = static_cast<float>(std::min(width, height)) - 14.0f;
    const float radius   = std::max(12.0f, diameter * 0.5f);
    const float cx       = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy       = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    // Radial manga calibration ticks
    constexpr int numTicks = 17;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle) - juce::MathConstants<float>::halfPi;
        const bool isMajor = (i == 0 || i == numTicks / 2 || i == numTicks - 1);
        const float rInner = radius + 3.0f;
        const float rOuter = radius + (isMajor ? 8.5f : 5.5f);

        g.setColour(ink);
        g.drawLine(cx + std::cos(angle) * rInner, cy + std::sin(angle) * rInner,
                   cx + std::cos(angle) * rOuter, cy + std::sin(angle) * rOuter,
                   isMajor ? 2.0f : 1.2f);
    }

    // Flat monochrome circle
    g.setColour(paper);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(ink);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.2f);
    g.drawEllipse(cx - radius * 0.78f, cy - radius * 0.78f, radius * 1.56f, radius * 1.56f, 1.0f);

    // Thick pointer line
    const float pointerAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle)
                             - juce::MathConstants<float>::halfPi;
    g.drawLine(cx, cy,
               cx + std::cos(pointerAngle) * (radius - 2.0f),
               cy + std::sin(pointerAngle) * (radius - 2.0f),
               3.0f);

    g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
}

BreachResonatorAudioProcessorEditor::BreachResonatorAudioProcessorEditor(BreachResonatorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1040, 640);
    setLookAndFeel(&mangaLnf);

    auto configureKnob = [this](juce::Slider& s, std::unique_ptr<SliderAttachment>& att, const char* paramId)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
        addAndMakeVisible(s);
        att = std::make_unique<SliderAttachment>(audioProcessor.getAPVTS(), paramId, s);
    };

    configureKnob(decayKnob, decayAtt, breach::ParamID::decay);
    configureKnob(spaceKnob, spaceAtt, breach::ParamID::octaveSpread);
    configureKnob(driftKnob, driftAtt, breach::ParamID::drift);
    configureKnob(toneKnob,  toneAtt,  breach::ParamID::tone);
    configureKnob(shiftKnob, shiftAtt, breach::ParamID::shiftHz);
    configureKnob(depthKnob, depthAtt, breach::ParamID::depth);

    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible(rootBox);
    rootAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::rootNote, rootBox);

    chordBox.addItemList({ "Octaves", "Major", "Minor", "Maj7", "Min7", "Dom7", "Sus2", "Sus4", "Add9", "Dim7", "Scale" }, 1);
    addAndMakeVisible(chordBox);
    chordAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::chordType, chordBox);

    voicesBox.addItemList({ "4 Voices", "5 Voices", "6 Voices", "7 Voices", "8 Voices" }, 1);
    addAndMakeVisible(voicesBox);
    voicesAtt = std::make_unique<ComboAttachment>(audioProcessor.getAPVTS(), breach::ParamID::voiceCount, voicesBox);

    addAndMakeVisible(arpButton);
    arpAtt = std::make_unique<ButtonAttachment>(audioProcessor.getAPVTS(), breach::ParamID::arpEnabled, arpButton);

    invertButton.onClick = [this]
    {
        isInverted = !isInverted;
        mangaLnf.inverted = isInverted;
        repaint();
    };
    addAndMakeVisible(invertButton);

    startTimerHz(60);
}

BreachResonatorAudioProcessorEditor::~BreachResonatorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BreachResonatorAudioProcessorEditor::selectMode(int modeIdx)
{
    audioProcessor.setMode(modeIdx);
    slashCutProgress = 1.0f;
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
    lastSnapshot = audioProcessor.getVisualTelemetry().getSnapshot();

    if (lastSnapshot.transientPulseCount != lastTransientCount)
    {
        lastTransientCount = lastSnapshot.transientPulseCount;
        flashFramesRemaining = 2; // 1-frame manga impact invert flash
    }
    else if (flashFramesRemaining > 0)
    {
        --flashFramesRemaining;
    }

    if (slashCutProgress > 0.0f)
    {
        slashCutProgress = std::max(0.0f, slashCutProgress - 0.09f); // ~0.18s at 60Hz
    }

    repaint();
}

void BreachResonatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Top Header Bar (56px)
    auto header = area.removeFromTop(56).reduced(12, 10);
    invertButton.setBounds(header.removeFromRight(84));
    header.removeFromRight(8);
    arpButton.setBounds(header.removeFromRight(66));
    header.removeFromRight(8);
    voicesBox.setBounds(header.removeFromRight(96));
    header.removeFromRight(8);
    chordBox.setBounds(header.removeFromRight(104));
    header.removeFromRight(8);
    rootBox.setBounds(header.removeFromRight(72));

    // Bottom Firearm Mode Dock (120px)
    auto bottomDock = area.removeFromBottom(120);
    const int slotW = bottomDock.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        modeSlotBounds[static_cast<size_t>(i)] = bottomDock.removeFromLeft(i == 4 ? bottomDock.getWidth() : slotW);

    // Symmetrical Left & Right Knob Wings (204px each)
    auto leftWing  = area.removeFromLeft(204).reduced(18, 12);
    auto rightWing = area.removeFromRight(204).reduced(18, 12);
    centerPanelBounds = area.reduced(12, 14);

    const int knobH = leftWing.getHeight() / 3;
    decayKnob.setBounds(leftWing.removeFromTop(knobH).reduced(14, 10));
    spaceKnob.setBounds(leftWing.removeFromTop(knobH).reduced(14, 10));
    driftKnob.setBounds(leftWing.reduced(14, 10));

    toneKnob.setBounds(rightWing.removeFromTop(knobH).reduced(14, 10));
    shiftKnob.setBounds(rightWing.removeFromTop(knobH).reduced(14, 10));
    depthKnob.setBounds(rightWing.reduced(14, 10));
}

void BreachResonatorAudioProcessorEditor::drawFirearmSilhouette(juce::Graphics& g, int modeIdx,
                                                                juce::Rectangle<float> area, juce::Colour inkCol)
{
    g.setColour(inkCol);
    juce::Path p;
    const float cx = area.getCentreX();
    const float cy = area.getCentreY() - 8.0f;

    switch (modeIdx)
    {
        case 0: // PISTOL
            p.startNewSubPath(cx - 38, cy - 8);
            p.lineTo(cx + 34, cy - 8);
            p.lineTo(cx + 34, cy + 4);
            p.lineTo(cx - 12, cy + 4);
            p.lineTo(cx - 22, cy + 26);
            p.lineTo(cx - 36, cy + 26);
            p.lineTo(cx - 28, cy + 4);
            p.lineTo(cx - 38, cy + 4);
            p.closeSubPath();
            break;

        case 1: // SHOTGUN
            p.startNewSubPath(cx - 64, cy + 12);
            p.lineTo(cx - 36, cy - 4);
            p.lineTo(cx + 62, cy - 4);
            p.lineTo(cx + 62, cy + 2);
            p.lineTo(cx + 22, cy + 2);
            p.lineTo(cx + 22, cy + 7);
            p.lineTo(cx - 24, cy + 7);
            p.lineTo(cx - 56, cy + 22);
            p.closeSubPath();
            break;

        case 2: // SMG
            p.startNewSubPath(cx - 46, cy - 10);
            p.lineTo(cx + 42, cy - 10);
            p.lineTo(cx + 42, cy + 3);
            p.lineTo(cx + 12, cy + 3);
            p.lineTo(cx + 16, cy + 24);
            p.lineTo(cx + 6,  cy + 24);
            p.lineTo(cx + 2,  cy + 3);
            p.lineTo(cx - 16, cy + 3);
            p.lineTo(cx - 22, cy + 20);
            p.lineTo(cx - 32, cy + 20);
            p.lineTo(cx - 28, cy + 3);
            p.lineTo(cx - 46, cy + 3);
            p.closeSubPath();
            break;

        case 3: // SNIPER
            p.startNewSubPath(cx - 72, cy + 12);
            p.lineTo(cx - 38, cy - 2);
            p.lineTo(cx + 74, cy - 2);
            p.lineTo(cx + 74, cy + 2);
            p.lineTo(cx + 8,  cy + 2);
            p.lineTo(cx + 4,  cy + 14);
            p.lineTo(cx - 6,  cy + 14);
            p.lineTo(cx - 10, cy + 2);
            p.lineTo(cx - 32, cy + 2);
            p.lineTo(cx - 64, cy + 20);
            p.closeSubPath();
            // Scope
            p.addRectangle(cx - 22, cy - 11, 38, 6);
            break;

        case 4: // REVOLVER
        default:
            p.startNewSubPath(cx - 42, cy + 22);
            p.lineTo(cx - 28, cy + 2);
            p.lineTo(cx - 14, cy - 10);
            p.lineTo(cx + 46, cy - 10);
            p.lineTo(cx + 46, cy + 2);
            p.lineTo(cx - 6,  cy + 2);
            p.lineTo(cx - 26, cy + 24);
            p.closeSubPath();
            p.addEllipse(cx - 12, cy - 9, 16, 14);
            break;
    }

    g.strokePath(p, juce::PathStrokeType(2.0f));
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
    g.drawRect(bounds, 3);

    const float bx = static_cast<float>(bounds.getX());
    const float by = static_cast<float>(bounds.getY());
    const float bw = static_cast<float>(bounds.getWidth());
    const float bh = static_cast<float>(bounds.getHeight());

    const float impactX = bx + bw * 0.62f;
    const float impactY = by + bh * 0.50f;

    // 1. Concrete Wall Slab + Halftone Screentone Dots
    const float wallLeft = bx + bw * 0.52f;
    g.drawLine(wallLeft, by, wallLeft - 14.0f, by + bh, 2.5f);
    g.drawLine(bx + bw * 0.84f, by, bx + bw * 0.84f, by + bh, 2.0f);

    for (float dy = by + 10.0f; dy < by + bh - 8.0f; dy += 8.0f)
    {
        for (float dx = bx + bw * 0.84f + 6.0f; dx < bx + bw - 6.0f; dx += 8.0f)
            g.fillEllipse(dx, dy, 2.2f, 2.2f);
    }

    // 2. Speed Lines driven by inputPeak
    const float peak = juce::jlimit(0.0f, 1.0f, lastSnapshot.inputPeak * 1.6f);
    const float lineLen = bw * (0.28f + 0.24f * peak);
    for (int i = -6; i <= 6; ++i)
    {
        if (i == 0) continue;
        const float yOff = static_cast<float>(i) * 16.0f;
        const float startX = bx + 12.0f;
        const float endX = startX + lineLen * (0.7f + 0.3f * std::abs(std::sin(static_cast<float>(i) * 1.7f)));
        g.drawLine(startX, impactY + yOff, endX, impactY + yOff * 0.65f, (i % 2 == 0) ? 2.0f : 1.0f);
    }

    // 3. Radial Cracks & Debris driven by 8-Voice Resonator Energies
    const int voices = juce::jlimit(4, 8, lastSnapshot.activeVoiceCount);
    const float baseDecay = static_cast<float>(decayKnob.getValue());
    for (int v = 0; v < voices; ++v)
    {
        const float energy = lastSnapshot.voiceEnergies[static_cast<size_t>(v)];
        const float angle = (static_cast<float>(v) / static_cast<float>(voices)) * juce::MathConstants<float>::twoPi + 0.35f;
        const float crackLen = 28.0f + (baseDecay * 85.0f) + (energy * 110.0f);

        const float midX = impactX + std::cos(angle + 0.14f) * (crackLen * 0.52f);
        const float midY = impactY + std::sin(angle + 0.14f) * (crackLen * 0.52f);
        const float endX = impactX + std::cos(angle) * crackLen;
        const float endY = impactY + std::sin(angle) * crackLen;

        g.drawLine(impactX, impactY, midX, midY, 2.2f);
        g.drawLine(midX, midY, endX, endY, 1.4f);

        // Flying debris shards
        if (energy > 0.08f || baseDecay > 0.45f)
        {
            const float shardDist = crackLen * 0.72f + energy * 38.0f;
            const float sx = impactX + std::cos(angle - 0.22f) * shardDist;
            const float sy = impactY + std::sin(angle - 0.22f) * shardDist;
            const float sz = 4.0f + energy * 7.0f;

            juce::Path shard;
            shard.addTriangle(sx, sy - sz, sx + sz, sy + sz * 0.6f, sx - sz * 0.8f, sy + sz * 0.8f);
            g.setColour(paper);
            g.fillPath(shard);
            g.setColour(ink);
            g.strokePath(shard, juce::PathStrokeType(1.6f));
        }
    }

    // 4. Breach Crater & Bullet Projectile (Varies with Firearm Mode)
    const int mode = lastSnapshot.activeMode;
    if (mode == 1) // Shotgun: scatter holes
    {
        g.fillEllipse(impactX - 16, impactY - 22, 16, 16);
        g.fillEllipse(impactX - 8,  impactY - 4,  20, 20);
        g.fillEllipse(impactX - 18, impactY + 14, 14, 14);
    }
    else if (mode == 3) // Sniper: elongated piercing crater
    {
        g.fillEllipse(impactX - 28, impactY - 9, 52, 18);
    }
    else
    {
        g.fillEllipse(impactX - 18, impactY - 18, 36, 36);
    }

    // Bullet projectile vibrating with inputRms
    const float jitterY = std::sin(static_cast<float>(lastTransientCount) * 3.1f + peak * 25.0f) * peak * 5.0f;
    const float bulletX = impactX - 96.0f + peak * 26.0f;
    const float bulletY = impactY + jitterY;

    juce::Path bullet;
    bullet.startNewSubPath(bulletX - 24, bulletY - 7);
    bullet.lineTo(bulletX + 6,  bulletY - 7);
    bullet.quadraticTo(bulletX + 26, bulletY, bulletX + 6, bulletY + 7);
    bullet.lineTo(bulletX - 24, bulletY + 7);
    bullet.closeSubPath();

    g.setColour(paper);
    g.fillPath(bullet);
    g.setColour(ink);
    g.strokePath(bullet, juce::PathStrokeType(2.2f));

    // 5. Diagonal Manga Slash Cut Animation on Mode Switch
    if (slashCutProgress > 0.01f)
    {
        const float slashW = bw * 0.25f * slashCutProgress;
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

    // Header Title & Divider
    g.drawLine(0.0f, 56.0f, static_cast<float>(getWidth()), 56.0f, 2.0f);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(22.0f).withStyle("Bold")));
    g.drawText("BREACH // CHORD RESONATOR", 18, 12, 340, 32, juce::Justification::centredLeft);

    // Center Manga Impact Canvas
    drawImpactPanel(g, centerPanelBounds);

    // Symmetrical Knob Captions
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f).withStyle("Bold")));
    auto drawLabelAbove = [&](juce::Slider& s, const juce::String& label)
    {
        auto b = s.getBounds();
        g.drawText(label, b.getX(), b.getY() - 14, b.getWidth(), 14, juce::Justification::centred);
    };

    drawLabelAbove(decayKnob, "DECAY");
    drawLabelAbove(spaceKnob, "SPACE");
    drawLabelAbove(driftKnob, "DRIFT");
    drawLabelAbove(toneKnob,  "TONE");
    drawLabelAbove(shiftKnob, "SHIFT (HZ)");
    drawLabelAbove(depthKnob, "DEPTH / MIX");

    // Bottom Firearm Mode Dock
    g.drawLine(0.0f, static_cast<float>(getHeight() - 120),
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 120), 2.0f);

    static constexpr std::array<const char*, 5> kGunNames{
        "PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER"
    };
    static constexpr std::array<const char*, 5> kModeSubtitles{
        "A: BANDPASS", "B: COMB STRING", "C: FORMANT", "D: DISTORTED", "E: LONG PAD"
    };

    const int activeMode = audioProcessor.getCurrentMode();
    for (int i = 0; i < 5; ++i)
    {
        const auto slot = modeSlotBounds[static_cast<size_t>(i)];
        const bool selected = (i == activeMode);

        g.setColour(selected ? ink : paper);
        g.fillRect(slot);

        g.setColour(ink);
        g.drawRect(slot, 1);

        const auto fg = selected ? paper : ink;
        drawFirearmSilhouette(g, i, slot.toFloat().reduced(10.0f, 12.0f), fg);

        g.setColour(fg);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f).withStyle("Bold")));
        g.drawText(kGunNames[static_cast<size_t>(i)], slot.getX(), slot.getBottom() - 36, slot.getWidth(), 16,
                   juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f)));
        g.drawText(kModeSubtitles[static_cast<size_t>(i)], slot.getX(), slot.getBottom() - 20, slot.getWidth(), 14,
                   juce::Justification::centred);
    }
}
