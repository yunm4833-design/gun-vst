#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace breach::nonlinear
{
// Independently implemented integrated waveshaping. A piecewise-linear curve
// has an exact piecewise-quadratic antiderivative; no runtime quadrature/LUT
// allocation and no external DSP library dependency.
class Curve
{
public:
    enum class Shape { Tanh, Sniper };
    static double evaluate(double x, Shape shape) noexcept
    {
        if (shape == Shape::Tanh) return std::tanh(x);
        // Retain quadratic asymmetry around zero, but bound the bend so the
        // negative half never folds upward at large input amplitudes.
        const double bend = 0.48 * (1.0 - 1.0 / (1.0 + 0.25 * x * x));
        return std::tanh(1.6 * x + bend);
    }
    explicit Curve(Shape shape)
    {
        for (size_t i = 0; i <= intervals; ++i)
        {
            values[i] = evaluate(-extent + static_cast<double>(i) * step, shape);
            if (i > 0) integrals[i] = integrals[i-1] + (values[i-1] + values[i]) * (0.5 * step);
        }
    }
    double value(double x) const noexcept
    {
        if (x <= -extent) return values.front();
        if (x >= extent) return values.back();
        const auto index = static_cast<size_t>((x + extent) / step);
        const double fraction = (x + extent) / step - static_cast<double>(index);
        return values[index] + fraction * (values[index+1] - values[index]);
    }
    double integral(double x) const noexcept
    {
        if (x <= -extent) return (x + extent) * values.front();
        if (x >= extent) return integrals.back() + (x - extent) * values.back();
        const auto index = static_cast<size_t>((x + extent) / step);
        const double distance = x + extent - static_cast<double>(index) * step;
        return integrals[index] + values[index] * distance
            + (values[index+1] - values[index]) * distance * distance / (2.0 * step);
    }
private:
    static constexpr size_t intervals = 8192;
    static constexpr double extent = 16.0, step = extent * 2.0 / static_cast<double>(intervals);
    std::array<double, intervals + 1> values {}, integrals {};
};

// First-order ADAA followed by a half-sample averaging filter: nominal total
// delay is one processing sample. This reduces aliases but also rolls off the
// top end; 2X/4X reduces that coloration. It is not a transparent delay filter.
class ADAA
{
public:
    void reset() noexcept { previousInput = previousOutput = 0.0; }
    float process(float input, const Curve& curve) noexcept
    {
        const double x = input, delta = x - previousInput;
        const double average = std::abs(delta) < 1.0e-4
            ? curve.value((x + previousInput) * 0.5)
            : (curve.integral(x) - curve.integral(previousInput)) / delta;
        const double output = (average + previousOutput) * 0.5;
        previousInput = x; previousOutput = average;
        return static_cast<float>(output);
    }
private:
    double previousInput = 0.0, previousOutput = 0.0;
};

class DCBlocker
{
public:
    void prepare(double rate) noexcept { pole = std::exp(-2.0 * 3.14159265358979323846 * 5.0 / rate); reset(); }
    void reset() noexcept { previousInput = previousOutput = 0.0; }
    float process(float x) noexcept
    {
        const double y = (1.0 + pole) * 0.5 * (x - previousInput) + pole * previousOutput;
        previousInput = x; previousOutput = y; return static_cast<float>(y);
    }
private:
    double pole = 0.999, previousInput = 0.0, previousOutput = 0.0;
};

class FixedDelay
{
public:
    void reset() noexcept { samples.fill(0.0f); index = 0; }
    float process(float x, size_t length) noexcept
    {
        if (length == 0) return x;
        const float y = samples[index]; samples[index] = x;
        if (++index == length) index = 0;
        return y;
    }
private:
    std::array<float, 8> samples {};
    size_t index = 0;
};

class Engine
{
public:
    void prepare(double rate, int factor) noexcept
    {
        padding = static_cast<size_t>(2 * factor - 2);
        for (auto& voice : voices) for (auto& channel : voice) channel.dc.prepare(rate);
        reset();
    }
    void reset() noexcept
    {
        for (auto& c : excitation) c.reset();
        for (auto& voice : voices) for (auto& c : voice) { c.shaper.reset(); c.dc.reset(); c.delay.reset(); }
        for (auto& c : dryDelay) c.reset();
        for (auto& c : outputDelay) c.reset();
    }
    float excite(float x, float drive, size_t channel) noexcept
    {
        // Moderate level compensation avoids forcing every driven input into
        // the resonator/output limiter at approximately the same amplitude.
        const float makeup = 1.0f / std::sqrt(1.0f + 0.25f * (drive - 1.0f));
        return excitation[channel].process(x * drive, tanhCurve) * makeup;
    }
    float voice(float x, size_t voiceIndex, size_t channel, bool sniper) noexcept
    {
        auto& v = voices[voiceIndex][channel];
        const float saturated = v.dc.process(v.shaper.process(x, sniperCurve)) * 0.8f;
        const float delayed = v.delay.process(x, 1);
        return sniper ? saturated : delayed;
    }
    float dry(float x, size_t channel) noexcept { return dryDelay[channel].process(x, 2); }
    float output(float x, size_t channel) noexcept { return outputDelay[channel].process(x, padding); }
private:
    struct Voice { ADAA shaper; DCBlocker dc; FixedDelay delay; };
    Curve tanhCurve { Curve::Shape::Tanh }, sniperCurve { Curve::Shape::Sniper };
    std::array<ADAA, 2> excitation;
    std::array<std::array<Voice, 2>, 12> voices;
    std::array<FixedDelay, 2> dryDelay, outputDelay;
    size_t padding = 0;
};
} // namespace breach::nonlinear
