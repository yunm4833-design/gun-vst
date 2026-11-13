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
    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
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

    return { params.begin(), params.end() };
}

void BreachResonatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);

    oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));

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
    excHpfStateL = excHpfStateR = 0.0f;
    excPrevInL = excPrevInR = 0.0f;
    tiltLpL = tiltLpR = 0.0f;
    envFast = envSlow = modEnvState = transientHold = 0.0f;

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
    oversampler2x->reset();
    oversampler4x->reset();
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
    if (numSamples <= 0)
        return;

    if (buffer.getNumChannels() == 1)
    {
        // Safely process mono if needed
    }

    if (panicRequested.exchange(false, std::memory_order_acquire))
    {
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

    const int osChoice = juce::jlimit(0, 2, static_cast<int>(apvts.getRawParameterValue("oversampling")->load()));
    juce::dsp::AudioBlock<float> baseBlock(buffer);

    if (osChoice == 1)
    {
        auto osBlock = oversampler2x->processSamplesUp(baseBlock);
        processCoreBlock(osBlock, currentSampleRate * 2.0, hostBpm);
        oversampler2x->processSamplesDown(baseBlock);
    }
    else if (osChoice == 2)
    {
        auto osBlock = oversampler4x->processSamplesUp(baseBlock);
        processCoreBlock(osBlock, currentSampleRate * 4.0, hostBpm);
        oversampler4x->processSamplesDown(baseBlock);
    }
    else
    {
        processCoreBlock(baseBlock, currentSampleRate, hostBpm);
    }
}

void BreachResonatorAudioProcessor::processCoreBlock(juce::dsp::AudioBlock<float>& block, double sr, double hostBpm)
{
    const int numChannels = static_cast<int>(block.getNumChannels());
    const int numSamples = static_cast<int>(block.getNumSamples());
    if (numChannels <= 0 || numSamples <= 0)
        return;

    float* chL = block.getChannelPointer(0);
    float* chR = (numChannels > 1) ? block.getChannelPointer(1) : chL;

    // Update parameter smoothing time
    const float smoothSec = apvts.getRawParameterValue("paramSmoothMs")->load() * 0.001f;
    smDrive.reset(sr, smoothSec);
    smSpace.reset(sr, smoothSec);
    smDecay.reset(sr, smoothSec);
    smTone.reset(sr, smoothSec);
    smDensity.reset(sr, smoothSec);
    smMix.reset(sr, smoothSec);
    smInGain.reset(sr, smoothSec);
    smOutGain.reset(sr, smoothSec);
    smWidth.reset(sr, smoothSec);

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
    const float attackMs = apvts.getRawParameterValue("resAttack")->load();
    const float fbDamping = apvts.getRawParameterValue("fbDamping")->load();
    const float stiffness = apvts.getRawParameterValue("stiffness")->load();
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

    const int mixCurve = static_cast<int>(apvts.getRawParameterValue("mixCurve")->load());
    const bool limiterOn = apvts.getRawParameterValue("softLimiter")->load() > 0.5f;

    std::array<float, kMaxVoices> targetFreqs {};
    int activeVoices = 8;
    rebuildTargetFrequencies(targetFreqs, activeVoices);

    std::array<float, kMaxVoices> voiceLevels {};
    std::array<float, kMaxVoices> voicePanL {};
    std::array<float, kMaxVoices> voicePanR {};
    std::array<float, kMaxVoices> voiceDecayMul {};

    for (int v = 0; v < kMaxVoices; ++v)
    {
        voiceLevels[static_cast<size_t>(v)] = apvts.getRawParameterValue("vLevel_" + juce::String(v))->load();
        const float pan = apvts.getRawParameterValue("vPan_" + juce::String(v))->load();
        const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        voicePanL[static_cast<size_t>(v)] = std::cos(panAngle);
        voicePanR[static_cast<size_t>(v)] = std::sin(panAngle);
        voiceDecayMul[static_cast<size_t>(v)] = apvts.getRawParameterValue("vDecay_" + juce::String(v))->load();
    }

    const float hpfCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * excHpfHz / static_cast<float>(sr));
    const float attackCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(0.5f, attackMs) * 0.001f * static_cast<float>(sr)));
    const float modAttCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(1.0f, envAttMs) * 0.001f * static_cast<float>(sr)));
    const float modRelCoeff = 1.0f - std::exp(-1.0f / (juce::jmax(5.0f, envRelMs) * 0.001f * static_cast<float>(sr)));
    const float crossfadeInc = 1.0f / (0.020f * static_cast<float>(sr));

    const float lfo1Hz = lfo1Sync ? static_cast<float>(hostBpm / 60.0) * juce::jlimit(0.125f, 8.0f, lfo1RateRaw * 0.25f) : lfo1RateRaw;
    const float lfo2Hz = lfo2Sync ? static_cast<float>(hostBpm / 60.0) * juce::jlimit(0.125f, 8.0f, lfo2RateRaw * 0.25f) : lfo2RateRaw;

    float sumInSq = 0.0f;
    float sumOutSq = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float driveVal   = smDrive.getNextValue();
        const float spaceVal   = smSpace.getNextValue();
        const float decayVal   = smDecay.getNextValue();
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
        sumInSq += monoIn * monoIn;

        // Transient detector & envelope follower
        const float absIn = std::abs(monoIn);
        envFast = 0.96f * envFast + 0.04f * absIn;
        envSlow = 0.9985f * envSlow + 0.0015f * absIn;
        const float delta = juce::jmax(0.0f, envFast - envSlow * 1.45f);
        if (delta > 0.035f)
            transientHold = juce::jmin(1.0f, transientHold + delta * 6.0f);
        else
            transientHold *= 0.9994f;

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

        const float totalDrive = 1.0f + (driveVal * 4.5f + excDrive * 5.5f);
        const float excL = std::tanh(hpL * totalDrive);
        const float excR = std::tanh(hpR * totalDrive);

        // Determine active topology (0=SVF Bandpass, 1=Waveguide Comb, 2=Dual Formant)
        int activeTopo = 0;
        if (topoOverride > 0)
            activeTopo = topoOverride - 1;
        else if (gunMode == 1)
            activeTopo = 1; // SHOTGUN -> Waveguide Comb
        else if (gunMode == 2)
            activeTopo = 2; // SMG -> Dual Formant

        float resSumL = 0.0f;
        float resSumR = 0.0f;

        const float effectiveDensity = juce::jlimit(0.05f, 1.0f, densityVal + modDepth * 0.35f);
        const int voicesToRender = juce::jlimit(1, activeVoices,
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
            const float fL = juce::jlimit(25.0f, static_cast<float>(sr * 0.42), targetFreqs[vs] * warpFactor * (1.0f + unisonOffset));
            const float fR = juce::jlimit(25.0f, static_cast<float>(sr * 0.42), targetFreqs[vs] * warpFactor * (1.0f - unisonOffset));

            const float vDecay = juce::jlimit(0.05f, 0.9985f,
                0.82f + 0.176f * std::pow(juce::jlimit(0.05f, 1.0f, decayVal * voiceDecayMul[vs]), 0.35f));

            float outVoiceL = 0.0f;
            float outVoiceR = 0.0f;

            if (activeTopo == 1)
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

                    const float fbGain = juce::jlimit(0.50f, 0.996f, vDecay);
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
                    const float a1 = 1.0f / (1.0f + g * (g + k));
                    const float a2 = g * a1;
                    const float a3 = g * a2;

                    const float v3 = inS - ic2;
                    const float v1 = a1 * ic1 + a2 * v3;
                    const float v2 = ic2 + a2 * ic1 + a3 * v3;
                    ic1 = 2.0f * v1 - ic1;
                    ic2 = 2.0f * v2 - ic2;
                    return v1 * std::sqrt(Q * 0.25f);
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
                else if (gunMode == 3) // SNIPER asymmetric harmonic saturation
                {
                    bpL = std::tanh(bpL * 1.6f + 0.12f * bpL * bpL);
                    bpR = std::tanh(bpR * 1.6f + 0.12f * bpR * bpR);
                }

                outVoiceL = bpL;
                outVoiceR = bpR;
            }

            const float vGain = voiceLevels[vs] * vL.attackEnv;
            outVoiceL *= vGain;
            outVoiceR *= vGain;

            vL.energyMetric = 0.995f * vL.energyMetric + 0.005f * (std::abs(outVoiceL) + std::abs(outVoiceR));

            // Equal-power stereo panning per voice
            resSumL += outVoiceL * voicePanL[vs];
            resSumR += outVoiceR * voicePanR[vs];
        }

        const float normScale = 1.0f / std::sqrt(static_cast<float>(voicesToRender));
        float wetL = resSumL * normScale;
        float wetR = resSumR * normScale;

        // Schroeder Allpass Space Diffuser (boosted in REVOLVER / Space)
        const float spaceAmount = juce::jlimit(0.0f, 1.0f, spaceVal + (gunMode == 4 ? 0.35f : 0.0f) + modDrift * 0.25f);
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

        float outL = (dryL * dryGain + wetL * wetGain) * outGainLin;
        float outR = (dryR * dryGain + wetR * wetGain) * outGainLin;

        if (limiterOn)
        {
            outL = std::tanh(outL);
            outR = std::tanh(outR);
        }

        chL[i] = outL;
        if (numChannels > 1)
            chR[i] = outR;

        const float monoOut = 0.5f * (outL + outR);
        sumOutSq += monoOut * monoOut;
    }

    // Publish lock-free UI telemetry
    const float invN = 1.0f / static_cast<float>(numSamples);
    telemetry.inputRms.store(std::sqrt(sumInSq * invN), std::memory_order_relaxed);
    telemetry.outputRms.store(std::sqrt(sumOutSq * invN), std::memory_order_relaxed);
    telemetry.transientImpulse.store(transientHold, std::memory_order_relaxed);
    telemetry.activeArpVoice.store(arpStep, std::memory_order_relaxed);
    telemetry.lfo1Value.store(evaluateLfo(lfo1Shape, lfo1Phase + lfo1PhaseOff), std::memory_order_relaxed);
    telemetry.lfo2Value.store(evaluateLfo(lfo2Shape, lfo2Phase + lfo2PhaseOff), std::memory_order_relaxed);
    telemetry.envValue.store(juce::jlimit(0.0f, 1.0f, modEnvState * 4.0f), std::memory_order_relaxed);

    for (int v = 0; v < kMaxVoices; ++v)
        telemetry.voiceEnergies[static_cast<size_t>(v)].store(voicesL[static_cast<size_t>(v)].energyMetric, std::memory_order_relaxed);
}

void BreachResonatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BreachResonatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* BreachResonatorAudioProcessor::createEditor()
{
    return new BreachResonatorAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreachResonatorAudioProcessor();
}
