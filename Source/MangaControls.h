#pragma once
#include "PluginProcessor.h"
#include "MangaLookAndFeel.h"

class MangaSettingsBackdrop : public juce::Component
{
public:
    explicit MangaSettingsBackdrop(MangaLookAndFeel& l) : laf(l) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(laf.getPaperColour()); g.setColour(laf.getInkColour());
        g.drawRect(getLocalBounds(), 2);
        g.setFont(laf.getMangaFont(16.0f, false));
        g.drawText("CHORD / ARP", 12, 6, 130, 20, juce::Justification::centredLeft, false);
    }
private:
    MangaLookAndFeel& laf;
};

// Repeated piano keys edit the original pitch-class parameters.
class MangaKeyboard : public juce::Component, public juce::SettableTooltipClient
{
public:
    MangaKeyboard(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l) : processor(p), laf(l)
    {
        setTooltip("Click a key to toggle its pitch class. Right-click for chord and arp settings.");
        setTitle("Chord keyboard");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    std::function<void()> onSettings;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(laf.getInkColour());
        const auto keys = getKeys();
        for (const auto& key : keys)
        {
            const bool active = processor.apvts.getRawParameterValue("note_" + juce::String(key.note))->load() > 0.5f;
            g.setColour(key.black ? laf.getInkColour() : laf.getPaperColour());
            g.fillRect(key.bounds);
            if (!key.black)
            {
                g.setColour(laf.getInkColour());
                g.drawRect(key.bounds, 0.6f);
            }
            if (active)
            {
                g.setColour(key.black ? laf.getPaperColour() : laf.getInkColour());
                g.fillEllipse(key.bounds.getCentreX() - 1.3f, key.bounds.getBottom() - 4.5f, 2.6f, 2.6f);
            }
        }
        g.setColour(laf.getPaperColour());
        g.setFont(laf.getMangaFont(23.0f, false));
        for (const auto& pair : std::initializer_list<std::pair<const char*, int>>{{"C", 0}, {"A", 322}, {"F", 355}, {"B", 641}})
            g.drawText(pair.first, pair.second, 0, 33, getHeight(), juce::Justification::centred, false);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onSettings) onSettings(); return; }
        auto keys = getKeys();
        for (auto it = keys.rbegin(); it != keys.rend(); ++it)
            if (it->bounds.contains(e.position))
            {
                auto* p = processor.apvts.getParameter("note_" + juce::String(it->note));
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getValue() < 0.5f ? 1.0f : 0.0f);
                p->endChangeGesture();
                repaint(); return;
            }
        if (onSettings) onSettings();
    }
private:
    struct Key { juce::Rectangle<float> bounds; int note; bool black; };
    std::vector<Key> getKeys() const
    {
        std::vector<Key> keys;
        constexpr int whiteNotes[7] = {0, 2, 4, 5, 7, 9, 11};
        for (int bank = 0; bank < 2; ++bank)
        {
            const float left = bank == 0 ? 34.0f : 389.0f;
            const float width = (bank == 0 ? 270.0f : 252.0f) / 21.0f;
            for (int i = 0; i < 21; ++i)
                keys.push_back({{left + i * width, 3.0f, width, 33.0f}, whiteNotes[(i + (bank == 0 ? 0 : 3)) % 7], false});
            for (int i = 0; i < 20; ++i)
            {
                int note = whiteNotes[(i + (bank == 0 ? 0 : 3)) % 7];
                if (note != 4 && note != 11)
                    keys.push_back({{left + (i + 1) * width - 3.6f, 3.0f, 7.2f, 19.0f}, note + 1, true});
            }
        }
        return keys;
    }
    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& laf;
};

// UI macros write existing parameters; no additional host parameter IDs.
