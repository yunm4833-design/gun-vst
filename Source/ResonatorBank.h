#pragma once
#include "DspBuildingBlocks.h"
#include "ChordVoicingEngine.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace breach::dsp
{
class ResonatorBank
{
public:
    static constexpr int kMaxVoices = breach::kMaxVoices;

    void prepare(double sampleRate)
    {
        fs = std::max(8000.0, sampleRate);
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& voice = voices[static_cast<size_t>(v)];
            voice.svfPrimary.prepare(fs);
            voice.svfSecondary.prepare(fs);
            voice.svfDampLp.prepare(fs);
            voice.comb.prepare(fs);
            voice.combUnison.prepare(fs);
            voice.diffuser1.prepare(fs, 35.0f);
            voice.diffuser2.prepare(fs, 55.0f);
            voice.driftPhase = static_cast<float>(v) * 0.83f;
            voice.driftFreqHz = 0.17f + static_cast<float>(v) * 0.043f;
            voice.energyMeter = 0.0f;
            voice.arpGateSmoothed = 1.0f;
            voice.attackEnv = 1.0f;
        }
    }

    void reset() noexcept
    {
        for (auto& voice : voices)
        {
            voice.svfPrimary.reset();
            voice.svfSecondary.reset();
            voice.svfDampLp.reset();
            voice.comb.reset();
            voice.combUnison.reset();
            voice.diffuser1.reset();
            voice.diffuser2.reset();
            voice.energyMeter = 0.0f;
            voice.attackEnv = 1.0f;
        }
    }

    void triggerAttackSwell() noexcept
    {
        for (auto& voice : voices)
            voice.attackEnv = 0.0f;
    }

    void processSample(float excitedMonoIn,
                       ResonatorMode firearmMode,
                       ResonatorModelOverride modelOverride,
                       int activeVoiceCount,
                       const std::array<VoicePitchSpec, kMaxVoices>& pitchSpecs,
                       const std::array<float, kMaxVoices>& arpGateTargets,
                       const std::array<float, kMaxVoices>& perVoiceLevels,
                       const std::array<float, kMaxVoices>& perVoiceDecayMul,
                       float attackMs,
                       float baseDecay,
                       float fbDamping,
                       float stiffness,
                       float filterQNorm,
                       float driftAmount,
                       float unisonAmount,
                       const MotionLfoBank& motionLfo,
                       bool motionEnabled,
                       float motionStrength,
                       MotionShape motionShape,
                       float& outWetL,
                       float& outWetR) noexcept
    {
        float sumL = 0.0f;
        float sumR = 0.0f;

        const float gateCoeff = 0.992f;
        const float energyDecay = 0.9985f;
        const float attackInc = 1.0f / std::max(1.0f, attackMs * 0.001f * static_cast<float>(fs));
        const float qScale = 0.40f + 1.80f * std::pow(juce::jlimit(0.0f, 1.0f, filterQNorm), 1.35f);

        const float unisonCents = unisonAmount * 22.0f;
        const float unisonDetuneUp = std::pow(2.0f, unisonCents / 1200.0f);
        const float unisonDetuneDn = std::pow(2.0f, -unisonCents / 1200.0f);

        // Resolve effective resonator topology (either Firearm Mode or Stage 2 Model Override)
        ResonatorMode effectiveTopo = firearmMode;
        if (modelOverride == ResonatorModelOverride::BandpassSVF)
            effectiveTopo = ResonatorMode::CleanBandpass;
        else if (modelOverride == ResonatorModelOverride::CombString)
            effectiveTopo = ResonatorMode::CombString;
        else if (modelOverride == ResonatorModelOverride::DualFormant)
            effectiveTopo = ResonatorMode::DualFormant;

        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& voice = voices[static_cast<size_t>(v)];
            const bool isActive = (v < activeVoiceCount);

            const float targetGate = isActive ? arpGateTargets[static_cast<size_t>(v)] : 0.0f;
            if (targetGate > 0.5f && voice.arpGateSmoothed < 0.05f)
                voice.attackEnv = 0.0f;

            voice.arpGateSmoothed = flushDenormal(gateCoeff * voice.arpGateSmoothed + (1.0f - gateCoeff) * targetGate);

            if (!isActive && voice.energyMeter < 1.0e-5f)
                continue;

            voice.attackEnv = std::min(1.0f, voice.attackEnv + attackInc);

            // Drift slow random frequency wander
            voice.driftPhase += voice.driftFreqHz / static_cast<float>(fs);
            if (voice.driftPhase >= 1.0f)
                voice.driftPhase -= 1.0f;

            const float driftWave = std::sin(kTwoPi * voice.driftPhase) * 0.65f
                                  + std::sin(kTwoPi * voice.driftPhase * 2.31f + 1.1f) * 0.35f;
            const float driftCents = driftWave * driftAmount * 35.0f;

            float motionPitchCents = 0.0f;
            float motionGainMul = 1.0f;
            if (motionEnabled && motionStrength > 0.001f)
            {
                const float lfoVal = motionLfo.getVoiceValue(v, std::max(1, activeVoiceCount), motionShape);
                motionPitchCents = lfoVal * motionStrength * 42.0f;
                motionGainMul = 1.0f - motionStrength * 0.50f * (0.5f - 0.5f * lfoVal);
            }

            const float freq = pitchSpecs[static_cast<size_t>(v)].frequencyHz * std::pow(2.0f, (driftCents + motionPitchCents) / 1200.0f);

            // Feedback damping attenuates high-frequency resonance energy naturally
            const float hfRoll = 1.0f - fbDamping * 0.45f * juce::jlimit(0.0f, 1.0f, (freq - 500.0f) / 6500.0f);
            const float voiceDecay = juce::jlimit(0.0f, 1.0f, baseDecay * perVoiceDecayMul[static_cast<size_t>(v)] * (1.0f - fbDamping * 0.18f));
            const float voiceLevel = perVoiceLevels[static_cast<size_t>(v)];
            const float excitedInput = excitedMonoIn * voice.arpGateSmoothed * voice.attackEnv * motionGainMul * hfRoll * voiceLevel;

            float voiceL = 0.0f;
            float voiceR = 0.0f;

            switch (effectiveTopo)
            {
                case ResonatorMode::CleanBandpass:
                {
                    const float q = juce::jlimit(4.0f, 195.0f, (14.0f + std::pow(voiceDecay, 1.8f) * 115.0f) * qScale);
                    const float comp = std::min(3.6f, std::sqrt(q) * 0.30f);

                    if (unisonAmount > 0.01f)
                    {
                        voice.svfPrimary.setParameters(freq * unisonDetuneDn, q);
                        voice.svfSecondary.setParameters(freq * unisonDetuneUp, q);
                        const float bp1 = voice.svfPrimary.processBandpass(excitedInput) * comp;
                        const float bp2 = voice.svfSecondary.processBandpass(excitedInput) * comp;
                        voiceL = (bp1 + bp2 * (1.0f - unisonAmount * 0.4f)) * 0.68f;
                        voiceR = (bp2 + bp1 * (1.0f - unisonAmount * 0.4f)) * 0.68f;
                    }
                    else
                    {
                        voice.svfPrimary.setParameters(freq, q);
                        const float bp = voice.svfPrimary.processBandpass(excitedInput) * comp;
                        voiceL = voiceR = bp;
                    }
                    break;
                }

                case ResonatorMode::CombString:
                {
                    const float effectiveDecay = juce::jlimit(0.0f, 1.0f, voiceDecay * (0.75f + 0.25f * filterQNorm));
                    if (unisonAmount > 0.01f)
                    {
                        const float c1 = voice.comb.process(excitedInput, freq * unisonDetuneDn, effectiveDecay, fbDamping, stiffness) * 1.15f;
                        const float c2 = voice.combUnison.process(excitedInput, freq * unisonDetuneUp, effectiveDecay, fbDamping, stiffness) * 1.15f;
                        voiceL = (c1 + c2 * (1.0f - unisonAmount * 0.4f)) * 0.72f;
                        voiceR = (c2 + c1 * (1.0f - unisonAmount * 0.4f)) * 0.72f;
                    }
                    else
                    {
                        const float c = voice.comb.process(excitedInput, freq, effectiveDecay, fbDamping, stiffness) * 1.35f;
                        voiceL = voiceR = c;
                    }
                    break;
                }

                case ResonatorMode::DualFormant:
                {
                    const float q1 = juce::jlimit(4.0f, 185.0f, (12.0f + std::pow(voiceDecay, 1.7f) * 90.0f) * qScale);
                    const float freq2 = std::min(static_cast<float>(fs * 0.42), freq * (2.25f + stiffness * 0.65f));
                    const float q2 = q1 * 0.75f;

                    voice.svfPrimary.setParameters(freq, q1);
                    voice.svfSecondary.setParameters(freq2, q2);

                    const float f1 = voice.svfPrimary.processBandpass(excitedInput);
                    const float f2 = voice.svfSecondary.processBandpass(excitedInput) * 0.65f;
                    const float comp = std::min(3.3f, std::sqrt(q1) * 0.27f);
                    const float monoFormant = (f1 + f2) * comp;
                    voiceL = monoFormant;
                    voiceR = monoFormant;
                    break;
                }

                case ResonatorMode::DistortedRes:
                {
                    const float q = juce::jlimit(4.0f, 190.0f, (16.0f + std::pow(voiceDecay, 1.7f) * 100.0f) * qScale);
                    const float driveGain = 2.4f * std::min(3.4f, std::sqrt(q) * 0.33f);

                    if (unisonAmount > 0.01f)
                    {
                        voice.svfPrimary.setParameters(freq * unisonDetuneDn, q);
                        voice.svfSecondary.setParameters(freq * unisonDetuneUp, q);
                        const float s1 = fastTanh(voice.svfPrimary.processBandpass(excitedInput) * driveGain + 0.08f) - fastTanh(0.08f);
                        const float s2 = fastTanh(voice.svfSecondary.processBandpass(excitedInput) * driveGain + 0.08f) - fastTanh(0.08f);
                        voiceL = (s1 + s2 * (1.0f - unisonAmount * 0.4f)) * 0.65f;
                        voiceR = (s2 + s1 * (1.0f - unisonAmount * 0.4f)) * 0.65f;
                    }
                    else
                    {
                        voice.svfPrimary.setParameters(freq, q);
                        const float sat = fastTanh(voice.svfPrimary.processBandpass(excitedInput) * driveGain + 0.08f) - fastTanh(0.08f);
                        voiceL = voiceR = sat * 0.85f;
                    }
                    break;
                }

                case ResonatorMode::LongTailPad:
                default:
                {
                    const float longDecay = juce::jlimit(0.0f, 1.0f, 0.45f + voiceDecay * 0.54f);
                    const float q = juce::jlimit(6.0f, 205.0f, (25.0f + std::pow(longDecay, 1.6f) * 130.0f) * qScale);
                    voice.svfPrimary.setParameters(freq, q);
                    const float bp = voice.svfPrimary.processBandpass(excitedInput) * std::min(3.5f, std::sqrt(q) * 0.30f);

                    const float d1 = (11.3f + static_cast<float>(v) * 2.7f) * 0.001f * static_cast<float>(fs);
                    const float d2 = (19.7f + static_cast<float>(v) * 3.1f) * 0.001f * static_cast<float>(fs);
                    const float diff1 = voice.diffuser1.process(bp, d1, 0.68f);
                    const float diff2 = voice.diffuser2.process(diff1, d2, 0.65f);
                    voiceL = diff1 * 0.6f + diff2 * 0.6f;
                    voiceR = diff2 * 1.15f;
                    break;
                }
            }

            // Per-voice Feedback Damping Lowpass
            const float dampCutoff = juce::jmap(fbDamping, 0.0f, 1.0f, 19000.0f, 1800.0f);
            voice.svfDampLp.setParameters(dampCutoff, 0.707f);
            voiceL = voice.svfDampLp.processLowpass(voiceL);
            voiceR = voiceL * 0.15f + voiceR * 0.85f;

            const float absE = 0.5f * (std::abs(voiceL) + std::abs(voiceR));
            if (absE > voice.energyMeter)
                voice.energyMeter = absE;
            else
                voice.energyMeter = flushDenormal(voice.energyMeter * energyDecay);

            sumL += voiceL * pitchSpecs[static_cast<size_t>(v)].panL;
            sumR += voiceR * pitchSpecs[static_cast<size_t>(v)].panR;
        }

        const float polyNorm = 1.0f / std::sqrt(static_cast<float>(std::max(1, activeVoiceCount)));
        outWetL = sumL * polyNorm;
        outWetR = sumR * polyNorm;
    }

    float getVoiceEnergy(int voiceIndex) const noexcept
    {
        if (voiceIndex < 0 || voiceIndex >= kMaxVoices)
            return 0.0f;
        return juce::jlimit(0.0f, 1.0f, voices[static_cast<size_t>(voiceIndex)].energyMeter * 1.8f);
    }

private:
    struct ResonatorVoice
    {
        TptSvf svfPrimary;
        TptSvf svfSecondary;
        TptSvf svfDampLp;
        FractionalCombFilter comb;
        FractionalCombFilter combUnison;
        AllpassDiffuser diffuser1;
        AllpassDiffuser diffuser2;
        float driftPhase = 0.0f;
        float driftFreqHz = 0.2f;
        float energyMeter = 0.0f;
        float arpGateSmoothed = 1.0f;
        float attackEnv = 1.0f;
    };

    double fs = 44100.0;
    std::array<ResonatorVoice, kMaxVoices> voices{};
};
} // namespace breach::dsp
