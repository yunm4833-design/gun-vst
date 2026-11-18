#pragma once

#include "ImpactTelemetry.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

class BreachResonatorAudioProcessor;
class MangaLookAndFeel;

class MangaSplitViewport : public juce::Component
{
public:
    MangaSplitViewport(BreachResonatorAudioProcessor&, MangaLookAndFeel&);

    void paint(juce::Graphics&) override;
    void tickAnimation(double nowMs);
    void triggerPreview();

private:
    struct DebrisParticle
    {
        juce::Point<float> velocity;
        float size = 0, spin = 0, age = 1, lifetime = .4f;
    };

    void refreshArtwork(juce::Colour paper, juce::Colour ink);
    void beginStrike(float strength);
    void emitDebris(float brightness);
    void drawSpeedLines(juce::Graphics&, juce::Colour ink) const;
    void drawCracks(juce::Graphics&, juce::Colour ink) const;
    void drawStrike(juce::Graphics&, juce::Colour paper, juce::Colour ink) const;
    void drawDebris(juce::Graphics&, juce::Colour paper, juce::Colour ink) const;
    void drawOutput(juce::Graphics&, juce::Colour paper, juce::Colour ink) const;

    BreachResonatorAudioProcessor& processor;
    MangaLookAndFeel& laf;
    std::unique_ptr<juce::Drawable> projectile, fracture;
    juce::Image wallCache;
    bool cachedInverted = false;
    std::array<juce::Path, breach::impact::voiceCount> cracks;
    std::array<DebrisParticle, 56> particles {};
    std::array<float, breach::impact::voiceCount> voiceInk {};
    std::array<float, 2> outputInk {};
    std::size_t nextParticle = 0;

    double lastTickMs = -1;
    float silenceSinceUpdate = 0, motionPhase = 0, excitationInk = 0;
    float strikeAge = 1, strikeStrength = 0;
    std::uint32_t lastStrike = 0, lastBlock = 0;
    int mode = 0, strikeMode = 0, letteringIndex = 0;
    bool hadMotion = false, debrisPending = false, manualStrike = false;
};
