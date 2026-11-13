#pragma once
#include "PluginParameters.h"
#include "VisualTelemetry.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace breach::dsp
{
struct VoicePitchSpec
{
    float frequencyHz = 440.0f;
    float panL = 0.707f;
    float panR = 0.707f;
};

class ChordVoicingEngine
{
public:
    static constexpr int kMaxVoices = breach::kMaxVoices;

    ChordVoicingEngine()
    {
        heldMidiNotes.fill(false);
    }

    void resetMidi() noexcept
    {
        heldMidiNotes.fill(false);
        activeMidiCount = 0;
    }

    bool handleMidiBuffer(const juce::MidiBuffer& midiMessages) noexcept
    {
        bool anyNoteOn = false;
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                const int note = juce::jlimit(0, 127, msg.getNoteNumber());
                if (!heldMidiNotes[static_cast<size_t>(note)])
                {
                    heldMidiNotes[static_cast<size_t>(note)] = true;
                    ++activeMidiCount;
                }
                anyNoteOn = true;
            }
            else if (msg.isNoteOff())
            {
                const int note = juce::jlimit(0, 127, msg.getNoteNumber());
                if (heldMidiNotes[static_cast<size_t>(note)])
                {
                    heldMidiNotes[static_cast<size_t>(note)] = false;
                    activeMidiCount = std::max(0, activeMidiCount - 1);
                }
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                resetMidi();
            }
        }
        return anyNoteOn;
    }

    bool hasActiveMidiNotes() const noexcept
    {
        return activeMidiCount > 0;
    }

    int computeVoices(const std::array<bool, 12>& pitchClasses,
                      int rootNote,
                      int baseOctave,
                      int spreadOctaves,
                      bool midiModeEnabled,
                      float warpAmount,
                      float stiffnessAmount,
                      int maxActiveVoices,
                      const std::array<float, kMaxVoices>& perVoicePan,
                      const std::array<float, kMaxVoices>& perVoiceDetuneCents,
                      std::array<VoicePitchSpec, kMaxVoices>& outSpecs,
                      uint16_t& outPitchClassMask) const noexcept
    {
        const int voiceCap = juce::jlimit(1, kMaxVoices, maxActiveVoices);
        std::array<float, kMaxVoices> rawFreqs{};
        int activeCount = 0;
        uint16_t mask = 0;

        if (midiModeEnabled)
        {
            if (activeMidiCount > 0)
            {
                std::array<int, 32> pressed{};
                int count = 0;
                for (int n = 0; n < 128 && count < 32; ++n)
                {
                    if (heldMidiNotes[static_cast<size_t>(n)])
                    {
                        pressed[static_cast<size_t>(count++)] = n;
                        mask |= static_cast<uint16_t>(1u << (n % 12));
                    }
                }

                activeCount = std::min(voiceCap, count * juce::jlimit(1, 3, spreadOctaves));
                activeCount = std::max(1, activeCount);
                for (int v = 0; v < activeCount; ++v)
                {
                    const int baseNote = pressed[static_cast<size_t>(v % count)];
                    const int octShift = (v / count) * 12;
                    const int midi = juce::jlimit(24, 115, baseNote + octShift);
                    rawFreqs[static_cast<size_t>(v)] = 440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
                }
            }
            else
            {
                activeCount = 0;
            }
        }
        else
        {
            std::array<int, 12> activeSemis{};
            int numSemis = 0;
            for (int pc = 0; pc < 12; ++pc)
            {
                if (pitchClasses[static_cast<size_t>(pc)])
                {
                    activeSemis[static_cast<size_t>(numSemis++)] = pc;
                    const int transposedPc = (pc + juce::jlimit(0, 11, rootNote)) % 12;
                    mask |= static_cast<uint16_t>(1u << transposedPc);
                }
            }

            if (numSemis == 0)
            {
                activeSemis[0] = 0;
                numSemis = 1;
                mask = static_cast<uint16_t>(1u << (juce::jlimit(0, 11, rootNote) % 12));
            }

            const int clampedOctave = juce::jlimit(1, 6, baseOctave);
            const int clampedSpread = juce::jlimit(1, 4, spreadOctaves);
            const int baseRootMidi = (clampedOctave + 1) * 12 + juce::jlimit(0, 11, rootNote);

            std::array<int, 48> pool{};
            int poolSize = 0;
            for (int oct = 0; oct < clampedSpread; ++oct)
            {
                for (int s = 0; s < numSemis; ++s)
                {
                    if (poolSize < 48)
                        pool[static_cast<size_t>(poolSize++)] = juce::jlimit(24, 116, baseRootMidi + activeSemis[static_cast<size_t>(s)] + oct * 12);
                }
            }

            activeCount = std::min(voiceCap, poolSize);
            for (int v = 0; v < activeCount; ++v)
            {
                const int poolIdx = (poolSize <= activeCount)
                    ? v
                    : static_cast<int>((static_cast<float>(v) / static_cast<float>(activeCount)) * static_cast<float>(poolSize));
                const int midi = pool[static_cast<size_t>(juce::jlimit(0, poolSize - 1, poolIdx))];
                rawFreqs[static_cast<size_t>(v)] = 440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
            }
        }

        outPitchClassMask = mask;

        const float f0 = (activeCount > 0) ? rawFreqs[0] : 220.0f;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            if (v >= activeCount)
            {
                outSpecs[static_cast<size_t>(v)] = { 440.0f, 0.0f, 0.0f };
                continue;
            }

            float freq = rawFreqs[static_cast<size_t>(v)];
            if (std::abs(warpAmount) > 1.0e-4f && v > 0)
            {
                const float ratio = std::max(1.0f, freq / f0);
                if (warpAmount > 0.0f)
                {
                    const float stretched = std::pow(ratio, 1.0f + 0.42f * warpAmount)
                                          + warpAmount * 0.14f * static_cast<float>(v) * std::sin(static_cast<float>(v) * 1.6180339f);
                    freq = f0 * std::max(1.0f, stretched);
                }
                else
                {
                    const float compressed = std::pow(ratio, 1.0f + 0.55f * warpAmount);
                    freq = f0 * std::max(1.0f, compressed);
                }
            }

            // Stage 2 Stiffness: metallic partial stretching f_n = f * sqrt(1 + B * n^2)
            if (stiffnessAmount > 0.001f && v > 0)
            {
                const float n = static_cast<float>(v + 1);
                const float B = stiffnessAmount * 0.018f;
                freq *= std::sqrt(1.0f + B * n * n);
            }

            // Per-voice Detune (+/- 50 cents)
            const float detuneMul = std::pow(2.0f, perVoiceDetuneCents[static_cast<size_t>(v)] / 1200.0f);
            freq = juce::jlimit(24.0f, 16000.0f, freq * detuneMul);

            // Per-voice Pan [-1, +1] -> Equal-Power Pan Law
            const float pan = juce::jlimit(-1.0f, 1.0f, perVoicePan[static_cast<size_t>(v)]);
            const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            outSpecs[static_cast<size_t>(v)].frequencyHz = freq;
            outSpecs[static_cast<size_t>(v)].panL = std::cos(angle);
            outSpecs[static_cast<size_t>(v)].panR = std::sin(angle);
        }

        return activeCount;
    }

private:
    std::array<bool, 128> heldMidiNotes{};
    int activeMidiCount = 0;
};
} // namespace breach::dsp
