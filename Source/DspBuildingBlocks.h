#pragma once
#include "PluginParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace breach::dsp
{
inline constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

inline float flushDenormal(float x) noexcept
{
    return (std::abs(x) < 1.0e-18f) ? 0.0f : x;
}

inline float fastTanh(float x) noexcept
{
    x = juce::jlimit(-5.0f, 5.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

class TptSvf
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = std::max(8000.0, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void setParameters(float cutoffHz, float q) noexcept
    {
        const float clampedFreq = juce::jlimit(20.0f, static_cast<float>(fs * 0.45), cutoffHz);
        const float clampedQ = juce::jlimit(0.4f, 220.0f, q);
        g = std::tan(juce::MathConstants<float>::pi * clampedFreq / static_cast<float>(fs));
        k = 1.0f / clampedQ;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float processBandpass(float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = flushDenormal(2.0f * v1 - ic1eq);
        ic2eq = flushDenormal(2.0f * v2 - ic2eq);
        return v1;
    }

    inline float processLowpass(float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = flushDenormal(2.0f * v1 - ic1eq);
        ic2eq = flushDenormal(2.0f * v2 - ic2eq);
        return v2;
    }

    inline float processHighpass(float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = flushDenormal(2.0f * v1 - ic1eq);
        ic2eq = flushDenormal(2.0f * v2 - ic2eq);
        return v0 - k * v1 - v2;
    }

private:
    double fs = 44100.0;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1eq = 0.0f, ic2eq = 0.0f;
};

class SpectralTiltFilter
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = std::max(8000.0, sampleRate);
        const float pivotHz = 1000.0f;
        const float omega = kTwoPi * pivotHz / static_cast<float>(fs);
        coeff = std::exp(-omega);
        reset();
    }

    void reset() noexcept
    {
        lpState = 0.0f;
    }

    inline float process(float input, float tiltAmount) noexcept
    {
        if (std::abs(tiltAmount) < 1.0e-4f)
            return input;

        lpState = flushDenormal(coeff * lpState + (1.0f - coeff) * input);
        const float lowBand = lpState;
        const float highBand = input - lowBand;

        const float tiltGain = juce::jlimit(-1.0f, 1.0f, tiltAmount) * 0.65f;
        const float lowGain  = std::pow(10.0f, -tiltGain * 0.30f);
        const float highGain = std::pow(10.0f,  tiltGain * 0.30f);

        return lowBand * lowGain + highBand * highGain;
    }

private:
    double fs = 44100.0;
    float coeff = 0.85f;
    float lpState = 0.0f;
};

// Fractional Comb Filter with Stiffness Dispersion Allpass (metallic string dispersion)
class FractionalCombFilter
{
public:
    void prepare(double sampleRate)
    {
        fs = std::max(8000.0, sampleRate);
        const int minSamples = static_cast<int>(std::ceil(fs / 18.0)) + 16;
        int size = 1;
        while (size < minSamples)
            size <<= 1;

        buffer.assign(static_cast<size_t>(size), 0.0f);
        mask = size - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
        dampState = 0.0f;
        dispState1 = 0.0f;
        dispState2 = 0.0f;
    }

    inline float process(float input, float freqHz, float decay, float damping, float stiffness) noexcept
    {
        if (buffer.empty())
            return input;

        const float clampedFreq = juce::jlimit(22.0f, static_cast<float>(fs * 0.42), freqHz);
        const float dampCoeff = juce::jlimit(0.05f, 0.95f, 1.0f - damping * 0.82f);
        const float phaseComp = (1.0f - dampCoeff) / (1.0f + dampCoeff);
        const float delaySamples = juce::jlimit(2.0f, static_cast<float>(mask - 4),
                                                static_cast<float>(fs) / clampedFreq - phaseComp);

        const int intDelay = static_cast<int>(delaySamples);
        const float frac = delaySamples - static_cast<float>(intDelay);

        const float y_1 = buffer[static_cast<size_t>((writeIdx - intDelay + 1) & mask)];
        const float y0  = buffer[static_cast<size_t>((writeIdx - intDelay)     & mask)];
        const float y1  = buffer[static_cast<size_t>((writeIdx - intDelay - 1) & mask)];
        const float y2  = buffer[static_cast<size_t>((writeIdx - intDelay - 2) & mask)];

        const float c_1 = -frac * (frac - 1.0f) * (frac - 2.0f) / 6.0f;
        const float c0  =  (frac + 1.0f) * (frac - 1.0f) * (frac - 2.0f) / 2.0f;
        const float c1  = -(frac + 1.0f) * frac * (frac - 2.0f) / 2.0f;
        const float c2  =  (frac + 1.0f) * frac * (frac - 1.0f) / 6.0f;

        float delayed = c_1 * y_1 + c0 * y0 + c1 * y1 + c2 * y2;

        // Stiffness dispersion allpass stages stretch upper harmonic partials (metallic bar/wire)
        if (stiffness > 0.001f)
        {
            const float a = juce::jlimit(0.0f, 0.72f, stiffness * 0.68f);
            const float ap1 = -a * delayed + dispState1;
            dispState1 = flushDenormal(delayed + a * ap1);
            const float ap2 = -a * ap1 + dispState2;
            dispState2 = flushDenormal(ap1 + a * ap2);
            delayed = ap2;
        }

        dampState = flushDenormal(dampState + dampCoeff * (delayed - dampState));

        const float feedback = juce::jlimit(0.0f, 0.994f, 0.72f + 0.274f * std::pow(decay, 0.45f));
        const float inputScale = 1.0f - feedback * 0.82f;

        const float nextSample = flushDenormal(input * inputScale + dampState * feedback);
        buffer[static_cast<size_t>(writeIdx)] = nextSample;
        writeIdx = (writeIdx + 1) & mask;

        return delayed;
    }

private:
    double fs = 44100.0;
    std::vector<float> buffer;
    int mask = 0;
    int writeIdx = 0;
    float dampState = 0.0f;
    float dispState1 = 0.0f;
    float dispState2 = 0.0f;
};

class AllpassDiffuser
{
public:
    void prepare(double sampleRate, float maxDelayMs)
    {
        fs = std::max(8000.0, sampleRate);
        const int minSamples = static_cast<int>(std::ceil(fs * maxDelayMs * 0.001)) + 8;
        int size = 1;
        while (size < minSamples)
            size <<= 1;

        buffer.assign(static_cast<size_t>(size), 0.0f);
        mask = size - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

    inline float process(float input, float delaySamples, float g) noexcept
    {
        if (buffer.empty())
            return input;

        const float clampedDelay = juce::jlimit(1.0f, static_cast<float>(mask - 2), delaySamples);
        const int idx0 = static_cast<int>(clampedDelay);
        const float frac = clampedDelay - static_cast<float>(idx0);

        const float s0 = buffer[static_cast<size_t>((writeIdx - idx0) & mask)];
        const float s1 = buffer[static_cast<size_t>((writeIdx - idx0 - 1) & mask)];
        const float delayed = s0 + frac * (s1 - s0);

        const float clampedG = juce::jlimit(-0.85f, 0.85f, g);
        const float v = flushDenormal(input + clampedG * delayed);
        buffer[static_cast<size_t>(writeIdx)] = v;
        writeIdx = (writeIdx + 1) & mask;

        return delayed - clampedG * v;
    }

private:
    double fs = 44100.0;
    std::vector<float> buffer;
    int mask = 0;
    int writeIdx = 0;
};

class HilbertFrequencyShifter
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = std::max(8000.0, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : pathA) s.reset();
        for (auto& s : pathB) s.reset();
        prevA = 0.0f;
        phase = 0.0;
    }

    inline float process(float input, float shiftHz) noexcept
    {
        if (std::abs(shiftHz) < 0.05f)
            return input;

        float a = input;
        for (size_t i = 0; i < 4; ++i)
            a = pathA[i].process(a, kCoeffsA[i]);

        const float I = prevA;
        prevA = a;

        float Q = input;
        for (size_t i = 0; i < 4; ++i)
            Q = pathB[i].process(Q, kCoeffsB[i]);

        const float cosP = static_cast<float>(std::cos(phase));
        const float sinP = static_cast<float>(std::sin(phase));

        phase += (juce::MathConstants<double>::twoPi * static_cast<double>(shiftHz)) / fs;
        if (phase > juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
        else if (phase < -juce::MathConstants<double>::twoPi)
            phase += juce::MathConstants<double>::twoPi;

        return I * cosP - Q * sinP;
    }

private:
    struct Allpass2nd
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

        inline float process(float x, float c) noexcept
        {
            const float y = flushDenormal(c * (x - y2) + x2);
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    static constexpr std::array<float, 4> kCoeffsA{
        0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f
    };
    static constexpr std::array<float, 4> kCoeffsB{
        0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f
    };

    double fs = 44100.0;
    std::array<Allpass2nd, 4> pathA{};
    std::array<Allpass2nd, 4> pathB{};
    float prevA = 0.0f;
    double phase = 0.0;
};

class MotionLfoBank
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = std::max(8000.0, sampleRate);
        phase = 0.0f;
        shValues.fill(0.0f);
    }

    void advance(float rateHz) noexcept
    {
        const float inc = juce::jlimit(0.01f, 50.0f, rateHz) / static_cast<float>(fs);
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            for (auto& val : shValues)
                val = rng.nextFloat() * 2.0f - 1.0f;
        }
    }

    inline float getVoiceValue(int voiceIndex, int totalVoices, MotionShape shape, float phaseOffsetNorm = 0.0f) const noexcept
    {
        const float voiceSpread = (totalVoices > 1)
            ? (static_cast<float>(voiceIndex) / static_cast<float>(totalVoices)) * 0.65f
            : 0.0f;
        float p = phase + voiceSpread + phaseOffsetNorm;
        p -= std::floor(p);

        switch (shape)
        {
            case MotionShape::Sine:
                return std::sin(kTwoPi * p);
            case MotionShape::Triangle:
                return 1.0f - 4.0f * std::abs(std::round(p - 0.25f) - (p - 0.25f));
            case MotionShape::Square:
                return (p < 0.5f) ? 1.0f : -1.0f;
            case MotionShape::Random:
                return shValues[static_cast<size_t>(voiceIndex % 12)];
            case MotionShape::Saw:
                return 2.0f * p - 1.0f;
            default:
                return 0.0f;
        }
    }

private:
    double fs = 44100.0;
    float phase = 0.0f;
    std::array<float, 12> shValues{};
    juce::Random rng{ 0x51A7E };
};

class EnvelopeFollower
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = std::max(8000.0, sampleRate);
        env = 0.0f;
    }

    void reset() noexcept { env = 0.0f; }

    inline float process(float monoSample, float attackMs, float releaseMs) noexcept
    {
        const float rect = std::abs(monoSample);
        const float attCoeff = std::exp(-1.0f / (std::max(0.5f, attackMs) * 0.001f * static_cast<float>(fs)));
        const float relCoeff = std::exp(-1.0f / (std::max(5.0f, releaseMs) * 0.001f * static_cast<float>(fs)));

        if (rect > env)
            env = flushDenormal(attCoeff * env + (1.0f - attCoeff) * rect);
        else
            env = flushDenormal(relCoeff * env + (1.0f - relCoeff) * rect);

        return juce::jlimit(0.0f, 1.0f, env * 2.5f);
    }

private:
    double fs = 44100.0;
    float env = 0.0f;
};
} // namespace breach::dsp
