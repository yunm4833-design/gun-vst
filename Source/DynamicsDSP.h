#pragma once
#include "ThirdParty/Faust/FaustOtt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace breach::dynamics
{
constexpr double pi = 3.14159265358979323846;
struct Stereo { float left, right; };
struct Controls
{
    bool enabled = false;
    bool useFaust = false;
    float depth = .35f, timePercent = 100, upward = .5f, downward = .5f;
    float inputDb = 0, outputDb = 0, lowHz = 120, highHz = 2500;
};

inline double softPositive(double x, double knee = 6.0) noexcept
{
    if (x <= -knee * .5) return 0;
    if (x >= knee * .5) return x;
    return (x + knee * .5) * (x + knee * .5) / (2 * knee);
}

// Independent upward/downward compression, with a smooth low-level boost floor.
// The floor only removes upward boost; it never gates the input signal.
inline double gainDb(double levelDb, std::size_t band, double depth, double up, double down) noexcept
{
    constexpr std::array<double, 3> upper {{-24, -21, -24}}, lower {{-42, -40, -46}};
    const double floor = std::clamp((levelDb + 90) / 24.0, 0.0, 1.0);
    const double fade = floor * floor * (3 - 2 * floor);
    const double lift = std::min(18.0, softPositive(lower[band] - levelDb) * .75 * up * depth) * fade;
    const double cut = std::min(36.0, softPositive(levelDb - upper[band]) * .875 * down * depth);
    return lift - cut;
}

class Biquad
{
public:
    void design(double sr, double hz, bool highpass) noexcept
    {
        const double k = std::tan(pi * hz / sr), k2 = k * k;
        const double norm = 1 / (1 + std::sqrt(2.0) * k + k2);
        b0 = (highpass ? 1 : k2) * norm;
        b1 = (highpass ? -2 : 2) * b0; b2 = b0;
        a1 = 2 * (k2 - 1) * norm; a2 = (1 - std::sqrt(2.0) * k + k2) * norm;
    }
    double process(double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2; s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { s1 = s2 = 0; }
private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
};
class LR4
{
public:
    void design(double sr, double hz, bool highpass) noexcept
    { for (auto& section : sections) section.design(sr, hz, highpass); }
    double process(double x) noexcept { return sections[1].process(sections[0].process(x)); }
    void reset() noexcept { for (auto& section : sections) section.reset(); }
private:
    std::array<Biquad, 2> sections;
};

// Phase-align the low band at the upper crossover. The sum has unity magnitude,
// with crossover phase rotation. The enabled signal is not mixed with raw audio.
class Crossover
{
public:
    void design(double sr, double lowHz, double highHz) noexcept
    {
        low.design(sr, lowHz, false); upper.design(sr, lowHz, true);
        mid.design(sr, highHz, false); high.design(sr, highHz, true);
        alignLow.design(sr, highHz, false); alignHigh.design(sr, highHz, true);
    }
    std::array<double, 3> process(double x) noexcept
    {
        const double l = low.process(x), u = upper.process(x);
        return {{alignLow.process(l) + alignHigh.process(l), mid.process(u), high.process(u)}};
    }
    void reset() noexcept
    { for (auto* f : {&low, &upper, &mid, &high, &alignLow, &alignHigh}) f->reset(); }
private:
    LR4 low, upper, mid, high, alignLow, alignHigh;
};

class Engine
{
public:
    void prepare(double sr) noexcept
    {
        sampleRate = std::max(8000.0, sr);
        controlInterval = std::max(1, static_cast<int>(std::lround(sampleRate / 1000)));
        smooth = 1 - std::exp(-1 / (.020 * sampleRate));
        bypassStep = 1 / (.020 * sampleRate);
        fastGainSmooth = -std::expm1(-1 / (.0015 * sampleRate));
        initialized = false; reset();
    }
    void reset() noexcept
    {
        for (auto& c : crossover) c.reset();
        envelope.fill(0); gains.fill(1); targetGains.fill(1); meters.fill(0); blend = 0; countdown = 0;
        designedLow = designedHigh = designedTime = -1; initialized = false;
    }
    void setControls(Controls next) noexcept
    {
        next.depth = std::clamp(next.depth, 0.0f, 1.0f);
        next.upward = std::clamp(next.upward, 0.0f, 1.0f);
        next.downward = std::clamp(next.downward, 0.0f, 1.0f);
        next.timePercent = std::clamp(next.timePercent, 10.0f, 400.0f);
        next.inputDb = std::clamp(next.inputDb, -18.0f, 18.0f);
        next.outputDb = std::clamp(next.outputDb, -18.0f, 18.0f);
        next.highHz = std::clamp(next.highHz, 1000.0f, static_cast<float>(sampleRate * .4));
        next.lowHz = std::clamp(next.lowHz, 40.0f, std::min(400.0f, next.highHz * .45f));
        if (initialized && next.useFaust != target.useFaust)
        {
            // The legacy follower stores peak amplitude, the Faust follower stores power.
            for (auto& level : envelope)
                level = next.useFaust ? level * level : std::sqrt(level);
            designedTime = -1;
        }
        target = next;
        if (!initialized) { current = next; initialized = true; }
    }
    bool idle() const noexcept { return blend <= 0 && (!target.enabled || target.depth <= 0); }
    const std::array<float, 3>& gainMeters() const noexcept { return meters; }
    Stereo process(Stereo input) noexcept
    {
        const bool active = target.enabled && target.depth > 0;
        const bool useFaust = target.useFaust;
        if (!active && blend <= 0) { meters.fill(0); return input; }
        auto slew = [&](float& value, float destination)
        { value += static_cast<float>(smooth * (destination - value)); };
        slew(current.depth, target.depth); slew(current.upward, target.upward); slew(current.downward, target.downward);
        slew(current.timePercent, target.timePercent); slew(current.inputDb, target.inputDb); slew(current.outputDb, target.outputDb);
        slew(current.lowHz, target.lowHz); slew(current.highHz, target.highHz);
        if (countdown-- <= 0)
        {
            countdown = controlInterval - 1;
            if (std::abs(current.lowHz - designedLow) > .001 || std::abs(current.highHz - designedHigh) > .001)
            {
                for (auto& c : crossover) c.design(sampleRate, current.lowHz, current.highHz);
                designedLow = current.lowHz; designedHigh = current.highHz;
            }
            if (std::abs(current.timePercent - designedTime) > .001)
            {
                const double time = current.timePercent * .01;
                constexpr double attacks[3] = {.004, .002, .001}, releases[3] = {.100, .070, .045};
                for (std::size_t b = 0; b < 3; ++b)
                {
                    if (useFaust)
                    {
                        attack[b] = -std::expm1(-1 / (faustOtt::bands[b].attack * sampleRate));
                        release[b] = -std::expm1(-1 / (faustOtt::releaseSeconds(b, time) * sampleRate));
                    }
                    else
                    {
                        attack[b] = 1 - std::exp(-1 / (attacks[b] * time * sampleRate));
                        release[b] = 1 - std::exp(-1 / (releases[b] * time * sampleRate));
                    }
                }
                designedTime = current.timePercent;
            }
            inGain = std::pow(10.0, current.inputDb / 20.0); outGain = std::pow(10.0, current.outputDb / 20.0);
        }
        const auto left = crossover[0].process(input.left * inGain);
        const auto right = crossover[1].process(input.right * inGain);
        double sumL = 0, sumR = 0;
        for (std::size_t b = 0; b < 3; ++b)
        {
            const double detected = useFaust ? .5 * (left[b] * left[b] + right[b] * right[b])
                : std::max(std::abs(left[b]), std::abs(right[b]));
            envelope[b] += (detected - envelope[b]) * (detected > envelope[b] ? attack[b] : release[b]);
            // Only the detector-to-gain conversion runs at the control rate.
            if (countdown == controlInterval - 1)
            {
                const double adjustment = useFaust
                    ? faustOtt::gainDb(10 * std::log10(std::max(envelope[b], 1e-20)), b,
                                       current.depth, 2 * current.upward, 2 * current.downward)
                    : gainDb(20 * std::log10(std::max(envelope[b], 1e-12)), b,
                             current.depth, current.upward, current.downward);
                targetGains[b] = std::pow(10.0, adjustment / 20);
            }
            const double gainStep = useFaust ? fastGainSmooth : (targetGains[b] < gains[b] ? attack[b] : release[b]);
            gains[b] += (targetGains[b] - gains[b]) * gainStep;
            sumL += left[b] * gains[b]; sumR += right[b] * gains[b];
            if (countdown == controlInterval - 1)
                meters[b] = static_cast<float>(20 * std::log10(std::max(gains[b], 1e-12)));
        }
        blend += std::clamp((active ? 1.0 : 0.0) - blend, -bypassStep, bypassStep);
        if (blend <= 0) { reset(); return input; }
        return {static_cast<float>(input.left + blend * (sumL * outGain - input.left)),
                static_cast<float>(input.right + blend * (sumR * outGain - input.right))};
    }
private:
    std::array<Crossover, 2> crossover;
    std::array<double, 3> envelope {}, gains {{1,1,1}}, targetGains {{1,1,1}}, attack {}, release {};
    std::array<float, 3> meters {};
    Controls current, target;
    double sampleRate = 48000, smooth = .001, bypassStep = .001, fastGainSmooth = .014, blend = 0;
    double designedLow = -1, designedHigh = -1, designedTime = -1, inGain = 1, outGain = 1;
    int controlInterval = 48, countdown = 0;
    bool initialized = false;
};
} // namespace breach::dynamics
