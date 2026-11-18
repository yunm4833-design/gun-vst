#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>

namespace breach::eq
{
constexpr std::size_t bandCount = 8;
constexpr double pi = 3.14159265358979323846;
constexpr double neutralQ = 0.7071067811865475244;
constexpr double minHz = 10, maxHz = 20000;

enum class Shape { lowCut12, lowCut24, lowCut48, lowShelf, bell, notch, highShelf, highCut12, highCut24, highCut48 };
constexpr std::array<const char*, 10> shapeNames {{
    "LOW CUT 12 DB/OCT", "LOW CUT 24 DB/OCT", "LOW CUT 48 DB/OCT", "LOW SHELF", "BELL", "NOTCH",
    "HIGH SHELF", "HIGH CUT 12 DB/OCT", "HIGH CUT 24 DB/OCT", "HIGH CUT 48 DB/OCT"
}};
constexpr bool hasGain(Shape shape) noexcept
{
    return shape == Shape::bell || shape == Shape::lowShelf || shape == Shape::highShelf;
}
constexpr bool hasQ(Shape shape) noexcept
{
    return shape != Shape::lowShelf && shape != Shape::highShelf;
}

struct BandSettings
{
    bool enabled = false;
    Shape shape = Shape::bell;
    double hz = 1000, gain = 0, q = neutralQ;
};
struct Rates
{
    double base = 48000, core = 48000;
    double upperFrequency() const noexcept { return std::min(maxHz, base * .49); }
};
struct Coefficients
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
struct Filter
{
    std::array<Coefficients, 4> sections;
    std::size_t count = 0;

    double magnitudeDb(std::complex<double> z, std::complex<double> z2) const noexcept
    {
        double db = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& c = sections[i];
            const auto numerator = c.b0 + c.b1 * z + c.b2 * z2;
            const auto denominator = 1.0 + c.a1 * z + c.a2 * z2;
            db += 20 * std::log10(std::max(1e-15, std::abs(numerator / denominator)));
        }
        return db;
    }
};

// Bilinear-transform biquads using the published Audio EQ Cookbook equations.
// Cut filters use Butterworth section damping at Q = 0.707. Higher orders keep
// all their sections, so a 48 dB/oct cut is an eighth-order cascade.
inline Filter design(BandSettings settings, Rates rates) noexcept
{
    Filter result;
    if (hasGain(settings.shape) && std::abs(settings.gain) < 1e-8) return result;

    const double hz = std::clamp(settings.hz, minHz, rates.upperFrequency());
    const double w = 2 * pi * hz / rates.core;
    const double sine = std::sin(w), cosine = std::cos(w);
    const double q = std::clamp(settings.q, .25, 8.0);
    auto add = [&](double b0, double b1, double b2, double a0, double a1, double a2)
    {
        result.sections[result.count++] = {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    };

    const auto shape = settings.shape;
    if (shape <= Shape::lowCut48 || shape >= Shape::highCut12)
    {
        const bool highpass = shape <= Shape::lowCut48;
        const auto offset = static_cast<int>(shape) - (highpass ? 0 : static_cast<int>(Shape::highCut12));
        const int stages = offset == 2 ? 4 : offset + 1;
        const double b0 = (1 + (highpass ? cosine : -cosine)) * .5;
        for (int stage = 0; stage < stages; ++stage)
        {
            const double butterworthQ = .5 / std::cos((2 * stage + 1) * pi / (4 * stages));
            // Adjust the most resonant section. Limit its Q to keep boosts
            // manageable when users stack steep filters.
            const double stageQ = stage == stages - 1 ? std::min(8.0, butterworthQ * q / neutralQ) : butterworthQ;
            const double alpha = sine / (2 * stageQ);
            add(b0, (highpass ? -2 : 2) * b0, b0, 1 + alpha, -2 * cosine, 1 - alpha);
        }
        return result;
    }

    const double alpha = sine / (2 * q);
    if (shape == Shape::notch)
    {
        add(1, -2 * cosine, 1, 1 + alpha, -2 * cosine, 1 - alpha);
        return result;
    }
    const double amplitude = std::pow(10.0, std::clamp(settings.gain, -18.0, 18.0) / 40);
    if (shape == Shape::bell)
    {
        add(1 + alpha * amplitude, -2 * cosine, 1 - alpha * amplitude,
            1 + alpha / amplitude, -2 * cosine, 1 - alpha / amplitude);
        return result;
    }

    // Fixed shelf slope S=1 gives a monotonic shelf.
    const double sum = amplitude + 1, difference = amplitude - 1;
    const double beta = std::sqrt(2 * amplitude) * sine;
    if (shape == Shape::lowShelf)
        add(amplitude * (sum - difference * cosine + beta), 2 * amplitude * (difference - sum * cosine),
            amplitude * (sum - difference * cosine - beta), sum + difference * cosine + beta,
            -2 * (difference + sum * cosine), sum + difference * cosine - beta);
    else
        add(amplitude * (sum + difference * cosine + beta), -2 * amplitude * (difference + sum * cosine),
            amplitude * (sum + difference * cosine - beta), sum - difference * cosine + beta,
            2 * (difference - sum * cosine), sum - difference * cosine - beta);
    return result;
}

struct Stereo { double left, right; };

class Band
{
public:
    void prepare(Rates newRates) noexcept
    {
        rates = newRates;
        interval = std::max(1, static_cast<int>(std::lround(rates.core / 2000)));
        smoothing = 1 - std::exp(-interval / (.018 * rates.core));
        fadeStep = 1 / (.012 * rates.core);
        reset();
    }

    void reset() noexcept
    {
        initialized = false;
        blend = 0;
        clearFilter();
    }

    void setTarget(BandSettings settings) noexcept
    {
        settings.hz = std::clamp(settings.hz, minHz, rates.upperFrequency());
        target = settings;
        if (!target.enabled && blend <= 0)
        {
            initialized = true;
            return;
        }
        targetLogHz = std::log(target.hz);
        if (blend <= 0)
        {
            activeShape = target.shape;
            startAtTarget();
            if (!initialized && target.enabled) blend = 1;
        }
        initialized = true;
    }

    bool idle() const noexcept { return blend <= 0 && !target.enabled; }

    Stereo process(Stereo input) noexcept
    {
        if (idle()) return input;

        if (activeShape != target.shape && blend <= 0)
        {
            activeShape = target.shape;
            startAtTarget();
        }
        const bool engaging = target.enabled && activeShape == target.shape;
        blend = std::clamp(blend + (engaging ? fadeStep : -fadeStep), 0.0, 1.0);
        if (blend <= 0)
        {
            clearFilter();
            startAtTarget();
            return input;
        }

        if (countdown-- <= 0)
        {
            countdown = interval - 1;
            if (activeShape == target.shape) updateCoefficients();
        }
        Stereo output = input;
        for (std::size_t i = 0; i < filter.count; ++i)
        {
            output.left = states[i][0].process(output.left, filter.sections[i]);
            output.right = states[i][1].process(output.right, filter.sections[i]);
        }
        if (blend >= 1) return output;
        return {input.left + (output.left - input.left) * blend,
                input.right + (output.right - input.right) * blend};
    }

private:
    struct State
    {
        double z1 = 0, z2 = 0;
        double process(double input, const Coefficients& c) noexcept
        {
            const double output = c.b0 * input + z1;
            z1 = c.b1 * input - c.a1 * output + z2;
            z2 = c.b2 * input - c.a2 * output;
            return output;
        }
    };

    void clearFilter() noexcept { states = {}; countdown = 0; }

    void startAtTarget() noexcept
    {
        currentLogHz = targetLogHz;
        currentGain = target.gain;
        currentQ = target.q;
        filter = design({true, target.shape, target.hz, currentGain, currentQ}, rates);
    }

    void updateCoefficients() noexcept
    {
        const auto previousLogHz = currentLogHz, previousGain = currentGain, previousQ = currentQ;
        auto smooth = [this](double& current, double next)
        {
            const auto delta = next - current;
            current = std::abs(delta) < 1e-6 ? next : current + delta * smoothing;
        };
        smooth(currentLogHz, targetLogHz);
        smooth(currentGain, target.gain);
        smooth(currentQ, target.q);
        if (std::abs(currentLogHz - previousLogHz) + std::abs(currentGain - previousGain)
            + std::abs(currentQ - previousQ) > 0)
        {
            const auto updated = design({true, activeShape, std::exp(currentLogHz), currentGain, currentQ}, rates);
            if (updated.count == 0 && filter.count != 0) states = {};
            filter = updated;
        }
    }

    Rates rates;
    BandSettings target;
    Shape activeShape = Shape::bell;
    Filter filter;
    std::array<std::array<State, 2>, 4> states {};
    double currentLogHz = 0, currentGain = 0, currentQ = neutralQ, targetLogHz = 0;
    double blend = 0, smoothing = .02, fadeStep = .002;
    int interval = 24, countdown = 0;
    bool initialized = false;
};

class Engine
{
public:
    void setRates(Rates next) noexcept
    {
        if (std::abs(next.base - rates.base) + std::abs(next.core - rates.core) <= 0) return;
        rates = next;
        for (auto& band : bands) band.prepare(next);
    }
    void reset() noexcept { for (auto& band : bands) band.reset(); }
    bool setTargets(bool enabled, const std::array<BandSettings, bandCount>& settings) noexcept
    {
        bool active = false;
        for (std::size_t i = 0; i < bands.size(); ++i)
        {
            auto target = settings[i];
            target.enabled = enabled && target.enabled;
            bands[i].setTarget(target);
            active = active || !bands[i].idle();
        }
        return active;
    }
    Stereo process(Stereo input) noexcept
    {
        for (auto& band : bands)
            if (!band.idle()) input = band.process(input);
        return input;
    }
private:
    Rates rates;
    std::array<Band, bandCount> bands;
};
} // namespace breach::eq
