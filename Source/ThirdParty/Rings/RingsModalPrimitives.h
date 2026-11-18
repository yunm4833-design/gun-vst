// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// BREACH adaptation of the Rings modal path and stmlib filter kernel.
// See MODAL_PROVENANCE.md for pinned originals and exact port changes.
#pragma once
#include <cmath>
namespace breach::ringsmodal {
class CosineOscillator {
 public:
  CosineOscillator() { }
  ~CosineOscillator() { }

  inline void Init(float frequency) {
    iir_coefficient_ = 2.0f * std::cos(6.2831853071795864769f * frequency);
    initial_amplitude_ = iir_coefficient_ * 0.25f;
    Start();
  }
  
  inline void InitApproximate(float frequency) {
    float sign = 16.0f;
    frequency -= 0.25f;
    if (frequency < 0.0f) {
      frequency = -frequency;
    } else {
      if (frequency > 0.5f) {
        frequency -= 0.5f;
      } else {
        sign = -16.0f;
      }
    }
    iir_coefficient_ = sign * frequency * (1.0f - 2.0f * frequency);
    initial_amplitude_ = iir_coefficient_ * 0.25f;
  }
  
  inline void Start() {
    y1_ = initial_amplitude_;
    y0_ = 0.5f;
  }
  
  inline float value() const {
    return y1_ + 0.5f;
  }

  inline float Next() {
    float temp = y0_;
    y0_ = iir_coefficient_ * y0_ - y1_;
    y1_ = temp;
    return temp + 0.5f;
  }
  
 private:
  float y1_;
  float y0_;
  float iir_coefficient_;
  float initial_amplitude_;

  CosineOscillator(const CosineOscillator&) = delete;
  CosineOscillator& operator=(const CosineOscillator&) = delete;
};


// Scalar bandpass branches copied from stmlib::Svf. Double precision replaces
// MCU float state; coefficient design uses exact tangent and explicit damping.
class Svf
{
public:
    void Reset() noexcept { state_1_ = state_2_ = 0.0; }
    void set_g_r(double g, double r) noexcept
    {
        g_ = g; r_ = r; h_ = 1.0 / (1.0 + r_ * g_ + g_ * g_);
    }
    double Process(double in) noexcept
    {
        double hp, bp, lp;
        hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
        bp = g_ * hp + state_1_;
        state_1_ = g_ * hp + bp;
        lp = g_ * bp + state_2_;
        state_2_ = g_ * bp + lp;
        return bp;
    }
private:
    double g_ = 0.1, r_ = 0.1, h_ = 1.0;
    double state_1_ = 0.0, state_2_ = 0.0;
};
} // namespace breach::ringsmodal
