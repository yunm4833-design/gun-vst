# DRIVE / SNIPER DSP upgrade - stage 2

This document describes stage 2. The cumulative source also contains the Rings
string adaptation described in RINGS_UPGRADE.md. Stage 2 includes the UI upgrade
and spatial stage 1. It changes
excitation DRIVE and the SNIPER post-resonator saturator. It does not replace the
existing TPT SVF, reconstruct the waveguide, or modify the output soft limiter.
No repository push or remote file update was performed.

## Implementation

Source/NonlinearDSP.h is independently implemented C++17, with no new external
DSP dependency. It uses first-order antiderivative antialiasing (ADAA).

- A piecewise-linear transfer curve has an exactly integrated quadratic
  antiderivative per table segment. Tables contain 8193 double-precision values
  and their integrals over [-16,16], with constant saturated extensions.
- Tables are created when the processor's engine is constructed, off the audio
  thread. Shaper state is independent for both excitation channels and all
  12 stereo voices. Processing, reset and rate changes allocate no memory.
- Near equal consecutive input samples, a midpoint limit avoids cancellation.
- The DRIVE curve remains tanh. A moderate compensation gain of
  1/sqrt(1 + 0.25*(drive - 1)) reduces excessive downstream excitation.
- SNIPER integrates the complete new nonlinear transfer, not merely the final
  tanh of a separately calculated distorted signal:

```text
f(x) = tanh(1.6*x + 0.48*(1 - 1/(1 + 0.25*x*x)))
```

The curve retains the old quadratic asymmetry close to zero, bounds its bend at
large inputs, and avoids the old negative-input polarity fold. Its output is
multiplied by 0.8 for headroom. Each stereo voice has a 5 Hz DC blocker with its
coefficient derived from the actual processing sample rate.

## Time alignment and oversampling

First-order ADAA has a nominal half-processing-sample delay and amplitude
coloration. A following averaging filter makes each nonlinear stage's nominal
low-frequency delay one processing sample. Non-SNIPER voice paths receive a
one-sample delay instead of that second nonlinear stage.

The dry path is delayed by two processing samples. A final internal delay pads
both paths to two BASE-rate samples at 1X/2X/4X. The upgraded processing path uses
separate JUCE IIR oversamplers with integer-latency compensation enabled.
The host receives the compensated oversampling delay plus those two samples:
2 samples at 1X, 6 at 2X, 8 at 4X on the tested JUCE 8.0.4 configuration.

This aligns the nominal added delay. ADAA/averaging roll off high frequencies;
they are not transparent delay filters, and a nonlinear or resonating wet path
cannot be made identical in phase to the dry path by a fixed delay. 2X/4X reduce
the high-frequency coloration. The DC blocker also has frequency-dependent
phase. Existing IIR oversampling has frequency-dependent group delay.

Change DRIVE DSP revision or oversampling while transport is stopped, and save
the project. Revision selection reports host latency immediately; OS automation
updates it at the processing block boundary. Rate/revision changes reset the
nonlinear engine and selected resampler without allocation. They do not promise
seamless tails or click-free live switching. Real DAW PDC behavior still needs
validation in the intended hosts. Legacy mode retains historical audio and
historical zero latency reporting, including its old oversampling limitations.

## Compatibility

All APVTS parameter definitions, IDs, order, defaults, ranges and version hints
are unchanged. Plugin identity and the PARAMETERS state-tree name are unchanged.

A separate, non-automatable root property nonlinearDspRevision is serialized:

- Missing or 1: legacy excitation and SNIPER; original audio/PDC behavior.
- 2: new nonlinear processing and compensated timing.

New instances default to 2. Both original v1.1.2 states and spatial-stage-1 states
load as nonlinear LEGACY. The spatialDspRevision property is independent, so
SPACE can remain NEW while DRIVE is compared separately. D / GLOBAL exposes
DRIVE DSP: NEW / LEGACY. The host is notified of non-parameter state changes.

## Verification and limits

Linux JUCE 8.0.4 Debug Standalone and VST3 builds passed. CTest covers:

- Original v1.1.2 audio fixtures and pre-stage-2 fixtures: zero sample error on
  this Linux build; both stored engine revisions restore correctly.
- Modern engine state/audio roundtrip, dry impulse alignment, latency updates,
  revision/OS switching and 12-voice max-drive stress at 44.1/48/96 kHz, 1X/2X/4X
  with 32/128/1024 sample blocks.
- Curve monotonicity and bounds, LUT error below 5e-6, near-equal/extreme inputs,
  per-stage nominal delay, reset and no processing-time allocations.
- SNIPER 100 Hz asymmetric-sine DC mean: approximately 0.01322 before the DC
  blocker and 5.7e-10 after settling (48 kHz test).
- A coherent 7010 Hz, amplitude-3 sine at 48 kHz times 1/2/4. Folded-harmonic
  energy (harmonics 2..25 above the processing Nyquist), normalized to each
  output's fundamental, fell by 15.19/23.12/19.96 dB for tanh and
  14.81/20.19/18.53 dB for the SNIPER curve versus direct evaluation of the SAME
  transfer. This is a unit test of the nonlinear stage at each processing rate,
  not a full-plugin alias specification or a cross-rate performance ranking.
- UI smoke tests: engine controls, all tabs, both themes, visible bounds and
  view-only parameter preservation.

ADAA reduces aliasing; it does not eliminate it. The waveguide's feedback tanh,
output limiter and other original nonlinearities remain potential alias sources.
The two gain changes are deliberate voicing/headroom decisions; this stage does
not add a loudness auto-match feature or change the limiter threshold behavior.
Windows/macOS builds, DAW hosting and Release CPU budgets are not yet validated.

Run the combined suite with BREACH_BUILD_DSP_TESTS=ON (see SPATIAL_UPGRADE.md).
The header-only nonlinear tests can also be built directly:

```sh
c++ -std=c++17 -O2 -ISource Tests/NonlinearDSPTests.cpp -o nonlinear-tests
./nonlinear-tests
```

BreachNonlinearAudition generates synthetic percussion plus DRIVE and SNIPER
legacy/new comparisons at 2X. The spatial engine is NEW in every comparison;
SPACE and shift are zero. Whole-file RMS is matched, not LUFS. These files show
the combined timbral/gain/ADAA changes and are not a controlled proof that a
particular real recording's perceived plastic sound has been solved.

No GPL chowdsp_utils or Odin 2 code is included. No STK/Rings/Faust code is added.
