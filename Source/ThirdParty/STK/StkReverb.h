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
// Header-only adaptation of STK JCRev, NRev and PitShift.
// See REVERB_PROVENANCE.md for source revisions and port changes.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace breach::stkverb
{
constexpr double pi = 3.14159265358979323846;
struct Stereo { double left, right; };
class Delay
{
public:
    void prepare(int size) { data.assign(static_cast<size_t>(std::max(1, size)), 0); index = 0; }
    void reset() noexcept { std::fill(data.begin(), data.end(), 0); index = 0; }
    double read() const noexcept { return data[index]; }
    double tick(double input) noexcept
    {
        const double output = data[index]; data[index] = input;
        if (++index == data.size()) index = 0;
        return output;
    }
    int length() const noexcept { return static_cast<int>(data.size()); }
private:
    std::vector<double> data;
    size_t index = 0;
};
inline int primeLength(double x) noexcept
{
    int value = std::max(3, static_cast<int>(std::floor(x)) | 1);
    for (;; value += 2)
    {
        bool prime = true;
        for (int n = 3; n * n <= value; n += 2) if (value % n == 0) { prime = false; break; }
        if (prime) return value;
    }
}
class Network
{
public:
    void prepare(double sr, bool jc, double size)
    {
        sampleRate = sr; hall = jc;
        constexpr int jcLengths[9] = {1116,1356,1422,1617,225,341,441,211,179};
        constexpr int nrLengths[12] = {1433,1601,1867,2053,2251,2399,347,113,37,59,53,43};
        for (size_t i = 0; i < (hall ? 4u : 6u); ++i)
            comb[i].prepare(primeLength((hall ? jcLengths[i] / 44100.0 : nrLengths[i] / 25641.0) * sr * size));
        for (size_t i = 0; i < (hall ? 3u : 6u); ++i)
            allpass[i].prepare(primeLength((hall ? jcLengths[i+4] / 44100.0 : nrLengths[i+6] / 25641.0) * sr * size));
        if (hall) for (size_t i = 0; i < 2; ++i)
            output[i].prepare(primeLength(jcLengths[i+7] / 44100.0 * sr * size));
        reset(); setDecay(2, .5);
    }
    void reset() noexcept
    {
        for (auto& d : comb) d.reset();
        for (auto& d : allpass) d.reset();
        for (auto& d : output) d.reset();
        lowpass.fill(0); postLowpass = 0;
    }
    void setDecay(double seconds, double damping) noexcept
    {
        seconds = std::clamp(seconds, .2, 8.0);
        for (size_t i = 0; i < (hall ? 4u : 6u); ++i)
            coefficient[i] = std::pow(10.0, -3.0 * comb[i].length() / (seconds * sampleRate));
        const double cutoff = std::min(sampleRate * .4, 12000.0 * std::pow(.05, std::clamp(damping, 0.0, 1.0)));
        pole = std::exp(-2 * pi * cutoff / sampleRate);
    }
    Stereo process(double input) noexcept
    {
        auto ap = [](Delay& d, double x)
        {
            const double delayed = d.read(), next = x + .7 * delayed;
            d.tick(next); return delayed - .7 * next;
        };
        double sum = 0;
        if (hall)
        {
            double diffused = input;
            for (size_t i = 0; i < 3; ++i) diffused = ap(allpass[i], diffused);
            for (size_t i = 0; i < 4; ++i)
            {
                lowpass[i] = pole * lowpass[i] + (1-pole) * coefficient[i] * comb[i].read();
                const double temp = diffused + lowpass[i]; comb[i].tick(temp); sum += temp;
            }
            return {output[0].tick(sum * .25), output[1].tick(sum * .25)};
        }
        for (size_t i = 0; i < 6; ++i)
        {
            lowpass[i] = pole * lowpass[i] + (1-pole) * comb[i].read();
            sum += comb[i].tick(input + coefficient[i] * lowpass[i]);
        }
        sum /= 6;
        for (size_t i = 0; i < 3; ++i) sum = ap(allpass[i], sum);
        postLowpass = pole * postLowpass + (1-pole) * sum;
        sum = ap(allpass[3], postLowpass);
        return {ap(allpass[4], sum), ap(allpass[5], sum)};
    }
private:
    std::array<Delay, 6> comb, allpass;
    std::array<Delay, 2> output;
    std::array<double, 6> coefficient {}, lowpass {};
    double sampleRate = 48000, pole = .7, postLowpass = 0;
    bool hall = false;
};

// STK's two half-window-spaced delay heads, rate=1-shift and Hann crossfade.
// Fractional reads and a physical 80 ms window replace STK's integer window LUT.
class OctaveShift
{
public:
    void prepare(double sr) { length = std::max(64, static_cast<int>(sr * .080)); data.assign(static_cast<size_t>(length+8), 0); reset(); }
    void reset() noexcept { std::fill(data.begin(),data.end(),0); index=0; delay=length*.25; }
    double process(double input) noexcept
    {
        data[index] = input;
        delay -= 1; if (delay < 0) delay += length;
        double other = delay + length * .5; if (other >= length) other -= length;
        const double weight = .5 - .5 * std::cos(2 * pi * delay / length);
        const double result = weight * read(delay + 2) + (1-weight) * read(other + 2);
        if (++index == data.size()) index=0;
        return result;
    }
private:
    double read(double samples) const noexcept
    {
        double position = static_cast<double>(index) - samples;
        if (position < 0) position += data.size();
        if (position >= static_cast<double>(data.size())) position = 0;
        const auto a = static_cast<size_t>(position), b = (a+1)%data.size();
        const double fraction = position - static_cast<double>(a);
        return data[a] + fraction * (data[b]-data[a]);
    }
    std::vector<double> data;
    size_t index=0;
    int length=3840;
    double delay=0;
};
} // namespace breach::stkverb
