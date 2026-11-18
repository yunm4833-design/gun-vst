# SHOTGUN physical model - stage 3

This document describes stage 3. The cumulative source also includes the STK
formant stage described in FORMANT_UPGRADE.md. Stage 3 contains the UI, spatial,
nonlinear and Rings string upgrades. Remote repositories have not been modified.

## Actual open-source integration

The new string engine is an adapted subset of Mutable Instruments Rings' String
model by Emilie Gillet, MIT licensed. This stage incorporates upstream source
code, unlike the independent implementations in stages 1 and 2.

See Source/ThirdParty/Rings/PROVENANCE.md for exact upstream commit IDs, source
paths, modifications and the unmodified reference files. RingsPrimitives.h
contains the imported FIR DampingFilter and stmlib DelayLine interpolation and
allpass methods. These methods are called from the live audio path via
Source/RingsStringDSP.h. They are not unused examples.

This is a reduced string/waveguide model, not the whole Rings instrument, its
64-mode modal bank, the VCV Rack plugin, or a neural-network model. No STK code,
GPL plugin wrapper, external SDK or model-weight download is required.

## Physical behavior

- Independent stereo strings for up to 12 voices.
- Fractional-delay propagation with the imported Hermite interpolation.
- Imported interpolated FIR damping and split-delay allpass dispersion.
- A pickup difference tap produces position-dependent spectral zeros.
- A sample-rate-derived one-pole loop lowpass controls high-frequency absorption.
- Fundamental phase correction accounts for the FIR, lowpass and dispersion.
- DECAY maps to target RT60 in seconds: 0.12 * 30^decay, with per-voice decay
  multiplication applied before clamping decay to [0,1]. Full range is 0.12-3.6 s;
  the current main parameter's existing minimum is 0.05, not zero.
- Loop gain is derived from round-trip frequency and capped below unity. Very
  dark/high-frequency settings can decay faster than the target, and dispersion
  changes group delay. The target is not an exact broadband-reverb RT60 promise.
- Maximum 4X delay storage is allocated in prepareToPlay, with a 20 Hz margin
  for the adapter's 25 Hz pitch floor. The supported upper string frequency is
  0.15 times the effective processing sample rate, e.g. 7200 Hz at 48 kHz 1X.
  Higher requested pitches are clamped. The upper bound increases with OS.
- The existing main-engine warp/unison/per-voice tuning mapping is retained.
  Thus a deliberate pitch warp is not removed by the adapter's tuning correction.
- Coefficients update near 3 kHz; main delay glides with a 1 ms time constant.
- No shared RNG, MCU tables, fixed-48k processing or Rings low-pitch resampling
  fallback is used. Random dispersion/curved bridge/infinite sustain are omitted.
- A safety ceiling handles extreme overload; it is not the normal damping model.

The new model may be quieter/shorter than the original saturated comb. There is
no plugin auto-loudness matching. Use the provided RMS-matched examples when
assessing tonal differences, and use the existing output/voice controls as needed.
This stage does not change the nonlinear engine, spatial engine or host latency.

## Controls and state

D / GLOBAL exposes STRING: RINGS / STRING: LEGACY, independent of SPACE DSP and
DRIVE DSP. RINGS applies to SHOTGUN in AUTO, and to COMB STRING manual override
under any gun mode. Other resonator topologies remain on their existing paths.

All APVTS IDs, order, types, ranges, defaults and version hints are unchanged.
A non-parameter root property stringDspRevision stores the engine choice:

- Missing or 1: original saturated Lagrange comb.
- 2: adapted Rings string model.

New instances default to RINGS. Original, stage-1 and stage-2 states without the
new property load as string LEGACY and preserve their old engine selections.
The host is notified when the non-parameter choice changes; save the project or
preset after choosing RINGS. The selection is not a host automation parameter.

Change engines/OS while stopped. Model/rate/topology changes clear the new bank;
engine-selection changes also clear frozen legacy comb histories. This avoids
recalling dormant tails but does not promise seamless live tail transitions.

## Validation

- Linux JUCE 8.0.4 Debug VST3 and Standalone builds passed.
- Six combined CTest targets cover all three stages: imported primitives, string
  modulation, overload, resets, no-allocation processing, state/audio restoration,
  topology isolation and 12-voice max-drive/stiffness renders at 44.1/48/96 kHz,
  1X/2X/4X with 32/128/1024 sample blocks.
- Existing original/stage-1 fixtures and six new pre-stage-3 fixtures replay with
  zero sample error on this Linux build in their legacy paths.
- Direct model impulse analysis covers requested 110/220/880 Hz, stiffness 0/0.7,
  absorption 0.3, decay 0.7 and processing rates 44.1/48/96/192/384 kHz. Estimated
  fundamental error is below 1 cent; same-condition fundamental T60 varies by
  less than 5% across those rates. These are sampled static model tests, not a
  guarantee for every note, modulation trajectory or full-plugin preset.
- AddressSanitizer/UndefinedBehaviorSanitizer model tests passed on Linux.
- UI smoke tests cover the new switch, both themes and all drawer bounds.
- Audio examples have matching whole-file RMS and no clipped PCM samples.

Windows/macOS builds, real DAW hosts and Release CPU budgets still need platform
validation. All added processing code is portable C++17. No Python dependency is
introduced into the build. Tools/analyze_rings.py is an optional offline measurement
script requiring NumPy/SciPy.

Run the normal combined DSP suite with BREACH_BUILD_DSP_TESTS=ON. The header-only
model test can also run without JUCE:

```sh
c++ -std=c++17 -O2 -ISource Tests/RingsDSPTests.cpp -o rings-tests
./rings-tests
python Tools/analyze_rings.py ./rings-tests /tmp/rings-measurements
```

BreachRingsAudition generates SHOTGUN comparisons at 2X with the SAME new nonlinear
and spatial engines for both files. One pair uses stiffness 0 / damping 0.25;
the other stiffness 0.7 / damping 0.6. SPACE and SHIFT are zero. These synthetic
percussion comparisons are matched by whole-file RMS, not LUFS. They demonstrate
the model change and do not establish subjective quality for a user's recording.

## Distribution notices

MIT copyright/license notices are retained with all adapted code and upstream
copies. Licenses/Rings-MIT.txt is copied into built plugin/app artifacts along
with the existing Barlow font license. On Apple, CMake registers them as bundle
resources; on Windows/Linux it copies them into artifact resources/sidecar files.
Keep these notices in distributed installers and plugin packages.
