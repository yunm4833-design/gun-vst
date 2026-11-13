#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace breach
{
enum class ResonatorMode : int
{
    CleanBandpass = 0, // A: Clean Bandpass (Pistol)
    CombString    = 1, // B: Comb String (Shotgun)
    DualFormant   = 2, // C: Dual Formant (SMG)
    DistortedRes  = 3, // D: Distorted Res (Sniper)
    LongTailPad   = 4, // E: Long-Tail Pad (Revolver)
    NumModes      = 5
};

enum class ScaleType : int
{
    Chromatic = 0,
    Major,
    NaturalMinor,
    HarmonicMinor,
    Dorian,
    Mixolydian,
    PentatonicMajor,
    PentatonicMinor
};

enum class ChordType : int
{
    UnisonOctaves = 0,
    Major,
    Minor,
    Major7,
    Minor7,
    Dominant7,
    Sus2,
    Sus4,
    Add9,
    Dim7,
    ScaleCluster
};

enum class ArpPattern : int
{
    Up = 0,
    Down,
    UpDown,
    Random,
    ChordPulse
};

enum class LfoShape : int
{
    Sine = 0,
    Triangle,
    Saw,
    Square,
    SampleAndHold
};

namespace ParamID
{
    inline constexpr const char* mode           = "mode";

    inline constexpr const char* rootNote       = "rootNote";
    inline constexpr const char* scaleType      = "scaleType";
    inline constexpr const char* chordType      = "chordType";
    inline constexpr const char* voiceCount     = "voiceCount";
    inline constexpr const char* octaveSpan     = "octaveSpan";
    inline constexpr const char* midiInput      = "midiInput";

    inline constexpr const char* decay          = "decay";
    inline constexpr const char* damping        = "damping";
    inline constexpr const char* tone           = "tone";

    inline constexpr const char* octaveSpread   = "octaveSpread";
    inline constexpr const char* detune         = "detune";
    inline constexpr const char* stereoWidth    = "stereoWidth";

    inline constexpr const char* drift          = "drift";
    inline constexpr const char* shiftHz        = "shiftHz";

    inline constexpr const char* arpEnabled     = "arpEnabled";
    inline constexpr const char* arpRate        = "arpRate";
    inline constexpr const char* arpPattern     = "arpPattern";
    inline constexpr const char* arpGate        = "arpGate";

    inline constexpr const char* lfo1Rate       = "lfo1Rate";
    inline constexpr const char* lfo1Shape      = "lfo1Shape";
    inline constexpr const char* lfo2Rate       = "lfo2Rate";
    inline constexpr const char* lfo2Shape      = "lfo2Shape";
    inline constexpr const char* envAttack      = "envAttack";
    inline constexpr const char* envRelease     = "envRelease";

    inline constexpr const char* lfo1ToDecay    = "lfo1ToDecay";
    inline constexpr const char* lfo1ToFilter   = "lfo1ToFilter";
    inline constexpr const char* lfo2ToDrift    = "lfo2ToDrift";
    inline constexpr const char* lfo2ToShift    = "lfo2ToShift";
    inline constexpr const char* envToDecay     = "envToDecay";
    inline constexpr const char* envToFilter    = "envToFilter";

    inline constexpr const char* hpfFreq        = "hpfFreq";
    inline constexpr const char* lpfFreq        = "lpfFreq";

    inline constexpr const char* depth          = "depth";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::mode, 1 }, "Resonator Mode",
        juce::StringArray{ "A: Clean Bandpass", "B: Comb String", "C: Dual Formant", "D: Distorted Res", "E: Long-Tail Pad" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::rootNote, 1 }, "Root Note",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::scaleType, 1 }, "Scale",
        juce::StringArray{ "Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Dorian", "Mixolydian", "Pentatonic Maj", "Pentatonic Min" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::chordType, 1 }, "Chord Type",
        juce::StringArray{ "Octaves", "Major", "Minor", "Maj7", "Min7", "Dom7", "Sus2", "Sus4", "Add9", "Dim7", "Scale Cluster" }, 4));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ ParamID::voiceCount, 1 }, "Voices", 4, 8, 6));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ ParamID::octaveSpan, 1 }, "Octave Span", 1, 3, 2));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::midiInput, 1 }, "MIDI Pitch Override", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::decay, 1 }, "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.65f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::damping, 1 }, "Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::tone, 1 }, "Tone",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::octaveSpread, 1 }, "Octave Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::detune, 1 }, "Detune (cents)",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::stereoWidth, 1 }, "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::drift, 1 }, "Drift / Smear",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::shiftHz, 1 }, "Freq Shift (Hz)",
        juce::NormalisableRange<float>(-500.0f, 500.0f, 0.1f, 0.5f, true), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::arpEnabled, 1 }, "Arp Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::arpRate, 1 }, "Arp Rate",
        juce::StringArray{ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 4));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::arpPattern, 1 }, "Arp Pattern",
        juce::StringArray{ "Up", "Down", "Up-Down", "Random", "Chord Pulse" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::arpGate, 1 }, "Arp Gate",
        juce::NormalisableRange<float>(0.05f, 1.0f, 0.001f), 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo1Rate, 1 }, "LFO 1 Rate (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.4f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::lfo1Shape, 1 }, "LFO 1 Shape",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square", "S&H" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo2Rate, 1 }, "LFO 2 Rate (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.4f), 2.4f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::lfo2Shape, 1 }, "LFO 2 Shape",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square", "S&H" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::envAttack, 1 }, "Env Attack (ms)",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f, 0.5f), 10.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::envRelease, 1 }, "Env Release (ms)",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.5f), 150.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo1ToDecay, 1 }, "LFO1 -> Decay",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo1ToFilter, 1 }, "LFO1 -> Filter",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo2ToDrift, 1 }, "LFO2 -> Drift",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lfo2ToShift, 1 }, "LFO2 -> Shift",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::envToDecay, 1 }, "Env -> Decay",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::envToFilter, 1 }, "Env -> Filter",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::hpfFreq, 1 }, "Wet HPF (Hz)",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.35f), 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::lpfFreq, 1 }, "Wet LPF (Hz)",
        juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.35f), 14000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::depth, 1 }, "Depth (Mix)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}
} // namespace breach
