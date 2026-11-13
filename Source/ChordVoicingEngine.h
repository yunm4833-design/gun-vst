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
    static constexpr int kMaxVoices = VisualTelemetry::kMaxVoices;

    ChordVoicingEngine()
    {
        heldMidiNotes.fill(false);
    }

    void resetMidi() noexcept
    {
        heldMidiNotes.fill(false);
        activeMidiCount = 0;
    }

    void handleMidiBuffer(const juce::MidiBuffer& midiMessages) noexcept
    {
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
    }

    bool hasActiveMidiNotes() const noexcept
    {
        return activeMidiCount > 0;
    }

    void computeVoices(int rootNote,
                       ScaleType scale,
                       ChordType chord,
                       int voiceCount,
                       int octaveSpan,
                       float octaveSpread,
                       float detuneCents,
                       float stereoWidth,
                       bool midiOverrideEnabled,
                       std::array<VoicePitchSpec, kMaxVoices>& outSpecs) const noexcept
    {
        const int clampedVoices = juce::jlimit(4, kMaxVoices, voiceCount);
        std::array<float, kMaxVoices> midiPitches{};

        if (midiOverrideEnabled && activeMidiCount > 0)
        {
            std::array<int, 32> pressed{};
            int count = 0;
            for (int n = 0; n < 128 && count < 32; ++n)
            {
                if (heldMidiNotes[static_cast<size_t>(n)])
                    pressed[static_cast<size_t>(count++)] = n;
            }

            for (int v = 0; v < clampedVoices; ++v)
            {
                const int baseNote = pressed[static_cast<size_t>(v % count)];
                const int octShift = (v / count) * 12;
                midiPitches[static_cast<size_t>(v)] = static_cast<float>(juce::jlimit(24, 115, baseNote + octShift));
            }
        }
        else
        {
            std::array<int, 16> intervals{};
            int intervalCount = buildChordIntervals(scale, chord, intervals);

            const int baseRootMidi = 48 + juce::jlimit(0, 11, rootNote);
            const int maxOctaves = juce::jlimit(1, 3, octaveSpan);

            for (int v = 0; v < clampedVoices; ++v)
            {
                const int degreeIdx = v % intervalCount;
                const int rawOctave = v / intervalCount;
                const int effectiveOctave = static_cast<int>(std::floor(
                    static_cast<float>(rawOctave % maxOctaves) * (0.35f + 0.65f * octaveSpread)));

                const int midiNote = baseRootMidi + intervals[static_cast<size_t>(degreeIdx)] + effectiveOctave * 12;
                midiPitches[static_cast<size_t>(v)] = static_cast<float>(juce::jlimit(28, 112, midiNote));
            }
        }

        for (int v = 0; v < kMaxVoices; ++v)
        {
            if (v >= clampedVoices)
            {
                outSpecs[static_cast<size_t>(v)] = { 440.0f, 0.0f, 0.0f };
                continue;
            }

            const float normPos = (clampedVoices > 1)
                ? (static_cast<float>(v) / static_cast<float>(clampedVoices - 1)) * 2.0f - 1.0f
                : 0.0f;

            const float alternatingSign = (v % 2 == 0) ? -1.0f : 1.0f;
            const float stereoPos = alternatingSign * (0.25f + 0.75f * std::abs(normPos)) * stereoWidth;

            const float centOffset = normPos * detuneCents;
            const float exactMidi = midiPitches[static_cast<size_t>(v)] + (centOffset / 100.0f);
            const float freq = 440.0f * std::pow(2.0f, (exactMidi - 69.0f) / 12.0f);

            const float angle = (stereoPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            outSpecs[static_cast<size_t>(v)].frequencyHz = freq;
            outSpecs[static_cast<size_t>(v)].panL = std::cos(angle);
            outSpecs[static_cast<size_t>(v)].panR = std::sin(angle);
        }
    }

private:
    static int buildChordIntervals(ScaleType scale, ChordType chord, std::array<int, 16>& outIntervals) noexcept
    {
        auto setNotes = [&](std::initializer_list<int> list) -> int
        {
            int idx = 0;
            for (int val : list)
                if (idx < 16) outIntervals[static_cast<size_t>(idx++)] = val;
            return idx;
        };

        switch (chord)
        {
            case ChordType::UnisonOctaves: return setNotes({ 0, 12, 24, 7 });
            case ChordType::Major:         return setNotes({ 0, 4, 7, 12 });
            case ChordType::Minor:         return setNotes({ 0, 3, 7, 12 });
            case ChordType::Major7:        return setNotes({ 0, 4, 7, 11 });
            case ChordType::Minor7:        return setNotes({ 0, 3, 7, 10 });
            case ChordType::Dominant7:     return setNotes({ 0, 4, 7, 10 });
            case ChordType::Sus2:          return setNotes({ 0, 2, 7, 12 });
            case ChordType::Sus4:          return setNotes({ 0, 5, 7, 12 });
            case ChordType::Add9:          return setNotes({ 0, 4, 7, 14 });
            case ChordType::Dim7:          return setNotes({ 0, 3, 6, 9 });
            case ChordType::ScaleCluster:
            default:
            {
                switch (scale)
                {
                    case ScaleType::Major:           return setNotes({ 0, 2, 4, 5, 7, 9, 11 });
                    case ScaleType::NaturalMinor:    return setNotes({ 0, 2, 3, 5, 7, 8, 10 });
                    case ScaleType::HarmonicMinor:   return setNotes({ 0, 2, 3, 5, 7, 8, 11 });
                    case ScaleType::Dorian:          return setNotes({ 0, 2, 3, 5, 7, 9, 10 });
                    case ScaleType::Mixolydian:      return setNotes({ 0, 2, 4, 5, 7, 9, 10 });
                    case ScaleType::PentatonicMajor: return setNotes({ 0, 2, 4, 7, 9 });
                    case ScaleType::PentatonicMinor: return setNotes({ 0, 3, 5, 7, 10 });
                    case ScaleType::Chromatic:
                    default:                         return setNotes({ 0, 1, 2, 3, 4, 5, 6, 7 });
                }
            }
        }
    }

    std::array<bool, 128> heldMidiNotes{};
    int activeMidiCount = 0;
};
} // namespace breach::dsp
