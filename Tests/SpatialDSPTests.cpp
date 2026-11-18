#include "SpatialDSP.h"
#include <complex>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

static bool watchAllocations = false;
static size_t allocations = 0;
void* operator new(std::size_t size)
{
    if (watchAllocations) ++allocations;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

using namespace breach::spatial;
static void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }

static void allpassTest()
{
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0, 384000.0})
    {
        DelayAllpass filter; filter.prepare(sr, 0.0051, 0.57f);
        require(std::abs(static_cast<double>(filter.delaySamples()) / sr - 0.0051) <= 0.51 / sr, "Delay duration changed with sample rate");
        double energy = 0.0;
        std::array<std::complex<double>, 4> response {};
        constexpr double frequencies[] = {100.0, 1000.0, 7000.0, 18000.0};
        for (int n = 0; n < static_cast<int>(sr * 0.4); ++n)
        {
            const double y = filter.process(n == 0 ? 1.0f : 0.0f); energy += y * y;
            for (size_t k = 0; k < response.size(); ++k)
                response[k] += y * std::polar(1.0, -2.0 * pi * frequencies[k] * n / sr);
        }
        require(std::abs(energy - 1.0) < 2.0e-6, "Allpass does not preserve impulse energy");
        for (auto h : response) require(std::abs(std::abs(h) - 1.0) < 2.0e-5, "Allpass magnitude is not unity");
    }
    std::cout << "PASS: allpass unity gain, energy and delay duration\n";
}

static void diffuserTest()
{
    const std::array<float, 4> x {{0.2f, -0.7f, 1.0f, 0.4f}};
    const auto y = StereoDiffuser::mix(x);
    double a = 0.0, b = 0.0;
    for (size_t k = 0; k < 4; ++k) { a += x[k]*x[k]; b += y[k]*y[k]; }
    require(std::abs(a-b) < 1.0e-6, "Mixing matrix is not orthogonal");
    double referenceCentroid = 0.0;
    for (double sr : {48000.0, 96000.0, 192000.0})
    {
        StereoDiffuser d; d.prepare(sr);
        double energy = 0.0, centroid = 0.0, stereoDifference = 0.0;
        for (int n = 0; n < static_cast<int>(sr); ++n)
        {
            const auto output = d.process({n == 0 ? 1.0f : 0.0f, 0.0f}, 1.0f);
            const double e = output.left*output.left + output.right*output.right;
            energy += e; centroid += e * n / sr;
            stereoDifference += std::abs(output.left - output.right);
        }
        centroid /= energy;
        require(energy <= 1.00001 && energy > 0.05, "Diffusion energy outside passive bounds");
        require(stereoDifference > 0.1, "Diffuser produced no stereo spread");
        if (sr == 48000.0) referenceCentroid = centroid;
        else require(std::abs(centroid - referenceCentroid) < 2.0 / 48000.0, "Oversampling changed diffusion duration");
        d.reset(); const auto quiet = d.process({0,0}, 1.0f);
        require(quiet.left == 0 && quiet.right == 0, "Reset leaked old diffusion tail");
        const auto bypass = d.process({0.25f,-0.125f}, 0.0f);
        require(bypass.left == 0.25f && bypass.right == -0.125f, "Zero-space path is not exact");
    }
    std::cout << "PASS: diffusion passivity, stereo spread, reset and 1X/2X/4X duration\n";
}

static void shifterTest()
{
    double worstRejection = -200.0, maxGainError = 0.0;
    for (double base : {44100.0, 48000.0, 96000.0})
        for (double factor : {1.0, 2.0, 4.0})
            for (double frequency : {20.0, 100.0, 1000.0, 7000.0, 18000.0})
                for (float shift : {-40.0f, 40.0f})
                {
                    const double sr = base * factor;
                    StereoFrequencyShifter shifter; shifter.prepare(sr);
                    const int warmup = static_cast<int>(sr * 0.5), count = static_cast<int>(sr * 0.25);
                    std::complex<double> wanted {}, image {};
                    for (int n = 0; n < warmup + count; ++n)
                    {
                        const float input = static_cast<float>(0.5 * std::sin(2*pi*frequency*n/sr));
                        const auto output = shifter.process({input, input}, shift);
                        require(output.left == output.right, "Mono image shifted differently in stereo channels");
                        if (n >= warmup)
                        {
                            wanted += static_cast<double>(output.left) * std::polar(1.0, -2*pi*(frequency+shift)*n/sr);
                            image += static_cast<double>(output.left) * std::polar(1.0, -2*pi*(frequency-shift)*n/sr);
                        }
                    }
                    const double rejection = 20*std::log10(std::max(1.0e-15, std::abs(image)/std::abs(wanted)));
                    const double gainError = std::abs(20*std::log10(std::abs(wanted)*4.0/count));
                    worstRejection = std::max(worstRejection, rejection); maxGainError = std::max(maxGainError, gainError);
                    require(rejection < -58.0, "Image sideband suppression below 58 dB");
                    require(gainError < 0.02, "Frequency shifter changed wanted-tone gain");
                    for (int n = 0; n < static_cast<int>(sr * 0.03); ++n) shifter.process({0.2f, -0.1f}, 0.0f);
                    const auto bypass = shifter.process({0.2f,-0.1f},0.0f);
                    require(bypass.left == 0.2f && bypass.right == -0.1f, "Zero shift did not settle to exact bypass");
                }
    std::cout << "PASS: shifter +/-40 Hz, 20 Hz-18 kHz, 44.1/48/96 kHz x 1/2/4; worst image "
              << worstRejection << " dBc, max gain error " << maxGainError << " dB\n";
}

static void realtimeTest()
{
    SpatialEngine engine; engine.prepare(48000.0); engine.selectRate(0);
    watchAllocations = true;
    for (int i = 0; i < 60000; ++i)
    {
        if (i % 1000 == 0) engine.selectRate((i / 1000) % 3);
        if (i % 7777 == 0) { engine.reset(); engine.selectRate(0); }
        const float input = static_cast<float>(std::sin(i * 0.15));
        const auto y = engine.process({input,-input}, static_cast<float>(i % 1000) / 1000.0f,
                                       500.0f * static_cast<float>(std::sin(i * 0.003)));
        require(std::isfinite(y.left) && std::isfinite(y.right), "Non-finite modulation/rate-switch output");
    }
    watchAllocations = false;
    require(allocations == 0, "Audio processing allocated memory");
    std::cout << "PASS: no allocations during processing, reset or rate selection\n";
}
int main()
{
    try { allpassTest(); diffuserTest(); shifterTest(); realtimeTest(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
