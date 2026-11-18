# Five-mode level and control calibration - stage 6

This cumulative source contains the UI and DSP stages 1-5 plus a versioned balance
layer. No GitHub repository was updated. The project version remains 1.1.2 while
these staged changes are evaluated locally.

## What changes

All five modes now share progressive DRIVE / excitation-drive mappings, correctly
persistent macro smoothing, continuous DENSITY voice weights, and fixed wet-level
trims. The existing Rings string/modal bodies and STK formants retain their own
spectral and decay characteristics. This is a calibration pass, not a new neural
model or a claim of equal loudness on arbitrary input.

SNIPER previously multiplied an already high-gain SVF bandpass by a Q-dependent
boost before saturation. This caused early saturation, a large level jump, and
little useful DECAY range. In BALANCED mode it uses the previously integrated MIT
Rings/stmlib TPT SVF kernel with peak normalization before the existing asymmetric
ADAA saturation. Its pole radius/angle are designed for physical decay and pitch:
RT60 = 0.04 * 30^d seconds, with the existing Q modulation scaling this target.
Targets are bounded to 0.025-1.6 seconds; unmodulated endpoints are 0.04-1.2 s.
Coefficient targets update near 2 kHz and unchanged targets are cached. Static
coefficient design and filter states use double precision. SNIPER's old path is
retained in LEVELS: LEGACY.

No additional open-source project, dependency, network fetch, or runtime SDK was
added. The existing Rings license/provenance applies to the reused filter kernel.
Balance curves, coefficient design, measurement harness and trims are BREACH code.

## Fixed trims

| Actual upgraded resonator profile | Wet trim |
| --- | ---: |
| PISTOL modal | -2.3 dB |
| SHOTGUN string | +2.5 dB |
| SMG formant | 0.0 dB |
| SNIPER normalized SVF | +12.0 dB |
| REVOLVER modal | -4.1 dB |

The trim follows the actual engine, including manual COMB STRING / DUAL FORMANT
overrides. Legacy engine fallbacks use unity trim. SNIPER normalization is part of
the balance revision and is active with either DRIVE DSP selection. The trims are
after spatial processing, tilt and width, before dry/wet mix, output gain and the
existing limiter. The dry path is not trimmed. Gains ramp for 30 ms when their
target changes; a new instance starts directly at its configured target.

These are constant gains, never computed from the input envelope or output RMS.
There is no adaptive AGC, noise gate, silent-tail amplification or per-file level
matching. Settings such as chord, drive, decay, tilt, feedback damping, per-voice
level, topology and input spectrum can still cause large differences. The output
limiter and output gain retain their existing behavior and IDs.

## Control response

- DRIVE and EXC DRIVE: (exp(ln(3.9810717)*x)-1)/(3.9810717-1), for x in 0..1.
  Both endpoints are retained. The lower half gives finer excitation control;
  the last part still reaches the previous maximum drive. No APVTS range changes.
- DECAY: the existing 0.05..1 parameter is mapped to 0..1 before the engine's
  existing physical decay curve. Short and long mode characters are retained.
  Per-voice decay multipliers and Q modulation still apply.
- DENSITY: target voices = 1 + density * (available voices - 1). Each voice is
  weighted by clamp(target - voiceIndex, 0, 1). Sum-of-squared-weight normalization
  avoids a discrete gain jump when a voice begins fading in. This is power
  normalization, not a guarantee for correlated harmonic sums. Mod Depth follows
  the same continuous weights.
- DRIVE, SPACE, DECAY, TONE, DENSITY, MIX, input/output gain and stereo width now
  retain their smoothing ramps across host blocks. EXC DRIVE is also smoothed.
  Changing SMOOTH preserves current ramp values, including the current wet trim.
  Other advanced controls and the three mix curves retain their existing mapping.

All available chord/MIDI voices are processed even when their density weight is
zero, so newly audible voices have live filter state instead of frozen tails.
This raises CPU use at low DENSITY compared with the previous early-exit loop,
especially at 12 voices / 4x. VOICE CAP still limits actual voice count. Absolute
CPU budgets need Release builds on the target Windows/macOS hosts; this stage is
not a claim of improved whole-plugin CPU use. A future optimization must preserve
continuous voice entry and the level calibration.

## Recall and UI

D // GLOBAL has a LEVELS: BALANCED / LEVELS: LEGACY button. New instances default
to BALANCED. The non-parameter root property balanceRevision is 1 (LEGACY) or 2
(BALANCED). A missing property selects LEGACY, independently of all five earlier
DSP revision properties. Thus both original sessions and stage 1-5 sessions retain
their previous audio and previous smoothing behavior without manual migration.
Switch revisions with playback stopped when evaluating an existing project.
For the complete upgraded sound, also select the NEW / RINGS / STK engine buttons.

No APVTS ID, parameter order, type, range, default, version hint, plugin ID, MIDI
mapping or automation lane changed. Source and UI text remain ASCII. This revision
adds no latency; previous nonlinear latency compensation remains in place.

## Offline calibration and limits

Measurement uses 48 kHz stereo synthetic proxies: drums, bass, C-major pad,
vocal-like harmonics, independent shorter-decay drums, and a transposed pad.
All active inputs have -20 dBFS RMS. Each render is 3 seconds of input plus one
second of tail, MIX=100%, limiter OFF, DRIVE=.45, EXC DRIVE=.3, oversampling=2x,
SPACE=0, shift=0, motion OFF, C-major resonator. REVOLVER keeps its minimum SPACE.

Reported levels use a 48 kHz K-weighted, 400 ms / 100 ms-hop estimate with -70 dB
absolute and -10 dB relative gates. They are comparative offline estimates, not
certified LUFS/conformance measurements. Four calibration inputs choose the
median relative mode offsets; trims are rounded to 0.1 dB and bounded to +/-12 dB.
The two holdout inputs are not used for fitting. BEFORE is stage 5 with its earlier
upgraded engines enabled. AFTER adds this balance layer.

| Synthetic input | Before spread | Balanced spread |
| --- | ---: | ---: |
| Drums | 20.76 dB | 2.91 dB |
| Bass | 12.34 dB | 3.41 dB |
| C-major pad | 6.68 dB | 3.35 dB |
| Vocal-like | 13.74 dB | 4.21 dB |
| Holdout drums | 21.47 dB | 3.13 dB |
| Holdout transposed pad | 22.18 dB | 19.69 dB |

The median spread across these six inputs is 17.25 -> 3.38 dB. The transposed pad
remains a clear limitation: string/modal profiles respond weakly to out-of-chord
energy, while formants pass different spectral bands. Fixed gains cannot make
those responses equal for all inputs. Real user recordings, broader parameter
grids and subjective listening are still needed before release.

A/B WAVs use one common master attenuation across every mode and both stages.
They deliberately retain level differences, including the holdout failure case.
They are synthetic comparisons; they were not individually normalized or used as
proof of subjective improvement.

## Validation and reproduction

Enable BREACH_BUILD_DSP_TESTS and BREACH_BUILD_UI_TESTS in the usual CMake build.
BreachBalanceTests checks monotonic mappings, density continuity/power, fixed trim
bounds, normalized SNIPER peak gain, silence and RT60 at 32/44.1/48/96 kHz.
BreachBalanceIntegration verifies pre-edit stage 5 audio fixtures, balanced state
roundtrip, 1x/2x/4x automation at 64/256/1024 samples, silence and fully dry latency.
Fixtures under Tests/Fixtures/modal_stage5 must not be regenerated by new code.
BreachUiSmoke exercises both LEVELS selections as well as existing navigation,
themes, bounds, macro editing and state preservation.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBREACH_BUILD_DSP_TESTS=ON -DBREACH_BUILD_UI_TESTS=ON
cmake --build build --config Debug --parallel
ctest --test-dir build --output-on-failure
# Executable paths depend on generator/platform/configuration.
./build/BreachBalanceMeasure /tmp/breach-before --legacy
./build/BreachBalanceMeasure /tmp/breach-after
python Tests/AnalyseBalance.py /tmp/breach-before /tmp/breach-after /tmp/breach-report
```

The optional analysis script needs Python, NumPy and SciPy, only on the analysis
machine. The plugin still needs only C++17/JUCE and bundled source. Windows/macOS
builds, DAW automation/recall in an actual host, real-time performance and real
recording listening tests have not been performed in this Linux environment.
