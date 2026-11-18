#pragma once
#include "ThirdParty/STK/StkFormant.h"
#include "RingTimeDSP.h"

namespace breach::formant
{
constexpr double pi = 3.14159265358979323846;
struct Parameters { double frequency, radius, gain; };

inline std::array<Parameters, 4> design(double sr, float pitch, float vowel,
                                       float decay, float damping, float qMod, double ringSeconds = 0) noexcept
{
    const double position = std::clamp(static_cast<double>(vowel), 0.0, 1.0) * 4.0;
    const auto first = static_cast<size_t>(position);
    const auto second = std::min(size_t(4), first + 1);
    const double fraction = position - static_cast<double>(first);
    // Partial pitch tracking moves the vocal body without erasing the note anchor.
    const double tract = std::clamp(std::pow(std::max(25.0, static_cast<double>(pitch)) / 220.0, 0.22), 0.7, 1.6);
    const double loss = (0.65 + 2.0 * std::clamp(static_cast<double>(damping), 0.0, 1.0))
        * std::exp2(-0.5 * std::clamp(static_cast<double>(qMod), -1.0, 1.0))
        / (ringSeconds > 0 ? 2.15 * (ringSeconds / .19)
                          : (0.65 + 2.85 * std::clamp(static_cast<double>(decay), 0.0, 1.0)));
    std::array<Parameters, 4> result {};
    double sum = 0.0;
    for (size_t n = 0; n < 4; ++n)
    {
        const auto& a = stkport::vowels[first][n]; const auto& b = stkport::vowels[second][n];
        const double frequency = (a.frequency + fraction * (b.frequency - a.frequency)) * tract;
        // STK table radii are interpreted at a 44.1 kHz reference. Interpolate
        // physical loss (log radius) so OS/rate changes retain bandwidth in Hz.
        const double logRadius = std::log(a.radius) + fraction * (std::log(b.radius) - std::log(a.radius));
        const double radius = std::exp(logRadius * (44100.0 / sr) * loss);
        const double gain = std::pow(10.0, (a.gainDb + fraction * (b.gainDb - a.gainDb)) / 20.0);
        const double fade = std::clamp((sr * 0.45 - frequency) / (sr * 0.08), 0.0, 1.0);
        result[n] = {std::clamp(frequency, 20.0, sr * 0.42), radius, gain * fade};
        sum += gain;
    }
    for (auto& p : result) p.gain /= std::max(1.0e-9, sum);
    return result;
}

// Four imported STK formants plus a pitched resonator. This processes the
// external excitation; it does not run STK's singing wavetable/noise generator.
class Voice
{
public:
    void prepare(double sr) noexcept
    {
        sampleRate = sr; interval = std::max(1, static_cast<int>(std::lround(sr / 2000.0)));
        const double seconds = static_cast<double>(interval) / sr;
        anchor.prepare(sr, seconds); for (auto& f : filters) f.prepare(sr, seconds);
        countdown = 0;
    }
    void reset() noexcept { anchor.reset(); for (auto& f : filters) f.reset(); countdown = 0; }
    float process(float input, float pitch, float vowel, float decay, float damping, float qMod, double ringSeconds = 0) noexcept
    {
        if (countdown-- <= 0)
        {
            const auto parameters = design(sampleRate, pitch, vowel, decay, damping, qMod, ringSeconds);
            const double onset = ringSeconds > 0 ? ring::onsetGain(ringSeconds, .19) : 1;
            for (size_t n = 0; n < 4; ++n)
            {
                filters[n].setTargets(parameters[n].frequency, parameters[n].radius, parameters[n].gain * onset);
                filters[n].advance();
            }
            const double rt60 = ringSeconds > 0 ? std::clamp(ringSeconds * std::exp2(.35 * std::clamp(static_cast<double>(qMod), -1.0, 1.0)), .006, ring::maxSeconds)
                : 0.04 * std::pow(20.0, std::clamp(static_cast<double>(decay), 0.0, 1.0));
            anchor.setTargets(std::clamp(static_cast<double>(pitch), 25.0, sampleRate * 0.4),
                              std::exp(-std::log(1000.0) / (rt60 * sampleRate)), onset);
            anchor.advance(); countdown = interval - 1;
        }
        double body = 0.0;
        for (auto& f : filters) body += f.tick(input);
        // Nominal send calibration for the new constant-peak filters. This
        // is fixed gain, not automatic loudness matching or a compressor.
        return static_cast<float>(4.0 * (0.6 * anchor.tick(input) + 0.4 * body));
    }
private:
    std::array<stkport::FormSwep, 4> filters;
    stkport::FormSwep anchor;
    double sampleRate = 48000.0;
    int interval = 24, countdown = 0;
};

class Bank
{
public:
    void prepare(double sr) noexcept { activeRate = 0.0; selectRate(sr); }
    void selectRate(double sr) noexcept
    {
        if (activeRate == sr) return;
        activeRate = sr; for (auto& voice : voices) for (auto& channel : voice) channel.prepare(sr);
    }
    void reset() noexcept { for (auto& voice : voices) for (auto& channel : voice) channel.reset(); }
    float process(size_t voice, size_t channel, float input, float pitch, float vowel,
                  float decay, float damping, float qMod, double ringSeconds = 0) noexcept
    {
        return voices[voice][channel].process(input, pitch, vowel, decay, damping, qMod, ringSeconds);
    }
private:
    std::array<std::array<Voice, 2>, 12> voices;
    double activeRate = 0.0;
};
} // namespace breach::formant
