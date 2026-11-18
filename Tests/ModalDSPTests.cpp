#include "ModalDSP.h"
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <chrono>

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
using namespace breach::modal;

static void poleTests()
{
    double largestPitchError=0,largestTimeError=0;
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
        for(float f:{55.0f,220.0f,880.0f,4000.0f})
            for(bool revolver:{false,true})
            {
                const auto modes=design(sr,f,.6f,.4f,.55f,.3f,.2f,revolver);
                size_t active=0;double weightSum=0;
                for(const auto& m:modes)if(m.active)
                {
                    ++active;weightSum+=m.weight;
                    const double d=1+m.r*m.g+m.g*m.g;
                    const double a1=2*(m.g*m.g-1)/d;
                    const double a2=(1-m.r*m.g+m.g*m.g)/d;
                    require(a2>0 && a2<1,"Modal pole radius is unstable");
                    const double radius=std::sqrt(a2);
                    const double frequency=std::acos(std::clamp(-a1/(2*radius),-1.0,1.0))*sr/(2*pi);
                    const double rt60=-std::log(1000.0)/(sr*std::log(radius));
                    const double requested=std::min(m.frequency,sr*.44);
                    largestPitchError=std::max(largestPitchError,std::abs(1200*std::log2(frequency/requested)));
                    largestTimeError=std::max(largestTimeError,std::abs(rt60/m.rt60-1));
                    require(m.weight>=0,"Negative modal gain");
                }
                require(active>0 && std::abs(weightSum-1)<1e-8,"Modal weights not normalized");
                if(f==220)require(active==(revolver?12:6),"Profile mode count changed");
                require(modes[0].frequency==f,"Fundamental transposed by material model");
                require(modes[1].frequency>2*f,"STIFFNESS did not disperse upper modes");
            }
    require(largestPitchError<.001,"Pole frequency not matched");
    require(largestTimeError<1e-8,"Physical decay is sample-rate dependent");
    const auto p=design(48000,220,.6f,.4f,.55f,.3f,0,false);
    const auto r=design(48000,220,.6f,.4f,.55f,.3f,0,true);
    require(r[0].rt60>p[0].rt60*2,"REVOLVER did not have longer body decay");
    std::cout<<"PASS: stable matched poles, normalized modal gains, 6/12 profiles; max pitch error "
             <<largestPitchError<<" cents, max relative decay error "<<largestTimeError<<'\n';
}

static void kernelResponse()
{
    const double sr=48000;
    const auto mode=design(sr,220,.6f,.4f,.55f,.3f,0,false)[0];
    breach::ringsmodal::Svf filter;filter.set_g_r(mode.g,mode.r);filter.Reset();
    std::array<std::complex<double>,4> measured {};
    const double frequencies[]={0,220,1000,20000};
    for(int n=0;n<static_cast<int>(sr*1.5);++n)
    {
        const double y=filter.Process(n==0?1.0:0.0);
        for(size_t k=0;k<4;++k)measured[k]+=y*std::polar(1.0,-2*pi*frequencies[k]*n/sr);
    }
    const double d=1+mode.r*mode.g+mode.g*mode.g;
    const double b=mode.g/d,a1=2*(mode.g*mode.g-1)/d,a2=(1-mode.r*mode.g+mode.g*mode.g)/d;
    for(size_t k=0;k<4;++k)
    {
        const auto z=std::polar(1.0,-2*pi*frequencies[k]/sr);
        const auto expected=b*(1.0-z*z)/(1.0+a1*z+a2*z*z);
        require(std::abs(measured[k]-expected)<1e-8,"Imported SVF does not match analytic response");
    }
    std::cout<<"PASS: imported stmlib SVF recurrence matches its z-domain response\n";
}
static void realtimeTests()
{
    Bank bank;
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
    {
        bank.prepare(sr);bank.selectProfile(true);watchAllocations=true;double energy=0;
        for(int n=0;n<static_cast<int>(sr*.2);++n)
        {
            const float pitch=static_cast<float>(220+70*std::sin(n/sr*40));
            const float y=bank.process(0,0,n%4096==0?.3f:0.0f,pitch,.8f,.5f,.55f,.4f,
                                      static_cast<float>(std::sin(n/sr*80)),true);
            require(std::isfinite(y) && std::abs(y)<10,"Unstable swept modal body");energy+=y*y;
        }
        require(energy>1e-8,"Silent modal body");bank.reset();
        require(bank.process(0,0,0,220,.8f,.5f,.55f,.4f,0,true)==0,"Reset leaked modal tail");
        bank.selectProfile(false);bank.selectRate(48000);watchAllocations=false;
    }
    require(allocations==0,"Modal processing or reset allocated memory");
    std::cout<<"PASS: multi-rate pitch/Q modulation, reset, profile/rate switches and no allocations\n";
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==8)
        {
            const double sr=std::stod(argv[1]);const float pitch=std::stof(argv[2]);const bool revolver=std::stoi(argv[3])!=0;
            const float decay=std::stof(argv[4]),structure=std::stof(argv[5]);const double seconds=std::stod(argv[6]);
            std::ofstream out(argv[7],std::ios::binary);Voice voice;voice.prepare(sr);
            for(int n=0;n<static_cast<int>(sr*seconds);++n)
            {
                const float y=voice.process(n==0?1.0f:0.0f,pitch,decay,structure,.55f,.3f,0,revolver);
                out.write(reinterpret_cast<const char*>(&y),sizeof(y));
            }
            return out.good()?0:1;
        }
        if(argc==2 && std::string(argv[1])=="--benchmark")
        {
            for(double sr:{48000.0,192000.0,384000.0})for(bool revolver:{false,true})
            {
                Bank bank;bank.prepare(sr);bank.selectProfile(revolver);volatile double checksum=0;
                const auto start=std::chrono::steady_clock::now();
                for(int n=0;n<static_cast<int>(sr*.25);++n)
                    for(size_t v=0;v<12;++v)for(size_t c=0;c<2;++c)
                        checksum+=bank.process(v,c,n%4000==0?.2f:0.0f,static_cast<float>(110*std::exp2(v/12.0)),.6f,.4f,.55f,.3f,0,revolver);
                const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                std::cout<<sr<<" Hz "<<(revolver?"REVOLVER":"PISTOL")<<" 12 stereo voices: model-only time/render ratio "<<seconds/.25<<" checksum "<<checksum<<'\n';
            }
            return 0;
        }
        poleTests();kernelResponse();realtimeTests();
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
