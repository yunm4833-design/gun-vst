#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

class MangaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MangaLookAndFeel()
    {
        setInverted(false);
    }

    void setInverted(bool shouldInvert)
    {
        inverted = shouldInvert;
        refreshThemeColours();
    }

    bool isInverted() const noexcept { return inverted; }

    juce::Colour getPaperColour() const noexcept
    {
        return inverted ? juce::Colour(0xff111111) : juce::Colour(0xfff7f7f5);
    }

    juce::Colour getInkColour() const noexcept
    {
        return inverted ? juce::Colour(0xfff7f7f5) : juce::Colour(0xff111111);
    }

    juce::Colour getMutedInkColour() const noexcept
    {
        return inverted ? juce::Colour(0xffaaaaaa) : juce::Colour(0xff555555);
    }

    juce::Colour getPanelTintColour() const noexcept
    {
        return inverted ? juce::Colour(0xff1b1b1b) : juce::Colour(0xffededeb);
    }

    void refreshThemeColours()
    {
        const auto paper = getPaperColour();
        const auto ink   = getInkColour();

        setColour(juce::ResizableWindow::backgroundColourId, paper);
        setColour(juce::Label::textColourId, ink);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

        // Ensure unselected buttons have dark ink text on paper background,
        // and selected buttons have paper text on dark ink background.
        setColour(juce::TextButton::buttonColourId, paper);
        setColour(juce::TextButton::buttonOnColourId, ink);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TextButton::textColourOnId, paper);

        setColour(juce::ComboBox::backgroundColourId, paper);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, ink);
        setColour(juce::ComboBox::arrowColourId, ink);
        setColour(juce::ComboBox::focusedOutlineColourId, ink);

        setColour(juce::PopupMenu::backgroundColourId, paper);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, ink);
        setColour(juce::PopupMenu::highlightedTextColourId, paper);

        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxBackgroundColourId, paper);
        setColour(juce::Slider::textBoxOutlineColourId, ink.withAlpha(0.45f));
        setColour(juce::Slider::textBoxHighlightColourId, ink.withAlpha(0.2f));
        setColour(juce::Slider::rotarySliderFillColourId, ink);
        setColour(juce::Slider::rotarySliderOutlineColourId, ink);
        setColour(juce::Slider::thumbColourId, ink);
        setColour(juce::Slider::trackColourId, ink);
    }

    juce::Font getMangaFont(float height, bool bold = true) const
    {
        juce::FontOptions opts(height, bold ? juce::Font::bold : juce::Font::plain);
        return juce::Font(opts);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.75f);
        const bool isOn = button.getToggleState() || shouldDrawButtonAsDown;

        auto bg = isOn ? getInkColour() : getPaperColour();
        if (!isOn && shouldDrawButtonAsHighlighted)
            bg = getPanelTintColour();

        g.setColour(bg);
        g.fillRect(bounds);

        g.setColour(getInkColour());
        g.drawRect(bounds, 1.5f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool shouldDrawButtonAsDown) override
    {
        const bool isOn = button.getToggleState() || shouldDrawButtonAsDown;
        g.setColour(isOn ? getPaperColour() : getInkColour());

        const auto text = button.getButtonText();
        const int w = button.getWidth();
        const int h = button.getHeight();

        // Fit font cleanly even on compact semitone buttons like C#, D#, F#, G#, A#
        float fontSize = juce::jlimit(9.5f, 13.0f, static_cast<float>(h) * 0.48f);
        if (w <= 34 && text.length() >= 2)
            fontSize = juce::jmin(fontSize, 10.5f);

        g.setFont(getMangaFont(fontSize, true));
        // Minimal 1px horizontal inset so labels never truncate to "..."
        g.drawFittedText(text, 1, 0, juce::jmax(1, w - 2), h, juce::Justification::centred, 1, 0.72f);
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool /*isButtonDown*/,
                      int /*buttonX*/,
                      int /*buttonY*/,
                      int /*buttonW*/,
                      int /*buttonH*/,
                      juce::ComboBox& /*box*/) override
    {
        const juce::Rectangle<float> bounds(0.75f, 0.75f, static_cast<float>(width) - 1.5f, static_cast<float>(height) - 1.5f);
        g.setColour(getPaperColour());
        g.fillRect(bounds);

        g.setColour(getInkColour());
        g.drawRect(bounds, 1.5f);

        // Compact chevron arrow on the right
        const float arrowZoneW = 16.0f;
        const float cx = static_cast<float>(width) - arrowZoneW * 0.55f;
        const float cy = static_cast<float>(height) * 0.5f;

        juce::Path p;
        p.startNewSubPath(cx - 3.5f, cy - 2.0f);
        p.lineTo(cx, cy + 2.5f);
        p.lineTo(cx + 3.5f, cy - 2.0f);
        g.strokePath(p, juce::PathStrokeType(1.6f));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        // Tight margins so text like "1 OCT", "12 VOX", "AUTO (GUN MODE)" never clips to "..."
        label.setBounds(4, 1, juce::jmax(1, box.getWidth() - 18), juce::jmax(1, box.getHeight() - 2));
        const float fontSize = juce::jlimit(9.5f, 12.0f, static_cast<float>(box.getHeight()) * 0.46f);
        label.setFont(getMangaFont(fontSize, true));
        label.setColour(juce::Label::textColourId, getInkColour());
        label.setJustificationType(juce::Justification::centredLeft);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& /*slider*/) override
    {
        const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.5f - 5.0f;
        if (radius <= 4.0f)
            return;

        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto ink = getInkColour();
        const auto paper = getPaperColour();

        // Outer dial fill
        g.setColour(paper);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // 13 manga dial ticks around perimeter
        g.setColour(ink);
        constexpr int numTicks = 13;
        for (int i = 0; i < numTicks; ++i)
        {
            const float frac = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float a = rotaryStartAngle + frac * (rotaryEndAngle - rotaryStartAngle);
            const float sinA = std::sin(a);
            const float cosA = -std::cos(a);
            const float rInner = radius + 0.5f;
            const float rOuter = radius + ((i == 0 || i == 6 || i == 12) ? 4.2f : 2.6f);
            g.drawLine(cx + sinA * rInner, cy + cosA * rInner,
                       cx + sinA * rOuter, cy + cosA * rOuter, 1.3f);
        }

        // Double manga ink rings
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.8f);
        const float innerR = radius * 0.80f;
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.9f);

        // Pointer needle
        const float sinP = std::sin(angle);
        const float cosP = -std::cos(angle);
        g.drawLine(cx, cy, cx + sinP * (radius * 0.84f), cy + cosP * (radius * 0.84f), 2.1f);

        // Center hub dot
        g.fillEllipse(cx - 2.4f, cy - 2.4f, 4.8f, 4.8f);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float /*minSliderPos*/,
                          float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle /*style*/,
                          juce::Slider& slider) override
    {
        const auto ink = getInkColour();
        const auto paper = getPaperColour();

        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float left = static_cast<float>(x) + 4.0f;
        const float right = static_cast<float>(x + width) - 4.0f;
        const float trackW = juce::jmax(4.0f, right - left);
        const float trackH = 5.0f;

        // Track outline
        g.setColour(paper);
        g.fillRect(left, cy - trackH * 0.5f, trackW, trackH);
        g.setColour(ink);
        g.drawRect(left, cy - trackH * 0.5f, trackW, trackH, 1.2f);

        // Fill from center if bipolar, else from left
        const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
        const float clampedPos = juce::jlimit(left, right, sliderPos);
        if (isBipolar)
        {
            const float mid = left + trackW * 0.5f;
            g.drawLine(mid, cy - 5.0f, mid, cy + 5.0f, 1.0f);
            const float fStart = juce::jmin(mid, clampedPos);
            const float fEnd = juce::jmax(mid, clampedPos);
            g.fillRect(fStart, cy - trackH * 0.5f, fEnd - fStart, trackH);
        }
        else
        {
            g.fillRect(left, cy - trackH * 0.5f, clampedPos - left, trackH);
        }

        // Rectangular manga fader cap
        const float thumbW = 6.0f;
        const float thumbH = 13.0f;
        g.setColour(paper);
        g.fillRect(clampedPos - thumbW * 0.5f, cy - thumbH * 0.5f, thumbW, thumbH);
        g.setColour(ink);
        g.drawRect(clampedPos - thumbW * 0.5f, cy - thumbH * 0.5f, thumbW, thumbH, 1.5f);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
        l->setFont(getMangaFont(10.0f, true));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, getInkColour());
        l->setColour(juce::Label::backgroundColourId, getPaperColour());
        l->setColour(juce::Label::outlineColourId, getInkColour().withAlpha(0.45f));
        return l;
    }

private:
    bool inverted = false;
};
