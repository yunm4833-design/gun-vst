# SMG / formant model - stage 4

This document describes stage 4. The cumulative source also contains the modal
stage documented in MODAL_UPGRADE.md. Stage 4 includes stages 1-3 and adds an external-input formant
resonator based on the open-source STK FormSwep / Phonemes implementation.
GitHub repositories have not been modified.

## Source selection

STK was already on the user's approved project list. Its permissive source
license allows this integration with the copyright/license notices preserved.
No unlisted GitHub library or model has been introduced. Faust was evaluated,
but no Faust or Odin code/data/compiler/runtime was added.

Source/ThirdParty/STK/PROVENANCE.md records the pinned upstream commit, exact
source files, ported code and modifications. The upstream copies and hashes are
included for review. StkFormant.h contains actual imported coefficient design,
sweep logic, tick recurrence and selected measured phoneme datasets. It is called
by the processing path, not just retained as an example.

This is not the full STK VoicForm singing synthesizer, a speech recognizer, an
AI model, or a neural network. No external sample/raw-wave files are required.

## New sound model

Each of the 12 possible stereo voices has four parallel STK formants plus one
STK resonator tuned to the voice's existing note. The output combines 60% pitched
anchor and 40% vowel body, followed by a fixed 4X send calibration. This fixed gain
helps avoid an excessively quiet wet path; it is not an auto-loudness algorithm.

- A/E/I/O/U use the upstream ahh/ehh/eee/ohh/ooo datasets.
- The four relative gains are normalized, and the individual filters use the
  upstream constant-peak normalization. This avoids multiplying peak gain by Q.
- Vowel frequency, logarithmic radius and gain in dB interpolate continuously.
- The body follows pitch partially via clamp((pitch/220)^0.22, 0.7, 1.6), while
  the anchor tracks the actual requested note. This is an artistic resonator,
  not a claim to reproduce a complete human vocal tract.
- Upstream radii are interpreted at 44.1 kHz and converted through log radius
  to the actual processing rate, retaining physical damping bandwidth in Hz.
- Double-precision filter coefficients/state support narrow high-rate resonances.
- Parameters update near 2 kHz with STK-style target sweeps calibrated to 8 ms.
  This is not sample-by-sample audio-rate coefficient modulation.
- Formants near Nyquist are attenuated, rather than piling up at a clamp point.
- The filter bank rejects DC and uses no newly introduced nonlinear saturation.

New mode may differ substantially in level, decay and spectrum from the original
high-gain dual bandpasses. The supplied examples match output RMS for comparison;
there is no plugin auto-match. Existing output gain and dry/wet remain available.
The output limiter, earlier ADAA processing and spatial stages are unchanged.

## Parameters and operating scope

The new model operates only in SMG/AUTO, or with DUAL FORMANT manual override.
Other topologies retain their existing processing. D / GLOBAL exposes
FORMANT: STK / FORMANT: LEGACY independently of the other engine selections.

No APVTS parameter was added, removed, reordered or redefined. Existing mappings:

| Control | Additional role in new formant mode |
| --- | --- |
| TONE / COLOR | Morph A-E-I-O-U as TONE goes from 0 to 1; original post tilt EQ still applies. |
| Modulation TILT destination | Adds to the vowel position using the existing 0.35 modulation scale; still affects post tilt. |
| FB DAMPING | Increases formant bandwidth/loss. |
| DECAY and per-voice decay | Set note-anchor decay and scale body resonance duration. |
| Modulation Q destination | Narrows/broadens formant damping. |
| Root, notes, MIDI, detune, unison, warp | Continue to determine the voice anchor pitch; body pitch tracking is partial. |

This adapts the existing TONE and FB DAMPING behavior within the new topology;
legacy mode preserves their old behavior. TONE is not a separate vowel parameter.
The main TONE control and the new engine switch have tooltips describing this.

A non-parameter root property formantDspRevision stores 1 (legacy) or 2 (STK).
Missing markers select legacy for original and stage-1/2/3 sessions. New instances
default to STK. The selection is stored in plugin state and notifies the host of
a non-parameter change; it is not a new host automation lane. Other engine version
markers retain their independent selections.

Change engine choices while stopped and save the project afterwards. Model/rate/
topology changes reset the new bank; changing the active formant engine clears
legacy SVF histories to avoid resuming frozen tails. Seamless live tail changes
are not promised. No fixed latency was added in this stage, and the prior stage's
host latency behavior is retained. These IIR filters have frequency-dependent
phase/group delay rather than a single compensatable linear-phase delay.

## Verification

Linux JUCE 8.0.4 Debug VST3 and Standalone builds passed. Eight CTest targets cover
the accumulated changes, including:

- Original, stage-1, stage-2 and six newly captured stage-3 audio fixtures: zero
  sample error on this Linux build when restored to their legacy engine paths.
- New STK state/audio roundtrip, actual output changes, other-topology isolation,
  12-voice max-drive/Q/vowel modulation at host 44.1/48/96 kHz and 1X/2X/4X with
  32/128/1024 sample blocks.
- Imported recurrence compared with its closed-form z-domain impulse response at
  processing rates 44.1/48/96/192/384 kHz.
- Actual vowel data, interpolation, normalized weights, physical bandwidth mapping,
  finite modulated output, reset and no processing-time allocations.
- Optional direct-body impulse analysis across those rates: all five vowel spectra
  differ; strongest peak positions vary by less than 10 Hz on this grid, DC is
  rejected, and the static body gain remains below unity. These measurements exclude
  the pitched anchor and fixed send gain. Broad resonances' peaks can differ from
  their pole-angle frequency labels, as in the upstream filter.
- AddressSanitizer/UndefinedBehaviorSanitizer unit tests on Linux.
- UI switch, all tabs, both themes and visible component bounds.

Windows/macOS, real DAW hosting and production Release CPU budgets still require
platform validation. These objective checks do not establish the best subjective
sound for arbitrary material. No claim of realistic speech synthesis is made.

The regular test option is BREACH_BUILD_DSP_TESTS=ON. Header tests can also run
without JUCE:

```sh
c++ -std=c++17 -O2 -ISource Tests/FormantDSPTests.cpp -o formant-tests
./formant-tests
python Tools/analyze_formants.py ./formant-tests /tmp/formant-measurements
```

The optional analysis script requires NumPy/SciPy, not plugin builds.
BreachFormantAudition renders synthetic percussion with SMG and TONE=0 / 0.5,
comparing old dual bands with the new STK mode. Both use spatial/nonlinear NEW,
2X OS, no SPACE or SHIFT, FB DAMPING=0.3. Files are matched by whole-file RMS,
not LUFS, and verified not to clip. These are diagnostic examples, not recordings
of a user project or a subjective sound-quality verdict.

## Distribution

Source notices, STK LICENSE.txt, original source copies and pinned-source hashes
are included. Licenses/STK.txt is copied into plugin/app artifact licenses alongside
the Rings/font notices. Keep all required notices with distributed installers.
