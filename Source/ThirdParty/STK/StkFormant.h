/*
The Synthesis ToolKit in C++ (STK)

Copyright (c) 1995-2023 Perry R. Cook and Gary P. Scavone

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

Any person wishing to distribute modifications to the Software is
asked to send the modifications to the original developer so that they
can be incorporated into the canonical version.  This is, however, not
a binding provision of this license.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
// BREACH header-only adaptation of STK FormSwep / Phonemes.
// Original sources, revision and modifications: PROVENANCE.md.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace breach::stkport {
struct Formant { double frequency, radius, gainDb; };
inline constexpr std::array<std::array<Formant, 4>, 5> vowels {{
    {{{770, 0.95, 0}, {1153, 0.97, -9}, {2450, 0.78, -29}, {3140, 0.8, -39}}}, // A: ahh
    {{{515, 0.977, 10}, {1805, 0.81, -10}, {2526, 0.875, -10}, {3103, 0.4, -13}}}, // E: ehh
    {{{273, 0.996, 10}, {2086, 0.945, -16}, {2754, 0.979, -12}, {3270, 0.44, -17}}}, // I: eee
    {{{637, 0.91, 0}, {895, 0.9, -3}, {2556, 0.95, -17}, {3070, 0.91, -20}}}, // O: ohh
    {{{349, 0.986, -10}, {918, 0.94, -20}, {2350, 0.96, -27}, {2731, 0.95, -33}}}, // U: ooo
}};

// The setResonance coefficient formulas and tick difference equation below
// are retained from STK FormSwep. Fixed storage replaces the STK Filter base.
class FormSwep
{
public:
    void prepare(double sr, double controlSeconds) noexcept
    {
        sampleRate = sr;
        sweepRate_ = std::clamp(controlSeconds / 0.008, 0.0, 1.0);
        reset();
    }
    void reset() noexcept
    {
        inputs_ = {}; outputs_ = {}; b_ = {}; a_ = {};
        frequency_ = targetFrequency_ = 0.0; radius_ = targetRadius_ = 0.0;
        gain_ = targetGain_ = 0.0; dirty_ = false; initialized = false;
    }
    void setResonance(double frequency, double radius) noexcept
    {
        radius_ = radius; frequency_ = frequency;
        a_[2] = radius * radius;
        a_[1] = -2.0 * radius * std::cos(6.2831853071795864769 * frequency / sampleRate);
        b_[0] = 0.5 - 0.5 * a_[2]; b_[1] = 0.0; b_[2] = -b_[0];
    }
    void setTargets(double frequency, double radius, double gain) noexcept
    {
        frequency = std::clamp(frequency, 20.0, sampleRate * 0.42);
        radius = std::clamp(radius, 0.01, 0.9999999);
        if (!initialized)
        {
            setResonance(frequency, radius); gain_ = gain;
            targetFrequency_ = frequency; targetRadius_ = radius; targetGain_ = gain;
            initialized = true; return;
        }
        if (frequency == targetFrequency_ && radius == targetRadius_ && gain == targetGain_) return;
        dirty_ = true;
        startFrequency_ = frequency_; startRadius_ = radius_; startGain_ = gain_;
        targetFrequency_ = frequency; targetRadius_ = radius; targetGain_ = gain;
        deltaFrequency_ = frequency - frequency_; deltaRadius_ = radius - radius_; deltaGain_ = gain - gain_;
        sweepState_ = 0.0;
    }
    // Upstream tick's parameter sweep moved to a fixed physical control rate.
    void advance() noexcept
    {
        if (!dirty_) return;
        sweepState_ += sweepRate_;
        if (sweepState_ >= 1.0)
        {
            sweepState_ = 1.0; dirty_ = false;
            radius_ = targetRadius_; frequency_ = targetFrequency_; gain_ = targetGain_;
        }
        else
        {
            radius_ = startRadius_ + deltaRadius_ * sweepState_;
            frequency_ = startFrequency_ + deltaFrequency_ * sweepState_;
            gain_ = startGain_ + deltaGain_ * sweepState_;
        }
        setResonance(frequency_, radius_);
    }
    double tick(double input) noexcept
    {
        inputs_[0] = gain_ * input;
        double output = b_[0] * inputs_[0] + b_[1] * inputs_[1] + b_[2] * inputs_[2];
        output -= a_[2] * outputs_[2] + a_[1] * outputs_[1];
        inputs_[2] = inputs_[1]; inputs_[1] = inputs_[0];
        outputs_[2] = outputs_[1]; outputs_[1] = output;
        return output;
    }
private:
    std::array<double, 3> inputs_ {}, outputs_ {}, a_ {}, b_ {};
    double sampleRate = 48000.0;
    double frequency_ = 0.0, radius_ = 0.0, gain_ = 0.0;
    double startFrequency_ = 0.0, startRadius_ = 0.0, startGain_ = 0.0;
    double targetFrequency_ = 0.0, targetRadius_ = 0.0, targetGain_ = 0.0;
    double deltaFrequency_ = 0.0, deltaRadius_ = 0.0, deltaGain_ = 0.0;
    double sweepState_ = 0.0, sweepRate_ = 0.1;
    bool dirty_ = false, initialized = false;
};
} // namespace breach::stkport
