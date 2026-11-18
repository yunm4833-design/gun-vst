#include "OutputEqDSP.h"
#include <iostream>
#include <stdexcept>

using namespace breach::eq;

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static double response(const Filter& filter, double hz, Rates rates)
{
    const auto z = std::polar(1.0, -2 * pi * hz / rates.core);
    return filter.magnitudeDb(z, z * z);
}

int main()
{
    try
    {
        for (const Rates rates : {Rates{8000, 8000}, Rates{44100, 44100}, Rates{48000, 192000}, Rates{96000, 384000}})
        {
            for (int type = 0; type < static_cast<int>(shapeNames.size()); ++type)
            {
                const auto shape = static_cast<Shape>(type);
                for (double cutoff : {minHz, 300.0, maxHz})
                    for (double q : {.25, neutralQ, 8.0})
                        for (double gain : {-18.0, 18.0})
                        {
                            const auto filter = design({true, shape, cutoff, gain, q}, rates);
                            require(filter.count >= 1 && filter.count <= 4, "Unexpected filter section count");
                            for (std::size_t i = 0; i < filter.count; ++i)
                            {
                                const auto& c = filter.sections[i];
                                require(std::abs(c.a2) < 1 && 1 + c.a1 + c.a2 > 0 && 1 - c.a1 + c.a2 > 0, "Unstable filter poles");
                            }
                            require(std::isfinite(response(filter, cutoff, rates)), "Non-finite frequency response");
                        }
            }
            for (int index = 0; index < 3; ++index)
            {
                const auto highpass = design({true, static_cast<Shape>(index), 400, 0, neutralQ}, rates);
                const auto lowpass = design({true, static_cast<Shape>(index + 7), 400, 0, neutralQ}, rates);
                const double slope = (12 << index);
                require(std::abs(response(highpass, 400, rates) + 3.0103) < .001, "High-pass cutoff is not Butterworth at neutral Q");
                require(std::abs(response(lowpass, 400, rates) + 3.0103) < .001, "Low-pass cutoff is not Butterworth at neutral Q");
                require(std::abs(response(highpass, 25, rates) - response(highpass, 50, rates) + slope) < .25, "Low-cut slope is wrong");
                require(response(highpass, 2400, rates) > -.01, "Low cut reduces the high passband");
                require(response(lowpass, 1600, rates) < -slope * 1.95, "High cut lacks out-of-band rejection");
                require(response(lowpass, 25, rates) > -.01, "High cut reduces the low passband");
            }
            require(std::abs(response(design({true, Shape::bell, 400, 12, 1}, rates), 400, rates) - 12) < 1e-6, "Bell gain is wrong");
            require(response(design({true, Shape::notch, 400, 0, 1}, rates), 400, rates) < -120, "Notch does not reject its center");
            require(response(design({true, Shape::lowShelf, 400, -12, 1}, rates), 25, rates) < -11.9, "Low shelf does not cut its passband");
            require(response(design({true, Shape::highShelf, 400, 12, 1}, rates), 2400, rates) > 11.9, "High shelf does not boost its passband");
        }
        Engine engine;
        std::array<BandSettings, bandCount> bands {};
        bands[0] = {true, Shape::lowCut48, 150, 0, neutralQ};
        require(!engine.setTargets(false, bands), "Disabled EQ did not bypass");
        require(engine.setTargets(true, bands), "Active low cut is idle");
        double peak = 0;
        for (int i = 0; i < 24000; ++i)
        {
            if (i == 8000) { bands[0].hz = 19000; bands[0].q = 8; engine.setTargets(true, bands); }
            if (i == 12000) { bands[0].shape = Shape::highCut48; bands[0].hz = 10; engine.setTargets(true, bands); }
            const double x = std::sin(i * .11) * .2;
            const auto y = engine.process({x, x * .25});
            require(std::isfinite(y.left) && std::abs(y.right - y.left * .25) < 1e-12, "Channels differ or EQ is non-finite during automation");
            peak = std::max(peak, std::abs(y.left));
        }
        require(peak < 2, "Fast automated EQ produced a transient above its tested bound");
        engine.setTargets(false, bands);
        for (int i = 0; i < 1000; ++i) engine.process({0, 0});
        require(!engine.setTargets(false, bands), "EQ did not finish bypass fade");
        std::cout << "PASS: all shapes, slopes, cutoff, pole stability, channel matching and bypass\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
