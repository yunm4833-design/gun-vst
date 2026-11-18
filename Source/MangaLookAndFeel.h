#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "BinaryData.h"

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
        setColour(juce::Slider::textBoxOutlineColourId, ink.withAlpha(0.55f));
        setColour(juce::Slider::textBoxHighlightColourId, ink.withAlpha(0.2f));
        setColour(juce::Slider::rotarySliderFillColourId, ink);
        setColour(juce::Slider::rotarySliderOutlineColourId, ink);
        setColour(juce::Slider::thumbColourId, ink);
        setColour(juce::Slider::trackColourId, ink);
    }

    juce::Font getMangaFont(float height, bool bold = true) const
    {
        juce::Font font(juce::FontOptions(typeface).withHeight(height * 1.2f));
        if (bold) font = font.boldened();
        return font;
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
        g.fillRoundedRectangle(bounds, 3.0f);

        g.setColour(getInkColour());
        g.drawRoundedRectangle(bounds, 3.0f, 1.7f);
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

        float fontSize = juce::jlimit(12.0f, 25.0f, static_cast<float>(h) * 0.68f);
        if (w <= 34 && text.length() >= 2)
            fontSize = juce::jmin(fontSize, 10.5f);

        g.setFont(getMangaFont(fontSize, false));
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
        label.setBounds(4, 1, juce::jmax(1, box.getWidth() - 18), juce::jmax(1, box.getHeight() - 2));
        const float fontSize = juce::jlimit(12.0f, 23.0f, static_cast<float>(box.getHeight()) * 0.67f);
        label.setFont(getMangaFont(fontSize, false));
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
        const float diameter = static_cast<float>(juce::jmin(width, height));
        const float radius = diameter * 0.5f - juce::jmin(13.0f, diameter * 0.16f);
        const float ornamentScale = juce::jmin(1.0f, radius / 25.0f);
        if (radius <= 4.0f)
            return;

        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto ink = getInkColour();
        const auto paper = getPaperColour();

        g.setColour(paper);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(ink);
        constexpr int numTicks = 31;
        for (int i = 0; i < numTicks; ++i)
        {
            const float frac = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float a = rotaryStartAngle + frac * (rotaryEndAngle - rotaryStartAngle);
            const float sinA = std::sin(a);
            const float cosA = -std::cos(a);
            const float rInner = radius + 7.0f * ornamentScale;
            const float rOuter = radius + (i % 3 == 0 ? 13.0f : 10.0f) * ornamentScale;
            g.drawLine(cx + sinA * rInner, cy + cosA * rInner,
                       cx + sinA * rOuter, cy + cosA * rOuter, 1.3f);
        }

        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.8f);
        const float innerR = radius - 3.5f * ornamentScale;
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 0.9f);

        const float sinP = std::sin(angle);
        const float cosP = -std::cos(angle);
        g.drawLine(cx, cy, cx + sinP * (radius - 5.0f * ornamentScale), cy + cosP * (radius - 5.0f * ornamentScale), 3.5f * ornamentScale);
        for (int i = 0; i < 120; ++i)
        {
            const float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 120.0f;
            g.drawLine(cx + std::sin(a) * (radius + 1.5f * ornamentScale), cy + std::cos(a) * (radius + 1.5f * ornamentScale),
                       cx + std::sin(a) * (radius + 3.5f * ornamentScale), cy + std::cos(a) * (radius + 3.5f * ornamentScale), 0.55f);
        }
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
        if (static_cast<bool>(slider.getProperties()["screentone"]))
        {
            auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y + height / 2 - 9), static_cast<float>(width), 18.0f);
            g.setColour(paper); g.fillRoundedRectangle(track, 2.0f);
            g.setColour(ink);
            for (float yy = track.getY() + 2; yy < track.getBottom() - 1; yy += 3)
                for (float xx = track.getX() + 2; xx < track.getRight() - 1; xx += 3)
                    g.fillEllipse(xx, yy, 1.0f, 1.0f);
            g.drawRoundedRectangle(track, 2.0f, 1.4f);
            const float pos = juce::jlimit(track.getX() + 2, track.getRight() - 2, sliderPos);
            g.fillRect(pos - 1.5f, track.getY(), 3.0f, track.getHeight());
            return;
        }

        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float left = static_cast<float>(x) + 4.0f;
        const float right = static_cast<float>(x + width) - 4.0f;
        const float trackW = juce::jmax(4.0f, right - left);
        const float trackH = 5.0f;

        g.setColour(paper);
        g.fillRect(left, cy - trackH * 0.5f, trackW, trackH);
        g.setColour(ink);
        g.drawRect(left, cy - trackH * 0.5f, trackW, trackH, 1.2f);

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

        const float thumbW = 6.0f;
        const float thumbH = 13.0f;
        g.setColour(paper);
        g.fillRect(clampedPos - thumbW * 0.5f, cy - thumbH * 0.5f, thumbW, thumbH);
        g.setColour(ink);
        g.drawRect(clampedPos - thumbW * 0.5f, cy - thumbH * 0.5f, thumbW, thumbH, 1.5f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const bool isSliderBox = (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr);
        const auto ink   = getInkColour();
        const auto paper = getPaperColour();

        if (isSliderBox)
        {
            const auto bounds = label.getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(paper);
            g.fillRect(bounds);
            g.setColour(ink.withAlpha(0.55f));
            g.drawRect(bounds, 1.0f);
        }
        else
        {
            g.fillAll(label.findColour(juce::Label::backgroundColourId));
        }

        if (!label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            g.setColour(ink.withAlpha(alpha));
            g.setFont(isSliderBox ? getMangaFont(10.0f, true) : label.getFont());

            const auto textArea = isSliderBox
                ? label.getLocalBounds().reduced(2, 1)
                : label.getBorderSize().subtractedFrom(label.getLocalBounds());

            g.drawFittedText(label.getText(),
                             textArea,
                             isSliderBox ? juce::Justification::centred : label.getJustificationType(),
                             juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / g.getCurrentFont().getHeight())),
                             0.75f);
        }
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
        l->setFont(getMangaFont(10.0f, true));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, getInkColour());
        l->setColour(juce::Label::backgroundColourId, getPaperColour());
        l->setColour(juce::Label::outlineColourId, getInkColour().withAlpha(0.55f));
        return l;
    }

private:
    juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::BarlowCondensedRegular_ttf, BinaryData::BarlowCondensedRegular_ttfSize);
    bool inverted = false;
};
