#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace breach::impact
{
inline constexpr std::size_t voiceCount = 12;
inline constexpr auto relaxed = std::memory_order_relaxed;

struct Telemetry
{
    std::atomic<float> excitation { 0 };
    std::array<std::atomic<float>, voiceCount> voices {};
    std::atomic<float> brightness { .25f };
    std::array<std::atomic<float>, 2> output {};
    // Upper bits identify an onset. The low byte holds its strength.
    std::atomic<std::uint32_t> strike { 0 };
    std::atomic<std::uint32_t> renderedBlocks { 0 };
};

class Analysis
{
public:
    void reset(Telemetry& telemetry) noexcept
    {
        excitationPower = lowPower = highPower = 0;
        fastPower = baseline = windowPower = 0;
        windowPosition = cooldown = capture = quietWindows = 0;
        strikeStrength = 0;
        armed = true;
        lowState.fill(0);
        voicePower.fill(0);
        outputPower.fill(0);
        publishCore(telemetry);
        publishOutput(telemetry);
    }

    void setRates(double coreRate, double outputRate) noexcept
    {
        if (std::abs(rate - coreRate) > .01)
        {
            rate = coreRate;
            envelopeStep = step(coreRate, .025);
            splitStep = step(coreRate, 1.0 / (6.283185307179586 * 1200.0));
            windowSize = std::max(1, static_cast<int>(coreRate * .001 + .5));
            windowPosition = 0;
            windowPower = 0;
        }
        if (std::abs(baseRate - outputRate) > .01)
        {
            baseRate = outputRate;
            outputStep = step(outputRate, .04);
        }
    }

    void observeExcitation(float left, float right) noexcept
    {
        const float power = stereoPower(left, right);
        excitationPower += envelopeStep * (power - excitationPower);
        windowPower += power;
        if (++windowPosition == windowSize)
            detectAttack();
    }

    void observeVoice(std::size_t slot, float left, float right) noexcept
    {
        auto& power = voicePower[slot];
        power += envelopeStep * (stereoPower(left, right) - power);
    }

    void decaySilentVoices(std::size_t first, int samples) noexcept
    {
        if (first >= voiceCount) return;
        const float retain = std::pow(1.0f - envelopeStep, static_cast<float>(samples));
        for (auto slot = first; slot < voiceCount; ++slot)
            voicePower[slot] *= retain;
    }

    void observeResonance(float left, float right) noexcept
    {
        lowState[0] += splitStep * (left - lowState[0]);
        lowState[1] += splitStep * (right - lowState[1]);
        lowPower += envelopeStep * (stereoPower(lowState[0], lowState[1]) - lowPower);
        highPower += envelopeStep * (stereoPower(left - lowState[0], right - lowState[1]) - highPower);
    }

    void observeOutput(float left, float right) noexcept
    {
        outputPower[0] += outputStep * (left * left - outputPower[0]);
        outputPower[1] += outputStep * (right * right - outputPower[1]);
    }

    void publishCore(Telemetry& telemetry) const noexcept
    {
        telemetry.excitation.store(std::sqrt(excitationPower), relaxed);
        const float total = lowPower + highPower;
        telemetry.brightness.store(total > 1.0e-10f ? highPower / total : .25f, relaxed);
        for (std::size_t slot = 0; slot < voiceCount; ++slot)
            telemetry.voices[slot].store(std::sqrt(voicePower[slot]), relaxed);
        telemetry.strike.store((strikeNumber << 8) | strikeStrength, relaxed);
    }

    void publishOutput(Telemetry& telemetry) const noexcept
    {
        for (std::size_t channel = 0; channel < outputPower.size(); ++channel)
            telemetry.output[channel].store(std::sqrt(outputPower[channel]), relaxed);
        telemetry.renderedBlocks.fetch_add(1, std::memory_order_release);
    }

private:
    static float stereoPower(float left, float right) noexcept
    {
        return .5f * (left * left + right * right);
    }

    static float step(double sampleRate, double seconds) noexcept
    {
        return static_cast<float>(-std::expm1(-1.0 / (seconds * sampleRate)));
    }

    void detectAttack() noexcept
    {
        const float power = windowPower / static_cast<float>(windowSize);
        windowPower = 0;
        windowPosition = 0;

        const float previous = fastPower;
        fastPower += .30f * (power - fastPower);
        if (cooldown > 0) --cooldown;
        quietWindows = fastPower < baseline * .44f + .000035f ? std::min(quietWindows + 1, 7) : 0;
        if (quietWindows == 7) armed = true;

        // Sustained material needs a stronger edge until a quiet stretch rearms detection.
        const float novelty = armed ? 2.6f : 5.0f;
        const float rise = armed ? 1.14f : 1.28f;
        if (cooldown == 0 && fastPower > baseline * novelty + .00009f
            && fastPower > previous * rise)
        {
            strikeNumber = (strikeNumber + 1u) & 0x00ffffffu;
            strikeStrength = 1;
            cooldown = 38;
            capture = 8;
            armed = false;
        }
        if (capture > 0)
        {
            const auto level = static_cast<std::uint32_t>(std::clamp(std::sqrt(fastPower) * 300.0f, 1.0f, 255.0f));
            strikeStrength = std::max(strikeStrength, level);
            --capture;
        }
        baseline += .028f * (power - baseline);
    }

    double rate = 0, baseRate = 0;
    float envelopeStep = 0, splitStep = 0, outputStep = 0;
    float excitationPower = 0, lowPower = 0, highPower = 0;
    float fastPower = 0, baseline = 0, windowPower = 0;
    int windowSize = 1, windowPosition = 0, cooldown = 0, capture = 0, quietWindows = 0;
    bool armed = true;
    std::uint32_t strikeNumber = 0, strikeStrength = 0;
    std::array<float, 2> lowState {}, outputPower {};
    std::array<float, voiceCount> voicePower {};
};
} // namespace breach::impact
