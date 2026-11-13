#include "PluginProcessor.h"
#include "PluginEditor.h"

BreachResonatorAudioProcessor::BreachResonatorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", breach::createParameterLayout())
{
    using namespace breach;

    modeParam         = apvts.getRawParameterValue(ParamID::mode);
    rootNoteParam     = apvts.getRawParameterValue(ParamID::rootNote);
    scaleTypeParam    = apvts.getRawParameterValue(ParamID::scaleType);
    chordTypeParam    = apvts.getRawParameterValue(ParamID::chordType);
    voiceCountParam   = apvts.getRawParameterValue(ParamID::voiceCount);
    octaveSpanParam   = apvts.getRawParameterValue(ParamID::octaveSpan);
    midiInputParam    = apvts.getRawParameterValue(ParamID::midiInput);

    decayParam        = apvts.getRawParameterValue(ParamID::decay);
    dampingParam      = apvts.getRawParameterValue(ParamID::damping);
    toneParam         = apvts.getRawParameterValue(ParamID::tone);

    octaveSpreadParam = apvts.getRawParameterValue(ParamID::octaveSpread);
    detuneParam       = apvts.getRawParameterValue(ParamID::detune);
    stereoWidthParam  = apvts.getRawParameterValue(ParamID::stereoWidth);

    driftParam        = apvts.getRawParameterValue(ParamID::drift);
    shiftHzParam      = apvts.getRawParameterValue(ParamID::shiftHz);

    arpEnabledParam   = apvts.getRawParameterValue(ParamID::arpEnabled);
    arpRateParam      = apvts.getRawParameterValue(ParamID::arpRate);
    arpPatternParam   = apvts.getRawParameterValue(ParamID::arpPattern);
    arpGateParam      = apvts.getRawParameterValue(ParamID::arpGate);

    lfo1RateParam     = apvts.getRawParameterValue(ParamID::lfo1Rate);
    lfo1ShapeParam    = apvts.getRawParameterValue(ParamID::lfo1Shape);
    lfo2RateParam     = apvts.getRawParameterValue(ParamID::lfo2Rate);
    lfo2ShapeParam    = apvts.getRawParameterValue(ParamID::lfo2Shape);
    envAttackParam    = apvts.getRawParameterValue(ParamID::envAttack);
    envReleaseParam   = apvts.getRawParameterValue(ParamID::envRelease);

    lfo1ToDecayParam  = apvts.getRawParameterValue(ParamID::lfo1ToDecay);
    lfo1ToFilterParam = apvts.getRawParameterValue(ParamID::lfo1ToFilter);
    lfo2ToDriftParam  = apvts.getRawParameterValue(ParamID::lfo2ToDrift);
    lfo2ToShiftParam  = apvts.getRawParameterValue(ParamID::lfo2ToShift);
    envToDecayParam   = apvts.getRawParameterValue(ParamID::envToDecay);
    envToFilterParam  = apvts.getRawParameterValue(ParamID::envToFilter);

    hpfFreqParam      = apvts.getRawParameterValue(ParamID::hpfFreq);
    lpfFreqParam      = apvts.getRawParameterValue(ParamID::lpfFreq);
    depthParam        = apvts.getRawParameterValue(ParamID::depth);
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

void BreachResonatorAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = std::max(8000.0, sampleRate);
    modeCrossfadeTotalSamples = std::max(1, static_cast<int>(std::round(currentSampleRate * 0.020)));
    modeCrossfadeSamplesRemaining = 0;

    bankPrimary.prepare(currentSampleRate);
    bankSecondary.prepare(currentSampleRate);
    voicingEngine.resetMidi();

    smearDiffuserL1.prepare(currentSampleRate, 25.0f);
    smearDiffuserL2.prepare(currentSampleRate, 40.0f);
    smearDiffuserR1.prepare(currentSampleRate, 28.0f);
    smearDiffuserR2.prepare(currentSampleRate, 43.0f);

    shifterL.prepare(currentSampleRate);
    shifterR.prepare(currentSampleRate);

    wetHpfL.prepare(currentSampleRate);
    wetHpfR.prepare(currentSampleRate);
    wetLpfL.prepare(currentSampleRate);
    wetLpfR.prepare(currentSampleRate);

    lfo1.prepare(currentSampleRate);
    lfo2.prepare(currentSampleRate);
    envFollower.prepare(currentSampleRate);

    constexpr double rampSec = 0.015;
    smoothDecay.reset(currentSampleRate, rampSec);
    smoothDamping.reset(currentSampleRate, rampSec);
    smoothTone.reset(currentSampleRate, rampSec);
    smoothDrift.reset(currentSampleRate, rampSec);
    smoothShiftHz.reset(currentSampleRate, rampSec);
    smoothHpf.reset(currentSampleRate, rampSec);
    smoothLpf.reset(currentSampleRate, rampSec);
    smoothDepth.reset(currentSampleRate, rampSec);

    activeMode = static_cast<int>(modeParam->load(std::memory_order_relaxed));
    previousMode = activeMode;
    currentModeIndex.store(activeMode, std::memory_order_relaxed);

    arpBeatAccumulator = 0.0;
    arpStepCounter = 0;
    fastEnv = slowEnv = 0.0f;
    transientCooldownSamples = 0;
}

void BreachResonatorAudioProcessor::releaseResources()
{
    bankPrimary.reset();
    bankSecondary.reset();
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

double BreachResonatorAudioProcessor::getArpDivisionInBeats(int rateIndex) noexcept
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

    voicingEngine.handleMidiBuffer(midiMessages);

    const int requestedMode = static_cast<int>(modeParam->load(std::memory_order_relaxed));
    if (requestedMode != activeMode)
    {
        previousMode = activeMode;
        activeMode = requestedMode;
        currentModeIndex.store(activeMode, std::memory_order_relaxed);
        modeCrossfadeSamplesRemaining = modeCrossfadeTotalSamples;
        bankSecondary.reset();
    }

    smoothDecay.setTargetValue(decayParam->load(std::memory_order_relaxed));
    smoothDamping.setTargetValue(dampingParam->load(std::memory_order_relaxed));
    smoothTone.setTargetValue(toneParam->load(std::memory_order_relaxed));
    smoothDrift.setTargetValue(driftParam->load(std::memory_order_relaxed));
    smoothShiftHz.setTargetValue(shiftHzParam->load(std::memory_order_relaxed));
    smoothHpf.setTargetValue(hpfFreqParam->load(std::memory_order_relaxed));
    smoothLpf.setTargetValue(lpfFreqParam->load(std::memory_order_relaxed));
    smoothDepth.setTargetValue(depthParam->load(std::memory_order_relaxed));

    const int voiceCount = static_cast<int>(voiceCountParam->load(std::memory_order_relaxed));
    const bool midiOverride = midiInputParam->load(std::memory_order_relaxed) > 0.5f;

    voicingEngine.computeVoices(
        static_cast<int>(rootNoteParam->load(std::memory_order_relaxed)),
        static_cast<breach::ScaleType>(static_cast<int>(scaleTypeParam->load(std::memory_order_relaxed))),
        static_cast<breach::ChordType>(static_cast<int>(chordTypeParam->load(std::memory_order_relaxed))),
        voiceCount,
        static_cast<int>(octaveSpanParam->load(std::memory_order_relaxed)),
        octaveSpreadParam->load(std::memory_order_relaxed),
        detuneParam->load(std::memory_order_relaxed),
        stereoWidthParam->load(std::memory_order_relaxed),
        midiOverride,
        currentVoiceSpecs);

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

    const bool arpOn = arpEnabledParam->load(std::memory_order_relaxed) > 0.5f;
    const double stepBeats = getArpDivisionInBeats(static_cast<int>(arpRateParam->load(std::memory_order_relaxed)));
    const double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    const auto arpPattern = static_cast<breach::ArpPattern>(static_cast<int>(arpPatternParam->load(std::memory_order_relaxed)));
    const float arpGateLen = arpGateParam->load(std::memory_order_relaxed);

    const float lfo1Rate = lfo1RateParam->load(std::memory_order_relaxed);
    const auto lfo1Shape = static_cast<breach::LfoShape>(static_cast<int>(lfo1ShapeParam->load(std::memory_order_relaxed)));
    const float lfo2Rate = lfo2RateParam->load(std::memory_order_relaxed);
    const auto lfo2Shape = static_cast<breach::LfoShape>(static_cast<int>(lfo2ShapeParam->load(std::memory_order_relaxed)));
    const float envAtt = envAttackParam->load(std::memory_order_relaxed);
    const float envRel = envReleaseParam->load(std::memory_order_relaxed);

    const float modLfo1Decay  = lfo1ToDecayParam->load(std::memory_order_relaxed);
    const float modLfo1Filter = lfo1ToFilterParam->load(std::memory_order_relaxed);
    const float modLfo2Drift  = lfo2ToDriftParam->load(std::memory_order_relaxed);
    const float modLfo2Shift  = lfo2ToShiftParam->load(std::memory_order_relaxed);
    const float modEnvDecay   = envToDecayParam->load(std::memory_order_relaxed);
    const float modEnvFilter  = envToFilterParam->load(std::memory_order_relaxed);

    auto* leftData  = buffer.getWritePointer(0);
    auto* rightData = buffer.getWritePointer(totalNumOutputChannels > 1 ? 1 : 0);
    const auto* inLData = buffer.getReadPointer(0);
    const auto* inRData = buffer.getReadPointer(totalNumInputChannels > 1 ? 1 : 0);

    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float blockPeak = 0.0f;
    int currentLitVoice = -1;

    std::array<float, breach::VisualTelemetry::kMaxVoices> arpGates{};
    arpGates.fill(1.0f);

    for (int n = 0; n < numSamples; ++n)
    {
        const float dryL = inLData[n];
        const float dryR = inRData[n];
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
            transientCooldownSamples = static_cast<int>(currentSampleRate * 0.085);
        }

        if (arpOn)
        {
            arpBeatAccumulator += beatsPerSample;
            if (arpBeatAccumulator >= stepBeats)
            {
                arpBeatAccumulator -= stepBeats;
                if (arpPattern == breach::ArpPattern::UpDown)
                {
                    arpStepCounter += arpPingPongDir;
                    if (arpStepCounter >= voiceCount - 1) { arpStepCounter = voiceCount - 1; arpPingPongDir = -1; }
                    else if (arpStepCounter <= 0)         { arpStepCounter = 0; arpPingPongDir = 1; }
                }
                else
                {
                    arpStepCounter = (arpStepCounter + 1) % std::max(1, voiceCount);
                }
                arpRandomVoice = arpRng.nextInt(std::max(1, voiceCount));
            }

            const double phaseInStep = arpBeatAccumulator / stepBeats;
            const bool gateOpen = (phaseInStep <= static_cast<double>(arpGateLen));

            int targetVoice = 0;
            switch (arpPattern)
            {
                case breach::ArpPattern::Up:
                case breach::ArpPattern::UpDown:
                    targetVoice = arpStepCounter % voiceCount;
                    break;
                case breach::ArpPattern::Down:
                    targetVoice = (voiceCount - 1) - (arpStepCounter % voiceCount);
                    break;
                case breach::ArpPattern::Random:
                    targetVoice = arpRandomVoice;
                    break;
                case breach::ArpPattern::ChordPulse:
                    targetVoice = -1;
                    break;
            }

            currentLitVoice = gateOpen ? targetVoice : -1;
            for (int v = 0; v < breach::VisualTelemetry::kMaxVoices; ++v)
            {
                if (arpPattern == breach::ArpPattern::ChordPulse)
                    arpGates[static_cast<size_t>(v)] = gateOpen ? 1.0f : 0.0f;
                else
                    arpGates[static_cast<size_t>(v)] = (gateOpen && v == targetVoice) ? 1.0f : 0.0f;
            }
        }
        else
        {
            arpGates.fill(1.0f);
            currentLitVoice = -1;
        }

        const float lfo1Val = lfo1.process(lfo1Rate, lfo1Shape);
        const float lfo2Val = lfo2.process(lfo2Rate, lfo2Shape);
        const float envVal  = envFollower.process(monoIn, envAtt, envRel);

        const float baseDecay   = smoothDecay.getNextValue();
        const float baseDamping = smoothDamping.getNextValue();
        const float baseTone    = smoothTone.getNextValue();
        const float baseDrift   = smoothDrift.getNextValue();
        const float baseShift   = smoothShiftHz.getNextValue();
        const float baseHpf     = smoothHpf.getNextValue();
        const float baseLpf     = smoothLpf.getNextValue();
        const float depth       = smoothDepth.getNextValue();

        const float modDecay = juce::jlimit(0.0f, 1.0f,
            baseDecay + lfo1Val * modLfo1Decay * 0.45f + envVal * modEnvDecay * 0.45f);
        const float modDrift = juce::jlimit(0.0f, 1.0f,
            baseDrift + lfo2Val * modLfo2Drift * 0.5f);
        const float modShift = juce::jlimit(-500.0f, 500.0f,
            baseShift + lfo2Val * modLfo2Shift * 250.0f);

        const float filterModSemis = (lfo1Val * modLfo1Filter + envVal * modEnvFilter) * 24.0f;
        const float filterMul = std::pow(2.0f, filterModSemis / 12.0f);
        const float modHpf = juce::jlimit(20.0f, 4000.0f, baseHpf * filterMul);
        const float modLpf = juce::jlimit(300.0f, 20000.0f, baseLpf * filterMul);

        float wetL = 0.0f;
        float wetR = 0.0f;

        if (modeCrossfadeSamplesRemaining > 0)
        {
            float outPrevL = 0.0f, outPrevR = 0.0f;
            float outNextL = 0.0f, outNextR = 0.0f;

            bankPrimary.processSample(dryL, dryR, static_cast<breach::ResonatorMode>(previousMode),
                                      voiceCount, currentVoiceSpecs, arpGates,
                                      modDecay, baseDamping, baseTone, modDrift, outPrevL, outPrevR);

            bankSecondary.processSample(dryL, dryR, static_cast<breach::ResonatorMode>(activeMode),
                                        voiceCount, currentVoiceSpecs, arpGates,
                                        modDecay, baseDamping, baseTone, modDrift, outNextL, outNextR);

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
                bankSecondary.prepare(currentSampleRate);
            }
        }
        else
        {
            bankPrimary.processSample(dryL, dryR, static_cast<breach::ResonatorMode>(activeMode),
                                      voiceCount, currentVoiceSpecs, arpGates,
                                      modDecay, baseDamping, baseTone, modDrift, wetL, wetR);
        }

        if (modDrift > 0.005f)
        {
            const float smearGain = modDrift * 0.62f;
            wetL = smearDiffuserL1.process(wetL, static_cast<float>(currentSampleRate * 0.0073), smearGain);
            wetL = smearDiffuserL2.process(wetL, static_cast<float>(currentSampleRate * 0.0131), smearGain);
            wetR = smearDiffuserR1.process(wetR, static_cast<float>(currentSampleRate * 0.0089), smearGain);
            wetR = smearDiffuserR2.process(wetR, static_cast<float>(currentSampleRate * 0.0157), smearGain);
        }

        wetL = shifterL.process(wetL, modShift);
        wetR = shifterR.process(wetR, modShift);

        wetHpfL.setParameters(modHpf, 0.707f);
        wetHpfR.setParameters(modHpf, 0.707f);
        wetLpfL.setParameters(modLpf, 0.707f);
        wetLpfR.setParameters(modLpf, 0.707f);

        wetL = wetLpfL.processLowpass(wetHpfL.processHighpass(wetL));
        wetR = wetLpfR.processLowpass(wetHpfR.processHighpass(wetR));

        wetL = breach::dsp::fastTanh(wetL * 0.85f) * 1.15f;
        wetR = breach::dsp::fastTanh(wetR * 0.85f) * 1.15f;

        const float dryGain = std::cos(depth * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(depth * juce::MathConstants<float>::halfPi);

        leftData[n]  = dryL * dryGain + wetL * wetGain;
        rightData[n] = dryR * dryGain + wetR * wetGain;
    }

    const float invN = 1.0f / static_cast<float>(numSamples);
    telemetry.inputRmsL.store(std::sqrt(sumSqL * invN), std::memory_order_relaxed);
    telemetry.inputRmsR.store(std::sqrt(sumSqR * invN), std::memory_order_relaxed);
    telemetry.inputPeak.store(blockPeak, std::memory_order_relaxed);
    telemetry.activeVoiceCount.store(voiceCount, std::memory_order_relaxed);
    telemetry.activeMode.store(activeMode, std::memory_order_relaxed);
    telemetry.activeArpStep.store(currentLitVoice, std::memory_order_relaxed);
    telemetry.midiOverrideActive.store(midiOverride && voicingEngine.hasActiveMidiNotes(), std::memory_order_relaxed);

    for (int v = 0; v < breach::VisualTelemetry::kMaxVoices; ++v)
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
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BreachResonatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreachResonatorAudioProcessor();
}
