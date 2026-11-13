#pragma once
#include <atomic>
#include <array>
#include <cstdint>

namespace breach
{
struct VisualTelemetry
{
    static constexpr int kMaxVoices = 8;

    std::atomic<float> inputRmsL{ 0.0f };
    std::atomic<float> inputRmsR{ 0.0f };
    std::atomic<float> inputPeak{ 0.0f };
    std::atomic<uint32_t> transientPulseCount{ 0 };

    std::array<std::atomic<float>, kMaxVoices> voiceEnergies{};
    std::array<std::atomic<float>, kMaxVoices> voiceFrequencies{};

    std::atomic<int> activeVoiceCount{ 6 };
    std::atomic<int> activeMode{ 0 };
    std::atomic<int> activeArpStep{ -1 };
    std::atomic<bool> midiOverrideActive{ false };

    VisualTelemetry()
    {
        for (int i = 0; i < kMaxVoices; ++i)
        {
            voiceEnergies[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
            voiceFrequencies[static_cast<size_t>(i)].store(440.0f, std::memory_order_relaxed);
        }
    }

    struct Snapshot
    {
        float inputRmsL = 0.0f;
        float inputRmsR = 0.0f;
        float inputPeak = 0.0f;
        uint32_t transientPulseCount = 0;
        std::array<float, kMaxVoices> voiceEnergies{};
        std::array<float, kMaxVoices> voiceFrequencies{};
        int activeVoiceCount = 6;
        int activeMode = 0;
        int activeArpStep = -1;
        bool midiOverrideActive = false;
    };

    Snapshot getSnapshot() const noexcept
    {
        Snapshot s;
        s.inputRmsL = inputRmsL.load(std::memory_order_relaxed);
        s.inputRmsR = inputRmsR.load(std::memory_order_relaxed);
        s.inputPeak = inputPeak.load(std::memory_order_relaxed);
        s.transientPulseCount = transientPulseCount.load(std::memory_order_relaxed);
        s.activeVoiceCount = activeVoiceCount.load(std::memory_order_relaxed);
        s.activeMode = activeMode.load(std::memory_order_relaxed);
        s.activeArpStep = activeArpStep.load(std::memory_order_relaxed);
        s.midiOverrideActive = midiOverrideActive.load(std::memory_order_relaxed);

        for (int i = 0; i < kMaxVoices; ++i)
        {
            s.voiceEnergies[static_cast<size_t>(i)] = voiceEnergies[static_cast<size_t>(i)].load(std::memory_order_relaxed);
            s.voiceFrequencies[static_cast<size_t>(i)] = voiceFrequencies[static_cast<size_t>(i)].load(std::memory_order_relaxed);
        }
        return s;
    }
};
} // namespace breach
