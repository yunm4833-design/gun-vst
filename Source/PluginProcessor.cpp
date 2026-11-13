#include "PluginProcessor.h"
#include "PluginEditor.h"

BreachResonatorAudioProcessor::BreachResonatorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", breach::createParameterLayout())
{
    using namespace breach;

    modeParam        = apvts.getRawParameterValue(ParamID::mode);
    chordPresetParam = apvts.getRawParameterValue(ParamID::chordPreset);
    rootNoteParam    = apvts.getRawParameterValue(ParamID::rootNote);
    octaveParam      = apvts.getRawParameterValue(ParamID::octave);
    spreadParam      = apvts.getRawParameterValue(ParamID::spread);
    midiModeParam    = apvts.getRawParameterValue(ParamID::midiMode);
    attackParam      = apvts.getRawParameterValue(ParamID::attack);
    decayParam       = apvts.getRawParameterValue(ParamID::decay);

    for (size_t i = 0; i < 12; ++i)
    {
        noteParams[i] = apvts.getRawParameterValue(ParamID::noteToggles[i]);
        apvts.addParameterListener(ParamID::noteToggles[i], this);
    }
    apvts.addParameterListener(ParamID::chordPreset, this);

    arpOnParam       = apvts.getRawParameterValue(ParamID::arpOn);
    arpSyncParam     = apvts.getRawParameterValue(ParamID::arpSync);
    arpRateSyncParam = apvts.getRawParameterValue(ParamID::arpRateSync);
    arpRateHzParam   = apvts.getRawParameterValue(ParamID::arpRateHz);
    arpPatternParam  = apvts.getRawParameterValue(ParamID::arpPattern);

    warpParam        = apvts.getRawParameterValue(ParamID::warp);
    driftParam       = apvts.getRawParameterValue(ParamID::drift);
    shiftHzParam     = apvts.getRawParameterValue(ParamID::shiftHz);

    motionOnParam       = apvts.getRawParameterValue(ParamID::motionOn);
    motionStrengthParam = apvts.getRawParameterValue(ParamID::motionStrength);
    motionSyncParam     = apvts.getRawParameterValue(ParamID::motionSync);
    motionRateSyncParam = apvts.getRawParameterValue(ParamID::motionRateSync);
    motionRateHzParam   = apvts.getRawParameterValue(ParamID::motionRateHz);
    motionShapeParam    = apvts.getRawParameterValue(ParamID::motionShape);
    unisonParam         = apvts.getRawParameterValue(ParamID::unison);

    filterTiltParam  = apvts.getRawParameterValue(ParamID::filterTilt);
    filterQParam     = apvts.getRawParameterValue(ParamID::filterQ);
    filterLowParam   = apvts.getRawParameterValue(ParamID::filterLow);
    filterHighParam  = apvts.getRawParameterValue(ParamID::filterHigh);
    depthParam       = apvts.getRawParameterValue(ParamID::depth);

    // Stage 2 Pointers
    voiceCapParam = apvts.getRawParameterValue(ParamID::voiceCap);
    for (size_t v = 0; v < kMaxVoices; ++v)
    {
        voiceLevelParams[v]    = apvts.getRawParameterValue(ParamID::voiceLevel[v]);
        voicePanParams[v]      = apvts.getRawParameterValue(ParamID::voicePan[v]);
        voiceDetuneParams[v]   = apvts.getRawParameterValue(ParamID::voiceDetune[v]);
        voiceDecayMulParams[v] = apvts.getRawParameterValue(ParamID::voiceDecayMul[v]);
    }

    resModelParam  = apvts.getRawParameterValue(ParamID::resModel);
    fbDampingParam = apvts.getRawParameterValue(ParamID::fbDamping);
    stiffnessParam = apvts.getRawParameterValue(ParamID::stiffness);
    excHpfHzParam  = apvts.getRawParameterValue(ParamID::excHpfHz);
    excDriveParam  = apvts.getRawParameterValue(ParamID::excDrive);

    advLfo1ShapeParam    = apvts.getRawParameterValue(ParamID::advLfo1Shape);
    advLfo1RateHzParam   = apvts.getRawParameterValue(ParamID::advLfo1RateHz);
    advLfo1RateSyncParam = apvts.getRawParameterValue(ParamID::advLfo1RateSync);
    advLfo1SyncParam     = apvts.getRawParameterValue(ParamID::advLfo1Sync);
    advLfo1PhaseParam    = apvts.getRawParameterValue(ParamID::advLfo1Phase);

    advLfo2ShapeParam    = apvts.getRawParameterValue(ParamID::advLfo2Shape);
    advLfo2RateHzParam   = apvts.getRawParameterValue(ParamID::advLfo2RateHz);
    advLfo2RateSyncParam = apvts.getRawParameterValue(ParamID::advLfo2RateSync);
    advLfo2SyncParam     = apvts.getRawParameterValue(ParamID::advLfo2Sync);
    advLfo2PhaseParam    = apvts.getRawParameterValue(ParamID::advLfo2Phase);

    advEnvAttackParam    = apvts.getRawParameterValue(ParamID::advEnvAttack);
    advEnvReleaseParam   = apvts.getRawParameterValue(ParamID::advEnvRelease);
    advEnvAmountParam    = apvts.getRawParameterValue(ParamID::advEnvAmount);

    for (size_t s = 0; s < 3; ++s)
        for (size_t d = 0; d < 6; ++d)
            modMatrixParams[s][d] = apvts.getRawParameterValue(ParamID::modMatrix[s][d]);

    inputGainDbParam     = apvts.getRawParameterValue(ParamID::inputGainDb);
    outputGainDbParam    = apvts.getRawParameterValue(ParamID::outputGainDb);
    stereoWidthParam     = apvts.getRawParameterValue(ParamID::stereoWidth);
    dryWetCurveParam     = apvts.getRawParameterValue(ParamID::dryWetCurve);
    oversamplingParam    = apvts.getRawParameterValue(ParamID::oversampling);
    limiterOnParam       = apvts.getRawParameterValue(ParamID::limiterOn);
    limiterThreshDbParam = apvts.getRawParameterValue(ParamID::limiterThreshDb);
    smoothTimeMsParam    = apvts.getRawParameterValue(ParamID::smoothTimeMs);

    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
}

BreachResonatorAudioProcessor::~BreachResonatorAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener(breach::ParamID::chordPreset, this);
    for (size_t i = 0; i < 12; ++i)
        apvts.removeParameterListener(breach::ParamID::noteToggles[i], this);
}

void BreachResonatorAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (isSyncingChordSwitches.load(std::memory_order_acquire))
        return;

    if (parameterID == breach::ParamID::chordPreset)
    {
        const int presetIdx = static_cast<int>(std::round(newValue));
        if (presetIdx > 0)
        {
            pendingPresetToApply.store(presetIdx, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }
    else
    {
        const int currentPreset = static_cast<int>(chordPresetParam->load(std::memory_order_relaxed));
        if (currentPreset != 0)
        {
            pendingSwitchToCustom.store(true, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }
}

void BreachResonatorAudioProcessor::handleAsyncUpdate()
{
    const int presetIdx = pendingPresetToApply.exchange(-1, std::memory_order_acq_rel);
    if (presetIdx > 0 && presetIdx < static_cast<int>(breach::ChordPresetType::NumPresets))
    {
        isSyncingChordSwitches.store(true, std::memory_order_release);
        const auto mask = breach::getPitchClassMaskForPreset(static_cast<breach::ChordPresetType>(presetIdx));
        for (size_t i = 0; i < 12; ++i)
        {
            if (auto* p = apvts.getParameter(breach::ParamID::noteToggles[i]))
            {
                const float target = mask[i] ? 1.0f : 0.0f;
                if (std::abs(p->getValue() - target) > 0.01f)
                    p->setValueNotifyingHost(target);
            }
        }
        isSyncingChordSwitches.store(false, std::memory_order_release);
        pendingSwitchToCustom.store(false, std::memory_order_release);
        return;
    }

    if (pendingSwitchToCustom.exchange(false, std::memory_order_acq_rel))
    {
        isSyncingChordSwitches.store(true, std::memory_order_release);
        if (auto* p = apvts.getParameter(breach::ParamID::chordPreset))
            p->setValueNotifyingHost(0.0f);
        isSyncingChordSwitches.store(false, std::memory_order_release);
    }
}

void BreachResonatorAudioProcessor::setMode(int index)
{
    const int clamped = juce::jlimit(0, static_cast<int>(breach::ResonatorMode::NumModes) - 1, index);
    currentModeIndex.store(clamped, std::memory_order_relaxed);

    if (auto* param = apvts.getParameter(breach::ParamID::mode))
    {
        const float normalized = param->convertTo0to1(static_cast<float>(clamped));
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalized);
        param->endChangeGesture();
    }
}

void BreachResonatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    baseSampleRate = std::max(8000.0, sampleRate);
    maxBlockSizePrepared = std::max(32, samplesPerBlock);

    oversampler2x->initProcessing(static_cast<size_t>(maxBlockSizePrepared));
    oversampler4x->initProcessing(static_cast<size_t>(maxBlockSizePrepared));
    oversampler2x->reset();
    oversampler4x->reset();

    const int osChoice = static_cast<int>(oversamplingParam->load(std::memory_order_relaxed));
    lastOversamplingChoice = osChoice;
    const double osMul = (osChoice == 1) ? 2.0 : ((osChoice == 2) ? 4.0 : 1.0);
    preparedEngineRate = baseSampleRate * osMul;

    modeCrossfadeTotalSamples = std::max(1, static_cast<int>(std::round(preparedEngineRate * 0.020)));
    modeCrossfadeSamplesRemaining = 0;

    bankPrimary.prepare(preparedEngineRate);
    bankSecondary.prepare(preparedEngineRate);
    voicingEngine.resetMidi();

    excHpf.prepare(preparedEngineRate);

    smearDiffuserL1.prepare(preparedEngineRate, 25.0f);
    smearDiffuserL2.prepare(preparedEngineRate, 40.0f);
    smearDiffuserR1.prepare(preparedEngineRate, 28.0f);
    smearDiffuserR2.prepare(preparedEngineRate, 43.0f);

    shifterL.prepare(preparedEngineRate);
    shifterR.prepare(preparedEngineRate);

    motionLfo.prepare(preparedEngineRate);
    advLfo1.prepare(preparedEngineRate);
    advLfo2.prepare(preparedEngineRate);
    advEnvFollower.prepare(preparedEngineRate);

    tiltL.prepare(preparedEngineRate);
    tiltR.prepare(preparedEngineRate);
    wetHpfL.prepare(preparedEngineRate);
    wetHpfR.prepare(preparedEngineRate);
    wetLpfL.prepare(preparedEngineRate);
    wetLpfR.prepare(preparedEngineRate);

    lastSmoothTimeMs = smoothTimeMsParam->load(std::memory_order_relaxed);
    const double rampSec = static_cast<double>(lastSmoothTimeMs) * 0.001;
    smoothAttack.reset(preparedEngineRate, rampSec);
    smoothDecay.reset(preparedEngineRate, rampSec);
    smoothWarp.reset(preparedEngineRate, rampSec);
    smoothDrift.reset(preparedEngineRate, rampSec);
    smoothShiftHz.reset(preparedEngineRate, rampSec);
    smoothMotionStrength.reset(preparedEngineRate, rampSec);
    smoothUnison.reset(preparedEngineRate, rampSec);
    smoothFilterTilt.reset(preparedEngineRate, rampSec);
    smoothFilterQ.reset(preparedEngineRate, rampSec);
    smoothFilterLow.reset(preparedEngineRate, rampSec);
    smoothFilterHigh.reset(preparedEngineRate, rampSec);
    smoothDepth.reset(preparedEngineRate, rampSec);
    smoothInputGain.reset(preparedEngineRate, rampSec);
    smoothOutputGain.reset(preparedEngineRate, rampSec);
    smoothStereoWidth.reset(preparedEngineRate, rampSec);

    activeMode = static_cast<int>(modeParam->load(std::memory_order_relaxed));
    previousMode = activeMode;
    currentModeIndex.store(activeMode, std::memory_order_relaxed);

    arpPhaseAccumulator = 0.0;
    arpStepCounter = 0;
    fastEnv = slowEnv = 0.0f;
    transientCooldownSamples = 0;
    manualImpulseSamplesRemaining = 0;
}

void BreachResonatorAudioProcessor::releaseResources()
{
    bankPrimary.reset();
    bankSecondary.reset();
    oversampler2x->reset();
    oversampler4x->reset();
}

bool BreachResonatorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    return true;
}

double BreachResonatorAudioProcessor::getDivisionInBeats(int rateIndex) noexcept
{
    static constexpr std::array<double, 9> kBeats{
        4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125
    };
    return kBeats[static_cast<size_t>(juce::jlimit(0, 8, rateIndex))];
}

void BreachResonatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples == 0)
        return;

    // Handle MIDI Note-On/Off
    const bool newMidiNoteTriggered = voicingEngine.handleMidiBuffer(midiMessages);
    if (newMidiNoteTriggered)
    {
        bankPrimary.triggerAttackSwell();
        if (modeCrossfadeSamplesRemaining > 0)
            bankSecondary.triggerAttackSwell();
    }

    // Handle Manual UI Trigger button
    if (manualTriggerRequested.exchange(false, std::memory_order_acquire))
    {
        manualImpulseSamplesRemaining = static_cast<int>(baseSampleRate * 0.018); // 18ms punchy impulse burst
        telemetry.transientPulseCount.fetch_add(1, std::memory_order_relaxed);
        bankPrimary.triggerAttackSwell();
    }

    // Check Oversampling rate change (1x / 2x / 4x)
    const int osChoice = static_cast<int>(oversamplingParam->load(std::memory_order_relaxed));
    if (osChoice != lastOversamplingChoice)
    {
        lastOversamplingChoice = osChoice;
        const double osMul = (osChoice == 1) ? 2.0 : ((osChoice == 2) ? 4.0 : 1.0);
        preparedEngineRate = baseSampleRate * osMul;
        modeCrossfadeTotalSamples = std::max(1, static_cast<int>(std::round(preparedEngineRate * 0.020)));
        bankPrimary.prepare(preparedEngineRate);
        bankSecondary.prepare(preparedEngineRate);
        excHpf.prepare(preparedEngineRate);
        smearDiffuserL1.prepare(preparedEngineRate, 25.0f);
        smearDiffuserL2.prepare(preparedEngineRate, 40.0f);
        smearDiffuserR1.prepare(preparedEngineRate, 28.0f);
        smearDiffuserR2.prepare(preparedEngineRate, 43.0f);
        shifterL.prepare(preparedEngineRate);
        shifterR.prepare(preparedEngineRate);
        motionLfo.prepare(preparedEngineRate);
        advLfo1.prepare(preparedEngineRate);
        advLfo2.prepare(preparedEngineRate);
        advEnvFollower.prepare(preparedEngineRate);
        tiltL.prepare(preparedEngineRate);
        tiltR.prepare(preparedEngineRate);
        wetHpfL.prepare(preparedEngineRate);
        wetHpfR.prepare(preparedEngineRate);
        wetLpfL.prepare(preparedEngineRate);
        wetLpfR.prepare(preparedEngineRate);
        oversampler2x->reset();
        oversampler4x->reset();
    }

    // Update parameter smoothing ramp time if changed in Global tab
    const float currentSmoothMs = smoothTimeMsParam->load(std::memory_order_relaxed);
    if (std::abs(currentSmoothMs - lastSmoothTimeMs) > 0.1f)
    {
        lastSmoothTimeMs = currentSmoothMs;
        const double rampSec = static_cast<double>(currentSmoothMs) * 0.001;
        smoothAttack.reset(preparedEngineRate, rampSec);
        smoothDecay.reset(preparedEngineRate, rampSec);
        smoothWarp.reset(preparedEngineRate, rampSec);
        smoothDrift.reset(preparedEngineRate, rampSec);
        smoothShiftHz.reset(preparedEngineRate, rampSec);
        smoothMotionStrength.reset(preparedEngineRate, rampSec);
        smoothUnison.reset(preparedEngineRate, rampSec);
        smoothFilterTilt.reset(preparedEngineRate, rampSec);
        smoothFilterQ.reset(preparedEngineRate, rampSec);
        smoothFilterLow.reset(preparedEngineRate, rampSec);
        smoothFilterHigh.reset(preparedEngineRate, rampSec);
        smoothDepth.reset(preparedEngineRate, rampSec);
        smoothInputGain.reset(preparedEngineRate, rampSec);
        smoothOutputGain.reset(preparedEngineRate, rampSec);
        smoothStereoWidth.reset(preparedEngineRate, rampSec);
    }

    // Inject manual UI impulse into input buffer before oversampling if active
    if (manualImpulseSamplesRemaining > 0)
    {
        auto* lPtr = buffer.getWritePointer(0);
        auto* rPtr = buffer.getWritePointer(totalNumOutputChannels > 1 ? 1 : 0);
        for (int n = 0; n < numSamples && manualImpulseSamplesRemaining > 0; ++n)
        {
            const float env = static_cast<float>(manualImpulseSamplesRemaining) / static_cast<float>(baseSampleRate * 0.018);
            const float burst = (impulseRng.nextFloat() * 2.0f - 1.0f) * env * env * 0.85f;
            lPtr[n] += burst;
            rPtr[n] += burst;
            --manualImpulseSamplesRemaining;
        }
    }

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (const auto pos = playHead->getPosition())
        {
            if (const auto hostBpm = pos->getBpm())
                if (*hostBpm > 10.0)
                    bpm = *hostBpm;
        }
    }

    juce::dsp::AudioBlock<float> baseBlock(buffer);
    if (osChoice == 1)
    {
        auto osBlock = oversampler2x->processSamplesUp(baseBlock);
        processOversampledBlock(osBlock, baseSampleRate * 2.0, bpm);
        oversampler2x->processSamplesDown(baseBlock);
    }
    else if (osChoice == 2)
    {
        auto osBlock = oversampler4x->processSamplesUp(baseBlock);
        processOversampledBlock(osBlock, baseSampleRate * 4.0, bpm);
        oversampler4x->processSamplesDown(baseBlock);
    }
    else
    {
        processOversampledBlock(baseBlock, baseSampleRate, bpm);
    }
}

void BreachResonatorAudioProcessor::processOversampledBlock(juce::dsp::AudioBlock<float>& block,
                                                            double effectiveSampleRate,
                                                            double bpm)
{
    const int numSamples = static_cast<int>(block.getNumSamples());
    const int numChannels = static_cast<int>(block.getNumChannels());
    if (numSamples == 0 || numChannels == 0)
        return;

    // 1. Check Mode Switch (20ms equal-power crossfade)
    const int requestedMode = static_cast<int>(modeParam->load(std::memory_order_relaxed));
    if (requestedMode != activeMode)
    {
        previousMode = activeMode;
        activeMode = requestedMode;
        currentModeIndex.store(activeMode, std::memory_order_relaxed);
        modeCrossfadeSamplesRemaining = modeCrossfadeTotalSamples;
        bankSecondary.reset();
    }

    // 2. Update Smoothers
    smoothAttack.setTargetValue(attackParam->load(std::memory_order_relaxed));
    smoothDecay.setTargetValue(decayParam->load(std::memory_order_relaxed));
    smoothWarp.setTargetValue(warpParam->load(std::memory_order_relaxed));
    smoothDrift.setTargetValue(driftParam->load(std::memory_order_relaxed));
    smoothShiftHz.setTargetValue(shiftHzParam->load(std::memory_order_relaxed));
    smoothMotionStrength.setTargetValue(motionStrengthParam->load(std::memory_order_relaxed));
    smoothUnison.setTargetValue(unisonParam->load(std::memory_order_relaxed));
    smoothFilterTilt.setTargetValue(filterTiltParam->load(std::memory_order_relaxed));
    smoothFilterQ.setTargetValue(filterQParam->load(std::memory_order_relaxed));
    smoothFilterLow.setTargetValue(filterLowParam->load(std::memory_order_relaxed));
    smoothFilterHigh.setTargetValue(filterHighParam->load(std::memory_order_relaxed));
    smoothDepth.setTargetValue(depthParam->load(std::memory_order_relaxed));

    smoothInputGain.setTargetValue(juce::Decibels::decibelsToGain(inputGainDbParam->load(std::memory_order_relaxed)));
    smoothOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDbParam->load(std::memory_order_relaxed)));
    smoothStereoWidth.setTargetValue(stereoWidthParam->load(std::memory_order_relaxed));

    // 3. Gather Per-Voice Stage 2 Arrays
    const int voiceCap = breach::voiceCapIndexToCount(static_cast<int>(voiceCapParam->load(std::memory_order_relaxed)));
    std::array<float, breach::kMaxVoices> perVoiceLevels{};
    std::array<float, breach::kMaxVoices> perVoicePans{};
    std::array<float, breach::kMaxVoices> perVoiceDetunes{};
    std::array<float, breach::kMaxVoices> perVoiceDecayMuls{};

    for (size_t v = 0; v < breach::kMaxVoices; ++v)
    {
        perVoiceLevels[v]    = voiceLevelParams[v]->load(std::memory_order_relaxed);
        perVoicePans[v]      = voicePanParams[v]->load(std::memory_order_relaxed);
        perVoiceDetunes[v]   = voiceDetuneParams[v]->load(std::memory_order_relaxed);
        perVoiceDecayMuls[v] = voiceDecayMulParams[v]->load(std::memory_order_relaxed);
    }

    const auto resModel = static_cast<breach::ResonatorModelOverride>(static_cast<int>(resModelParam->load(std::memory_order_relaxed)));
    const float fbDamping = fbDampingParam->load(std::memory_order_relaxed);
    const float stiffness = stiffnessParam->load(std::memory_order_relaxed);
    const float excHpfHz  = excHpfHzParam->load(std::memory_order_relaxed);
    const float excDrive  = excDriveParam->load(std::memory_order_relaxed);

    // 4. Modulation Rates & Matrix Weights
    const bool arpOn = arpOnParam->load(std::memory_order_relaxed) > 0.5f;
    const bool arpSync = arpSyncParam->load(std::memory_order_relaxed) > 0.5f;
    const double arpCyclesPerSample = arpSync
        ? ((bpm / 60.0) / getDivisionInBeats(static_cast<int>(arpRateSyncParam->load(std::memory_order_relaxed)))) / effectiveSampleRate
        : static_cast<double>(arpRateHzParam->load(std::memory_order_relaxed)) / effectiveSampleRate;
    const auto arpPattern = static_cast<breach::ArpPattern>(static_cast<int>(arpPatternParam->load(std::memory_order_relaxed)));

    const bool motionOn = motionOnParam->load(std::memory_order_relaxed) > 0.5f;
    const bool motionSync = motionSyncParam->load(std::memory_order_relaxed) > 0.5f;
    const float motionRateHz = motionSync
        ? static_cast<float>((bpm / 60.0) / getDivisionInBeats(static_cast<int>(motionRateSyncParam->load(std::memory_order_relaxed))))
        : motionRateHzParam->load(std::memory_order_relaxed);
    const auto motionShape = static_cast<breach::MotionShape>(static_cast<int>(motionShapeParam->load(std::memory_order_relaxed)));

    const auto advLfo1Shape = static_cast<breach::MotionShape>(static_cast<int>(advLfo1ShapeParam->load(std::memory_order_relaxed)));
    const bool advLfo1Sync  = advLfo1SyncParam->load(std::memory_order_relaxed) > 0.5f;
    const float advLfo1Rate = advLfo1Sync
        ? static_cast<float>((bpm / 60.0) / getDivisionInBeats(static_cast<int>(advLfo1RateSyncParam->load(std::memory_order_relaxed))))
        : advLfo1RateHzParam->load(std::memory_order_relaxed);
    const float advLfo1PhaseNorm = advLfo1PhaseParam->load(std::memory_order_relaxed) / 360.0f;

    const auto advLfo2Shape = static_cast<breach::MotionShape>(static_cast<int>(advLfo2ShapeParam->load(std::memory_order_relaxed)));
    const bool advLfo2Sync  = advLfo2SyncParam->load(std::memory_order_relaxed) > 0.5f;
    const float advLfo2Rate = advLfo2Sync
        ? static_cast<float>((bpm / 60.0) / getDivisionInBeats(static_cast<int>(advLfo2RateSyncParam->load(std::memory_order_relaxed))))
        : advLfo2RateHzParam->load(std::memory_order_relaxed);
    const float advLfo2PhaseNorm = advLfo2PhaseParam->load(std::memory_order_relaxed) / 360.0f;

    const float envAttMs  = advEnvAttackParam->load(std::memory_order_relaxed);
    const float envRelMs  = advEnvReleaseParam->load(std::memory_order_relaxed);
    const float envAmount = advEnvAmountParam->load(std::memory_order_relaxed);

    std::array<std::array<float, 6>, 3> mat{};
    for (size_t s = 0; s < 3; ++s)
        for (size_t d = 0; d < 6; ++d)
            mat[s][d] = modMatrixParams[s][d]->load(std::memory_order_relaxed);

    const auto dwCurve = static_cast<breach::DryWetCurveType>(static_cast<int>(dryWetCurveParam->load(std::memory_order_relaxed)));
    const bool limiterEnabled = limiterOnParam->load(std::memory_order_relaxed) > 0.5f;
    const float limiterThreshLin = juce::Decibels::decibelsToGain(limiterThreshDbParam->load(std::memory_order_relaxed));

    std::array<bool, 12> pitchClasses{};
    for (size_t i = 0; i < 12; ++i)
        pitchClasses[i] = noteParams[i]->load(std::memory_order_relaxed) > 0.5f;

    const bool midiMode = midiModeParam->load(std::memory_order_relaxed) > 0.5f;
    const int rootNote = static_cast<int>(rootNoteParam->load(std::memory_order_relaxed));
    const int baseOctave = static_cast<int>(octaveParam->load(std::memory_order_relaxed));
    const int spreadOctaves = static_cast<int>(spreadParam->load(std::memory_order_relaxed));

    auto* leftData  = block.getChannelPointer(0);
    auto* rightData = block.getChannelPointer(numChannels > 1 ? 1 : 0);

    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float blockPeak = 0.0f;
    int currentLitVoice = -1;
    int lastActiveVoiceCount = voiceCap;
    uint16_t lastActiveMask = 0;

    std::array<float, breach::kMaxVoices> arpGates{};
    arpGates.fill(1.0f);

    for (int n = 0; n < numSamples; ++n)
    {
        const float inGain = smoothInputGain.getNextValue();
        const float dryL = leftData[n] * inGain;
        const float dryR = rightData[n] * inGain;
        const float monoIn = 0.5f * (dryL + dryR);

        sumSqL += dryL * dryL;
        sumSqR += dryR * dryR;
        const float absPeak = std::max(std::abs(dryL), std::abs(dryR));
        blockPeak = std::max(blockPeak, absPeak);

        fastEnv = 0.92f * fastEnv + 0.08f * absPeak;
        slowEnv = 0.9985f * slowEnv + 0.0015f * absPeak;
        if (transientCooldownSamples > 0)
            --transientCooldownSamples;

        if (transientCooldownSamples == 0 && fastEnv > 0.14f && fastEnv > slowEnv * 2.6f)
        {
            telemetry.transientPulseCount.fetch_add(1, std::memory_order_relaxed);
            transientCooldownSamples = static_cast<int>(effectiveSampleRate * 0.085);
            bankPrimary.triggerAttackSwell();
        }

        // Advance LFOs & Env Follower
        motionLfo.advance(motionRateHz);
        advLfo1.advance(advLfo1Rate);
        advLfo2.advance(advLfo2Rate);

        const float sLfo1 = advLfo1.getVoiceValue(0, 1, advLfo1Shape, advLfo1PhaseNorm);
        const float sLfo2 = advLfo2.getVoiceValue(0, 1, advLfo2Shape, advLfo2PhaseNorm);
        const float sEnv  = advEnvFollower.process(monoIn, envAttMs, envRelMs) * envAmount;

        auto evalMod = [&](int destIdx) noexcept -> float
        {
            return sLfo1 * mat[0][static_cast<size_t>(destIdx)]
                 + sLfo2 * mat[1][static_cast<size_t>(destIdx)]
                 + sEnv  * mat[2][static_cast<size_t>(destIdx)];
        };

        const float attackMs      = smoothAttack.getNextValue();
        const float decayVal      = smoothDecay.getNextValue();
        const float baseWarp      = smoothWarp.getNextValue();
        const float baseDrift     = smoothDrift.getNextValue();
        const float baseShift     = smoothShiftHz.getNextValue();
        const float motionStr     = smoothMotionStrength.getNextValue();
        const float unisonVal     = smoothUnison.getNextValue();
        const float baseTilt      = smoothFilterTilt.getNextValue();
        const float baseQ         = smoothFilterQ.getNextValue();
        const float rawLowHz      = smoothFilterLow.getNextValue();
        const float rawHighHz     = smoothFilterHigh.getNextValue();
        const float baseDepth     = smoothDepth.getNextValue();
        const float outGain       = smoothOutputGain.getNextValue();
        const float stereoWidth   = smoothStereoWidth.getNextValue();

        const float modWarp  = juce::jlimit(-1.0f, 1.0f, baseWarp  + evalMod(0));
        const float modDrift = juce::jlimit(0.0f,  1.0f, baseDrift + evalMod(1));
        const float modShift = juce::jlimit(-500.0f, 500.0f, baseShift + evalMod(2) * 350.0f);
        const float modQ     = juce::jlimit(0.0f,  1.0f, baseQ     + evalMod(3));
        const float modTilt  = juce::jlimit(-1.0f, 1.0f, baseTilt  + evalMod(4));
        const float modDepth = juce::jlimit(0.0f,  1.0f, baseDepth + evalMod(5));

        const int activeVoiceCount = voicingEngine.computeVoices(
            pitchClasses, rootNote, baseOctave, spreadOctaves,
            midiMode, modWarp, stiffness, voiceCap,
            perVoicePans, perVoiceDetunes, currentVoiceSpecs, lastActiveMask);
        lastActiveVoiceCount = activeVoiceCount;

        // Arp Sequencer
        if (arpOn && activeVoiceCount > 0)
        {
            arpPhaseAccumulator += arpCyclesPerSample;
            if (arpPhaseAccumulator >= 1.0)
            {
                arpPhaseAccumulator -= 1.0;
                if (arpPattern == breach::ArpPattern::UpDown && activeVoiceCount > 1)
                {
                    arpStepCounter += arpPingPongDir;
                    if (arpStepCounter >= activeVoiceCount - 1) { arpStepCounter = activeVoiceCount - 1; arpPingPongDir = -1; }
                    else if (arpStepCounter <= 0)               { arpStepCounter = 0; arpPingPongDir = 1; }
                }
                else
                {
                    arpStepCounter = (arpStepCounter + 1) % activeVoiceCount;
                }
                arpRandomVoice = arpRng.nextInt(activeVoiceCount);
            }

            const bool gateOpen = (arpPhaseAccumulator <= 0.65);
            int targetVoice = 0;
            switch (arpPattern)
            {
                case breach::ArpPattern::Up:
                case breach::ArpPattern::UpDown:
                    targetVoice = arpStepCounter % activeVoiceCount;
                    break;
                case breach::ArpPattern::Down:
                    targetVoice = (activeVoiceCount - 1) - (arpStepCounter % activeVoiceCount);
                    break;
                case breach::ArpPattern::Random:
                    targetVoice = arpRandomVoice % activeVoiceCount;
                    break;
                case breach::ArpPattern::Simultaneous:
                    targetVoice = -1;
                    break;
            }

            currentLitVoice = gateOpen ? targetVoice : -1;
            for (int v = 0; v < breach::kMaxVoices; ++v)
            {
                if (v >= activeVoiceCount)
                    arpGates[static_cast<size_t>(v)] = 0.0f;
                else if (arpPattern == breach::ArpPattern::Simultaneous)
                    arpGates[static_cast<size_t>(v)] = gateOpen ? 1.0f : 0.0f;
                else
                    arpGates[static_cast<size_t>(v)] = (gateOpen && v == targetVoice) ? 1.0f : 0.0f;
            }
        }
        else
        {
            for (int v = 0; v < breach::kMaxVoices; ++v)
                arpGates[static_cast<size_t>(v)] = (v < activeVoiceCount) ? 1.0f : 0.0f;
            currentLitVoice = -1;
        }

        // Excitation Pre-Filter (HPF) + Excitation Drive
        excHpf.setParameters(excHpfHz, 0.707f);
        float excitedSignal = excHpf.processHighpass(monoIn);
        if (excDrive > 0.001f)
        {
            const float preGain = 1.0f + excDrive * 5.5f;
            excitedSignal = breach::dsp::fastTanh(excitedSignal * preGain) / std::sqrt(preGain);
        }

        // Resonator Bank
        float wetL = 0.0f;
        float wetR = 0.0f;

        if (modeCrossfadeSamplesRemaining > 0)
        {
            float outPrevL = 0.0f, outPrevR = 0.0f;
            float outNextL = 0.0f, outNextR = 0.0f;

            bankPrimary.processSample(excitedSignal, static_cast<breach::ResonatorMode>(previousMode), resModel,
                                      activeVoiceCount, currentVoiceSpecs, arpGates,
                                      perVoiceLevels, perVoiceDecayMuls,
                                      attackMs, decayVal, fbDamping, stiffness, modQ, modDrift, unisonVal,
                                      motionLfo, motionOn, motionStr, motionShape, outPrevL, outPrevR);

            bankSecondary.processSample(excitedSignal, static_cast<breach::ResonatorMode>(activeMode), resModel,
                                        activeVoiceCount, currentVoiceSpecs, arpGates,
                                        perVoiceLevels, perVoiceDecayMuls,
                                        attackMs, decayVal, fbDamping, stiffness, modQ, modDrift, unisonVal,
                                        motionLfo, motionOn, motionStr, motionShape, outNextL, outNextR);

            const float alpha = 1.0f - (static_cast<float>(modeCrossfadeSamplesRemaining)
                                      / static_cast<float>(modeCrossfadeTotalSamples));
            const float fadeOut = std::cos(alpha * juce::MathConstants<float>::halfPi);
            const float fadeIn  = std::sin(alpha * juce::MathConstants<float>::halfPi);

            wetL = outPrevL * fadeOut + outNextL * fadeIn;
            wetR = outPrevR * fadeOut + outNextR * fadeIn;

            --modeCrossfadeSamplesRemaining;
            if (modeCrossfadeSamplesRemaining == 0)
            {
                bankPrimary = std::move(bankSecondary);
                bankSecondary.prepare(effectiveSampleRate);
            }
        }
        else
        {
            bankPrimary.processSample(excitedSignal, static_cast<breach::ResonatorMode>(activeMode), resModel,
                                      activeVoiceCount, currentVoiceSpecs, arpGates,
                                      perVoiceLevels, perVoiceDecayMuls,
                                      attackMs, decayVal, fbDamping, stiffness, modQ, modDrift, unisonVal,
                                      motionLfo, motionOn, motionStr, motionShape, wetL, wetR);
        }

        // Drift Allpass Smear
        if (modDrift > 0.005f)
        {
            const float smearGain = modDrift * 0.62f;
            wetL = smearDiffuserL1.process(wetL, static_cast<float>(effectiveSampleRate * 0.0073), smearGain);
            wetL = smearDiffuserL2.process(wetL, static_cast<float>(effectiveSampleRate * 0.0131), smearGain);
            wetR = smearDiffuserR1.process(wetR, static_cast<float>(effectiveSampleRate * 0.0089), smearGain);
            wetR = smearDiffuserR2.process(wetR, static_cast<float>(effectiveSampleRate * 0.0157), smearGain);
        }

        // Shift SSB Shifter
        wetL = shifterL.process(wetL, modShift);
        wetR = shifterR.process(wetR, modShift);

        // Filter Tilt + Range Band-Limit
        wetL = tiltL.process(wetL, modTilt);
        wetR = tiltR.process(wetR, modTilt);

        const float safeLowHz  = juce::jlimit(20.0f, 18000.0f, rawLowHz);
        const float safeHighHz = juce::jlimit(safeLowHz + 40.0f, 20000.0f, rawHighHz);

        wetHpfL.setParameters(safeLowHz, 0.707f);
        wetHpfR.setParameters(safeLowHz, 0.707f);
        wetLpfL.setParameters(safeHighHz, 0.707f);
        wetLpfR.setParameters(safeHighHz, 0.707f);

        wetL = wetLpfL.processLowpass(wetHpfL.processHighpass(wetL));
        wetR = wetLpfR.processLowpass(wetHpfR.processHighpass(wetR));

        // Global Stereo Width (Mid/Side processing on wet signal)
        const float mid  = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * stereoWidth;
        wetL = mid + side;
        wetR = mid - side;

        // Dry/Wet Curve Mixing
        float dryGain = 1.0f;
        float wetGain = 0.0f;
        switch (dwCurve)
        {
            case breach::DryWetCurveType::Linear:
                dryGain = 1.0f - modDepth;
                wetGain = modDepth;
                break;
            case breach::DryWetCurveType::SCurve:
            {
                const float s = modDepth * modDepth * (3.0f - 2.0f * modDepth);
                dryGain = std::cos(s * juce::MathConstants<float>::halfPi);
                wetGain = std::sin(s * juce::MathConstants<float>::halfPi);
                break;
            }
            case breach::DryWetCurveType::EqualPower:
            default:
                dryGain = std::cos(modDepth * juce::MathConstants<float>::halfPi);
                wetGain = std::sin(modDepth * juce::MathConstants<float>::halfPi);
                break;
        }

        float outL = (dryL * dryGain + wetL * wetGain) * outGain;
        float outR = (dryR * dryGain + wetR * wetGain) * outGain;

        // Global Soft Limiter with configurable threshold
        if (limiterEnabled && limiterThreshLin > 1.0e-4f)
        {
            outL = breach::dsp::fastTanh(outL / limiterThreshLin) * limiterThreshLin;
            outR = breach::dsp::fastTanh(outR / limiterThreshLin) * limiterThreshLin;
        }

        leftData[n]  = outL;
        rightData[n] = outR;
    }

    // Publish Telemetry Snapshot
    const float invN = 1.0f / static_cast<float>(numSamples);
    telemetry.inputRmsL.store(std::sqrt(sumSqL * invN), std::memory_order_relaxed);
    telemetry.inputRmsR.store(std::sqrt(sumSqR * invN), std::memory_order_relaxed);
    telemetry.inputPeak.store(blockPeak, std::memory_order_relaxed);
    telemetry.activeVoiceCount.store(lastActiveVoiceCount, std::memory_order_relaxed);
    telemetry.activeMode.store(activeMode, std::memory_order_relaxed);
    telemetry.activeArpStep.store(currentLitVoice, std::memory_order_relaxed);
    telemetry.midiModeActive.store(midiMode, std::memory_order_relaxed);
    telemetry.activePitchClassMask.store(lastActiveMask, std::memory_order_relaxed);

    for (int v = 0; v < breach::kMaxVoices; ++v)
    {
        const float energy = (modeCrossfadeSamplesRemaining > 0)
            ? std::max(bankPrimary.getVoiceEnergy(v), bankSecondary.getVoiceEnergy(v))
            : bankPrimary.getVoiceEnergy(v);

        telemetry.voiceEnergies[static_cast<size_t>(v)].store(energy, std::memory_order_relaxed);
        telemetry.voiceFrequencies[static_cast<size_t>(v)].store(
            currentVoiceSpecs[static_cast<size_t>(v)].frequencyHz, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* BreachResonatorAudioProcessor::createEditor()
{
    return new BreachResonatorAudioProcessorEditor(*this);
}

void BreachResonatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("advancedPanelOpen", advancedPanelOpen.load(std::memory_order_relaxed), nullptr);
    state.setProperty("advancedActiveTab", advancedActiveTab.load(std::memory_order_relaxed), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BreachResonatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        advancedPanelOpen.store(static_cast<bool>(tree.getProperty("advancedPanelOpen", false)), std::memory_order_relaxed);
        advancedActiveTab.store(static_cast<int>(tree.getProperty("advancedActiveTab", 0)), std::memory_order_relaxed);
        apvts.replaceState(tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreachResonatorAudioProcessor();
}
