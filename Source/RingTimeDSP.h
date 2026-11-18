#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace breach::ring
{
constexpr double ln1000 = 6.907755278982137052;
constexpr double maxSeconds = 10.0;

struct Range { double shortest, longest; };

inline Range rangeFor(int topology, int mode) noexcept
{
    if (topology == 1) return {.06, 4.0};
    if (topology == 2) return {.025, 1.45};
    if (mode == 4) return {.07, 3.8};
    if (mode == 3) return {.024, 1.8};
    return {.025, 1.55};
}

inline double targetSeconds(Range range, float knob) noexcept
{
    const float position = std::clamp((knob - .05f) / .95f, 0.0f, 1.0f);
    return range.shortest * std::pow(range.longest / range.shortest, static_cast<double>(position));
}

// Bound the physical time after the trim, rather than clipping a knob value.
inline double voiceSeconds(double target, float trim) noexcept
{
    return std::clamp(target * static_cast<double>(trim), .006, maxSeconds);
}

// Onset compensation for peak-normalized resonators. The upper bound also
// limits the extra level for sustained input at long ring times.
inline double onsetGain(double seconds, double reference) noexcept
{
    return std::clamp(seconds / reference, .075, 6.0);
}

// Fixed strike calibration. Short times reach less of their steady-state gain
// during the attack. Limit the correction to about -2 to +4 dB.
inline float strikeGain(double seconds, double reference) noexcept
{
    constexpr double riseCompensation = .06;
    return static_cast<float>(std::clamp((1 + riseCompensation / seconds) / (1 + riseCompensation / reference), .78, 1.55));
}

// Relative tuning of the chord. Slot zero retains its individual tuning.
constexpr std::array<float, 12> tunePattern {{0, 1, -1, .58f, -.58f, .82f, -.82f, .35f, -.35f, .71f, -.71f, .46f}};
constexpr std::array<float, 12> panPattern {{-.72f, .72f, -1, 1, -.45f, .45f, -.82f, .82f, -.6f, .6f, -.91f, .91f}};
} // namespace breach::ring
