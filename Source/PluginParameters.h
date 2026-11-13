#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace breach
{
static constexpr int kMaxVoices = 12;

enum class ResonatorMode : int
{
    CleanBandpass = 0, // A: Clean Bandpass (Pistol - Transient)
    CombString    = 1, // B: Comb String (Shotgun - Scatter)
    DualFormant   = 2, // C: Dual Formant (SMG - Burst)
    DistortedRes  = 3, // D: Distorted Res (Sniper - Pierce)
    LongTailPad   = 4, // E: Long-Tail Pad (Revolver - Crush)
    NumModes      = 5
};

enum class ResonatorModelOverride : int
{
    FollowFirearm = 0,
    BandpassSVF   = 1,
    CombString    = 2,
    DualFormant   = 3
};

enum class ChordPresetType : int
{
    Custom = 0,
    MajorTriad,
    MinorTriad,
    Major7th,
    Minor7th,
    Dominant7th,
    Sus2,
    Sus4,
    Dim7,
    Aug,
    Pentatonic,
    NumPresets
};

enum class ArpPattern : int
{
    Up = 0,
    Down,
    UpDown,
    Random,
    Simultaneous
};

enum class MotionShape : int
{
    Sine = 0,
    Triangle,
    Square,
    Random,
    Saw
};

enum class DryWetCurveType : int
{
    EqualPower = 0,
    Linear     = 1,
    SCurve     = 2
};

enum class ModSource : int
{
    Lfo1 = 0,
    Lfo2 = 1,
    Env  = 2,
    NumSources = 3
};

enum class ModDest : int
{
    Warp = 0,
    Drift,
    Shift,
    Q,
    Tilt,
    Depth,
    NumDests = 6
};

namespace ParamID
{
    // Firearm Mode
    inline constexpr const char* mode = "mode";

    // ---------------- STAGE 1: MAIN PANEL ----------------
    // 1. Chord Section
    inline constexpr std::array<const char*, 12> noteToggles{
        "noteC", "noteCs", "noteD", "noteDs", "noteE", "noteF",
        "noteFs", "noteG", "noteGs", "noteA", "noteAs", "noteB"
    };
    inline constexpr const char* chordPreset = "chordPreset";
    inline constexpr const char* rootNote    = "rootNote";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* spread      = "spread";
    inline constexpr const char* midiMode    = "midiMode";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* decay       = "decay";

    // 2. Arp Module
    inline constexpr const char* arpOn       = "arpOn";
    inline constexpr const char* arpSync     = "arpSync";
    inline constexpr const char* arpRateSync = "arpRateSync";
    inline constexpr const char* arpRateHz   = "arpRateHz";
    inline constexpr const char* arpPattern  = "arpPattern";

    // 3. Spacing Module
    inline constexpr const char* warp        = "warp";
    inline constexpr const char* drift       = "drift";
    inline constexpr const char* shiftHz     = "shiftHz";

    // 4. Motion Module
    inline constexpr const char* motionOn       = "motionOn";
    inline constexpr const char* motionStrength = "motionStrength";
    inline constexpr const char* motionSync     = "motionSync";
    inline constexpr const char* motionRateSync = "motionRateSync";
    inline constexpr const char* motionRateHz   = "motionRateHz";
    inline constexpr const char* motionShape    = "motionShape";
    inline constexpr const char* unison         = "unison";

    // 5. Filter Module
    inline constexpr const char* filterTilt  = "filterTilt";
    inline constexpr const char* filterQ     = "filterQ";
    inline constexpr const char* filterLow   = "filterLow";
    inline constexpr const char* filterHigh  = "filterHigh";

    // 6. Depth
    inline constexpr const char* depth       = "depth";

    // ---------------- STAGE 2: ADVANCED PANEL ----------------
    // Tab A: Voices (12 voices individual Level, Pan, Detune, DecayMul + VoiceCap)
    inline constexpr const char* voiceCap    = "voiceCap"; // 0=4, 1=6, 2=8, 3=12
    inline constexpr std::array<const char*, kMaxVoices> voiceLevel{
        "v0Level", "v1Level", "v2Level", "v3Level", "v4Level", "v5Level",
        "v6Level", "v7Level", "v8Level", "v9Level", "v10Level", "v11Level"
    };
    inline constexpr std::array<const char*, kMaxVoices> voicePan{
        "v0Pan", "v1Pan", "v2Pan", "v3Pan", "v4Pan", "v5Pan",
        "v6Pan", "v7Pan", "v8Pan", "v9Pan", "v10Pan", "v11Pan"
    };
    inline constexpr std::array<const char*, kMaxVoices> voiceDetune{
        "v0Detune", "v1Detune", "v2Detune", "v3Detune", "v4Detune", "v5Detune",
        "v6Detune", "v7Detune", "v8Detune", "v9Detune", "v10Detune", "v11Detune"
    };
    inline constexpr std::array<const char*, kMaxVoices> voiceDecayMul{
        "v0DecayMul", "v1DecayMul", "v2DecayMul", "v3DecayMul", "v4DecayMul", "v5DecayMul",
        "v6DecayMul", "v7DecayMul", "v8DecayMul", "v9DecayMul", "v10DecayMul", "v11DecayMul"
    };

    // Tab B: Resonator
    inline constexpr const char* resModel    = "resModel";
    inline constexpr const char* fbDamping   = "fbDamping";
    inline constexpr const char* stiffness   = "stiffness";
    inline constexpr const char* excHpfHz    = "excHpfHz";
    inline constexpr const char* excDrive    = "excDrive";

    // Tab C: Modulation (2 LFOs + 1 Env Follower + 3x6 Mod Matrix)
    inline constexpr const char* advLfo1Shape    = "advLfo1Shape";
    inline constexpr const char* advLfo1RateHz   = "advLfo1RateHz";
    inline constexpr const char* advLfo1RateSync = "advLfo1RateSync";
    inline constexpr const char* advLfo1Sync     = "advLfo1Sync";
    inline constexpr const char* advLfo1Phase    = "advLfo1Phase";

    inline constexpr const char* advLfo2Shape    = "advLfo2Shape";
    inline constexpr const char* advLfo2RateHz   = "advLfo2RateHz";
    inline constexpr const char* advLfo2RateSync = "advLfo2RateSync";
    inline constexpr const char* advLfo2Sync     = "advLfo2Sync";
    inline constexpr const char* advLfo2Phase    = "advLfo2Phase";

    inline constexpr const char* advEnvAttack    = "advEnvAttack";
    inline constexpr const char* advEnvRelease   = "advEnvRelease";
    inline constexpr const char* advEnvAmount    = "advEnvAmount";

    // 3 Sources x 6 Destinations = 18 routing amounts
    inline constexpr std::array<std::array<const char*, 6>, 3> modMatrix{{
        {{ "modLfo1Warp", "modLfo1Drift", "modLfo1Shift", "modLfo1Q", "modLfo1Tilt", "modLfo1Depth" }},
        {{ "modLfo2Warp", "modLfo2Drift", "modLfo2Shift", "modLfo2Q", "modLfo2Tilt", "modLfo2Depth" }},
        {{ "modEnvWarp",  "modEnvDrift",  "modEnvShift",  "modEnvQ",  "modEnvTilt",  "modEnvDepth"  }}
    }};

    // Tab D: Global
    inline constexpr const char* inputGainDb     = "inputGainDb";
    inline constexpr const char* outputGainDb    = "outputGainDb";
    inline constexpr const char* stereoWidth     = "stereoWidth";
    inline constexpr const char* dryWetCurve     = "dryWetCurve";
    inline constexpr const char* oversampling    = "oversampling"; // 0=1x, 1=2x, 2=4x
    inline constexpr const char* limiterOn       = "limiterOn";
    inline constexpr const char* limiterThreshDb = "limiterThreshDb";
    inline constexpr const char* smoothTimeMs    = "smoothTimeMs";
}

inline int voiceCapIndexToCount(int idx) noexcept
{
    switch (idx)
    {
        case 0: return 4;
        case 1: return 6;
        case 2: return 8;
        case 3:
        default: return 12;
    }
}

inline std::array<bool, 12> getPitchClassMaskForPreset(ChordPresetType preset) noexcept
{
    std::array<bool, 12> mask{};
    mask.fill(false);

    auto setSteps = [&](std::initializer_list<int> steps)
    {
        for (int s : steps)
            mask[static_cast<size_t>((s % 12 + 12) % 12)] = true;
    };

    switch (preset)
    {
        case ChordPresetType::MajorTriad:  setSteps({ 0, 4, 7 }); break;
        case ChordPresetType::MinorTriad:  setSteps({ 0, 3, 7 }); break;
        case ChordPresetType::Major7th:    setSteps({ 0, 4, 7, 11 }); break;
        case ChordPresetType::Minor7th:    setSteps({ 0, 3, 7, 10 }); break;
        case ChordPresetType::Dominant7th: setSteps({ 0, 4, 7, 10 }); break;
        case ChordPresetType::Sus2:        setSteps({ 0, 2, 7 }); break;
        case ChordPresetType::Sus4:        setSteps({ 0, 5, 7 }); break;
        case ChordPresetType::Dim7:        setSteps({ 0, 3, 6, 9 }); break;
        case ChordPresetType::Aug:         setSteps({ 0, 4, 8 }); break;
        case ChordPresetType::Pentatonic:  setSteps({ 0, 3, 5, 7, 10 }); break;
        case ChordPresetType::Custom:
        default:
            setSteps({ 0, 3, 7 });
            break;
    }
    return mask;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::mode, 1 }, "Resonator Mode",
        juce::StringArray{ "A: Clean Bandpass", "B: Comb String", "C: Dual Formant", "D: Distorted Res", "E: Long-Tail Pad" }, 0));

    // ---------------- STAGE 1 ----------------
    static constexpr std::array<const char*, 12> kNoteLabels{
        "Note C", "Note C#", "Note D", "Note D#", "Note E", "Note F",
        "Note F#", "Note G", "Note G#", "Note A", "Note A#", "Note B"
    };
    for (size_t i = 0; i < 12; ++i)
    {
        const bool defaultOn = (i == 0 || i == 3 || i == 7);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ ParamID::noteToggles[i], 1 }, kNoteLabels[i], defaultOn));
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::chordPreset, 1 }, "Chord Preset",
        juce::StringArray{
            "Custom", "Major Triad", "Minor Triad", "Major 7th", "Minor 7th",
            "Dominant 7th", "Sus2", "Sus4", "Dim7", "Aug", "Pentatonic"
        }, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::rootNote, 1 }, "Root",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ ParamID::octave, 1 }, "Octave", 1, 6, 3));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ ParamID::spread, 1 }, "Spread", 1, 4, 2));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::midiMode, 1 }, "MIDI Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::attack, 1 }, "Attack (ms)",
        juce::NormalisableRange<float>(0.5f, 500.0f, 0.1f, 0.4f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::decay, 1 }, "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.65f));

    // Arp
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::arpOn, 1 }, "Arp On", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::arpSync, 1 }, "Arp Sync", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::arpRateSync, 1 }, "Arp Rate (Sync)",
        juce::StringArray{ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::arpRateHz, 1 }, "Arp Rate (Hz)",
        juce::NormalisableRange<float>(0.2f, 25.0f, 0.01f, 0.45f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::arpPattern, 1 }, "Arp Pattern",
        juce::StringArray{ "Up", "Down", "Up-Down", "Random", "Simultaneous" }, 0));

    // Spacing
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::warp, 1 }, "Warp",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::drift, 1 }, "Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::shiftHz, 1 }, "Shift (Hz)",
        juce::NormalisableRange<float>(-500.0f, 500.0f, 0.1f, 0.5f, true), 0.0f));

    // Motion
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::motionOn, 1 }, "Motion On", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::motionStrength, 1 }, "Motion Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::motionSync, 1 }, "Motion Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::motionRateSync, 1 }, "Motion Rate (Sync)",
        juce::StringArray{ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::motionRateHz, 1 }, "Motion Rate (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.4f), 1.2f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::motionShape, 1 }, "Motion Shape",
        juce::StringArray{ "Sine", "Triangle", "Square", "Random", "Saw" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::unison, 1 }, "Unison",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    // Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::filterTilt, 1 }, "Filter Tilt",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::filterQ, 1 }, "Filter Q",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::filterLow, 1 }, "Filter Low (Hz)",
        juce::NormalisableRange<float>(20.0f, 18000.0f, 1.0f, 0.35f), 60.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::filterHigh, 1 }, "Filter High (Hz)",
        juce::NormalisableRange<float>(40.0f, 20000.0f, 1.0f, 0.35f), 16000.0f));

    // Depth
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::depth, 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    // ---------------- STAGE 2: ADVANCED PANEL ----------------
    // A. Voices
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::voiceCap, 1 }, "Voice Cap",
        juce::StringArray{ "4 Voices", "6 Voices", "8 Voices", "12 Voices" }, 2)); // Default 8

    for (size_t v = 0; v < kMaxVoices; ++v)
    {
        const juce::String vNum(static_cast<int>(v + 1));
        const float defaultPan = (v % 2 == 0) ? -0.35f : 0.35f;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ ParamID::voiceLevel[v], 1 }, "Voice " + vNum + " Level",
            juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ ParamID::voicePan[v], 1 }, "Voice " + vNum + " Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), defaultPan));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ ParamID::voiceDetune[v], 1 }, "Voice " + vNum + " Detune (ct)",
            juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ ParamID::voiceDecayMul[v], 1 }, "Voice " + vNum + " Decay Mul",
            juce::NormalisableRange<float>(0.25f, 2.0f, 0.01f), 1.0f));
    }

    // B. Resonator
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::resModel, 1 }, "Resonator Model",
        juce::StringArray{ "Auto (Gun Mode)", "Bandpass SVF", "Comb String", "Dual Formant" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::fbDamping, 1 }, "Feedback Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::stiffness, 1 }, "Stiffness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::excHpfHz, 1 }, "Excitation HPF (Hz)",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.35f), 40.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::excDrive, 1 }, "Excitation Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.15f));

    // C. Modulation
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::advLfo1Shape, 1 }, "LFO 1 Shape",
        juce::StringArray{ "Sine", "Triangle", "Square", "Random", "Saw" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advLfo1RateHz, 1 }, "LFO 1 Rate (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.4f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::advLfo1RateSync, 1 }, "LFO 1 Sync Rate",
        juce::StringArray{ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::advLfo1Sync, 1 }, "LFO 1 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advLfo1Phase, 1 }, "LFO 1 Phase",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::advLfo2Shape, 1 }, "LFO 2 Shape",
        juce::StringArray{ "Sine", "Triangle", "Square", "Random", "Saw" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advLfo2RateHz, 1 }, "LFO 2 Rate (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.4f), 2.1f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::advLfo2RateSync, 1 }, "LFO 2 Sync Rate",
        juce::StringArray{ "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 4));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::advLfo2Sync, 1 }, "LFO 2 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advLfo2Phase, 1 }, "LFO 2 Phase",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 90.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advEnvAttack, 1 }, "Env Attack (ms)",
        juce::NormalisableRange<float>(0.5f, 250.0f, 0.1f, 0.4f), 8.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advEnvRelease, 1 }, "Env Release (ms)",
        juce::NormalisableRange<float>(10.0f, 1500.0f, 1.0f, 0.4f), 160.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::advEnvAmount, 1 }, "Env Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    static constexpr std::array<const char*, 3> kSrcNames{ "LFO1", "LFO2", "Env" };
    static constexpr std::array<const char*, 6> kDstNames{ "Warp", "Drift", "Shift", "Q", "Tilt", "Depth" };
    for (size_t s = 0; s < 3; ++s)
    {
        for (size_t d = 0; d < 6; ++d)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ ParamID::modMatrix[s][d], 1 },
                juce::String(kSrcNames[s]) + " -> " + juce::String(kDstNames[d]),
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
        }
    }

    // D. Global
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::inputGainDb, 1 }, "Input Gain (dB)",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::outputGainDb, 1 }, "Output Gain (dB)",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::stereoWidth, 1 }, "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::dryWetCurve, 1 }, "Dry/Wet Curve",
        juce::StringArray{ "Equal Power", "Linear", "S-Curve" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamID::oversampling, 1 }, "Oversampling",
        juce::StringArray{ "1x", "2x", "4x" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::limiterOn, 1 }, "Output Soft Limiter", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::limiterThreshDb, 1 }, "Limiter Threshold (dB)",
        juce::NormalisableRange<float>(-18.0f, 0.0f, 0.1f), -0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamID::smoothTimeMs, 1 }, "Param Smooth (ms)",
        juce::NormalisableRange<float>(2.0f, 100.0f, 0.5f, 0.5f), 15.0f));

    return { params.begin(), params.end() };
}
} // namespace breach
