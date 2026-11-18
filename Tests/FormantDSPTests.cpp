#include "FormantDSP.h"
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>

static bool watchAllocations=false;
static size_t allocations=0;
void* operator new(std::size_t size)
{
    if(watchAllocations)++allocations;
    if(void* p=std::malloc(size==0?1:size))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
using namespace breach::formant;

static void filterResponse()
{
    // Compare the imported recurrence against its closed-form z response.
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
    {
        const double radius=std::exp(-pi*80/sr),omega=2*pi*1000/sr;
        breach::stkport::FormSwep filter;filter.prepare(sr,0.0005);filter.setTargets(1000,radius,1);
        const double points[]={0,200,1000,7000,18000};
        std::array<std::complex<double>,5> measured {};
        for(int n=0;n<static_cast<int>(sr*.4);++n)
        {
            const double y=filter.tick(n==0?1.0:0.0);
            for(size_t k=0;k<5;++k)measured[k]+=y*std::polar(1.0,-2*pi*points[k]*n/sr);
        }
        for(size_t k=0;k<5;++k)
        {
            const auto z=std::polar(1.0,-2*pi*points[k]/sr);
            const auto reference=(1-radius*radius)*0.5*(1.0-z*z)/(1.0-2*radius*std::cos(omega)*z+radius*radius*z*z);
            require(std::abs(measured[k]-reference)<1e-7,"STK recurrence response differs from reference");
        }
    }
    std::cout<<"PASS: imported STK filter response at five processing rates\n";
}
static void designTest()
{
    const auto original=design(44100,220,0,.55f,.3f,0);
    require(std::abs(original[0].frequency-770)<1e-6,"STK vowel A frequency not in live design");
    const auto i=design(44100,220,.5f,.55f,.3f,0);
    require(std::abs(i[0].frequency-273)<1e-6,"STK vowel I frequency not in live design");
    double maxRelative=0;
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
    {
        const auto mapped=design(sr,220,0,.55f,.3f,0);
        double sum=0;
        for(size_t k=0;k<4;++k)
        {
            const double bw=-std::log(mapped[k].radius)*sr/pi;
            const double reference=-std::log(original[k].radius)*44100/pi;
            maxRelative=std::max(maxRelative,std::abs(bw/reference-1));
            require(mapped[k].radius>0 && mapped[k].radius<1,"Unstable mapped radius");sum+=mapped[k].gain;
        }
        require(std::abs(sum-1)<1e-9,"Vowel gains not normalized");
    }
    require(maxRelative<1e-10,"Physical formant bandwidth changed with sample rate");
    const auto bright=design(48000,220,.4f,.55f,0,0),dark=design(48000,220,.4f,.55f,1,0);
    for(size_t k=0;k<4;++k)require(dark[k].radius<bright[k].radius,"Damping did not broaden formants");
    const auto midpoint=design(48000,220,.125f,.55f,.3f,0);
    require(std::abs(midpoint[0].frequency-(770+515)*.5)<1e-6,"Vowel interpolation discontinuity");
    std::cout<<"PASS: actual STK vowel data, interpolation, gain normalization and sample-rate bandwidth mapping\n";
}
static void realtimeTest()
{
    Bank bank;
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
    {
        bank.prepare(sr);watchAllocations=true;
        double energy=0;float peak=0;
        for(int n=0;n<static_cast<int>(sr*.5);++n)
        {
            const float pitch=static_cast<float>(220+90*std::sin(n/sr*20));
            const float vowel=static_cast<float>(.5+.5*std::sin(n/sr*50));
            const float x=static_cast<float>(.5*std::sin(n/sr*6000));
            const float y=bank.process(0,0,x,pitch,vowel,.8f,.4f,static_cast<float>(std::sin(n/sr*40)));
            require(std::isfinite(y),"Nonfinite swept formant output");energy+=y*y;peak=std::max(peak,std::abs(y));
        }
        require(energy>1e-6 && peak<6,"Silent or unstable swept formants");
        bank.reset();require(bank.process(0,0,0,220,.5f,.8f,.4f,0)==0,"Reset leaked old tail");
        bank.selectRate(48000);watchAllocations=false;
    }
    require(allocations==0,"Formant processing/reset allocated memory");
    std::cout<<"PASS: multi-rate vowel/pitch/Q modulation, reset and allocation checks\n";
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==7)
        {
            const double sr=std::stod(argv[1]);const float pitch=std::stof(argv[2]),vowel=std::stof(argv[3]);
            const double seconds=std::stod(argv[4]);const int bodyOnly=std::stoi(argv[5]);
            std::ofstream out(argv[6],std::ios::binary);
            Voice voice;voice.prepare(sr);
            std::array<breach::stkport::FormSwep,4> body;
            auto p=design(sr,pitch,vowel,.55f,.3f,0);
            for(size_t k=0;k<4;++k){body[k].prepare(sr,.0005);body[k].setTargets(p[k].frequency,p[k].radius,p[k].gain);}
            for(int n=0;n<static_cast<int>(sr*seconds);++n)
            {
                const float input=n==0?1.0f:0.0f;
                float y=0;
                if(bodyOnly)for(auto& f:body)y+=static_cast<float>(f.tick(input));
                else y=voice.process(input,pitch,vowel,.55f,.3f,0);
                out.write(reinterpret_cast<const char*>(&y),sizeof(y));
            }
            return out.good()?0:1;
        }
        filterResponse();designTest();realtimeTest();
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
