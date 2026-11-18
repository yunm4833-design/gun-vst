# PISTOL / REVOLVER modal bodies - stage 5

This cumulative source includes the UI and DSP stages 1-4, plus the Rings modal
body adaptation described here. No GitHub repository was updated.

## Open-source implementation

The implementation adapts Mutable Instruments Rings Resonator and stmlib Svf /
CosineOscillator, using the already-approved Rings project and MIT source license.
No unlisted repository, neural model, firmware binary or runtime SDK is added.
Source/ThirdParty/Rings/MODAL_PROVENANCE.md records exact upstream versions and
changes, and the included upstream copies have SHA-256 hashes.

Source/ThirdParty/Rings/RingsModalPrimitives.h contains actual imported SVF and
cosine recurrence code. Source/ModalDSP.h adapts the Rings modal frequency/stretch,
stiffness and brightness-loss progression and weighted odd/even accumulation.
These routines execute in the live audio path; they are not unused references.
This is a reduced adaptation, not the complete 64-mode Rings instrument.

## Profiles

| Profile | Modal cap | Unmodulated base decay target |
| --- | --- | --- |
| PISTOL | 6 | 0.05 * 20^decay seconds |
| REVOLVER | 12 | 0.16 * 22^decay seconds |

Each of up to 12 voices has independent left/right modal states. The fundamental
is retained at the existing voice's requested frequency; upper partials spread
using the Rings harmonic/stretch progression. REVOLVER has a larger material
stretch range, a different modal pickup weighting and longer base decay. The
existing REVOLVER minimum SPACE boost and the upgraded diffuser remain in place.

The source cosine amplitude generator uses its exact initializer. Modal weights
are nonnegative and normalized, with a high-frequency fade. Like Rings, the bank
uses unnormalized bandpass output: lengthening decay does not proportionally
shrink the onset of a short strike. Resonant peak gain therefore rises with Q.
A fixed 0.75 send gain calibrates the level; there is no automatic loudness
matching. Sustained resonant input can still reach the existing output limiter.

The pole radius is exp(-ln(1000)/(RT60*SR)); its angle is the requested oscillation
frequency. These digital poles are mapped into the TPT g/r coefficients used by
the imported SVF. This preserves static decay time and modal pitch across sample
rates, without using the original MCU LUT/Q approximations. High-mode damping is
frequency/material-dependent; per-mode targets are constrained to 0.012-3.6 s.
Q modulation can scale the base decay before that limit.

Six versus twelve modes and the different decay/material profiles are deliberate
voicing choices, not a claim that more modes always improve subjective quality.
The new bodies can sound substantially quieter/different from the old high-gain
single resonance. Use the RMS-matched examples when comparing timbre.

## Existing controls and scope

- DECAY and per-voice decay: physical modal decay targets.
- STIFFNESS: upper-partial inharmonicity (in addition to the original outer-engine
  pitch warp mapping, which is retained).
- TONE / TILT modulation: modal brightness/loss as well as the existing post EQ.
- FB DAMPING: progressively shorter high-mode decay.
- Q modulation: scales modal decay/resonance duration.
- Root/chord/MIDI, detune, unison, arp, motion, voice level and pan still apply.

Modal mode is used only by PISTOL/REVOLVER with AUTO or the BANDPASS SVF override.
Manual COMB STRING or DUAL FORMANT overrides are respected. SHOTGUN, SMG and
SNIPER keep their existing string/formant/SVF/saturation implementations.

No APVTS definition, ID, order, type, range, default or version hint changed.
The non-parameter state property modalDspRevision is 1 (LEGACY) or 2 (RINGS).
Missing markers select LEGACY for all earlier project generations; new instances
default to RINGS. Other engine version markers remain independent.

D / GLOBAL exposes MODAL: RINGS / MODAL: LEGACY. Stop playback before changing
engines, then save the project/preset. The host receives a non-parameter state
change notification. The selection is not a new host automation parameter.
Model/profile/rate/topology changes clear the new bank. Active-engine selection
changes also clear legacy SVF histories, so dormant tails are not resumed.
Seamless live tail transitions are not promised.

No fixed latency or resampling stage was introduced here. The nonlinear stage's
existing host latency reporting remains in effect. Resonator IIR group delay is
frequency-dependent and is not an additional fixed PDC delay.

## CPU behavior

Parameter design runs near 1 kHz with a 5 ms control glide. Settled coefficients
are cached, and inactive modes are not processed. All storage is fixed-size;
processing/reset/profile/rate selection allocate no memory.

A local standalone -O2 model-only benchmark with 12 stereo voices measured CPU
wall time / rendered audio time at approximately:

| Effective rate | PISTOL | REVOLVER |
| --- | --- | --- |
| 48 kHz | 0.030 | 0.040 |
| 192 kHz | 0.168 | 0.183 |
| 384 kHz | 0.316 | 0.345 |

These are short synthetic model-only measurements on this machine, not complete
plugin CPU percentages, worst-case modulation bounds or DAW performance promises.
High sample rates with 4X oversampling and 12 voices still need production host
profiling. Modulating coefficients continuously increases the design workload.

## Validation

- Linux JUCE 8.0.4 Debug VST3 and Standalone builds passed.
- Ten combined CTest targets cover all stages, including legacy fixture audio,
  new engine state/audio recall, PISTOL/REVOLVER isolation, other-gun/manual-override
  isolation and 12-voice pitch/Q modulation at host 44.1/48/96 kHz, 1X/2X/4X with
  32/128/1024 sample blocks.
- Six new pre-stage-5 fixture states/audio reproduce with zero sample error on
  this Linux build. Existing earlier-generation fixtures continue to pass.
- The imported SVF impulse response matches its analytic z-domain response.
- Pole algebra tests cover stable radii, static pitch/decay targets, normalized
  weights and six/twelve-mode design at five processing rates.
- Direct impulse measurements at f0=110/220/880 Hz, decay=0.6, structure=0.4,
  brightness=0.55, damping=0.3, both profiles, and effective rates
  44.1/48/96/192/384 kHz: estimated fundamental error below 1 cent, fundamental
  RT60 within 5% of target and cross-rate variation below 3% on that grid.
  This is a sampled static model test, not a guarantee over arbitrary modulation.
- Modulated reset/rate/profile/allocation tests and Linux ASan/UBSan checks pass.
- UI smoke tests verify engine switches, visible bounds, themes and parameter
  preservation. Both earlier sound engines and the reference UI remain available.

Windows/macOS builds, real DAW hosting, full-plugin Release CPU limits and final
subjective sound approval remain unverified. There is no claim of a finished
commercial release or of realistic firearm recordings.

The normal combined suite uses BREACH_BUILD_DSP_TESTS=ON. Direct header tests:

```sh
c++ -std=c++17 -O2 -ISource Tests/ModalDSPTests.cpp -o modal-tests
./modal-tests
./modal-tests --benchmark
python Tools/analyze_modal.py ./modal-tests /tmp/modal-measurements
```

The optional measurement script uses NumPy/SciPy; plugin builds do not.
BreachModalAudition generates old/new PISTOL and REVOLVER comparisons from the
same synthetic percussion, at 2X with earlier engines NEW, STIFFNESS=0.4,
TONE=0.55, FB DAMPING=0.3, SPACE knob=0 and SHIFT=0. REVOLVER retains its existing
minimum SPACE boost. Whole-file RMS is matched, not LUFS. These diagnostic files
are not recordings of real weapons or of a user DAW project.

MIT notices remain in source and in the existing Licenses/Rings-MIT.txt resource
copied into distributed artifacts. Keep them with installers/plugin packages.
