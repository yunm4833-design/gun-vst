# SMEAR, input multiband dynamics and post reverb - stage 7

This cumulative source includes stages 1-6. GitHub was not updated. All existing
APVTS IDs, order, types, ranges and defaults are retained; 17 new parameters are
appended. Source/comments/UI remain ASCII and the UI remains black/white with
INVERT support. The window sizes stay 1024x765 expanded / 1024x562 collapsed.

## Reading the supplied effect-chain screenshot

The visible order is OTT -> BREACH Resonator -> Pro-Q 4 -> ValhallaVintageVerb ->
Fruity Limiter. No plugin settings are visible. This suggests separate jobs:
input dynamics shape the excitation, BREACH creates tuned resonances, EQ shapes
the result, reverb creates a separate late field, and the final limiter controls
output. The screenshot cannot establish thresholds, EQ curves or reverb settings.

This stage keeps those jobs separate rather than calling every spatial change
SPACE. It does not reproduce Pro-Q 4, ValhallaVintageVerb, Fruity Limiter or Xfer
OTT. The existing TONE/tilt and output soft limiter remain their existing designs.

```text
INPUT -> [OTT-style 3-band dynamics] -> BREACH input gain / excitation
      -> resonator + SMEAR + TONE / WIDTH -> main dry/wet mix
      -> [HALL / PLATE / SPACE / SHIMMER reverb] -> output gain / soft limiter
```

The input dynamics affect both the subsequent dry and resonant paths, just as an
external OTT before BREACH does. Reverb affects the completed main dry/wet mix,
including fully dry main MIX settings, just as an external reverb after BREACH
does. The dynamics are base-rate; reverb follows the selected 1x/2x/4x core rate
so it can remain before the existing output gain and oversampled limiter.
All five gun modes share these modules. No gun preset silently enables them.
Both modules default OFF, allowing use with an existing external chain.

## Main knobs

| Knob | Musical role |
| --- | --- |
| DRIVE | Excitation saturation; independent of input dynamics depth |
| SMEAR | Short diffusion / transient blur, from focused to spread |
| DECAY | Resonator ringing length; separate from reverb decay |
| TONE | Existing tilt, modal brightness / SMG vowel shaping |
| DENSITY | Existing continuously weighted resonator voices |
| MIX | Original dry/resonator balance before the separate reverb |

SMEAR retains the automation ID `space`, range 0..1 and default .30. The host's
legacy parameter name Space is also retained so existing automation can be found.
Only the plugin-facing main label becomes SMEAR. New instances use SHORT SMEAR:
a compact two-stage, two-channel unitary diffusion network, with delays
0.7-12.7 ms and allpass coefficients .48/.36. Both channels are retained at the
output: unlike the earlier four-lane projection, the full-SMEAR endpoint preserves
integrated stereo energy. Intermediate amounts use a convex blend; correlated
signals can still change level, and no adaptive gain matching is used. There is no reverb-tank feedback, predelay control,
room size or seconds-long tail. The smoothstep amount curve gives finer control
near the endpoints. With modulation routes at zero, SMEAR=0 is truly focused in
all modes; REVOLVER no longer forces a hidden +.35 diffusion amount in SHORT mode.
Actual REVOLVER resonator decay remains independent and can still ring for seconds.

B // RESONATOR has SMEAR: SHORT / SMEAR: LEGACY. Selecting SHORT also chooses the
upgraded spatial engine. Selecting the old spatial engine uses legacy diffusion;
its separate state marker still exists. Models fade over 20 ms with both spatial
networks kept live, including at zero amount. The new `smearDspRevision` root
property defaults to 2, and missing properties select 1. Thus earlier sessions
retain their old diffusion curve, REVOLVER minimum amount and spatial selection.
Changing the model with transport stopped is recommended for comparisons.

## E // DYNAMICS

An independent implementation of generic OTT-style upward/downward compression:

- Three phase-aligned Linkwitz-Riley fourth-order bands, initially 120 / 2500 Hz.
- A fully linked stereo peak detector per band; left/right gain stays identical.
- DEPTH, TIME, UPWARD, DOWNWARD, input/output gain and both crossover frequencies.
- Three signed band gain readouts: positive lift / negative reduction in dB.
- 6 dB soft knees; per-band upper/lower thresholds. Full-depth/full-amount slopes
  approach 8:1 downward and 4:1 upward compression, with +18/-36 dB gain bounds.
- Upward boost smoothly returns to unity below the deep noise floor; there is no
  hard gate. Normal low-level input noise can still be lifted, as with any upward
  compressor. The detector does not inspect or alter BREACH's output level trims.
- Rate-correct attack/release, TIME scaling, smoothed controls and frequency
  targets, coefficient caching and a 1 kHz detector-to-gain control cadence.
- Enable/disable fades for 20 ms. OFF or settled DEPTH=0 is exact sample bypass,
  including the module's input/output trims. Active crossovers rotate phase even
  when their magnitude sum is flat; this is not a linear-phase multiband design.

No Xfer source, binary, preset or external plugin is bundled. This is not an exact
clone of that plugin's transfer functions. It adds no lookahead/fixed reported
latency. The active filters have their normal frequency-dependent phase response.

New parameters (appended after all original IDs):

| ID | Range | Default |
| --- | --- | --- |
| ottEnable | bool | OFF |
| ottDepth | 0..1 | .35 |
| ottTime | 10..400 percent | 100 |
| ottUp / ottDown | 0..1 each | .5 each |
| ottInGain / ottOutGain | -18..18 dB | 0 each |
| ottLowSplit | 40..400 Hz | 120 |
| ottHighSplit | 1000..8000 Hz | 2500 |

Crossovers clamp safely below Nyquist at low processing rates. Depth/Up/Down
values display as percentages in the UI while automation stores the ranges above.

## F // REVERB

Uses actual adapted code from the already-approved STK repository, pinned at the
same revision as the earlier formant stage. STK's permissive license and source
notices are included, as are original reference files and SHA-256 hashes.
Source/ThirdParty/STK/REVERB_PROVENANCE.md records all port and voicing changes.
No new third-party repository, IR download, Faust runtime or neural model is added.

| Mode | Implemented character |
| --- | --- |
| HALL | JCRev three-allpass/four-comb network, larger delays, unequal stereo taps |
| PLATE | Compact NRev six-comb/serial-allpass dense plate-style voicing |
| SPACE | NRev with much longer delay geometry and a more open late field |
| SHIMMER | Medium NRev plus the STK-style dual-head nominal octave-up late layer |

PLATE is an algorithmic voicing, not a physical plate simulation or a Dattorro
implementation. SPACE and SHIMMER are voicings/adaptations of NRev, not two new
external libraries. No claim of equivalence to commercial reverb algorithms is
made. The Shimmer control adds a single feed-forward octave layer with an 80 ms
Hann-crossfaded delay window and prefiltering to reduce octave-up aliasing. It has
granular/chorus sidebands; it is not a pitch-perfect phase vocoder and does not
produce a regenerating staircase of octave feedback. It is intentionally bounded.

BLEND, DECAY, PREDELAY, DAMPING, WIDTH and SHIMMER control only the reverb. Decay
sets a physical T60 target in the feedback combs; damping shortens upper-frequency
ringing, and actual broadband tail time depends on the selected network and tone.
Two independently excited, slightly offset cores retain stereo/anti-phase input,
with asymmetric wet crossfeed. Wet sums are normalized before the reverb mix.
The tail is real delayed/recirculating audio, separate from the short SMEAR path.

| New ID | Range | Default |
| --- | --- | --- |
| reverbEnable | bool | OFF |
| reverbType | HALL / PLATE / SPACE / SHIMMER | HALL |
| reverbMix | 0..1 | .18 |
| reverbDecay | .2..8 s | 2 s |
| reverbPreDelay | 0..200 ms | 20 ms |
| reverbDamping | 0..1 | .5 |
| reverbWidth | 0..2 | 1 |
| reverbShimmer | 0..1 | .5 |

OFF or settled BLEND=0 is exact bypass. Mode changes fade to dry, clear the old
network/auxiliary delays and fade into the selected model; they do not recall
frozen tails. Control changes are smoothed. No new fixed latency is reported;
reverb/predelay/window delays belong to the wet response. Enabled reverb reports
a conservative 20-second tail to the host; otherwise the previous 4 seconds remain.
The existing output gain and soft limiter apply after the new reverb.

## State, CPU and validation limits

Missing new parameter nodes are explicitly filled with defaults when loading an
older project, including into a previously used instance. OTT/reverb states are
reset on the audio thread following a state load. New metadata is append-only;
old parameter definitions/order/defaults and plugin identifiers are unchanged.
Earlier engine revision markers remain independent and earlier fixtures remain
valid. Source version stays 1.1.2 while these cumulative upgrades are evaluated.

All new DSP is C++17/header-only inside Source. Delay storage for every mode and
supported oversampling rate is allocated in prepare, not during sample rendering.
OFF modules skip sample DSP. Enabled reverb costs more at 4x, and all mode banks
reserve memory up front. Mode changes and panic/reset clear delay memory. Absolute
Release real-time CPU budgets on Windows/macOS are not established here. Existing
whole-plugin per-block allocations outside these new modules are not addressed.
The earlier low-DENSITY warm-voice CPU tradeoff still applies.

Tests cover dynamic crossover magnitude, stereo linking, upward/downward action,
exact bypass, short diffusion tail, four distinct reverb responses, physical decay
behavior, predelay, anti-phase excitation, nominal octave-up spectral output,
extreme controls and state/automation recall. Pre-edit stage 6 fixtures cover five
guns, mixed dry/wet, 44.1/48 kHz and all three OS choices. New automation tests use
aligned events at 64/1024 samples over five guns and all OS choices. UI smoke
covers six tabs, themes, bounds, both new enable controls and saved-state integrity.
Header unit executables also run with AddressSanitizer/UndefinedBehaviorSanitizer.

Actual DAW loading/automation, Windows/macOS/AU compilation, real-time performance,
real recordings and subjective listening-panel quality are not validated by
these offline Linux tests. The supplied WAVs are synthetic comparisons using one
common master gain; they are not individually loudness-matched or proof of a
subjective sound-quality improvement. If using the screenshot's external OTT or
Valhalla chain, leave the corresponding built-in module off to avoid stacking it.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBREACH_BUILD_DSP_TESTS=ON -DBREACH_BUILD_UI_TESTS=ON
cmake --build build --config Debug --parallel
ctest --test-dir build --output-on-failure
./build/BreachUiSmoke /tmp/breach-ui
./build/BreachStage7Audition /tmp/breach-audition
```

Executable paths vary with the generator/platform. See Docs/Stage7/VALIDATION.txt
for the delivered run's actual results and limitations.
