// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// BREACH adaptation; see ThirdParty/Rings/PROVENANCE.md.
#pragma once
#include "ThirdParty/Rings/RingsPrimitives.h"
#include <array>
#include <complex>

namespace breach::physical
{
constexpr double pi = 3.14159265358979323846;

// Adapted Rings String subset: excitation into a damped fractional-delay string,
// optional allpass dispersion, and a second pickup tap. No MCU resource tables,
// global RNG, VCV Rack runtime, or low-pitch resampler is needed in this port.
class String
{
public:
    void prepare(double maxSampleRate)
    {
        const auto capacity = static_cast<size_t>(std::ceil(maxSampleRate / 20.0)) + 32;
        string.Prepare(capacity); stretch.Prepare(capacity); reset();
    }
    void setSampleRate(double rate) noexcept
    {
        sampleRate = rate; controlInterval = std::max(1, static_cast<int>(std::lround(rate / 3000.0)));
        delaySmoothing = static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * rate)));
        reset();
    }
    void reset() noexcept
    {
        string.Reset(); stretch.Reset(); damping.Init(); countdown = 0;
        lowpassState = 0.0f; currentDelay = targetDelay = 100.0f; initialized = false;
    }
    float process(float input, float frequency, float decay, float absorption, float stiffness, double ringSeconds = 0) noexcept
    {
        frequency = std::clamp(frequency, 25.0f, static_cast<float>(sampleRate * 0.15));
        if (countdown-- <= 0)
        {
            configure(frequency, decay, absorption, stiffness, ringSeconds);
            countdown = controlInterval - 1;
        }
        currentDelay += delaySmoothing * (targetDelay - currentDelay);
        // Adapted from rings::String::ProcessInternal: split the propagation
        // delay and put a dispersive allpass in the second segment.
        float s = string.ReadHermite(std::max(2.0f, currentDelay));
        if (apDelay >= 4) s = stretch.Allpass(s, apDelay, apGain);
        const float pickup = string.Read(std::max(1.0f, currentDelay * 0.27f));
        s += input * 0.18f;
        s = damping.Process(s);
        lowpassState += lowpassAlpha * (s - lowpassState);
        // A safety ceiling is only active for overload, not the normal loss model.
        string.Write(std::clamp(lowpassState, -8.0f, 8.0f));
        // Difference pickup supplies position-dependent spectral zeros.
        return 0.8f * (s - pickup * 0.65f);
    }
    float delayForTest() const noexcept { return targetDelay; }
private:
    void configure(float frequency, float decay, float absorption, float stiffness, double ringSeconds = 0) noexcept
    {
        const double period = sampleRate / frequency;
        const float bright = 1.0f - std::clamp(absorption, 0.0f, 1.0f);
        // Physical seconds replace the old pitch-dependent feedback knob.
        const double rt60 = ringSeconds > 0 ? ringSeconds : 0.12 * std::pow(30.0, std::clamp(static_cast<double>(decay), 0.0, 1.0));
        const double targetGain = std::exp(-std::log(1000.0) / (rt60 * frequency));
        const double cutoff = std::min(sampleRate * 0.35,
            std::max(1000.0, static_cast<double>(frequency) * (4.0 + 36.0 * bright * bright)));
        lowpassAlpha = static_cast<float>(1.0 - std::exp(-2.0 * pi * cutoff / sampleRate));
        const float brightness = 0.75f + 0.25f * bright;
        const double omega = 2.0 * pi * frequency / sampleRate;
        const auto z = std::polar(1.0, -omega);
        const auto lp = static_cast<double>(lowpassAlpha) / (1.0 - (1.0 - lowpassAlpha) * z);
        const double firMagnitude = (1.0 + brightness) * 0.5 + (1.0 - brightness) * 0.5 * std::cos(omega);
        const float gain = static_cast<float>(std::min(0.99997, targetGain / (firMagnitude * std::abs(lp))));
        damping.Configure(gain, brightness, static_cast<size_t>(controlInterval));
        // Rings dispersion topology, with explicit fundamental phase correction.
        const float dispersion = std::clamp(stiffness, 0.0f, 1.0f) * 0.65f;
        const float stretchPoint = dispersion * (2.0f - dispersion) * 0.25f;
        apDelay = static_cast<size_t>(period * stretchPoint);
        apGain = -0.618f * dispersion / (0.15f + dispersion);
        double apPhaseDelay = 0.0;
        if (apDelay >= 4)
        {
            const auto delay = std::polar(1.0, -omega * static_cast<double>(apDelay));
            const auto response = (delay - static_cast<double>(apGain)) / (1.0 - static_cast<double>(apGain) * delay);
            apPhaseDelay = -std::arg(response) / omega;
        }
        const double lpPhaseDelay = -std::arg(lp) / omega;
        targetDelay = static_cast<float>(std::max(2.0, period - 1.0 - lpPhaseDelay - apPhaseDelay));
        if (!initialized) { currentDelay = targetDelay; initialized = true; }
    }
    ringsport::DelayLine string, stretch;
    ringsport::DampingFilter damping;
    double sampleRate = 48000.0;
    int controlInterval = 16, countdown = 0;
    float lowpassAlpha = 0.5f, lowpassState = 0.0f;
    float currentDelay = 100.0f, targetDelay = 100.0f, delaySmoothing = 0.02f, apGain = 0.0f;
    size_t apDelay = 0;
    bool initialized = false;
};

class Bank
{
public:
    void prepare(double maxSampleRate) { for (auto& voice : voices) for (auto& channel : voice) channel.prepare(maxSampleRate); }
    void selectRate(double rate) noexcept
    {
        if (rate == activeRate) return;
        activeRate = rate;
        for (auto& voice : voices) for (auto& channel : voice) channel.setSampleRate(rate);
    }
    void reset() noexcept { for (auto& voice : voices) for (auto& channel : voice) channel.reset(); }
    float process(size_t voice, size_t channel, float input, float frequency, float decay, float absorption, float stiffness, double ringSeconds = 0) noexcept
    {
        return voices[voice][channel].process(input, frequency, decay, absorption, stiffness, ringSeconds);
    }
private:
    std::array<std::array<String, 2>, 12> voices;
    double activeRate = 0.0;
};
} // namespace breach::physical
