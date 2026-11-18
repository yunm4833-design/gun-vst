# Rings / stmlib code incorporated in BREACH

Upstream author and copyright: Emilie Gillet. License: MIT, retained in the
adapted source headers, LICENSE.txt and verbatim upstream reference files.
These notices must accompany redistributed source/binaries as required by MIT.

Pinned Rings source:
https://github.com/VCVRack/pichenettes-eurorack/tree/9739c0227708fab7a28b4efef7b27a9a1bc098d1

- rings/dsp/string.h
- rings/dsp/string.cc

Pinned stmlib submodule:
https://github.com/pichenettes/stmlib/tree/c0c42ec91d315a2ec9c0a3845e593b71ffa429db

- dsp/delay_line.h

The upstream/ directory contains unmodified copies of those three files for
review/provenance. They are reference files, not separately compiled. Their
SHA-256 digests are in UPSTREAM_SHA256.txt.

## Compiled adaptation

RingsPrimitives.h contains the actual DampingFilter from string.h and the
DelayLine read/interpolation/allpass methods from stmlib. They are called from
Source/RingsStringDSP.h in the processing path, not merely retained as examples.

- Namespace changed to breach::ringsport; external stmlib includes/macros removed.
- DampingFilter arithmetic retained, including coefficient interpolation.
- Templated MCU delay storage replaced with runtime float vectors prepared off
  the audio thread. Indexing remains circular; split integral/fractional values
  are written explicitly instead of using the upstream macro.
- Copy-disallow macros replaced with deleted C++ copy operations.
- Top-level const on float return values removed (no arithmetic change).

RingsStringDSP.h is an adapted subset of rings::String::ProcessInternal, retaining
its string-delay, FIR-loss, split allpass-dispersion and second pickup-tap model.
This is an actual open-source code adaptation, not just an independently written
algorithm inspired by Rings, but it is not a full unmodified Rings instrument.

## BREACH-specific changes

- Arbitrary host/effective sample rates and per-instance, per-channel state.
- Runtime delay capacity supports a 25 Hz floor at 4X (prepared with 20 Hz margin).
- No MCU tables, stmlib globals, shared RNG or fixed 48 kHz assumptions.
- Original low-frequency resampling fallback omitted; sufficient memory is reserved.
- Original random dispersion, bridge-curving path and infinite-decay mapping omitted.
- A sample-rate-derived one-pole loop lowpass replaces the original SVF/resources
  dependency; fundamental phase delay is computed for the lowpass and allpass.
- DECAY maps to physical seconds and is converted to a per-round-trip loop gain.
  The gain is capped below unity, so very lossy/high-pitched cases can decay faster
  than the target. This is intentional passive behavior, not automatic gain boost.
- Pickup output is a weighted difference rather than exposing the raw aux output.
- Delay smoothing, 3 kHz control updates, overload safety, host parameter mapping
  and session-version handling are part of the BREACH adapter.

No VCV Rack wrapper, graphics, GPL plugin code, STK code or Rings modal-bank code
is included in the compiled implementation. No neural-network model is used.

## Stage 5 addition

The separate modal resonator port and its additional source files are documented
in MODAL_PROVENANCE.md. The string port described above remains independent.
