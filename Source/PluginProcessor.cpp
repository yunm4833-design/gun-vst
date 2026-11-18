#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // 10 built-in chord semitone masks
    constexpr int kPresetMasks[10][12] = {
        { 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 }, // 0: MAJOR TRIAD (C E G)
        { 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0 }, // 1: MINOR TRIAD (C D# G)
        { 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1 }, // 2: MAJ7 (C E G B)
        { 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0 }, // 3: MIN7 (C D# G A#)
        { 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 }, // 4: DOM7 (C E G A#)
        { 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0 }, // 5: SUS2 (C D G)
        { 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0 }, // 6: SUS4 (C F G)
        { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 }, // 7: DIM7 (C D# F# A)
        { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 }, // 8: AUG (C E G#)
        { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 }  // 9: PENTATONIC (C D E G A)
    };
}

BreachResonatorAudioProcessor::BreachResonatorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    outputEqEnabled = apvts.getRawParameterValue("outputEqEnable");
    oversamplingParameter = apvts.getRawParameterValue("oversampling");
    for (std::size_t band = 0; band < outputEqParameters.size(); ++band)
    {
        auto raw = [&](const char* suffix) { return apvts.getRawParameterValue(outputEqBandId(band, suffix)); };
        outputEqParameters[band] = {raw("Enable"), raw("Type"), raw("Freq"), raw("Gain"), raw("Q")};
    }
    nonlinearEngine = std::make_unique<breach::nonlinear::Engine>();
    nonlinearOversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    nonlinearOversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
}

juce::String BreachResonatorAudioProcessor::outputEqBandId(std::size_t band, const char* suffix)
{
    return "outputEq" + juce::String(static_cast<int>(band + 1)) + suffix;
}

breach::eq::BandSettings BreachResonatorAudioProcessor::readOutputEqBand(std::size_t band) const noexcept
{
    const auto& p = outputEqParameters[band];
    auto load = [](const auto* parameter) { return parameter->load(std::memory_order_relaxed); };
    return {load(p.enabled) > .5f, static_cast<breach::eq::Shape>(static_cast<int>(load(p.shape))),
            load(p.frequency), load(p.gain), load(p.q)};
}

breach::eq::Rates BreachResonatorAudioProcessor::getOutputEqRates() const noexcept
{
    const double base = outputEqBaseRate.load(std::memory_order_relaxed);
    const int choice = static_cast<int>(oversamplingParameter->load(std::memory_order_relaxed));
    return {base, base * (1 << juce::jlimit(0, 2, choice))};
}

juce::AudioProcessorValueTreeState::ParameterLayout BreachResonatorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 6 Primary Macro Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive", 1), "Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("space", 1), "Space", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(0.05f, 1.0f, 0.001f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Tone", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("density", 1), "Density", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.50f));

    // Gun Resonator Mode (0=PISTOL, 1=SHOTGUN, 2=SMG, 3=SNIPER, 4=REVOLVER)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gunMode", 1), "Gun Topology",
        juce::StringArray{ "PISTOL", "SHOTGUN", "SMG", "SNIPER", "REVOLVER" }, 1));

    // Chord & Scale Controls
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("chordPreset", 1), "Chord Preset",
        juce::StringArray{ "MAJOR TRIAD", "MINOR TRIAD", "MAJ7", "MIN7", "DOM7", "SUS2", "SUS4", "DIM7", "AUG", "PENTATONIC" }, 7));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("rootNote", 1), "Root Note",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("octaveSpread", 1), "Octave Spread",
        juce::StringArray{ "1 OCT", "2 OCT", "3 OCT", "4 OCT" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("voiceCap", 1), "Voice Count",
        juce::StringArray{ "4 VOX", "6 VOX", "8 VOX", "12 VOX" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("midiMode", 1), "MIDI Input Mode", false));

    // 12 Semitone toggles (default DIM7: C, D#, F#, A)
    for (int i = 0; i < 12; ++i)
    {
        const bool defaultOn = (kPresetMasks[7][i] != 0);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("note_" + juce::String(i), 1),
            juce::String("Note ") + kNoteNames[i],
            defaultOn));
    }

    // Arpeggiator & Motion
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("arpEnable", 1), "Arp Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arpDirection", 1), "Arp Direction",
        juce::StringArray{ "UP", "DOWN", "UP-DN", "RAND", "SYNC" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("motionEnable", 1), "Motion Enable", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("arpRate", 1), "Arp Rate",
        juce::NormalisableRange<float>(0.5f, 24.0f, 0.01f, 0.5f), 6.0f));

    // Tab A: 12 Per-Voice Controls (48 params)
    for (int v = 0; v < kMaxVoices; ++v)
    {
        const float defaultPan = (v % 2 == 0) ? -0.25f * (static_cast<float>(v) / 11.0f)
                                              :  0.25f * (static_cast<float>(v) / 11.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vLevel_" + juce::String(v), 1),
            "Voice " + juce::String(v + 1) + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vPan_" + juce::String(v), 1),
            "Voice " + juce::String(v + 1) + " Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), defaultPan));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vDetune_" + juce::String(v), 1),
            "Voice " + juce::String(v + 1) + " Detune",
            juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vDecay_" + juce::String(v), 1),
            "Voice " + juce::String(v + 1) + " Decay Mul",
            juce::NormalisableRange<float>(0.25f, 2.0f, 0.01f), 1.0f));
    }

    // Tab B: Resonator Physics & Excitation (6 params)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("resTopology", 1), "Resonator Topology",
        juce::StringArray{ "AUTO (GUN MODE)", "BANDPASS SVF", "COMB STRING", "DUAL FORMANT" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resAttack", 1), "Attack (ms)",
        juce::NormalisableRange<float>(0.5f, 500.0f, 0.1f, 0.4f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("fbDamping", 1), "FB Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stiffness", 1), "Stiffness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("excHpf", 1), "Exc HPF (Hz)",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.4f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("excDrive", 1), "Exc Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));

    // Tab C: Modulation & Hilbert Shifter (11 + 18 = 29 params)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lfo1Shape", 1), "LFO 1 Shape",
        juce::StringArray{ "SINE", "TRI", "SAW", "SQR", "S&H" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("lfo1Sync", 1), "LFO 1 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfo1Rate", 1), "LFO 1 Rate",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.5f), 1.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfo1Phase", 1), "LFO 1 Phase",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lfo2Shape", 1), "LFO 2 Shape",
        juce::StringArray{ "SINE", "TRI", "SAW", "SQR", "S&H" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("lfo2Sync", 1), "LFO 2 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfo2Rate", 1), "LFO 2 Rate",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.5f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfo2Phase", 1), "LFO 2 Phase",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 90.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("envAttack", 1), "Env Attack",
        juce::NormalisableRange<float>(1.0f, 250.0f, 0.5f, 0.5f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("envRelease", 1), "Env Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.5f), 140.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("envAmount", 1), "Env Amount",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("freqShift", 1), "Shift (Hz)",
        juce::NormalisableRange<float>(-500.0f, 500.0f, 0.5f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoUnison", 1), "Unison",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    // 3x6 Modulation Matrix (3 sources x 6 targets = 18 params)
    static constexpr const char* kModSrc[3] = { "lfo1", "lfo2", "env" };
    static constexpr const char* kModDst[6] = { "warp", "drift", "shift", "q", "tilt", "depth" };
    for (int s = 0; s < 3; ++s)
    {
        for (int d = 0; d < 6; ++d)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(juce::String("mod_") + kModSrc[s] + "_" + kModDst[d], 1),
                juce::String("Mod ") + kModSrc[s] + " -> " + kModDst[d],
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        }
    }

    // Tab D: Global Bus & Engine (7 params)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inGain", 1), "In Gain (dB)",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outGain", 1), "Out Gain (dB)",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoWidth", 1), "Width (%)",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 115.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mixCurve", 1), "Mix Curve",
        juce::StringArray{ "EQUAL POWER", "LINEAR", "S-CURVE" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("oversampling", 1), "Oversampling",
        juce::StringArray{ "1X OFF", "2X IIR", "4X IIR" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("softLimiter", 1), "Soft Limiter", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("paramSmoothMs", 1), "Smooth (ms)",
        juce::NormalisableRange<float>(2.0f, 100.0f, 0.5f), 15.0f));

    // Append-only parameters: existing IDs, order, ranges and defaults stay intact.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("ottEnable", 1), "OTT Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottDepth", 1), "OTT Depth", juce::NormalisableRange<float>(0, 1, .001f), .35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottTime", 1), "OTT Time (%)", juce::NormalisableRange<float>(10, 400, 1, .5f), 100));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottUp", 1), "OTT Upward", juce::NormalisableRange<float>(0, 1, .001f), .5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottDown", 1), "OTT Downward", juce::NormalisableRange<float>(0, 1, .001f), .5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottInGain", 1), "OTT In Gain (dB)", juce::NormalisableRange<float>(-18, 18, .1f), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottOutGain", 1), "OTT Out Gain (dB)", juce::NormalisableRange<float>(-18, 18, .1f), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottLowSplit", 1), "OTT Low Split (Hz)", juce::NormalisableRange<float>(40, 400, 1, .5f), 120));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ottHighSplit", 1), "OTT High Split (Hz)", juce::NormalisableRange<float>(1000, 8000, 1, .5f), 2500));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverbEnable", 1), "Reverb Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("reverbType", 1), "Reverb Type", juce::StringArray{"HALL", "PLATE", "SPACE", "SHIMMER"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbMix", 1), "Reverb Mix", juce::NormalisableRange<float>(0,1,.001f), .18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDecay", 1), "Reverb Decay (s)", juce::NormalisableRange<float>(.2f,8,.01f,.5f), 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbPreDelay", 1), "Reverb Predelay (ms)", juce::NormalisableRange<float>(0,200,.1f), 20));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDamping", 1), "Reverb Damping", juce::NormalisableRange<float>(0,1,.001f), .5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbWidth", 1), "Reverb Width", juce::NormalisableRange<float>(0,2,.01f), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbShimmer", 1), "Reverb Shimmer", juce::NormalisableRange<float>(0,1,.001f), .5f));

    // Output EQ parameters are appended after all previous host parameters.
    // AUv2 orders parameters by version hint, then by identifier hash.
    constexpr int outputEqVersion = 2;
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("outputEqEnable", outputEqVersion), "Output EQ Enable", false));
    juce::StringArray filterTypes;
    for (const auto* name : breach::eq::shapeNames) filterTypes.add(name);
    constexpr std::array<float, breach::eq::bandCount> frequencies {{80, 150, 300, 600, 1200, 2500, 5000, 12000}};
    constexpr std::array<breach::eq::Shape, breach::eq::bandCount> shapes {{
        breach::eq::Shape::lowCut24, breach::eq::Shape::lowShelf, breach::eq::Shape::bell, breach::eq::Shape::bell,
        breach::eq::Shape::bell, breach::eq::Shape::bell, breach::eq::Shape::highShelf, breach::eq::Shape::highCut12
    }};
    for (std::size_t band = 0; band < frequencies.size(); ++band)
    {
        const juce::String name = "Output EQ " + juce::String(static_cast<int>(band + 1)) + " ";
        auto id = [band](const char* suffix) { return juce::ParameterID(outputEqBandId(band, suffix), outputEqVersion); };
        juce::NormalisableRange<float> frequencyRange(
            static_cast<float>(breach::eq::minHz), static_cast<float>(breach::eq::maxHz),
            [](float lo, float hi, float value) { return lo * std::pow(hi / lo, value); },
            [](float lo, float hi, float hz) { return std::log(hz / lo) / std::log(hi / lo); },
            [](float lo, float hi, float hz) { return std::clamp(std::round(hz * 10) / 10, lo, hi); });
        params.push_back(std::make_unique<juce::AudioParameterBool>(id("Enable"), name + "Enable", band == 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(id("Type"), name + "Type", filterTypes, static_cast<int>(shapes[band])));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id("Freq"), name + "Frequency (Hz)", frequencyRange, frequencies[band],
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float hz, int)
            { return juce::String(hz, hz < 100 ? 1 : 0); })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id("Gain"), name + "Gain (dB)", juce::NormalisableRange<float>(-18, 18, .01f), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id("Q"), name + "Q", juce::NormalisableRange<float>(.25f, 8, 0, .35f), static_cast<float>(breach::eq::neutralQ),
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float q, int)
            { return juce::String(q, 3); })));
    }

    // Version 3 keeps the new voice macros after the version 2 EQ in AUv2.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("voiceTuneSpread", 3), "Tune Spread (cents)", juce::NormalisableRange<float>(0, 20, .1f), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("voicePanSpread", 3), "Pan Spread (%)", juce::NormalisableRange<float>(0, 100, 1), 0));

    return { params.begin(), params.end() };
}

void BreachResonatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);
    outputEqBaseRate.store(static_cast<float>(currentSampleRate), std::memory_order_relaxed);
    outputEq.reset();

    oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));

    nonlinearOversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
    nonlinearOversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));
    nonlinearLatencies[0].store(2);
    nonlinearLatencies[1].store(2 + juce::roundToInt(nonlinearOversampler2x->getLatencyInSamples()));
    nonlinearLatencies[2].store(2 + juce::roundToInt(nonlinearOversampler4x->getLatencyInSamples()));
    nonlinearProcessingChoice = -1;
    configureNonlinearProcessing(juce::jlimit(0, 2, static_cast<int>(apvts.getRawParameterValue("oversampling")->load())), getNonlinearDspRevision() >= 2);

    const int maxCombSamples = static_cast<int>(currentSampleRate * 4.0 / 20.0) + 64;
    for (int v = 0; v < kMaxVoices; ++v)
    {
        voicesL[static_cast<size_t>(v)].combBuffer.assign(static_cast<size_t>(maxCombSamples), 0.0f);
        voicesR[static_cast<size_t>(v)].combBuffer.assign(static_cast<size_t>(maxCombSamples), 0.0f);
        voicesL[static_cast<size_t>(v)].reset();
        voicesR[static_cast<size_t>(v)].reset();
    }

    diffuserL[0].prepare(static_cast<int>(currentSampleRate * 0.0051), 0.48f);
    diffuserL[1].prepare(static_cast<int>(currentSampleRate * 0.0079), 0.45f);
    diffuserR[0].prepare(static_cast<int>(currentSampleRate * 0.0057), 0.48f);
    diffuserR[1].prepare(static_cast<int>(currentSampleRate * 0.0083), 0.45f);

    hilbertL.reset();
    hilbertR.reset();
    dynamicsEngine.prepare(currentSampleRate);
    reverbEngine.prepare(currentSampleRate);
    balanceWasActive = balancedSniperWasActive = false;
    ringTimeWasActive = false;
    ringSmoothRate = 0;
    voiceMacroSampleRate = 0;
    macrosWereActive = false;
    balancedSmoothRate = 0.0; balancedSmoothTime = -1.0;
    balancedSniper.selectRate(currentSampleRate); balancedSniper.reset();
    modalBank.prepare(currentSampleRate);
    modalWasActive = false;
    formantBank.prepare(currentSampleRate);
    formantWasActive = false;
    stringBank.prepare(currentSampleRate * 4.0);
    stringWasActive = false;
    spatialEngine.prepare(currentSampleRate);
    spatialRevisionBlend = getSpatialDspRevision() >= 2 ? 1.0f : 0.0f;
    excHpfStateL = excHpfStateR = 0.0f;
    excPrevInL = excPrevInR = 0.0f;
    tiltLpL = tiltLpR = 0.0f;
    modEnvState = 0.0f;
    impactAnalysis.reset(telemetry.impact);

    const double smoothSec = 0.015;
    smDrive.reset(currentSampleRate, smoothSec);
    smSpace.reset(currentSampleRate, smoothSec);
    smDecay.reset(currentSampleRate, smoothSec);
    smTone.reset(currentSampleRate, smoothSec);
    smDensity.reset(currentSampleRate, smoothSec);
    smMix.reset(currentSampleRate, smoothSec);
    smInGain.reset(currentSampleRate, smoothSec);
    smOutGain.reset(currentSampleRate, smoothSec);
    smWidth.reset(currentSampleRate, smoothSec);
}

void BreachResonatorAudioProcessor::releaseResources()
{
    impactAnalysis.reset(telemetry.impact);
    oversampler2x->reset();
    oversampler4x->reset();
    nonlinearOversampler2x->reset();
    nonlinearOversampler4x->reset();
    nonlinearEngine->reset();
    stringBank.reset();
    formantBank.reset();
    modalBank.reset();
    balancedSniper.reset();
    dynamicsEngine.reset();
    reverbEngine.reset();
    outputEq.reset();
}

bool BreachResonatorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo() && mainOut != juce::AudioChannelSet::mono())
        return false;
    return mainIn == mainOut;
}

void BreachResonatorAudioProcessor::triggerPanicReset()
{
    panicRequested.store(true, std::memory_order_release);
}

void BreachResonatorAudioProcessor::applyChordPreset(int presetIndex)
{
    const int idx = juce::jlimit(0, 9, presetIndex);
    for (int i = 0; i < 12; ++i)
    {
        if (auto* p = apvts.getParameter("note_" + juce::String(i)))
        {
            const float val = (kPresetMasks[idx][i] != 0) ? 1.0f : 0.0f;
            p->setValueNotifyingHost(val);
        }
    }
}

float BreachResonatorAudioProcessor::evaluateLfo(int shape, float phase) const
{
    const float p = phase - std::floor(phase);
    switch (shape)
    {
        case 0: return std::sin(p * juce::MathConstants<float>::twoPi); // SINE
        case 1: return 4.0f * std::abs(p - 0.5f) - 1.0f;                // TRI
        case 2: return 2.0f * p - 1.0f;                                 // SAW
        case 3: return (p < 0.5f) ? 1.0f : -1.0f;                       // SQR
        case 4:                                                         // S&H
        default:
        {
            const uint32_t step = static_cast<uint32_t>(std::floor(phase));
            uint32_t h = step * 2654435761u;
            h ^= (h >> 16);
            return (static_cast<float>(h & 0xffffu) / 32767.5f) - 1.0f;
        }
    }
}

void BreachResonatorAudioProcessor::rebuildTargetFrequencies(std::array<float, kMaxVoices>& freqs, int& activeCount)
{
    static constexpr int kCapTable[4] = { 4, 6, 8, 12 };
    const int capIdx = juce::jlimit(0, 3, static_cast<int>(apvts.getRawParameterValue("voiceCap")->load()));
    const int maxAllowed = kCapTable[capIdx];

    const bool midiMode = apvts.getRawParameterValue("midiMode")->load() > 0.5f;
    std::vector<float> candidateNotes;
    candidateNotes.reserve(16);

    if (midiMode)
    {
        for (int n = 0; n < 128; ++n)
        {
            if (midiNotesHeld[static_cast<size_t>(n)])
                candidateNotes.push_back(static_cast<float>(n));
        }
    }

    if (candidateNotes.empty())
    {
        const int root = juce::jlimit(0, 11, static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));
        const int octaves = juce::jlimit(1, 4, static_cast<int>(apvts.getRawParameterValue("octaveSpread")->load()) + 1);

        std::vector<int> activeSemis;
        for (int i = 0; i < 12; ++i)
        {
            if (apvts.getRawParameterValue("note_" + juce::String(i))->load() > 0.5f)
                activeSemis.push_back(i);
        }
        if (activeSemis.empty())
            activeSemis.push_back(0);

        const int baseMidi = 48 + root; // C3 + root
        for (int oct = 0; oct < octaves; ++oct)
        {
            for (int semi : activeSemis)
                candidateNotes.push_back(static_cast<float>(baseMidi + oct * 12 + semi));
        }
    }

    activeCount = juce::jlimit(1, maxAllowed, static_cast<int>(candidateNotes.size()));
    for (int v = 0; v < kMaxVoices; ++v)
    {
        const float midiNote = candidateNotes[static_cast<size_t>(v % static_cast<int>(candidateNotes.size()))];
        const float detuneCents = apvts.getRawParameterValue("vDetune_" + juce::String(v))->load();
        const float exactMidi = midiNote + detuneCents * 0.01f;
        freqs[static_cast<size_t>(v)] = 440.0f * std::pow(2.0f, (exactMidi - 69.0f) / 12.0f);
    }
}

void BreachResonatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (buffer.getNumChannels() == 1)
    {
        // Safely process mono if needed
    }

    if (panicRequested.exchange(false, std::memory_order_acquire))
    {
        impactAnalysis.reset(telemetry.impact);
        for (int v = 0; v < kMaxVoices; ++v)
        {
            voicesL[static_cast<size_t>(v)].reset();
            voicesR[static_cast<size_t>(v)].reset();
        }
        diffuserL[0].reset();
        diffuserL[1].reset();
        diffuserR[0].reset();
        diffuserR[1].reset();
        hilbertL.reset();
        hilbertR.reset();
        spatialEngine.reset();
        stringBank.reset();
        formantBank.reset();
        modalBank.reset();
        balancedSniper.reset();
        dynamicsEngine.reset();
        reverbEngine.reset();
        outputEq.reset();
        nonlinearEngine->reset();
    }

    if (ancillaryResetRequested.exchange(false, std::memory_order_acquire))
    {
        dynamicsEngine.reset(); reverbEngine.reset(); outputEq.reset();
        voiceMacroSampleRate = 0;
    }

    // Track incoming MIDI notes
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            midiNotesHeld[static_cast<size_t>(msg.getNoteNumber())] = true;
        else if (msg.isNoteOff())
            midiNotesHeld[static_cast<size_t>(msg.getNoteNumber())] = false;
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            midiNotesHeld.fill(false);
    }

    double hostBpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                if (*bpm > 10.0)
                    hostBpm = *bpm;
        }
    }

    // Match the external chain: OTT-style input dynamics before BREACH gain,
    // excitation and the dry/wet split. Run at base rate, independently of OS.
    breach::dynamics::Controls dynamics;
    dynamics.useFaust = usesFaustOtt();
    dynamics.enabled = apvts.getRawParameterValue("ottEnable")->load() > .5f;
    dynamics.depth = apvts.getRawParameterValue("ottDepth")->load();
    dynamics.timePercent = apvts.getRawParameterValue("ottTime")->load();
    dynamics.upward = apvts.getRawParameterValue("ottUp")->load();
    dynamics.downward = apvts.getRawParameterValue("ottDown")->load();
    dynamics.inputDb = apvts.getRawParameterValue("ottInGain")->load();
    dynamics.outputDb = apvts.getRawParameterValue("ottOutGain")->load();
    dynamics.lowHz = apvts.getRawParameterValue("ottLowSplit")->load();
    dynamics.highHz = apvts.getRawParameterValue("ottHighSplit")->load();
    dynamicsEngine.setControls(dynamics);
    if (!dynamicsEngine.idle())
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);
        for (int n = 0; n < numSamples; ++n)
        {
            const auto out = dynamicsEngine.process({left[n], right[n]});
            left[n] = out.left; right[n] = out.right;
        }
    }
    for (size_t b = 0; b < 3; ++b)
        telemetry.ottGainDb[b].store(dynamicsEngine.idle() ? 0.0f : dynamicsEngine.gainMeters()[b], std::memory_order_relaxed);

    const int osChoice = juce::jlimit(0, 2, static_cast<int>(apvts.getRawParameterValue("oversampling")->load()));
    configureNonlinearProcessing(osChoice, getNonlinearDspRevision() >= 2);
    spatialEngine.selectRate(osChoice);
    reverbEngine.selectRate(osChoice);
    juce::dsp::AudioBlock<float> baseBlock(buffer);

    if (osChoice == 1)
    {
        auto* engine = nonlinearProcessingEnabled ? nonlinearOversampler2x.get() : oversampler2x.get();
        auto osBlock = engine->processSamplesUp(baseBlock);
        processCoreBlock(osBlock, currentSampleRate * 2.0, hostBpm);
        engine->processSamplesDown(baseBlock);
    }
    else if (osChoice == 2)
    {
        auto* engine = nonlinearProcessingEnabled ? nonlinearOversampler4x.get() : oversampler4x.get();
        auto osBlock = engine->processSamplesUp(baseBlock);
        processCoreBlock(osBlock, currentSampleRate * 4.0, hostBpm);
        engine->processSamplesDown(baseBlock);
    }
    else
    {
        processCoreBlock(baseBlock, currentSampleRate, hostBpm);
    }
    // Sample the delivered signal after the oversampler and all post effects.
    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getReadPointer(buffer.getNumChannels() > 1 ? 1 : 0);
    for (int sample = 0; sample < numSamples; ++sample)
        impactAnalysis.observeOutput(left[sample], right[sample]);
    impactAnalysis.publishOutput(telemetry.impact);
}

void BreachResonatorAudioProcessor::processCoreBlock(juce::dsp::AudioBlock<float>& block, double sr, double hostBpm)
{
    const int numChannels = static_cast<int>(block.getNumChannels());
    const int numSamples = static_cast<int>(block.getNumSamples());
    if (numChannels <= 0 || numSamples <= 0)
        return;

    float* chL = block.getChannelPointer(0);
    float* chR = (numChannels > 1) ? block.getChannelPointer(1) : chL;

    impactAnalysis.setRates(sr, currentSampleRate);
    const bool shortSmear = isShortSmearEnabled();
    const float spatialTarget = getSpatialDspRevision() >= 2 ? 1.0f : 0.0f;
    const float spatialBlendStep = static_cast<float>(1.0 / (0.02 * sr));

    const bool balanced = getBalanceRevision() >= 2;
    const bool physicalRing = usesRingTime();
    const bool ringChanged = physicalRing != ringTimeWasActive;
    ringTimeWasActive = physicalRing;
    if (ringChanged)
    {
        modalBank.reset();
        formantBank.reset();
        stringBank.reset();
        balancedSniper.reset();
    }
    const float requestedSmooth = apvts.getRawParameterValue("paramSmoothMs")->load() * 0.001f;
    if (balanced)
    {
        const bool changed = !balanceWasActive || balancedSmoothRate != sr || balancedSmoothTime != requestedSmooth;
        if (changed)
        {
            auto configure = [&](auto& smooth, float value)
            {
                const float current = balanceWasActive ? smooth.getCurrentValue() : value;
                smooth.reset(sr, requestedSmooth); smooth.setCurrentAndTargetValue(current);
            };
            configure(smDrive, apvts.getRawParameterValue("drive")->load());
            configure(smSpace, apvts.getRawParameterValue("space")->load());
            configure(smDecay, apvts.getRawParameterValue("decay")->load());
            configure(smTone, apvts.getRawParameterValue("tone")->load());
            configure(smDensity, apvts.getRawParameterValue("density")->load());
            configure(smMix, apvts.getRawParameterValue("mix")->load());
            configure(smInGain, juce::Decibels::decibelsToGain(apvts.getRawParameterValue("inGain")->load()));
            configure(smOutGain, juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outGain")->load()));
            configure(smWidth, apvts.getRawParameterValue("stereoWidth")->load() * .01f);
            configure(smExcDrive, apvts.getRawParameterValue("excDrive")->load());
            const float trimCurrent = balanceWasActive ? smWetTrim.getCurrentValue() : 1.0f;
            smWetTrim.reset(sr, .03); smWetTrim.setCurrentAndTargetValue(trimCurrent);
            balancedSmoothRate = sr; balancedSmoothTime = requestedSmooth;
        }
    }
    else
    {
    // Update parameter smoothing time
    const float smoothSec = apvts.getRawParameterValue("paramSmoothMs")->load() * 0.001f;
    smDrive.reset(sr, smoothSec);
    smSpace.reset(sr, smoothSec);
    if (!physicalRing) smDecay.reset(sr, smoothSec);
    else if (ringChanged || balanceWasActive || std::abs(ringSmoothRate - sr) > 0
             || std::abs(ringSmoothTime - smoothSec) > 0)
    {
        const float current = ringChanged || ringSmoothRate <= 0
            ? apvts.getRawParameterValue("decay")->load() : smDecay.getCurrentValue();
        smDecay.reset(sr, smoothSec);
        smDecay.setCurrentAndTargetValue(current);
        ringSmoothRate = sr;
        ringSmoothTime = smoothSec;
    }
    smTone.reset(sr, smoothSec);
    smDensity.reset(sr, smoothSec);
    smMix.reset(sr, smoothSec);
    smInGain.reset(sr, smoothSec);
    smOutGain.reset(sr, smoothSec);
    smWidth.reset(sr, smoothSec);

    }
    const bool balanceChanged = balanced != balanceWasActive;
    balanceWasActive = balanced;
    balancedSniper.selectRate(sr);
    const bool resetBalance = balanceResetRequested.exchange(false, std::memory_order_acquire) || balanceChanged;
    smExcDrive.setTargetValue(apvts.getRawParameterValue("excDrive")->load());

    smDrive.setTargetValue(apvts.getRawParameterValue("drive")->load());
    smSpace.setTargetValue(apvts.getRawParameterValue("space")->load());
    smDecay.setTargetValue(apvts.getRawParameterValue("decay")->load());
    smTone.setTargetValue(apvts.getRawParameterValue("tone")->load());
    smDensity.setTargetValue(apvts.getRawParameterValue("density")->load());
    smMix.setTargetValue(apvts.getRawParameterValue("mix")->load());
    smInGain.setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("inGain")->load()));
    smOutGain.setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outGain")->load()));
    smWidth.setTargetValue(apvts.getRawParameterValue("stereoWidth")->load() * 0.01f);

    const int gunMode = juce::jlimit(0, 4, static_cast<int>(apvts.getRawParameterValue("gunMode")->load()));
    if (gunMode != lastGunMode)
    {
        prevGunMode = lastGunMode;
        lastGunMode = gunMode;
        gunCrossfade = 0.0f;
    }

    const int topoOverride = juce::jlimit(0, 3, static_cast<int>(apvts.getRawParameterValue("resTopology")->load()));
    const int activeTopo = topoOverride > 0 ? topoOverride - 1 : (gunMode == 1 ? 1 : (gunMode == 2 ? 2 : 0));
    const auto ringRange = breach::ring::rangeFor(activeTopo, gunMode);
    const double ringReference = std::sqrt(ringRange.shortest * ringRange.longest);
    const bool sniperActive = balanced && activeTopo == 0 && gunMode == 3;
    if (resetBalance || sniperActive != balancedSniperWasActive) balancedSniper.reset();
    balancedSniperWasActive = sniperActive;
    const float attackMs = apvts.getRawParameterValue("resAttack")->load();
    const float fbDamping = apvts.getRawParameterValue("fbDamping")->load();
    const float stiffness = apvts.getRawParameterValue("stiffness")->load();
    const bool useRings = getStringDspRevision() >= 2;
    const bool stringActive = useRings && (topoOverride == 2 || (topoOverride == 0 && gunMode == 1));
    stringBank.selectRate(sr);
    const bool resetStringSelection = stringResetRequested.exchange(false, std::memory_order_acquire);
    if (resetStringSelection || stringActive != stringWasActive)
        stringBank.reset();
    if (resetStringSelection)
    {
        // A stopped-transport engine change must not recall frozen legacy tails.
        for (auto* voices : { &voicesL, &voicesR })
            for (auto& voice : *voices)
            {
                std::fill(voice.combBuffer.begin(), voice.combBuffer.end(), 0.0f);
                voice.combWritePos = 0; voice.combLpState = 0.0f; voice.allpassState = 0.0f;
            }
    }
    stringWasActive = stringActive;
    const bool useStkFormant = getFormantDspRevision() >= 2;
    const bool formantActive = useStkFormant && (topoOverride == 3 || (topoOverride == 0 && gunMode == 2));
    formantBank.selectRate(sr);
    const bool resetFormant = formantResetRequested.exchange(false, std::memory_order_acquire);
    if (resetFormant || formantActive != formantWasActive) formantBank.reset();
    if (resetFormant && (topoOverride == 3 || (topoOverride == 0 && gunMode == 2)))
    {
        for (auto* voices : { &voicesL, &voicesR })
            for (auto& voice : *voices)
            {
                voice.svf1Ic1 = voice.svf1Ic2 = 0.0f;
                voice.svf2Ic1 = voice.svf2Ic2 = 0.0f;
            }
    }
    formantWasActive = formantActive;
    const bool useModal = getModalDspRevision() >= 2;
    const bool modalTopology = (gunMode == 0 || gunMode == 4) && (topoOverride == 0 || topoOverride == 1);
    const bool modalActive = useModal && modalTopology;
    modalBank.selectRate(sr);
    if (modalActive) modalBank.selectProfile(gunMode == 4);
    const bool resetModal = modalResetRequested.exchange(false, std::memory_order_acquire);
    if (resetModal || modalActive != modalWasActive) modalBank.reset();
    if (resetModal && modalTopology)
    {
        for (auto* voices : { &voicesL, &voicesR })
            for (auto& voice : *voices)
            {
                voice.svf1Ic1 = voice.svf1Ic2 = 0.0f;
                voice.svf2Ic1 = voice.svf2Ic2 = 0.0f;
            }
    }
    modalWasActive = modalActive;
    if (balanced)
    {
        int profile = -1;
        if (activeTopo == 1 && useRings) profile = 1;
        else if (activeTopo == 2 && useStkFormant) profile = 2;
        else if (activeTopo == 0 && gunMode == 3) profile = 3;
        else if (activeTopo == 0 && modalActive) profile = gunMode == 4 ? 4 : 0;
        const float trimTarget = breach::balance::trim(profile);
        if (balanceChanged) smWetTrim.setCurrentAndTargetValue(trimTarget);
        else smWetTrim.setTargetValue(trimTarget);
    }
    const float excHpfHz = apvts.getRawParameterValue("excHpf")->load();
    const float excDrive = apvts.getRawParameterValue("excDrive")->load();

    const int lfo1Shape = static_cast<int>(apvts.getRawParameterValue("lfo1Shape")->load());
    const bool lfo1Sync = apvts.getRawParameterValue("lfo1Sync")->load() > 0.5f;
    const float lfo1RateRaw = apvts.getRawParameterValue("lfo1Rate")->load();
    const float lfo1PhaseOff = apvts.getRawParameterValue("lfo1Phase")->load() / 360.0f;

    const int lfo2Shape = static_cast<int>(apvts.getRawParameterValue("lfo2Shape")->load());
    const bool lfo2Sync = apvts.getRawParameterValue("lfo2Sync")->load() > 0.5f;
    const float lfo2RateRaw = apvts.getRawParameterValue("lfo2Rate")->load();
    const float lfo2PhaseOff = apvts.getRawParameterValue("lfo2Phase")->load() / 360.0f;

    const float envAttMs = apvts.getRawParameterValue("envAttack")->load();
    const float envRelMs = apvts.getRawParameterValue("envRelease")->load();
    const float envAmt = apvts.getRawParameterValue("envAmount")->load();
    const float baseFreqShift = apvts.getRawParameterValue("freqShift")->load();
    const float unisonAmt = apvts.getRawParameterValue("stereoUnison")->load();

    // Modulation matrix depths
    const float mLfo1Warp  = apvts.getRawParameterValue("mod_lfo1_warp")->load();
    const float mLfo1Drift = apvts.getRawParameterValue("mod_lfo1_drift")->load();
    const float mLfo1Shift = apvts.getRawParameterValue("mod_lfo1_shift")->load();
    const float mLfo1Q     = apvts.getRawParameterValue("mod_lfo1_q")->load();
    const float mLfo1Tilt  = apvts.getRawParameterValue("mod_lfo1_tilt")->load();
    const float mLfo1Depth = apvts.getRawParameterValue("mod_lfo1_depth")->load();

    const float mLfo2Warp  = apvts.getRawParameterValue("mod_lfo2_warp")->load();
    const float mLfo2Drift = apvts.getRawParameterValue("mod_lfo2_drift")->load();
    const float mLfo2Shift = apvts.getRawParameterValue("mod_lfo2_shift")->load();
    const float mLfo2Q     = apvts.getRawParameterValue("mod_lfo2_q")->load();
    const float mLfo2Tilt  = apvts.getRawParameterValue("mod_lfo2_tilt")->load();
    const float mLfo2Depth = apvts.getRawParameterValue("mod_lfo2_depth")->load();

    const float mEnvWarp   = apvts.getRawParameterValue("mod_env_warp")->load();
    const float mEnvDrift  = apvts.getRawParameterValue("mod_env_drift")->load();
    const float mEnvShift  = apvts.getRawParameterValue("mod_env_shift")->load();
    const float mEnvQ      = apvts.getRawParameterValue("mod_env_q")->load();
    const float mEnvTilt   = apvts.getRawParameterValue("mod_env_tilt")->load();
    const float mEnvDepth  = apvts.getRawParameterValue("mod_env_depth")->load();

    const bool arpOn = apvts.getRawParameterValue("arpEnable")->load() > 0.5f;
    const int arpDirMode = static_cast<int>(apvts.getRawParameterValue("arpDirection")->load());
    const bool motionOn = apvts.getRawParameterValue("motionEnable")->load() > 0.5f;
    const float arpRateHz = apvts.getRawParameterValue("arpRate")->load();

    breach::reverb::Controls reverb;
    reverb.enabled = apvts.getRawParameterValue("reverbEnable")->load() > .5f;
    reverb.type = static_cast<int>(apvts.getRawParameterValue("reverbType")->load());
    reverb.mix = apvts.getRawParameterValue("reverbMix")->load();
    reverb.decay = apvts.getRawParameterValue("reverbDecay")->load();
    reverb.predelayMs = apvts.getRawParameterValue("reverbPreDelay")->load();
    reverb.damping = apvts.getRawParameterValue("reverbDamping")->load();
    reverb.width = apvts.getRawParameterValue("reverbWidth")->load();
    reverb.shimmer = apvts.getRawParameterValue("reverbShimmer")->load();
    reverbEngine.setControls(reverb);

    outputEq.setRates({currentSampleRate, sr});
    std::array<breach::eq::BandSettings, breach::eq::bandCount> eqBands;
    for (std::size_t band = 0; band < eqBands.size(); ++band) eqBands[band] = readOutputEqBand(band);
    const bool runEq = outputEq.setTargets(outputEqEnabled->load(std::memory_order_relaxed) > .5f, eqBands);

    const int mixCurve = static_cast<int>(apvts.getRawParameterValue("mixCurve")->load());
    const bool limiterOn = apvts.getRawParameterValue("softLimiter")->load() > 0.5f;

    std::array<float, kMaxVoices> targetFreqs {};
    int activeVoices = 8;
    rebuildTargetFrequencies(targetFreqs, activeVoices);

    std::array<float, kMaxVoices> voiceLevels {};
    std::array<float, kMaxVoices> voicePanL {};
    std::array<float, kMaxVoices> voicePanR {};
    std::array<float, kMaxVoices> voiceDecayMul {};

    const float tuneSpread = apvts.getRawParameterValue("voiceTuneSpread")->load();
    const float panSpread = apvts.getRawParameterValue("voicePanSpread")->load() * .01f;
    const bool macrosSet = tuneSpread > 0 || panSpread > 0;
    const bool resetMacroGlide = std::abs(voiceMacroSampleRate - sr) > 0;
    if (resetMacroGlide) voiceMacroSampleRate = sr;
    bool runMacros = macrosSet;
    for (int v = 0; v < kMaxVoices; ++v)
    {
        voiceLevels[static_cast<size_t>(v)] = apvts.getRawParameterValue("vLevel_" + juce::String(v))->load();
        const float pan = apvts.getRawParameterValue("vPan_" + juce::String(v))->load();
        const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        voicePanL[static_cast<size_t>(v)] = std::cos(panAngle);
        voicePanR[static_cast<size_t>(v)] = std::sin(panAngle);
        voiceDecayMul[static_cast<size_t>(v)] = apvts.getRawParameterValue("vDecay_" + juce::String(v))->load();
        const auto slot = static_cast<size_t>(v);
        auto& glide = voiceMacroGlides[slot];
        auto update = [&](auto& smoother, float target)
        {
            if (resetMacroGlide)
            {
                smoother.reset(sr, .018);
                smoother.setCurrentAndTargetValue(target);
            }
            else if (!macrosSet && !macrosWereActive) smoother.setCurrentAndTargetValue(target);
            else smoother.setTargetValue(target);
            runMacros = runMacros || smoother.isSmoothing();
        };
        const float spreadPan = activeVoices > 1 ? juce::jlimit(-1.0f, 1.0f, pan + panSpread * breach::ring::panPattern[slot]) : pan;
        const float spreadAngle = (spreadPan + 1.0f) * .25f * juce::MathConstants<float>::pi;
        const float tune = std::exp2(tuneSpread * breach::ring::tunePattern[slot] / 1200.0f);
        update(glide.pitchRatio, tune);
        update(glide.left, std::cos(spreadAngle));
        update(glide.right, std::sin(spreadAngle));
    }

    macrosWereActive = runMacros;

    const float hpfCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * excHpfHz / static_cast<float>(sr));
    const float attackCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(0.5f, attackMs) * 0.001f * static_cast<float>(sr)));
    const float modAttCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(1.0f, envAttMs) * 0.001f * static_cast<float>(sr)));
    const float modRelCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(5.0f, envRelMs) * 0.001f * static_cast<float>(sr)));
    const float crossfadeInc = 1.0f / (0.020f * static_cast<float>(sr));

    const float lfo1Hz = lfo1Sync ? static_cast<float>(hostBpm / 60.0) * juce::jlimit(0.125f, 8.0f, lfo1RateRaw * 0.25f) : lfo1RateRaw;
    const float lfo2Hz = lfo2Sync ? static_cast<float>(hostBpm / 60.0) * juce::jlimit(0.125f, 8.0f, lfo2RateRaw * 0.25f) : lfo2RateRaw;

    for (int i = 0; i < numSamples; ++i)
    {
        const float driveRaw   = smDrive.getNextValue();
        const float driveVal   = balanced ? breach::balance::driveCurve(driveRaw) : driveRaw;
        const float spaceVal   = smSpace.getNextValue();
        const float decayRaw   = smDecay.getNextValue();
        const float decayVal   = balanced ? breach::balance::decayCurve(decayRaw) : decayRaw;
        const double ringTarget = physicalRing ? breach::ring::targetSeconds(ringRange, decayRaw) : 0;
        const float toneVal    = smTone.getNextValue();
        const float densityVal = smDensity.getNextValue();
        const float mixVal     = smMix.getNextValue();
        const float inGainLin  = smInGain.getNextValue();
        const float outGainLin = smOutGain.getNextValue();
        const float widthVal   = smWidth.getNextValue();

        if (gunCrossfade < 1.0f)
            gunCrossfade = juce::jmin(1.0f, gunCrossfade + crossfadeInc);

        const float dryL = chL[i] * inGainLin;
        const float dryR = (numChannels > 1 ? chR[i] : chL[i]) * inGainLin;
        const float monoIn = 0.5f * (dryL + dryR);
        const float absIn = std::abs(monoIn);
        if (absIn > modEnvState)
            modEnvState += modAttCoeff * (absIn - modEnvState);
        else
            modEnvState += modRelCoeff * (absIn - modEnvState);

        // Advance LFOs
        lfo1Phase += lfo1Hz / static_cast<float>(sr);
        if (lfo1Phase >= 1000.0f) lfo1Phase -= 1000.0f;
        lfo2Phase += lfo2Hz / static_cast<float>(sr);
        if (lfo2Phase >= 1000.0f) lfo2Phase -= 1000.0f;

        const float lfo1Out = evaluateLfo(lfo1Shape, lfo1Phase + lfo1PhaseOff);
        const float lfo2Out = evaluateLfo(lfo2Shape, lfo2Phase + lfo2PhaseOff);
        const float envOut  = juce::jlimit(-1.0f, 1.0f, modEnvState * 4.0f * envAmt);

        // Mod matrix sums
        const float modWarp  = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Warp  + lfo2Out * mLfo2Warp  + envOut * mEnvWarp);
        const float modDrift = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Drift + lfo2Out * mLfo2Drift + envOut * mEnvDrift);
        const float modShift = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Shift + lfo2Out * mLfo2Shift + envOut * mEnvShift);
        const float modQ     = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Q     + lfo2Out * mLfo2Q     + envOut * mEnvQ);
        const float modTilt  = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Tilt  + lfo2Out * mLfo2Tilt  + envOut * mEnvTilt);
        const float modDepth = juce::jlimit(-1.0f, 1.0f, lfo1Out * mLfo1Depth + lfo2Out * mLfo2Depth + envOut * mEnvDepth);

        // Arpeggiator stepping
        if (arpOn)
        {
            arpPhase += static_cast<double>(arpRateHz) / sr;
            if (arpPhase >= 1.0)
            {
                arpPhase -= 1.0;
                if (arpDirMode == 0) // UP
                {
                    arpStep = (arpStep + 1) % activeVoices;
                }
                else if (arpDirMode == 1) // DOWN
                {
                    arpStep = (arpStep - 1 + activeVoices) % activeVoices;
                }
                else if (arpDirMode == 2) // UP-DN
                {
                    arpStep += arpDir;
                    if (arpStep >= activeVoices - 1) { arpStep = activeVoices - 1; arpDir = -1; }
                    else if (arpStep <= 0) { arpStep = 0; arpDir = 1; }
                }
                else if (arpDirMode == 3) // RAND
                {
                    rngState = rngState * 1664525u + 1013904223u;
                    arpStep = static_cast<int>((rngState >> 16) % static_cast<uint32_t>(activeVoices));
                }
                else // SYNC (chord pulse)
                {
                    arpStep = (arpStep + 1) % activeVoices;
                }
            }
        }

        // Excitation HPF + Saturation Drive
        const float hpL = hpfCoeff * (excHpfStateL + dryL - excPrevInL);
        const float hpR = hpfCoeff * (excHpfStateR + dryR - excPrevInR);
        excPrevInL = dryL;
        excPrevInR = dryR;
        excHpfStateL = hpL;
        excHpfStateR = hpR;

        const float excitationDrive = balanced ? breach::balance::driveCurve(smExcDrive.getNextValue()) : excDrive;
        const float totalDrive = 1.0f + (driveVal * 4.5f + excitationDrive * 5.5f);
        const float excL = nonlinearProcessingEnabled ? nonlinearEngine->excite(hpL, totalDrive, 0) : std::tanh(hpL * totalDrive);
        const float excR = nonlinearProcessingEnabled ? nonlinearEngine->excite(hpR, totalDrive, 1) : std::tanh(hpR * totalDrive);

        impactAnalysis.observeExcitation(excL, numChannels > 1 ? excR : excL);

        float resSumL = 0.0f;
        float resSumR = 0.0f;

        const float effectiveDensity = juce::jlimit(balanced ? 0.0f : 0.05f, 1.0f, densityVal + modDepth * 0.35f);
        const int voicesToRender = balanced ? activeVoices : juce::jlimit(1, activeVoices,
            static_cast<int>(std::ceil(effectiveDensity * static_cast<float>(activeVoices))));

        for (int v = 0; v < voicesToRender; ++v)
        {
            const size_t vs = static_cast<size_t>(v);
            auto& vL = voicesL[vs];
            auto& vR = voicesR[vs];

            // Arpeggiator & Motion gating
            float gateTarget = 1.0f;
            if (arpOn && arpDirMode != 4)
            {
                gateTarget = (v == arpStep) ? 1.15f : 0.12f;
            }
            if (motionOn)
            {
                const float phaseSpread = static_cast<float>(v) / static_cast<float>(voicesToRender);
                const float motionWave = 0.72f + 0.28f * std::sin((lfo1Phase + phaseSpread) * juce::MathConstants<float>::twoPi);
                gateTarget *= motionWave;
            }

            vL.attackEnv += attackCoeff * (gateTarget - vL.attackEnv);
            vR.attackEnv = vL.attackEnv;

            // Pitch warp + stereo unison detuning
            const float unisonOffset = ((v % 2 == 0) ? -1.0f : 1.0f) * unisonAmt * 0.008f;
            const float warpFactor = 1.0f + modWarp * 0.06f + stiffness * 0.0025f * static_cast<float>(v * v);
            auto& glide = voiceMacroGlides[vs];
            const float tuned = runMacros ? targetFreqs[vs] * glide.pitchRatio.getNextValue() : targetFreqs[vs];
            const float fL = juce::jlimit(25.0f, static_cast<float>(sr * 0.42), tuned * warpFactor * (1.0f + unisonOffset));
            const float fR = juce::jlimit(25.0f, static_cast<float>(sr * 0.42), tuned * warpFactor * (1.0f - unisonOffset));

            const double ringSeconds = physicalRing ? breach::ring::voiceSeconds(ringTarget, voiceDecayMul[vs]) : 0;
            const float vDecay = physicalRing ? 0 : juce::jlimit(0.05f, 0.9985f,
                0.82f + 0.176f * std::pow(juce::jlimit(0.05f, 1.0f, decayVal * voiceDecayMul[vs]), 0.35f));

            float outVoiceL = 0.0f;
            float outVoiceR = 0.0f;

            if (sniperActive)
            {
                const float d = juce::jlimit(0.0f, 1.0f, decayVal * voiceDecayMul[vs]);
                outVoiceL = balancedSniper.process(vs, 0, excL, fL, d, modQ, ringSeconds);
                outVoiceR = balancedSniper.process(vs, 1, excR, fR, d, modQ, ringSeconds);
                if (!nonlinearProcessingEnabled)
                {
                    outVoiceL = std::tanh(outVoiceL * 1.6f + .12f * outVoiceL * outVoiceL);
                    outVoiceR = std::tanh(outVoiceR * 1.6f + .12f * outVoiceR * outVoiceR);
                }
            }
            else if (activeTopo == 0 && modalActive)
            {
                const float modalDecay = juce::jlimit(0.0f, 1.0f, decayVal * voiceDecayMul[vs]);
                const float brightness = juce::jlimit(0.0f, 1.0f, toneVal + modTilt * 0.35f);
                outVoiceL = modalBank.process(vs, 0, excL, fL, modalDecay, stiffness, brightness, fbDamping, modQ, gunMode == 4, ringSeconds);
                outVoiceR = modalBank.process(vs, 1, excR, fR, modalDecay, stiffness, brightness, fbDamping, modQ, gunMode == 4, ringSeconds);
            }
            else if (activeTopo == 2 && useStkFormant)
            {
                const float formantDecay = juce::jlimit(0.0f, 1.0f, decayVal * voiceDecayMul[vs]);
                const float vowel = juce::jlimit(0.0f, 1.0f, toneVal + modTilt * 0.35f);
                outVoiceL = formantBank.process(vs, 0, excL, fL, vowel, formantDecay, fbDamping, modQ, ringSeconds);
                outVoiceR = formantBank.process(vs, 1, excR, fR, vowel, formantDecay, fbDamping, modQ, ringSeconds);
            }
            else if (activeTopo == 1 && useRings)
            {
                const float physicalDecay = juce::jlimit(0.0f, 1.0f, decayVal * voiceDecayMul[vs]);
                outVoiceL = stringBank.process(vs, 0, excL, fL, physicalDecay, fbDamping, stiffness, ringSeconds);
                outVoiceR = stringBank.process(vs, 1, excR, fR, physicalDecay, fbDamping, stiffness, ringSeconds);
            }
            else if (activeTopo == 1)
            {
                // Waveguide Fractional-Delay Comb String with Lagrange Interpolation & Damping
                const auto runComb = [&](VoiceDSP& vox, float inSample, float freq) -> float
                {
                    const int bufLen = static_cast<int>(vox.combBuffer.size());
                    if (bufLen < 16)
                        return 0.0f;

                    const float delaySamples = juce::jlimit(4.0f, static_cast<float>(bufLen - 6), static_cast<float>(sr) / freq);
                    const int dInt = static_cast<int>(delaySamples);
                    const float frac = delaySamples - static_cast<float>(dInt);

                    auto readAt = [&](int offset) -> float
                    {
                        int r = vox.combWritePos - offset;
                        while (r < 0) r += bufLen;
                        while (r >= bufLen) r -= bufLen;
                        return vox.combBuffer[static_cast<size_t>(r)];
                    };

                    // 4-point Lagrange interpolation
                    const float y0 = readAt(dInt - 1);
                    const float y1 = readAt(dInt);
                    const float y2 = readAt(dInt + 1);
                    const float y3 = readAt(dInt + 2);

                    const float c0 = -frac * (frac - 1.0f) * (frac - 2.0f) / 6.0f;
                    const float c1 =  (frac + 1.0f) * (frac - 1.0f) * (frac - 2.0f) / 2.0f;
                    const float c2 = -(frac + 1.0f) * frac * (frac - 2.0f) / 2.0f;
                    const float c3 =  (frac + 1.0f) * frac * (frac - 1.0f) / 6.0f;
                    const float delayed = c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;

                    // Loop damping lowpass
                    const float dampAlpha = juce::jlimit(0.08f, 0.92f, 1.0f - fbDamping * 0.78f);
                    vox.combLpState += dampAlpha * (delayed - vox.combLpState);

                    // Allpass dispersion for string stiffness
                    const float apCoeff = stiffness * 0.45f;
                    const float apOut = -apCoeff * vox.combLpState + vox.allpassState;
                    vox.allpassState = vox.combLpState + apCoeff * apOut;

                    const float fbGain = physicalRing
                        ? static_cast<float>(std::exp(-breach::ring::ln1000 / (ringSeconds * freq)))
                        : juce::jlimit(0.50f, 0.996f, vDecay);
                    const float nextSample = std::tanh(inSample * 0.35f + apOut * fbGain);

                    vox.combBuffer[static_cast<size_t>(vox.combWritePos)] = nextSample;
                    if (++vox.combWritePos >= bufLen)
                        vox.combWritePos = 0;

                    return apOut;
                };

                outVoiceL = runComb(vL, excL, fL);
                outVoiceR = runComb(vR, excR, fR);
            }
            else
            {
                // Zero-Delay Feedback TPT State Variable Bandpass (or Coupled Dual Formant)
                const float baseQ = (gunMode == 3 || gunMode == 4) ? 28.0f : 14.0f;
                const float Q = juce::jlimit(2.0f, 65.0f, baseQ * (0.4f + vDecay * 1.4f) * (1.0f + modQ * 0.5f));
                const float k = 1.0f / Q;

                const auto runSvf = [&](float inS, float freq, float& ic1, float& ic2) -> float
                {
                    const float g = std::tan(juce::MathConstants<float>::pi * freq / static_cast<float>(sr));
                    // Bilinear damping for the requested physical ring time.
                    const float damping = physicalRing ? static_cast<float>(
                        breach::ring::ln1000 * (1.0 + g * g) / (sr * g * ringSeconds * (1.0 + modQ * .2))) : k;
                    const float a1 = 1.0f / (1.0f + g * (g + damping));
                    const float a2 = g * a1;
                    const float a3 = g * a2;

                    const float v3 = inS - ic2;
                    const float v1 = a1 * ic1 + a2 * v3;
                    const float v2 = ic2 + a2 * ic1 + a3 * v3;
                    ic1 = 2.0f * v1 - ic1;
                    ic2 = 2.0f * v2 - ic2;
                    return v1 * std::sqrt((physicalRing ? baseQ : Q) * 0.25f);
                };

                float bpL = runSvf(excL, fL, vL.svf1Ic1, vL.svf1Ic2);
                float bpR = runSvf(excR, fR, vR.svf1Ic1, vR.svf1Ic2);

                if (activeTopo == 2) // Dual Formant (SMG)
                {
                    const float f2L = juce::jmin(static_cast<float>(sr * 0.42), fL * 1.498f);
                    const float f2R = juce::jmin(static_cast<float>(sr * 0.42), fR * 1.498f);
                    bpL = 0.65f * bpL + 0.55f * runSvf(excL + bpL * 0.25f, f2L, vL.svf2Ic1, vL.svf2Ic2);
                    bpR = 0.65f * bpR + 0.55f * runSvf(excR + bpR * 0.25f, f2R, vR.svf2Ic1, vR.svf2Ic2);
                }
                else if (gunMode == 3 && !nonlinearProcessingEnabled) // Legacy SNIPER asymmetric harmonic saturation
                {
                    bpL = std::tanh(bpL * 1.6f + 0.12f * bpL * bpL);
                    bpR = std::tanh(bpR * 1.6f + 0.12f * bpR * bpR);
                }

                outVoiceL = bpL;
                outVoiceR = bpR;
            }

            if (nonlinearProcessingEnabled)
            {
                const bool sniper = activeTopo != 1 && activeTopo != 2 && gunMode == 3;
                outVoiceL = nonlinearEngine->voice(outVoiceL, vs, 0, sniper);
                outVoiceR = nonlinearEngine->voice(outVoiceR, vs, 1, sniper);
            }

            const float densityWeight = balanced ? breach::balance::voiceWeight(effectiveDensity, v, activeVoices) : 1.0f;
            float vGain = voiceLevels[vs] * vL.attackEnv * densityWeight;
            if (physicalRing) vGain *= breach::ring::strikeGain(ringSeconds, ringReference);
            outVoiceL *= vGain;
            outVoiceR *= vGain;

            // Equal-power stereo panning per voice
            const float panL = runMacros ? glide.left.getNextValue() : voicePanL[vs];
            const float panR = runMacros ? glide.right.getNextValue() : voicePanR[vs];
            resSumL += outVoiceL * panL;
            resSumR += outVoiceR * panR;
            const float pannedL = outVoiceL * panL;
            const float pannedR = outVoiceR * panR;
            impactAnalysis.observeVoice(vs, pannedL, numChannels > 1 ? pannedR : pannedL);
        }

        if (!balanced && voicesToRender < activeVoices)
            for (int voice = voicesToRender; voice < activeVoices; ++voice)
                impactAnalysis.observeVoice(static_cast<std::size_t>(voice), 0, 0);

        if (runMacros && !balanced)
            for (int voice = voicesToRender; voice < activeVoices; ++voice)
            {
                auto& glide = voiceMacroGlides[static_cast<size_t>(voice)];
                glide.pitchRatio.skip(1);
                glide.left.skip(1);
                glide.right.skip(1);
            }

        const float normScale = balanced ? breach::balance::densityNormalization(effectiveDensity, activeVoices) : 1.0f / std::sqrt(static_cast<float>(voicesToRender));
        float wetL = resSumL * normScale;
        float wetR = resSumR * normScale;

        impactAnalysis.observeResonance(wetL, numChannels > 1 ? wetR : wetL);
        const breach::spatial::Stereo spatialInput { wetL, wetR };

        // Legacy Schroeder Allpass Space Diffuser (boosted in REVOLVER / Space)
        const float smearCurve = spaceVal * spaceVal * (3.0f - 2.0f * spaceVal);
        const float spaceAmount = juce::jlimit(0.0f, 1.0f,
            (shortSmear ? smearCurve : spaceVal + (gunMode == 4 ? .35f : 0.0f)) + modDrift * .25f);
        if (spaceAmount > 0.005f)
        {
            const float diffL = diffuserL[1].process(diffuserL[0].process(wetL));
            const float diffR = diffuserR[1].process(diffuserR[0].process(wetR));
            wetL = wetL * (1.0f - spaceAmount * 0.65f) + diffL * (spaceAmount * 0.85f);
            wetR = wetR * (1.0f - spaceAmount * 0.65f) + diffR * (spaceAmount * 0.85f);
        }

        // Hilbert Frequency Shifter
        const float totalShiftHz = baseFreqShift + modShift * 180.0f;
        if (std::abs(totalShiftHz) > 0.05f)
        {
            wetL = hilbertL.process(wetL, totalShiftHz, sr);
            wetR = hilbertR.process(wetR, totalShiftHz, sr);
        }

        // Both spatial paths stay warm. Crossfade only when the non-parameter
        // engine revision changes; the legacy endpoint preserves its exact math.
        const auto upgraded = spatialEngine.process(spatialInput, spaceAmount, totalShiftHz, shortSmear);
        spatialRevisionBlend += juce::jlimit(-spatialBlendStep, spatialBlendStep, spatialTarget - spatialRevisionBlend);
        if (spatialRevisionBlend >= 1.0f)
        {
            wetL = upgraded.left;
            wetR = upgraded.right;
        }
        else if (spatialRevisionBlend > 0.0f)
        {
            wetL += spatialRevisionBlend * (upgraded.left - wetL);
            wetR += spatialRevisionBlend * (upgraded.right - wetR);
        }

        // Tilt EQ driven by Tone macro + Mod Tilt
        const float tiltControl = juce::jlimit(0.0f, 1.0f, toneVal + modTilt * 0.35f);
        const float tiltCoeff = 0.12f;
        tiltLpL += tiltCoeff * (wetL - tiltLpL);
        tiltLpR += tiltCoeff * (wetR - tiltLpR);
        const float tiltHpL = wetL - tiltLpL;
        const float tiltHpR = wetR - tiltLpR;
        const float lowGain  = 1.45f - tiltControl * 0.90f;
        const float highGain = 0.55f + tiltControl * 1.15f;
        wetL = tiltLpL * lowGain + tiltHpL * highGain;
        wetR = tiltLpR * lowGain + tiltHpR * highGain;

        // Stereo Width M/S processing
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * widthVal;
        wetL = mid + side;
        wetR = mid - side;

        if (balanced)
        {
            const float gain = smWetTrim.getNextValue(); wetL *= gain; wetR *= gain;
        }

        // Dry / Wet Mix Curve
        float dryGain = 1.0f - mixVal;
        float wetGain = mixVal;
        if (mixCurve == 0) // EQUAL POWER
        {
            const float angle = mixVal * juce::MathConstants<float>::halfPi;
            dryGain = std::cos(angle);
            wetGain = std::sin(angle);
        }
        else if (mixCurve == 2) // S-CURVE
        {
            const float s = mixVal * mixVal * (3.0f - 2.0f * mixVal);
            dryGain = 1.0f - s;
            wetGain = s;
        }

        const float alignedDryL = nonlinearProcessingEnabled ? nonlinearEngine->dry(dryL, 0) : dryL;
        const float alignedDryR = nonlinearProcessingEnabled ? nonlinearEngine->dry(dryR, 1) : dryR;
        float mixedL = alignedDryL * dryGain + wetL * wetGain;
        float mixedR = alignedDryR * dryGain + wetR * wetGain;
        if (runEq)
        {
            const auto equalized = outputEq.process({mixedL, mixedR});
            mixedL = static_cast<float>(equalized.left);
            mixedR = static_cast<float>(equalized.right);
        }
        if (!reverbEngine.idle())
        {
            const auto reverberated = reverbEngine.process({mixedL, mixedR});
            mixedL = reverberated.left;
            mixedR = reverberated.right;
        }
        float outL = mixedL * outGainLin;
        float outR = mixedR * outGainLin;

        if (limiterOn)
        {
            outL = std::tanh(outL);
            outR = std::tanh(outR);
        }

        if (nonlinearProcessingEnabled)
        {
            outL = nonlinearEngine->output(outL, 0);
            outR = nonlinearEngine->output(outR, 1);
        }
        chL[i] = outL;
        if (numChannels > 1)
            chR[i] = outR;
    }

    // Keep inaudible slots in sync when the voice count or legacy density changes.
    if (runMacros)
    {
        for (int voice = activeVoices; voice < kMaxVoices; ++voice)
        {
            auto& glide = voiceMacroGlides[static_cast<size_t>(voice)];
            glide.pitchRatio.skip(numSamples);
            glide.left.skip(numSamples);
            glide.right.skip(numSamples);
        }
    }

    impactAnalysis.decaySilentVoices(static_cast<std::size_t>(activeVoices), numSamples);
    impactAnalysis.publishCore(telemetry.impact);
}

void BreachResonatorAudioProcessor::configureNonlinearProcessing(int osChoice, bool enabled)
{
    const int choice = osChoice + (enabled ? 3 : 0);
    if (choice == nonlinearProcessingChoice) return;
    nonlinearProcessingChoice = choice;
    nonlinearProcessingEnabled = enabled;
    nonlinearEngine->prepare(currentSampleRate * static_cast<double>(1 << osChoice), 1 << osChoice);
    if (enabled)
    {
        nonlinearOversampler2x->reset();
        nonlinearOversampler4x->reset();
    }
    else
    {
        oversampler2x->reset(); oversampler4x->reset();
    }
    refreshNonlinearLatency();
}

void BreachResonatorAudioProcessor::refreshNonlinearLatency()
{
    const auto osChoice = static_cast<size_t>(juce::jlimit(0, 2, static_cast<int>(apvts.getRawParameterValue("oversampling")->load())));
    // Modern oversamplers compensate fractional latency internally. The two
    // extra base samples align both ADAA stages for every mode and OS choice.
    const int latency = getNonlinearDspRevision() >= 2 ? nonlinearLatencies[osChoice].load() : 0;
    if (getLatencySamples() != latency) setLatencySamples(latency);
}

void BreachResonatorAudioProcessor::setNonlinearDspRevision(int revision)
{
    nonlinearDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    refreshNonlinearLatency();
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setSmearDspRevision(int revision)
{
    smearDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

double BreachResonatorAudioProcessor::getTailLengthSeconds() const
{
    if (apvts.getRawParameterValue("reverbEnable")->load() > .5f) return 20;
    return usesRingTime() ? breach::ring::maxSeconds : 4;
}

void BreachResonatorAudioProcessor::setRingTimeEnabled(bool enabled)
{
    ringTimeEnabled.store(enabled, std::memory_order_relaxed);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

double BreachResonatorAudioProcessor::nominalRingSeconds(float decay) const noexcept
{
    const int gun = static_cast<int>(apvts.getRawParameterValue("gunMode")->load());
    const int override = static_cast<int>(apvts.getRawParameterValue("resTopology")->load());
    const int topology = override > 0 ? override - 1 : (gun == 1 ? 1 : (gun == 2 ? 2 : 0));
    return breach::ring::targetSeconds(breach::ring::rangeFor(topology, gun), decay);
}

void BreachResonatorAudioProcessor::setBalanceRevision(int revision)
{
    balanceRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    balanceResetRequested.store(true, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setModalDspRevision(int revision)
{
    modalDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    modalResetRequested.store(true, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setFormantDspRevision(int revision)
{
    formantDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    formantResetRequested.store(true, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setStringDspRevision(int revision)
{
    stringDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    stringResetRequested.store(true, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setSpatialDspRevision(int revision)
{
    spatialDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::setFaustOttEnabled(bool enabled)
{
    faustOttEnabled.store(enabled, std::memory_order_relaxed);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void BreachResonatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("ottDspRevision", usesFaustOtt() ? 2 : 1, nullptr);
    state.setProperty("smearDspRevision", getSmearDspRevision(), nullptr);
    state.setProperty("ringTimeRevision", usesRingTime() ? 2 : 1, nullptr);
    state.setProperty("balanceRevision", getBalanceRevision(), nullptr);
    state.setProperty("modalDspRevision", getModalDspRevision(), nullptr);
    state.setProperty("formantDspRevision", getFormantDspRevision(), nullptr);
    state.setProperty("stringDspRevision", getStringDspRevision(), nullptr);
    state.setProperty("nonlinearDspRevision", getNonlinearDspRevision(), nullptr);
    state.setProperty("spatialDspRevision", getSpatialDspRevision(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BreachResonatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        // Unversioned v1.1.2 sessions retain their original sound.
        const int revision = static_cast<int>(state.getProperty("spatialDspRevision", 1));
        const int nonlinearRevision = static_cast<int>(state.getProperty("nonlinearDspRevision", 1));
        const int stringRevision = static_cast<int>(state.getProperty("stringDspRevision", 1));
        const int formantRevision = static_cast<int>(state.getProperty("formantDspRevision", 1));
        const int modalRevision = static_cast<int>(state.getProperty("modalDspRevision", 1));
        const int balance = static_cast<int>(state.getProperty("balanceRevision", 1));
        // Missing module parameters must restore their defaults even when an
        // older session replaces a state that had those modules active.
        auto restoreDefaultIfMissing = [&](const juce::String& id)
        {
            if (state.getChildWithProperty("id", id).isValid()) return;
            auto* parameter = apvts.getParameter(id);
            juce::ValueTree child("PARAM");
            child.setProperty("id", id, nullptr);
            child.setProperty("value", parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
            state.appendChild(child, nullptr);
        };
        for (const auto* id : {"ottEnable", "ottDepth", "ottTime", "ottUp", "ottDown", "ottInGain", "ottOutGain", "ottLowSplit", "ottHighSplit",
                               "reverbEnable", "reverbType", "reverbMix", "reverbDecay", "reverbPreDelay", "reverbDamping", "reverbWidth", "reverbShimmer",
                               "outputEqEnable", "voiceTuneSpread", "voicePanSpread"})
            restoreDefaultIfMissing(id);
        for (std::size_t band = 0; band < breach::eq::bandCount; ++band)
            for (const auto* suffix : {"Enable", "Type", "Freq", "Gain", "Q"})
                restoreDefaultIfMissing(outputEqBandId(band, suffix));
        const int smear = static_cast<int>(state.getProperty("smearDspRevision", 1));
        smearDspRevision.store(smear >= 2 ? 2 : 1, std::memory_order_release);
        apvts.replaceState(state);
        faustOttEnabled.store(static_cast<int>(state.getProperty("ottDspRevision", 1)) >= 2, std::memory_order_relaxed);
        ringTimeEnabled.store(static_cast<int>(state.getProperty("ringTimeRevision", 1)) >= 2, std::memory_order_relaxed);
        ancillaryResetRequested.store(true, std::memory_order_release);
        balanceRevision.store(balance >= 2 ? 2 : 1, std::memory_order_release);
        balanceResetRequested.store(true, std::memory_order_release);
        modalDspRevision.store(modalRevision >= 2 ? 2 : 1, std::memory_order_release);
        modalResetRequested.store(true, std::memory_order_release);
        formantDspRevision.store(formantRevision >= 2 ? 2 : 1, std::memory_order_release);
        formantResetRequested.store(true, std::memory_order_release);
        stringDspRevision.store(stringRevision >= 2 ? 2 : 1, std::memory_order_release);
        stringResetRequested.store(true, std::memory_order_release);
        nonlinearDspRevision.store(nonlinearRevision >= 2 ? 2 : 1, std::memory_order_release);
        spatialDspRevision.store(revision >= 2 ? 2 : 1, std::memory_order_release);
        refreshNonlinearLatency();
    }
}

juce::AudioProcessorEditor* BreachResonatorAudioProcessor::createEditor()
{
    return new BreachResonatorAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreachResonatorAudioProcessor();
}
