#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace breach::spatial
{
constexpr double pi = 3.14159265358979323846;
struct Stereo { float left, right; };

// H(z) = (z^-D - g) / (1 - g*z^-D). Allocation occurs only in prepare().
class DelayAllpass
{
public:
    void prepare(double sampleRate, double seconds, float gain)
    {
        buffer.assign(static_cast<size_t>(std::max(1, static_cast<int>(std::lround(sampleRate * seconds)))), 0.0f);
        coefficient = std::clamp(gain, -0.95f, 0.95f); index = 0;
    }
    void reset() noexcept { std::fill(buffer.begin(), buffer.end(), 0.0f); index = 0; }
    float process(float input) noexcept
    {
        const float delayed = buffer[index];
        const float output = delayed - coefficient * input;
        buffer[index] = input + coefficient * output;
        if (++index == buffer.size()) index = 0;
        return output;
    }
    size_t delaySamples() const noexcept { return buffer.size(); }
private:
    std::vector<float> buffer;
    size_t index = 0;
    float coefficient = 0.5f;
};

// An orthonormal, feed-forward diffusion network. There is no reverb feedback
// loop or gain boost. Stereo encoding/decoding are unit-norm projections.
class StereoDiffuser
{
public:
    static std::array<float, 4> mix(const std::array<float, 4>& x) noexcept
    {
        return {{(x[0] + x[1] + x[2] + x[3]) * 0.5f,
                 (x[0] - x[1] + x[2] - x[3]) * 0.5f,
                 (x[0] + x[1] - x[2] - x[3]) * 0.5f,
                 (x[0] - x[1] - x[2] + x[3]) * 0.5f}};
    }
    void prepare(double sampleRate)
    {
        constexpr double times[2][4] = {{0.0051, 0.0073, 0.0113, 0.0137}, {0.0173, 0.0197, 0.0239, 0.0293}};
        for (size_t stage = 0; stage < 2; ++stage)
            for (size_t channel = 0; channel < 4; ++channel)
                stages[stage][channel].prepare(sampleRate, times[stage][channel], stage == 0 ? .57f : .43f);
    }
    void reset() noexcept { for (auto& stage : stages) for (auto& filter : stage) filter.reset(); }
    Stereo process(Stereo input, float amount) noexcept
    {
        constexpr float scale = 0.7071067811865475244f;
        // Use distinct orthogonal vectors for L and R, including for mono input.
        std::array<float, 4> x {{input.left * scale, input.right * scale, -input.left * scale, input.right * scale}};
        for (auto& stage : stages)
        {
            for (size_t c = 0; c < 4; ++c) x[c] = stage[c].process(x[c]);
            x = mix(x);
        }
        const Stereo diffuse {(x[0] - x[2]) * scale, (x[1] + x[3]) * scale};
        const float wet = std::clamp(amount, 0.0f, 1.0f) * 0.85f;
        // Run the network even at zero space so automation never resumes stale tails.
        return {input.left + wet * (diffuse.left - input.left), input.right + wet * (diffuse.right - input.right)};
    }
private:
    std::array<std::array<DelayAllpass, 4>, 2> stages;
};

// A complete two-channel unitary network: no channels are discarded during
// decoding. At full SMEAR the integrated stereo energy equals the input energy.
// A convex focused/diffuse blend is used between endpoints, without adaptive gain.
class ShortSmear
{
public:
    void prepare(double sampleRate)
    {
        constexpr double times[2][2]={{.0007,.0019},{.0037,.0127}};
        for(size_t stage=0;stage<2;++stage)for(size_t channel=0;channel<2;++channel)
            stages[stage][channel].prepare(sampleRate,times[stage][channel],stage==0?.48f:.36f);
    }
    void reset() noexcept {for(auto& stage:stages)for(auto& filter:stage)filter.reset();}
    Stereo process(Stereo input,float amount) noexcept
    {
        constexpr float rootHalf=.7071067811865475244f;
        float left=input.left,right=input.right;
        for(auto& stage:stages)
        {
            const float a=stage[0].process(left),b=stage[1].process(right);
            left=(a+b)*rootHalf;right=(a-b)*rootHalf;
        }
        const float wet=std::clamp(amount,0.0f,1.0f);
        return {input.left+wet*(left-input.left),input.right+wet*(right-input.right)};
    }
private:
    std::array<std::array<DelayAllpass,2>,2> stages;
};

// H(z) = (a - z^-2) / (1 - a*z^-2), plus one sample on the Q branch.
// Coefficients independently fitted from this transfer function; no external
// library code or runtime dependency. See Tools/design_hilbert.py.
class QuadratureNetwork
{
public:
    void reset() noexcept { inPhase = {}; quadrature = {}; delayedQ = 0.0; }
    std::array<double, 2> process(float input) noexcept
    {
        constexpr std::array<double, 8> iCoefficients {{
            0.07795730098415393, 0.4832710064938403, 0.7937441658474078, 0.9285464762981879,
            0.9764661305905062, 0.9923920735909155, 0.9975948175777952, 0.9993588208487786}};
        constexpr std::array<double, 8> qCoefficients {{
            0.2677453259696965, 0.6645400356022044, 0.8773757592862564, 0.9588513170308839,
            0.986597361795101, 0.9957004293649796, 0.9986941249875695, 0.9998079786314075}};
        double i = input, q = input;
        for (size_t n = 0; n < 8; ++n)
        {
            i = inPhase[n].process(i, iCoefficients[n]);
            q = quadrature[n].process(q, qCoefficients[n]);
        }
        const double outputQ = delayedQ; delayedQ = q;
        return {{i, outputQ}};
    }
private:
    struct Section
    {
        std::array<double, 2> state {{0.0, 0.0}};
        size_t index = 0;
        double process(double x, double a) noexcept
        {
            const double y = a * x + state[index];
            state[index] = a * y - x;
            index ^= 1u;
            return y;
        }
    };
    std::array<Section, 8> inPhase, quadrature;
    double delayedQ = 0.0;
};

class StereoFrequencyShifter
{
public:
    void prepare(double sr) noexcept { sampleRate = sr; fadeStep = static_cast<float>(1.0 / (0.02 * sr)); reset(); }
    void reset() noexcept { left.reset(); right.reset(); phase = 0.0; blend = 0.0f; }
    Stereo process(Stereo input, float shiftHz) noexcept
    {
        const auto l = left.process(input.left), r = right.process(input.right);
        // One coherent oscillator for both channels. Keep phase continuous at zero
        // and across sign changes; the quadrature filters are never hard-bypassed.
        phase += 2.0 * pi * static_cast<double>(shiftHz) / sampleRate;
        if (phase > pi) phase -= 2.0 * pi;
        if (phase < -pi) phase += 2.0 * pi;
        const float target = std::clamp(std::abs(shiftHz) * 0.5f, 0.0f, 1.0f);
        blend += std::clamp(target - blend, -fadeStep, fadeStep);
        if (blend == 0.0f) return input;
        const double cosine = std::cos(phase), sine = std::sin(phase);
        const float outL = static_cast<float>(l[0] * cosine - l[1] * sine);
        const float outR = static_cast<float>(r[0] * cosine - r[1] * sine);
        return {input.left + blend * (outL - input.left), input.right + blend * (outR - input.right)};
    }
private:
    QuadratureNetwork left, right;
    double sampleRate = 48000.0, phase = 0.0;
    float blend = 0.0f, fadeStep = 0.001f;
};

// All supported oversampling rates are prepared off the audio thread.
class SpatialEngine
{
public:
    void prepare(double baseSampleRate)
    {
        for (size_t i = 0; i < banks.size(); ++i)
        {
            const double sr = baseSampleRate * static_cast<double>(1u << i);
            banks[i].diffuser.prepare(sr); banks[i].compact.prepare(sr); banks[i].shifter.prepare(sr);
            banks[i].fadeStep = static_cast<float>(1 / (.020 * sr));
        }
        activeBank = -1;
    }
    void reset() noexcept { for (auto& bank : banks) bank.reset(); activeBank = -1; }
    void selectRate(int index) noexcept
    {
        index = std::clamp(index, 0, 2);
        if (index != activeBank)
        {
            activeBank = index;
            // No allocation, and no dormant audio from an earlier rate selection.
            banks[static_cast<size_t>(index)].reset();
        }
    }
    Stereo process(Stereo input, float space, float shift, bool shortSmear = false) noexcept
    {
        auto& bank = banks[static_cast<size_t>(activeBank)];
        const auto legacy = bank.diffuser.process(input, space);
        const auto compact = bank.compact.process(input, space);
        if (!bank.initialized) { bank.blend = shortSmear ? 1.0f : 0.0f; bank.initialized = true; }
        const float target = shortSmear ? 1.0f : 0.0f;
        bank.blend += std::clamp(target - bank.blend, -bank.fadeStep, bank.fadeStep);
        const Stereo diffuse = bank.blend <= 0 ? legacy : (bank.blend >= 1 ? compact : Stereo {
            legacy.left + bank.blend * (compact.left - legacy.left),
            legacy.right + bank.blend * (compact.right - legacy.right)});
        return bank.shifter.process(diffuse, shift);
    }
private:
    struct Bank
    {
        StereoDiffuser diffuser;
        ShortSmear compact;
        StereoFrequencyShifter shifter;
        float blend = 0, fadeStep = .001f;
        bool initialized = false;
        void reset() noexcept { diffuser.reset(); compact.reset(); shifter.reset(); blend = 0; initialized = false; }
    };
    std::array<Bank, 3> banks;
    int activeBank = -1;
};
} // namespace breach::spatial
