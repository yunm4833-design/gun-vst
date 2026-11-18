#pragma once
#include "ThirdParty/Rings/RingsModalPrimitives.h"
#include <algorithm>
#include <array>

namespace breach::modal
{
constexpr double pi = 3.14159265358979323846;
constexpr size_t maxModes = 12;
struct Mode { double frequency, rt60, weight, g, r; bool active; };

// Adapted Rings Resonator::ComputeFilters: harmonic/stretch progression,
// evolving stiffness and brightness-dependent loss. Physical-time damping and
// physical-time damping replace the original embedded parameter/Q maps.
inline std::array<Mode, maxModes> design(double sr, float pitch, float decay,
    float structure, float brightness, float damping, float qMod, bool revolver, double ringSeconds = 0) noexcept
{
    std::array<Mode,maxModes> modes {};
    const size_t count = revolver ? 12 : 6;
    const double fundamental = std::clamp(static_cast<double>(pitch),25.0,sr*.4);
    const double material = std::clamp(static_cast<double>(structure),0.0,1.0);
    const double bright = std::clamp(static_cast<double>(brightness),0.0,1.0);
    const double absorb = std::clamp(static_cast<double>(damping),0.0,1.0);
    const double decaySeconds = ringSeconds > 0 ? ringSeconds : (revolver ? .16 : .05) * std::pow(revolver ? 22.0 : 20.0,
        std::clamp(static_cast<double>(decay),0.0,1.0));
    const double qScale = std::exp2(.35 * std::clamp(static_cast<double>(qMod),-1.0,1.0));
    double stiffness = material * (revolver ? .08 : .035);
    double harmonic = fundamental, stretch_factor = 1.0;
    const double attenuation = std::pow(1.0-material,8);
    const double adjustedBrightness = bright * (1.0-.2*attenuation);
    double q_loss = adjustedBrightness * (2.0-adjustedBrightness) * .85 + .15;
    const double q_loss_damping_rate = material * (2.0-material) * .1;
    double partialDecay = decaySeconds;
    ringsmodal::CosineOscillator amplitudes;
    amplitudes.Init(static_cast<float>(revolver ? .13 : .19));
    double weightSum=0;
    for(size_t i=0;i<count;++i)
    {
        const double frequency=harmonic*stretch_factor;
        const double fade=std::clamp((sr*.45-frequency)/(sr*.05),0.0,1.0);
        const bool active=frequency<sr*.45;
        const double weight=std::clamp(static_cast<double>(amplitudes.Next()),0.0,1.0) * std::pow(.86+.1*bright,static_cast<double>(i)) * fade;
        const double rt60=std::clamp(partialDecay*qScale/(1.0+absorb*.12*i*i),ringSeconds > 0 ? .006 : .012,ringSeconds > 0 ? 10.0 : 3.6);
        // Match digital pole radius/time and oscillation frequency, then map
        // those poles to the TPT g/r representation used by stmlib::Svf.
        const double omega=2.0*pi*std::min(frequency,sr*.44)/sr;
        const double loss=std::log(1000.0)/(rt60*sr),rho=std::exp(-loss);
        const double oneMinus=-std::expm1(-loss);
        const double plus=oneMinus*oneMinus+4.0*rho*std::pow(std::sin(omega*.5),2);
        const double minus=oneMinus*oneMinus+4.0*rho*std::pow(std::cos(omega*.5),2);
        const double g=std::sqrt(plus/minus);
        const double r=2.0*(-std::expm1(-2.0*loss))/std::sqrt(plus*minus);
        modes[i]={frequency,rt60,active?weight:0.0,g,r,active};weightSum+=active?weight:0.0;
        stretch_factor+=stiffness;
        stiffness*=.98;
        q_loss+=q_loss_damping_rate*(1.0-q_loss);
        harmonic+=fundamental;
        partialDecay*=q_loss;
    }
    for(auto& mode:modes)mode.weight/=std::max(1.0e-12,weightSum);
    return modes;
}

class Voice
{
public:
    void prepare(double sr) noexcept
    {
        sampleRate=sr; interval=std::max(1,static_cast<int>(std::lround(sr/1000.0)));
        smoothing=1.0-std::exp(-static_cast<double>(interval)/(sr*.005)); reset();
    }
    void reset() noexcept
    {
        for(auto& filter:filters)filter.Reset();
        countdown=0;initialized=false;modes={};lastProfile=-1;
    }
    float process(float input,float pitch,float decay,float structure,float brightness,
                  float damping,float qMod,bool revolver,double ringSeconds = 0) noexcept
    {
        const int profile = revolver ? 1 : 0;
        if (profile != lastProfile) { reset(); lastProfile = profile; }
        if(countdown--<=0)
        {
            const std::array<double,7> requested {{pitch,decay,structure,brightness,damping,qMod,ringSeconds}};
            bool changed=!initialized;
            for(size_t k=0;k<requested.size();++k)
            {
                const double delta=requested[k]-controls[k];
                const double next=!initialized || std::abs(delta)<1.0e-7*(1.0+std::abs(requested[k]))
                    ? requested[k] : controls[k]+smoothing*delta;
                changed=changed || next!=controls[k];controls[k]=next;
            }
            initialized=true;
            if(changed)
            {
            const auto next=design(sampleRate,static_cast<float>(controls[0]),static_cast<float>(controls[1]),
                static_cast<float>(controls[2]),static_cast<float>(controls[3]),static_cast<float>(controls[4]),static_cast<float>(controls[5]),revolver,controls[6]);
            for(size_t k=0;k<maxModes;++k)
            {
                if(!next[k].active || !modes[k].active)filters[k].Reset();
                filters[k].set_g_r(next[k].g,next[k].r);
            }
            modes=next;
            }
            countdown=interval-1;
        }
        // Rings odd/even accumulation, projected back to each stereo channel.
        double odd=0,even=0;
        for(size_t i=0;i<maxModes;++i)if(modes[i].active)
        {
            const double value=modes[i].weight*filters[i].Process(input);
            if(i&1u)even+=value;else odd+=value;
        }
        // Rings uses the unnormalized modal bandpass: damping changes decay
        // without shrinking a short strike in proportion to resonance time.
        return static_cast<float>(0.75*(odd+even));
    }
private:
    std::array<ringsmodal::Svf,maxModes> filters;
    std::array<Mode,maxModes> modes {};
    std::array<double,7> controls {};
    double sampleRate=48000,smoothing=.18;
    int interval=48,countdown=0,lastProfile=-1;
    bool initialized=false;
};

class Bank
{
public:
    void prepare(double sr) noexcept {activeRate=0;activeProfile=-1;selectRate(sr);}
    void selectRate(double sr) noexcept
    {
        if(activeRate==sr)return;
        activeRate=sr;for(auto& voice:voices)for(auto& channel:voice)channel.prepare(sr);
    }
    void selectProfile(bool revolver) noexcept
    {
        const int profile=revolver?1:0;
        if(profile!=activeProfile){reset();activeProfile=profile;}
    }
    void reset() noexcept {for(auto& voice:voices)for(auto& channel:voice)channel.reset();}
    float process(size_t voice,size_t channel,float input,float pitch,float decay,float structure,
                  float brightness,float damping,float qMod,bool revolver,double ringSeconds = 0) noexcept
    {
        return voices[voice][channel].process(input,pitch,decay,structure,brightness,damping,qMod,revolver,ringSeconds);
    }
private:
    std::array<std::array<Voice,2>,12> voices;
    double activeRate=0;int activeProfile=-1;
};
} // namespace breach::modal
