// Copyright 2015 Emilie Gillet.
// Copyright 2014 Emilie Gillet. (DelayLine portions)
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
// BREACH port: adapted from Rings String and stmlib DelayLine.
// Pinned source revisions and modifications are listed in PROVENANCE.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace breach::ringsport {
class DampingFilter {
 public:
  DampingFilter() { }
  ~DampingFilter() { }
  
  void Init() {
    x_ = 0.0f;
    x__ = 0.0f;
    brightness_ = 0.0f;
    brightness_increment_ = 0.0f;
    damping_ = 0.0f;
    damping_increment_ = 0.0f;
  }
   
  inline void Configure(float damping, float brightness, size_t size) {
    if (!size) {
      damping_ = damping;
      brightness_ = brightness;
      damping_increment_ = 0.0f;
      brightness_increment_ = 0.0f;
    } else {
      float step = 1.0f / static_cast<float>(size);
      damping_increment_ = (damping - damping_) * step;
      brightness_increment_ = (brightness - brightness_) * step;
    }
  }
   
  inline float Process(float x) {
    float h0 = (1.0f + brightness_) * 0.5f;
    float h1 = (1.0f - brightness_) * 0.25f;
    float y = damping_ * (h0 * x_ + h1 * (x + x__));
    x__ = x_;
    x_ = x;
    brightness_ += brightness_increment_;
    damping_ += damping_increment_;
    return y;
  }
 private:
  float x_;
  float x__;
  float brightness_;
  float brightness_increment_;
  float damping_;
  float damping_increment_;
  
  DampingFilter(const DampingFilter&) = delete;
  DampingFilter& operator=(const DampingFilter&) = delete;
};


// Runtime storage replaces the upstream fixed-size MCU buffer. Only Prepare
// allocates; the interpolation and allpass recurrences below are upstream code.
class DelayLine {
 public:
  void Prepare(size_t capacity) { line_.assign(capacity, 0.0f); Reset(); }
  void Reset() { std::fill(line_.begin(), line_.end(), 0.0f); write_ptr_ = 0; delay_ = 1; }
  void Write(float x) { line_[write_ptr_] = x; write_ptr_ = (write_ptr_ + line_.size() - 1) % line_.size(); }
  inline float Allpass(const float sample, size_t delay, const float coefficient) {
    float read = line_[(write_ptr_ + delay) % line_.size()];
    float write = sample + coefficient * read;
    Write(write);
    return -write * coefficient + read;
  }

  inline float WriteRead(const float sample, float delay) {
    Write(sample);
    return Read(delay);
  }
  
  inline float Read() const {
    return line_[(write_ptr_ + delay_) % line_.size()];
  }
  
  inline float Read(size_t delay) const {
    return line_[(write_ptr_ + delay) % line_.size()];
  }

  inline float Read(float delay) const {
    const int32_t delay_integral = static_cast<int32_t>(delay);
    const float delay_fractional = delay - static_cast<float>(delay_integral);
    const float a = line_[(write_ptr_ + delay_integral) % line_.size()];
    const float b = line_[(write_ptr_ + delay_integral + 1) % line_.size()];
    return a + (b - a) * delay_fractional;
  }
  
  inline float ReadHermite(float delay) const {
    const int32_t delay_integral = static_cast<int32_t>(delay);
    const float delay_fractional = delay - static_cast<float>(delay_integral);
    const size_t t = write_ptr_ + static_cast<size_t>(delay_integral) + line_.size();
    const float xm1 = line_[(t - 1) % line_.size()];
    const float x0 = line_[(t) % line_.size()];
    const float x1 = line_[(t + 1) % line_.size()];
    const float x2 = line_[(t + 2) % line_.size()];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = delay_fractional;
    return (((a * f) - b_neg) * f + c) * f + x0;
  }


 private:
  std::vector<float> line_;
  size_t write_ptr_ = 0, delay_ = 1;
};
} // namespace breach::ringsport
