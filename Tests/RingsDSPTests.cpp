#include "RingsStringDSP.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>

static bool watchAllocations = false;
static size_t allocations = 0;
void* operator new(std::size_t size)
{
    if (watchAllocations) ++allocations;
    if (void* p=std::malloc(size==0?1:size))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
using breach::physical::String;

int main(int argc,char** argv)
{
    try
    {
        if(argc==8)
        {
            const double sr=std::stod(argv[1]);const float freq=std::stof(argv[2]);
            const float stiffness=std::stof(argv[3]),absorption=std::stof(argv[4]),decay=std::stof(argv[5]);
            const double seconds=std::stod(argv[6]);std::ofstream out(argv[7],std::ios::binary);
            String string;string.prepare(sr);string.setSampleRate(sr);
            for(int n=0;n<static_cast<int>(sr*seconds);++n)
            {const float y=string.process(n==0?1.0f:0.0f,freq,decay,absorption,stiffness);out.write(reinterpret_cast<const char*>(&y),sizeof(y));}
            return out.good()?0:1;
        }
        // Actual imported DampingFilter recurrence and DC behavior.
        breach::ringsport::DampingFilter damping;damping.Init();damping.Configure(0.7f,0.4f,0);
        require(std::abs(damping.Process(1.0f)-0.105f)<1e-6f,"Imported FIR impulse coefficient changed");
        require(std::abs(damping.Process(0.0f)-0.49f)<1e-6f,"Imported FIR centre coefficient changed");
        require(std::abs(damping.Process(0.0f)-0.105f)<1e-6f,"Imported FIR last coefficient changed");
        // DelayLine Hermite is the actual stmlib routine, not an unused vendor file.
        breach::ringsport::DelayLine delay;delay.Prepare(256);
        for(int n=0;n<64;++n)delay.Write(0.3f);
        require(std::abs(delay.ReadHermite(15.25f)-0.3f)<1e-6f,"Imported interpolation lost DC");
        String string;string.prepare(384000.0);
        for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0})
        {
            string.setSampleRate(sr);watchAllocations=true;
            double energy=0;
            for(int n=0;n<static_cast<int>(sr*0.2);++n)
            {
                const float f=static_cast<float>(220+40*std::sin(n/sr*50));
                const float y=string.process(n%4000==0?0.2f:0.0f,f,0.8f,0.4f,0.8f);
                require(std::isfinite(y) && std::abs(y)<10.0f,"Unstable modulated string");energy+=y*y;
            }
            string.reset();require(string.process(0,220,0.8f,0.4f,0.8f)==0,"Reset recalled a dormant tail");
            watchAllocations=false;require(energy>1e-8,"Model produced silence");
        }
        require(allocations==0,"Model allocated memory while processing/resetting");
        for(float f:{25.0f,220.0f,4000.0f,20000.0f})
        {
            string.setSampleRate(48000.0);
            for(int n=0;n<12000;++n)
            {
                const float y=string.process(n%31==0?50.0f:0.0f,f,1,1,1);
                require(std::isfinite(y) && std::abs(y)<100,"Overload safety failed");
            }
        }
        std::cout<<"PASS: imported FIR/Hermite execution, multi-rate modulation, reset, overload and no-allocation checks\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
