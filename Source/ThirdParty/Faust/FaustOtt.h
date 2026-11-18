#pragma once

// C++ adaptation of David Braun's co.xfer_ott in Faust Libraries, MIT.
// Upstream revision: 1afe09ab24797807e6e22ac5623514fd7db02c43.
// See Licenses/Faust-OTT-MIT.txt for the notice and adaptation details.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace breach::faustOtt
{
struct Band
{
    double upperThreshold, upperKnee, lowerThreshold, lowerKnee, liftSlope;
    double makeup, liftCap, gainFloor, attack;
    std::array<double, 3> releases, makeupShape, downwardShape;
};

inline constexpr std::array<Band, 3> bands {{
    { -38.9401, .5, -46.0171, .0552, .74786, 14.0348, 35.901, -26.26, .00621537,
      {{ .0100376, .0148167, .030877 }}, {{ 1.83819, -1.21424, .37606 }}, {{ .96509, .03154, .00337 }} },
    { -34.8204, .530023, -46.5885, .4509, .75601, 9.05203, 36.285, -31.70, .006,
      {{ .0118089, .0174314, .030877 }}, {{ 1.63851, -.83224, .19373 }}, {{ .96161, .02205, .01634 }} },
    { -41.6879, .5, -47.4393, .0338, .74738, 14.064, 35.872, -25.95, .0096577,
      {{ .00702632, .00702632, .0103717 }}, {{ 1.84440, -1.22723, .38284 }}, {{ .97286, .05778, -.03064 }} }
}};

inline double releaseSeconds(std::size_t band, double timeScale) noexcept
{
    const auto& points = bands[band].releases;
    if (timeScale <= .5) return points[0] + (points[1] - points[0]) * (2 * timeScale);
    if (timeScale <= 1) return points[1] + (points[2] - points[1]) * (2 * timeScale - 1);
    return points[2] * timeScale;
}

inline double softplus(double width, double x) noexcept
{
    return std::max(0.0, x) + width * std::log1p(std::exp(-std::abs(x) / width));
}

inline double cubic(const std::array<double, 3>& coefficients, double depth) noexcept
{
    return depth * (coefficients[0] + depth * (coefficients[1] + depth * coefficients[2]));
}

inline double downwardStrength(double strength) noexcept
{
    constexpr std::array<double, 6> fitted {{ 0, .189716, .389364, .590038, .793116, 1 }};
    if (strength >= 1) return strength;
    const auto index = static_cast<std::size_t>(strength * 5);
    const double fraction = strength * 5 - static_cast<double>(index);
    return fitted[index] + fraction * (fitted[index + 1] - fitted[index]);
}

inline double gainDb(double levelDb, std::size_t band, double depth, double upward, double downward) noexcept
{
    const auto& fit = bands[band];
    // Integrated input-stage tuning: limit lift close to the noise floor and
    // tie the model's makeup to the amount of cut actually applied.
    const double guard = std::clamp((levelDb + 90) / 24, 0.0, 1.0);
    const double lift = std::min(fit.liftCap, depth * upward * fit.liftSlope
        * softplus(fit.lowerKnee, fit.lowerThreshold - levelDb)) * guard * guard * (3 - 2 * guard);
    const double cut = std::min(1.0, cubic(fit.downwardShape, depth) * downwardStrength(downward))
        * softplus(fit.upperKnee, levelDb - fit.upperThreshold);
    const double makeup = std::min(cut * .75, fit.makeup * cubic(fit.makeupShape, depth) * std::min(1.0, downward));
    return std::clamp(lift + makeup - cut, fit.gainFloor, 24.0);
}
} // namespace breach::faustOtt
