#include "ImpactPanel.h"
#include "PluginProcessor.h"
#include "MangaLookAndFeel.h"
#include "BinaryData.h"

namespace
{
constexpr float width = 674.0f, height = 316.0f;
const juce::Point<float> impactPoint { 408.0f, 160.0f };
constexpr float strikeDuration = .39f;
constexpr float rotation = juce::MathConstants<float>::twoPi;

struct Brush
{
    int rays;
    float spread, reach, weight, projectileWidth, projectileHeight, burst, debrisSpread, duration;
};

constexpr Brush brushes[] {
    { 45, .88f, .86f, 1.05f, .85f, .91f, .85f, 1.0f, .80f },
    { 69, 1.30f, .98f, 1.30f, 1.00f, 1.04f, 1.16f, 1.24f, 1.0f },
    { 84, 1.06f, .84f, .83f, .74f, .84f, .78f, .94f, .87f },
    { 40, .55f, 1.22f, .88f, 1.16f, .74f, .72f, .64f, .92f },
    { 51, 1.12f, .95f, 1.68f, .97f, 1.19f, 1.27f, 1.06f, 1.10f }
};

constexpr const char* lettering[][3] {
    { "BANG!!", "CRACK!!", "SNAP!!" },
    { "BOOM!!", "BLAM!!", "BLAST!!" },
    { "BRRRRT!!", "RATATAT!!", "BURST!!" },
    { "PIERCE!!", "KRAKOW!!", "SHATTER!!" },
    { "THOOM!!", "CRUSH!!", "SLAM!!" }
};

// Each voice has a distinct direction and a fork in the wall.
constexpr float crackGeometry[][14] {
    { 395, 141, 374, 119, 365, 106, 350, 89, 332, 50, 342, 98, 321, 88 },
    { 404, 136, 408, 110, 398, 84, 388, 59, 397, 25, 384, 63, 379, 45 },
    { 418, 138, 443, 102, 458, 61, 480, 23, 511, -10, 478, 47, 492, 41 },
    { 428, 147, 468, 131, 497, 97, 546, 97, 576, 62, 513, 109, 525, 115 },
    { 432, 163, 463, 164, 495, 162, 525, 171, 580, 154, 516, 175, 527, 185 },
    { 427, 174, 460, 192, 487, 217, 523, 229, 572, 257, 512, 220, 526, 235 },
    { 416, 183, 433, 217, 439, 255, 462, 287, 472, 329, 459, 271, 470, 279 },
    { 405, 184, 391, 217, 391, 252, 393, 288, 368, 323, 398, 276, 407, 294 },
    { 396, 181, 373, 219, 352, 258, 327, 295, 326, 340, 331, 272, 317, 278 },
    { 390, 176, 378, 195, 356, 205, 350, 231, 337, 250, 332, 213, 317, 215 },
    { 384, 165, 355, 170, 325, 172, 296, 184, 263, 193, 306, 189, 298, 201 },
    { 385, 153, 353, 137, 316, 134, 283, 120, 247, 113, 299, 115, 292, 102 }
};

int selectedMode(const BreachResonatorAudioProcessor& processor)
{
    return juce::jlimit(0, 4, static_cast<int>(processor.apvts.getRawParameterValue("gunMode")->load()));
}

float unit(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float inkLevel(float rms, float floorDb, float topDb)
{
    if (!(rms > 0)) return 0;
    return unit((juce::Decibels::gainToDecibels(rms, floorDb) - floorDb) / (topDb - floorDb));
}

void follow(float& value, float target, float seconds)
{
    const float time = target > value ? .018f : .033f;
    value += (target - value) * (1.0f - std::exp(-seconds / time));
    if (value < .002f && target < .002f) value = 0.0f;
}
} // namespace

MangaSplitViewport::MangaSplitViewport(BreachResonatorAudioProcessor& p, MangaLookAndFeel& l)
    : processor(p), laf(l), mode(selectedMode(p))
{
    const auto& telemetry = processor.telemetry.impact;
    lastStrike = telemetry.strike.load(breach::impact::relaxed) >> 8;
    for (std::size_t i = 0; i < cracks.size(); ++i)
    {
        const auto& points = crackGeometry[i];
        auto& crack = cracks[i];
        const juce::Point<float> inner(points[0], points[1]);
        crack.startNewSubPath(impactPoint + (inner - impactPoint) * .66f);
        for (int pair = 0; pair <= 8; pair += 2)
            crack.lineTo(points[pair], points[pair + 1]);
        crack.startNewSubPath(points[4], points[5]);
        crack.lineTo(points[10], points[11]);
        crack.lineTo(points[12], points[13]);
    }
}

void MangaSplitViewport::tickAnimation(double nowMs)
{
    const float elapsed = lastTickMs < 0 ? 1.0f / 60.0f : static_cast<float>(juce::jmax(0.0, nowMs - lastTickMs) * .001);
    lastTickMs = nowMs;
    const auto& telemetry = processor.telemetry.impact;
    const auto block = telemetry.renderedBlocks.load(std::memory_order_acquire);
    silenceSinceUpdate = block == lastBlock ? silenceSinceUpdate + elapsed : 0.0f;
    lastBlock = block;
    const bool fresh = silenceSinceUpdate < .20f;
    const auto read = [fresh](const std::atomic<float>& signal)
    {
        return fresh ? signal.load(breach::impact::relaxed) : 0.0f;
    };

    const int previousMode = mode;
    mode = selectedMode(processor);
    follow(excitationInk, inkLevel(read(telemetry.excitation), -58, -9), elapsed);
    for (std::size_t i = 0; i < voiceInk.size(); ++i)
        follow(voiceInk[i], inkLevel(read(telemetry.voices[i]), -66, -22), elapsed);
    for (std::size_t i = 0; i < outputInk.size(); ++i)
        follow(outputInk[i], inkLevel(read(telemetry.output[i]), -68, 0), elapsed);

    motionPhase = std::fmod(motionPhase + elapsed * (1.0f + 2.2f * excitationInk), 1.0f);
    strikeAge += elapsed;
    for (auto& particle : particles)
        particle.age = juce::jmin(particle.lifetime, particle.age + elapsed);

    const auto strike = telemetry.strike.load(breach::impact::relaxed);
    const auto serial = strike >> 8;
    const float strength = static_cast<float>(strike & 255u) / 255.0f;
    if (serial != lastStrike && strength > 0 && fresh)
    {
        beginStrike(std::sqrt(strength));
        manualStrike = false;
    }
    else if (!manualStrike && strikeAge < .08f)
    {
        strikeStrength = juce::jmax(strikeStrength, std::sqrt(strength));
    }
    lastStrike = serial;

    if (debrisPending && strikeAge >= .023f)
    {
        if (strikeAge < .13f)
            emitDebris(manualStrike ? .45f : telemetry.brightness.load(breach::impact::relaxed));
        debrisPending = false;
    }

    const auto positive = [](float level) { return level > 0.0f; };
    const bool active = excitationInk > 0 || strikeAge < strikeDuration * brushes[strikeMode].duration
        || std::any_of(voiceInk.begin(), voiceInk.end(), positive)
        || std::any_of(outputInk.begin(), outputInk.end(), positive)
        || std::any_of(particles.begin(), particles.end(), [](const auto& p) { return p.age < p.lifetime; });
    if (active || hadMotion || previousMode != mode)
        repaint();
    hadMotion = active;
}

void MangaSplitViewport::beginStrike(float strength)
{
    strikeStrength = strength;
    strikeAge = 0;
    strikeMode = mode;
    letteringIndex = (letteringIndex + 1) % 3;
    debrisPending = true;
}

void MangaSplitViewport::triggerPreview()
{
    mode = selectedMode(processor);
    beginStrike(.82f);
    manualStrike = true;
    repaint();
}

void MangaSplitViewport::emitDebris(float brightness)
{
    const auto& brush = brushes[strikeMode];
    if (!std::isfinite(brightness)) brightness = .25f;
    const int count = 6 + juce::roundToInt(brightness * 18.0f + strikeStrength * 6.0f);
    for (int i = 0; i < count; ++i)
    {
        const float step = static_cast<float>(i);
        const float angle = step * 2.399963f + .4f * static_cast<float>(letteringIndex);
        const float scatter = static_cast<float>((i * 7) % 13) / 12.0f;
        const float speed = (95.0f + scatter * 270.0f) * (.52f + .48f * strikeStrength);
        auto& particle = particles[nextParticle];
        nextParticle = (nextParticle + 1) % particles.size();
        particle.velocity = { std::cos(angle) * speed, std::sin(angle) * speed * brush.debrisSpread };
        particle.size = juce::jmap(brightness, 10.0f, 2.5f) * (.50f + scatter * .7f) * brush.burst;
        particle.spin = angle;
        particle.lifetime = .25f + .25f * scatter;
        particle.age = 0;
    }
}

void MangaSplitViewport::refreshArtwork(juce::Colour paper, juce::Colour ink)
{
    wallCache = juce::Image(juce::Image::RGB, 1348, 632, true);
    juce::Graphics art(wallCache);
    art.fillAll(paper);
    if (auto wall = juce::Drawable::createFromImageData(BinaryData::Impact_svg, BinaryData::Impact_svgSize))
    {
        wall->replaceColour(juce::Colours::black, ink);
        wall->draw(art, 1.0f, juce::AffineTransform::scale(2.0f));
    }

    auto loadLayer = [paper, ink](const void* data, std::size_t size)
    {
        auto layer = juce::Drawable::createFromImageData(data, size);
        if (layer)
        {
            layer->replaceColour(juce::Colours::black, ink);
            layer->replaceColour(juce::Colours::white, paper);
        }
        return layer;
    };
    projectile = loadLayer(BinaryData::ImpactProjectile_svg, BinaryData::ImpactProjectile_svgSize);
    fracture = loadLayer(BinaryData::ImpactFracture_svg, BinaryData::ImpactFracture_svgSize);
    cachedInverted = laf.isInverted();
}

void MangaSplitViewport::drawSpeedLines(juce::Graphics& g, juce::Colour ink) const
{
    const auto& brush = brushes[mode];
    const float energy = excitationInk * excitationInk;
    const int count = 14 + juce::roundToInt(energy * static_cast<float>(brush.rays));
    for (int ray = 0; ray < count; ++ray)
    {
        const float position = static_cast<float>((ray * 37) % 137) / 136.0f;
        const float variation = static_cast<float>((ray * 19) % 29) / 28.0f;
        const float flicker = .77f + .23f * std::cos(rotation * (motionPhase + variation));
        const float y = impactPoint.y + (position - .5f) * (400.0f + 185.0f * energy * brush.spread);
        const float x = (ray % 4 == 0 ? 16.0f : -6.0f) + (1.0f - energy) * variation * 44.0f;
        const float reach = juce::jmin(365.0f, (91.0f + 53.0f * variation
            + energy * (92.0f + 145.0f * flicker)) * brush.reach);
        const float thickness = .45f + 2.6f * energy * brush.weight * (position * .75f + .25f);
        juce::Path rayInk;
        rayInk.addTriangle(x, y, x, y + thickness, reach, juce::jmap(reach / impactPoint.x, y, impactPoint.y));
        g.setColour(ink.withAlpha(.14f + energy * (.40f + .38f * flicker)));
        g.fillPath(rayInk);
    }
}

void MangaSplitViewport::drawCracks(juce::Graphics& g, juce::Colour ink) const
{
    for (std::size_t i = 0; i < cracks.size(); ++i)
    {
        const float amount = voiceInk[i] * voiceInk[i];
        if (amount < .003f) continue;
        g.setColour(ink.withAlpha(unit(amount * 1.65f)));
        g.strokePath(cracks[i], juce::PathStrokeType(.55f + amount * 2.25f, juce::PathStrokeType::curved));
    }
}

void MangaSplitViewport::drawStrike(juce::Graphics& g, juce::Colour paper, juce::Colour ink) const
{
    const auto& brush = brushes[strikeMode];
    const float visualAge = strikeAge / brush.duration;
    const float flash = unit(1.0f - visualAge / .24f);
    if (fracture && flash > 0)
    {
        const float scale = (.30f + .75f * strikeStrength) * brush.burst * (.72f + .28f * flash);
        const auto transform = juce::AffineTransform::scale(scale, scale, impactPoint.x, impactPoint.y);
        fracture->draw(g, flash, transform);
    }

    const float travel = unit(visualAge / .17f);
    const float bulletOpacity = unit((1.0f - travel) * 2.8f);
    if (projectile && bulletOpacity > 0)
    {
        const auto position = impactPoint.translated(-105.0f * travel, -15.0f * travel);
        for (int streak = 0; streak < 9; ++streak)
        {
            const float y = static_cast<float>(streak - 4) * brush.projectileHeight;
            const float start = position.x - (30.0f + static_cast<float>((streak * 11) % 9) * 6.3f);
            g.setColour(ink.withAlpha(bulletOpacity * .72f));
            g.drawLine(start, position.y + y, impactPoint.x - 4.0f, impactPoint.y + y * .6f, .85f);
        }
        const auto transform = juce::AffineTransform::translation(-impactPoint.x, -impactPoint.y)
            .scaled(brush.projectileWidth, brush.projectileHeight)
            .translated(position.x, position.y);
        projectile->draw(g, bulletOpacity, transform);
    }

    const float letteringOpacity = unit((strikeDuration - visualAge) / .13f);
    const float size = 42.0f + strikeStrength * 23.0f;
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::rotation(-.17f, 105.0f, 80.0f));
    juce::GlyphArrangement glyphs;
    const auto font = juce::Font(juce::FontOptions(size, juce::Font::bold | juce::Font::italic));
    glyphs.addFittedText(font, lettering[strikeMode][letteringIndex], 28.0f, 27.0f, 279.0f, 84.0f,
                         juce::Justification::centred, 1, .7f);
    juce::Path outline;
    glyphs.createPath(outline);
    g.setColour(paper.withAlpha(letteringOpacity));
    g.strokePath(outline, juce::PathStrokeType(5.2f, juce::PathStrokeType::curved));
    g.setColour(ink.withAlpha(letteringOpacity));
    g.fillPath(outline);
}

void MangaSplitViewport::drawDebris(juce::Graphics& g, juce::Colour paper, juce::Colour ink) const
{
    for (const auto& particle : particles)
    {
        if (particle.age >= particle.lifetime) continue;
        const auto point = impactPoint + particle.velocity * (.018f + particle.age * (1.0f - particle.age * .58f))
            + juce::Point<float>(0.0f, 135.0f * particle.age * particle.age);
        const float s = particle.size;
        const float alpha = unit(1.0f - particle.age / particle.lifetime);
        juce::Path shard;
        shard.startNewSubPath(-s, -.25f * s);
        shard.lineTo(.45f * s, -.75f * s);
        shard.lineTo(.8f * s, .26f * s);
        shard.lineTo(-.25f * s, .68f * s);
        shard.closeSubPath();
        const auto transform = juce::AffineTransform::rotation(particle.spin + particle.age * 5.0f)
            .translated(point.x, point.y);
        shard.applyTransform(transform);
        g.setColour((s > 4.5f ? paper : ink).withAlpha(alpha));
        g.fillPath(shard);
        g.setColour(ink.withAlpha(alpha));
        g.strokePath(shard, juce::PathStrokeType(.8f));
        if (s > 4.5f)
        {
            const auto start = juce::Point<float>(-.4f * s, -.44f * s).transformedBy(transform);
            const auto end = juce::Point<float>(.2f * s, .45f * s).transformedBy(transform);
            g.drawLine(juce::Line<float>(start, end), .85f);
        }
    }
}

void MangaSplitViewport::drawOutput(juce::Graphics& g, juce::Colour paper, juce::Colour ink) const
{
    constexpr int rows = 53;
    g.setColour(paper);
    g.fillRect(606, 4, 64, 308);
    g.setFont(laf.getMangaFont(15.0f, true));
    for (std::size_t channel = 0; channel < outputInk.size(); ++channel)
    {
        const int x = 609 + static_cast<int>(channel) * 30;
        g.setColour(ink);
        g.drawText(channel == 0 ? "L" : "R", x, 14, 25, 19, juce::Justification::centred);
        g.setColour(ink.withAlpha(.45f));
        g.drawRect(x, 40, 25, rows * 4 + 3);
        const int lit = juce::roundToInt(outputInk[channel] * static_cast<float>(rows));
        for (int row = 0; row < rows; ++row)
        {
            const bool on = row < lit;
            const int y = 251 - row * 4;
            g.setColour(ink.withAlpha(on ? .94f : .20f));
            for (int column = 0; column < 5; ++column)
                g.fillRect(x + 3 + column * 4, y, on ? 3 : 1, on ? 3 : 1);
        }
        g.setColour(ink.withAlpha(.55f));
        for (int step = 0; step < 4; ++step)
            g.drawHorizontalLine(264 + step * 6, static_cast<float>(x + 8), static_cast<float>(x + 17));
    }
}

void MangaSplitViewport::paint(juce::Graphics& g)
{
    const auto paper = laf.getPaperColour();
    const auto ink = laf.getInkColour();
    if (!wallCache.isValid() || cachedInverted != laf.isInverted())
        refreshArtwork(paper, ink);

    g.drawImage(wallCache, getLocalBounds().toFloat());
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / width, static_cast<float>(getHeight()) / height));
    {
        juce::Graphics::ScopedSaveState picture(g);
        g.reduceClipRegion(0, 0, 603, static_cast<int>(height));
        drawSpeedLines(g, ink);
        drawCracks(g, ink);
        if (strikeAge < strikeDuration * brushes[strikeMode].duration) drawStrike(g, paper, ink);
        drawDebris(g, paper, ink);
    }
    drawOutput(g, paper, ink);
    g.setColour(ink);
    g.drawRect(0.0f, 0.0f, width, height, 4.0f);
}
