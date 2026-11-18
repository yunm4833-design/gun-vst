# Spatial DSP upgrade - stage 1

This document describes stage 1. The cumulative source also includes stage 2,
documented in NONLINEAR_UPGRADE.md. Stage 1 upgrades SPACE and frequency shifting only. Existing resonators,
excitation, saturation, modulation parameters and the UI redesign are retained.
No remote repository changes are part of this delivery.

## Signal processing

Source/SpatialDSP.h is a self-contained C++17 header with no JUCE or third-party
runtime dependency. All storage is allocated during prepareToPlay.

- DelayAllpass implements (z^-D - g) / (1 - g*z^-D), with unity magnitude.
- StereoDiffuser uses two stages of four distinct delay allpasses, normalized
  Hadamard mixing and unit-norm stereo encoding/decoding. It is a feed-forward
  diffusion network, not a new feedback reverb. Its wet projection is passive.
- Delay lengths are calculated at the actual processing rate. Three banks are
  preallocated for 1X, 2X and 4X, keeping the physical delay times consistent.
- The Hilbert network uses eight second-order allpasses per branch and a
  one-sample offset on the quadrature branch. Double-precision filter states
  support poles close to the unit circle. A single oscillator serves both sides.
- Positive shift moves positive-frequency tones upward; negative shift moves
  them downward. Both branches remain active while shift is zero.
- Around zero shift, the processed contribution fades over 20 ms. At zero it
  settles to an exact sample-for-sample bypass. Very small shifts (below 2 Hz)
  intentionally blend the shifted and unshifted signals to avoid a hard switch.
- The stereo network runs at zero SPACE to avoid resuming frozen delay tails.
- At 1X/2X/4X rate changes, the newly selected bank is cleared without allocation.
  This prevents stale-tail recall; it does not promise seamless tail retention
  across a live oversampling switch. The rest of the original oversampling
  pipeline and its existing host-latency behavior are outside this stage.

The allpass Hilbert network has frequency-dependent phase/group delay, not a
single linear-phase FIR latency. No new fixed-latency compensation is claimed.
Shifting content past Nyquist still requires appropriate band limiting; this
stage does not make arbitrary above-Nyquist shifts alias-free.

## Session compatibility

The complete createParameterLayout function is byte-identical to the previous
version: IDs, order, types, ranges, defaults and version hints are unchanged.
The plugin manufacturer/product codes and state tree name are unchanged.

A non-parameter root property, spatialDspRevision, is saved in plugin state:

- Missing or 1: use the original spatial algorithms (LEGACY).
- 2: use the upgraded spatial algorithms (NEW).
- New instances default to NEW; unversioned v1.1.2 sessions load as LEGACY.

GLOBAL exposes SPACE DSP: NEW / SPACE DSP: LEGACY. This does not add an APVTS
parameter. Changing it notifies the host that non-parameter state changed.
Both paths stay warm and selection crossfades over 20 ms. Save the DAW project
or preset after switching if you want that engine choice recalled.

## Verification

- Linux JUCE 8.0.4 Debug VST3 and Standalone builds passed.
- Unit tests cover allpass frequency response/energy, diffusion duration and
  passivity, stereo coherence, reset, zero-effect bypass and allocation checks.
- Sinusoidal sideband tests at 20, 100, 1000, 7000 and 18000 Hz, +/-40 Hz shift,
  host rates 44.1/48/96 kHz and 1X/2X/4X: worst measured image -68.22 dBc;
  maximum wanted-tone gain deviation 0.00246 dB. This is a sampled test grid,
  not a claim over all frequencies, modulation trajectories or higher rates.
- Six legacy audio fixtures captured before the DSP edit reproduced with zero
  sample error on this Linux build. They cover all five modes and all OS choices.
- New-engine save/load audio recall, UI navigation and theme/bounds tests passed.
- Full-processor stress covers 12 enabled pitch classes, 12 voices, 44.1/48/96
  kHz, all OS choices, 32/128/1024-sample blocks and +/-500 Hz shifts.
- Windows/macOS builds, real DAW hosting and Release CPU budgets still require
  validation on their respective platforms. No DSP code depends on Linux APIs.

## Run tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBREACH_BUILD_DSP_TESTS=ON -DBREACH_BUILD_UI_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The session test uses Tests/Fixtures/v1_1_2, generated from the pre-upgrade
processor with deterministic synthetic input. The audio fixtures are little-
endian float32 captures from Linux; small cross-platform math differences may
require investigation rather than replacing the fixtures with new output.
The integration test requires a GUI-capable JUCE environment. The standalone
header tests can also run without JUCE:

```sh
c++ -std=c++17 -O2 -ISource Tests/SpatialDSPTests.cpp -o spatial-tests
./spatial-tests
```

BreachSpatialAudition exports synthetic stereo percussion and two legacy/new
comparisons (SPACE and SPACE plus +137 Hz shift). Outputs are matched to the
same whole-file RMS, not LUFS. REVOLVER retains its existing minimum SPACE boost.
These examples are diagnostic material, not a subjective sound-quality verdict.

## Algorithm provenance

The normalized Hadamard matrix and allpass recurrences are independently
implemented standard DSP formulas. Signalsmith DSP's delay/mixing architecture
was consulted; no external library source is bundled or required.

Hilbert pole values were independently fitted to the documented transfer
functions. Tools/design_hilbert.py records the optional offline design process
(NumPy/SciPy). Neither Python nor these packages is required to build the plugin.

ADAA for DRIVE/SNIPER is now implemented in stage 2 (NONLINEAR_UPGRADE.md).
SHOTGUN waveguide reconstruction, modal banks and formant redesign remain future
stages.
