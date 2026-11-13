#pragma once
#include "DspBuildingBlocks.h"
#include "ChordVoicingEngine.h"
#include <array>
#include <cmath>

namespace breach::dsp
{
class ResonatorBank
{
public:
    static constexpr int kMaxVoices = VisualTelemetry::kMaxVoices;

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
            voice.diffuser1.prepare(fs, 35.0f);
            voice.diffuser2.prepare(fs, 55.0f);
            voice.driftPhase = static_cast<float>(v) * 0.83f;
            voice.driftFreqHz = 0.17f + static_cast<float>(v) * 0.043f;
            voice.energyMeter = 0.0f;
            voice.arpGateSmoothed = 1.0f;
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
            voice.diffuser1.reset();
            voice.diffuser2.reset();
            voice.energyMeter = 0.0f;
        }
    }

    void processSample(float inL,
                       float inR,
                       ResonatorMode mode,
                       int activeVoiceCount,
                       const std::array<VoicePitchSpec, kMaxVoices>& pitchSpecs,
                       const std::array<float, kMaxVoices>& arpGateTargets,
                       float decay,
                       float damping,
                       float tone,
                       float driftAmount,
                       float& outWetL,
                       float& outWetR) noexcept
    {
        const float monoExcitation = 0.5f * (inL + inR);
        float sumL = 0.0f;
        float sumR = 0.0f;

        const float gateCoeff = 0.992f;
        const float energyDecay = 0.9985f;

        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& voice = voices[static_cast<size_t>(v)];
            const bool isActive = (v < activeVoiceCount);

            const float targetGate = isActive ? arpGateTargets[static_cast<size_t>(v)] : 0.0f;
            voice.arpGateSmoothed = flushDenormal(gateCoeff * voice.arpGateSmoothed + (1.0f - gateCoeff) * targetGate);

            if (!isActive && voice.energyMeter < 1.0e-5f)
                continue;

            voice.driftPhase += voice.driftFreqHz / static_cast<float>(fs);
            if (voice.driftPhase >= 1.0f)
                voice.driftPhase -= 1.0f;

            const float driftWave = std::sin(kTwoPi * voice.driftPhase) * 0.65f
                                  + std::sin(kTwoPi * voice.driftPhase * 2.31f + 1.1f) * 0.35f;
            const float driftCents = driftWave * driftAmount * 35.0f;
            const float freq = pitchSpecs[static_cast<size_t>(v)].frequencyHz * std::pow(2.0f, driftCents / 1200.0f);

            const float hfRoll = 1.0f - damping * 0.45f * juce::jlimit(0.0f, 1.0f, (freq - 600.0f) / 6000.0f);
            const float excitedInput = monoExcitation * voice.arpGateSmoothed * hfRoll;

            float voiceOut = 0.0f;

            switch (mode)
            {
                case ResonatorMode::CleanBandpass:
                {
                    const float q = 14.0f + std::pow(decay, 1.8f) * 125.0f;
                    voice.svfPrimary.setParameters(freq, q);
                    const float bp = voice.svfPrimary.processBandpass(excitedInput);
                    const float comp = std::min(3.5f, std::sqrt(q) * 0.32f);
                    voiceOut = bp * comp;
                    break;
                }

                case ResonatorMode::CombString:
                {
                    const float effectiveDamp = juce::jlimit(0.05f, 0.95f, damping * 0.85f + (0.2f - tone * 0.15f));
                    voiceOut = voice.comb.process(excitedInput, freq, decay, effectiveDamp) * 1.4f;
                    break;
                }

                case ResonatorMode::DualFormant:
                {
                    const float q1 = 12.0f + std::pow(decay, 1.7f) * 95.0f;
                    const float formantRatio = 2.25f + tone * 0.75f;
                    const float freq2 = std::min(static_cast<float>(fs * 0.42), freq * formantRatio);
                    const float q2 = q1 * 0.75f;

                    voice.svfPrimary.setParameters(freq, q1);
                    voice.svfSecondary.setParameters(freq2, q2);

                    const float f1 = voice.svfPrimary.processBandpass(excitedInput);
                    const float f2 = voice.svfSecondary.processBandpass(excitedInput) * 0.65f;
                    const float comp = std::min(3.2f, std::sqrt(q1) * 0.28f);
                    voiceOut = (f1 + f2) * comp;
                    break;
                }

                case ResonatorMode::DistortedRes:
                {
                    const float q = 16.0f + std::pow(decay, 1.7f) * 110.0f;
                    voice.svfPrimary.setParameters(freq, q);
                    const float bp = voice.svfPrimary.processBandpass(excitedInput);
                    const float driveGain = (2.2f + tone * 1.1f) * std::min(3.5f, std::sqrt(q) * 0.35f);
                    const float saturated = fastTanh(bp * driveGain + 0.08f) - fastTanh(0.08f);
                    voiceOut = saturated * 0.85f;
                    break;
                }

                case ResonatorMode::LongTailPad:
                default:
                {
                    const float longDecay = juce::jlimit(0.0f, 1.0f, 0.45f + decay * 0.54f);
                    const float q = 25.0f + std::pow(longDecay, 1.6f) * 145.0f;
                    voice.svfPrimary.setParameters(freq, q);
                    const float bp = voice.svfPrimary.processBandpass(excitedInput) * std::min(3.6f, std::sqrt(q) * 0.32f);

                    const float d1 = (11.3f + static_cast<float>(v) * 2.7f) * 0.001f * static_cast<float>(fs);
                    const float d2 = (19.7f + static_cast<float>(v) * 3.1f) * 0.001f * static_cast<float>(fs);
                    const float diff1 = voice.diffuser1.process(bp, d1, 0.68f);
                    const float diff2 = voice.diffuser2.process(diff1, d2, 0.65f);
                    voiceOut = diff2 * 1.15f;
                    break;
                }
            }

            const float dampCutoff = juce::jmap(damping, 0.0f, 1.0f, 18000.0f, 1600.0f);
            voice.svfDampLp.setParameters(dampCutoff, 0.707f);
            voiceOut = voice.svfDampLp.processLowpass(voiceOut);

            const float absE = std::abs(voiceOut);
            if (absE > voice.energyMeter)
                voice.energyMeter = absE;
            else
                voice.energyMeter = flushDenormal(voice.energyMeter * energyDecay);

            sumL += voiceOut * pitchSpecs[static_cast<size_t>(v)].panL;
            sumR += voiceOut * pitchSpecs[static_cast<size_t>(v)].panR;
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
        AllpassDiffuser diffuser1;
        AllpassDiffuser diffuser2;
        float driftPhase = 0.0f;
        float driftFreqHz = 0.2f;
        float energyMeter = 0.0f;
        float arpGateSmoothed = 1.0f;
    };

    double fs = 44100.0;
    std::array<ResonatorVoice, kMaxVoices> voices{};
};
} // namespace breach::dsp
