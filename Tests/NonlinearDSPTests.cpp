#include "NonlinearDSP.h"
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

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
using namespace breach::nonlinear;
constexpr double pi = 3.14159265358979323846;
static void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static double magnitude(const std::vector<float>& samples, int harmonic, int bin)
{
    std::complex<double> sum {};
    for (size_t n = 0; n < samples.size(); ++n)
        sum += static_cast<double>(samples[n]) * std::polar(1.0, -2*pi*harmonic*bin*static_cast<double>(n)/static_cast<double>(samples.size()));
    return std::abs(sum);
}
static void aliasTest(Curve::Shape shape)
{
    Curve curve(shape);
    for (int factor : {1,2,4})
    {
        const int count = 4800*factor, bin = 701;
        std::vector<float> direct, integrated;
        ADAA shaper;
        // Two coherent periods; discard startup and keep one analysis period.
        for (int n = 0; n < count*2; ++n)
        {
            const float x = static_cast<float>(3.0*std::sin(2*pi*bin*n/count));
            const float y = shaper.process(x,curve);
            if (n>=count) { direct.push_back(static_cast<float>(Curve::evaluate(x,shape))); integrated.push_back(y); }
        }
        double oldAlias=0,newAlias=0;
        for(int harmonic=2;harmonic<=25;++harmonic)
        {
            if(harmonic*bin < count/2) continue;
            const double oldMagnitude=magnitude(direct,harmonic,bin), newMagnitude=magnitude(integrated,harmonic,bin);
            oldAlias+=oldMagnitude*oldMagnitude; newAlias+=newMagnitude*newMagnitude;
        }
        const double relative=(newAlias/oldAlias)*std::pow(magnitude(direct,1,bin)/magnitude(integrated,1,bin),2);
        const double improvement=-10*std::log10(relative);
        std::cout << (shape==Curve::Shape::Tanh?"DRIVE":"SNIPER") << " " << factor << "X alias reduction " << improvement << " dB\n";
        require(improvement>5.0,"ADAA did not reduce folded harmonics by 5 dB");
    }
}
static void basicTest()
{
    for(auto shape:{Curve::Shape::Tanh,Curve::Shape::Sniper})
    {
        Curve curve(shape); ADAA shaper;
        double previous=-1.1;
        for(int n=0;n<=100000;++n)
        {
            const double x=-20+40.0*n/100000;
            const double v=curve.value(x);
            require(v>=previous-1e-12 && std::abs(v)<=1.000001,"Nonmonotonic or unbounded curve"); previous=v;
            require(std::abs(v-Curve::evaluate(x,shape))<5e-6,"LUT transfer approximation inaccurate");
        }
        float output=0;
        for(int n=0;n<1000;++n) output=shaper.process(0.15f,curve);
        require(std::abs(output-curve.value(0.15))<1e-6,"Constant-input ADAA unstable");
        shaper.reset(); require(std::abs(shaper.process(0,curve))<1e-6,"Shaper reset leaked old input");
        for(float input:{1e-10f,-1e-10f,1e-5f,1e5f,-1e5f,0.0f})
            require(std::isfinite(shaper.process(input,curve)),"Nonfinite extreme input");
    }
    for(double sr:{44100.0,48000.0,192000.0,384000.0})
    {
        DCBlocker dc; dc.prepare(sr); float y=0;
        for(int n=0;n<static_cast<int>(sr);++n) y=dc.process(0.2f);
        require(std::abs(y)<1e-9,"DC removal did not settle");
        dc.reset(); require(dc.process(0)==0,"DC reset leaked output");
    }
    {
        Curve curve(Curve::Shape::Sniper); ADAA shaper; DCBlocker dc; dc.prepare(48000.0);
        double directMean=0.0,blockedMean=0.0;
        for(int n=0;n<96000;++n)
        {
            const float x=static_cast<float>(0.8*std::sin(2*pi*100*n/48000.0));
            const float y=shaper.process(x,curve),blocked=dc.process(y);
            if(n>=48000) {directMean+=y/48000.0;blockedMean+=blocked/48000.0;}
        }
        require(std::abs(directMean)>1e-3 && std::abs(blockedMean)<1e-6,"Asymmetric shaper DC was not removed");
        std::cout << "SNIPER steady DC mean " << directMean << " -> " << blockedMean << '\n';
    }
    // Measure the nominal ADAA delay at a low-amplitude, low-frequency tone.
    {
        Curve curve(Curve::Shape::Tanh); ADAA shaper; std::complex<double> response {};
        for(int n=0;n<96000;++n)
        {
            const float x=static_cast<float>(0.001*std::sin(2*pi*100*n/48000.0));
            const float y=shaper.process(x,curve);
            if(n>=48000) response+=static_cast<double>(y)*std::polar(1.0,-2*pi*100*n/48000.0);
        }
        const double delay=-(std::arg(response)+pi/2)/(2*pi*100/48000.0);
        require(std::abs(delay-1.0)<0.001,"ADAA nominal one-sample group delay incorrect");
    }
    Engine engine;
    for(int factor:{1,2,4})
    {
        engine.prepare(48000.0*factor,factor);
        for(int n=0;n<2*factor+4;++n)
        {
            const float y=engine.output(engine.dry(n==0?1.0f:0.0f,0),0);
            require(y==(n==2*factor?1.0f:0.0f),"Dry compensation is not two base samples");
        }
    }
    watchAllocations=true;
    for(int n=0;n<50000;++n)
    {
        if(n%1000==0) engine.prepare(48000.0,1);
        const auto x=engine.excite(static_cast<float>(std::sin(n*0.2)),11.0f,0);
        const auto y=engine.voice(x,static_cast<size_t>(n%12),0,true);
        require(std::isfinite(engine.output(y,0)),"Nonfinite engine output");
    }
    watchAllocations=false; require(allocations==0,"Processing allocated memory");
    std::cout << "PASS: bounded monotonic curves, LUT accuracy, DC, reset, dry latency and allocation checks\n";
}
int main()
{
    try { basicTest(); aliasTest(Curve::Shape::Tanh); aliasTest(Curve::Shape::Sniper); }
    catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n';return 1; }
}
