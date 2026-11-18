# Additional Rings modal code used in BREACH stage 5

The existing Rings string integration remains unchanged. This stage adds the
modal Resonator path, still under Emilie Gillet's MIT license.

Pinned Rings repository revision:
9739c0227708fab7a28b4efef7b27a9a1bc098d1
https://github.com/VCVRack/pichenettes-eurorack/tree/9739c0227708fab7a28b4efef7b27a9a1bc098d1

Additional reference files: rings/dsp/resonator.h and rings/dsp/resonator.cc.

Pinned stmlib revision:
c0c42ec91d315a2ec9c0a3845e593b71ffa429db
https://github.com/pichenettes/stmlib/tree/c0c42ec91d315a2ec9c0a3845e593b71ffa429db

Additional reference files: dsp/filter.h and dsp/cosine_oscillator.h.
The original filter.h contains one non-ASCII en dash in a comment; that single
character is normalized to an ASCII hyphen to satisfy the source-text rule.
No executable upstream reference code is changed. Other upstream copies are
byte-identical. Hashes of included copies are in UPSTREAM_SHA256.txt.

## Compiled port and actual use

RingsModalPrimitives.h includes the scalar stmlib Svf bandpass state recurrence
and CosineOscillator. The bank uses Rings' original unnormalized bandpass output
so longer damping does not proportionally reduce a short strike's onset. Peak
response therefore depends on resonance; this is not a constant-peak EQ bank.
Double state/coefficient precision replaces float.
The cosine generator uses its exact cosine initializer instead of the fast
approximation. It is evaluated at control updates to generate modal amplitudes.

Source/ModalDSP.h adapts Resonator::ComputeFilters and Process:

- Retains harmonic/stretch progression, stiffness *= 0.98, evolving brightness
  loss and the odd/even weighted filter accumulation.
- Caps the lightweight engine at 12 modes (6 for PISTOL, 12 for REVOLVER), rather
  than allocating 64 modes for every stereo voice.
- Replaces the embedded LUT parameter maps and approximate frequency mapping
  with explicit per-instance sample rates and physical decay-time parameters.
- Matches digital pole radius and angle to target RT60 and frequency, then maps
  those poles into the TPT g/r representation used by the imported Svf kernel.
- Adds normalized modal weights, high-frequency fade, fixed 0.75 send calibration,
  family-specific material/decay tuning and existing-parameter mappings.
- Uses a 1 kHz control cadence with a 5 ms one-pole glide, caches settled
  coefficients, and clears inactive modes rather than resuming stale tails.
- Recombines odd/even sums per input channel; it does not expose the upstream
  main/aux outputs as an additional plugin audio bus.
- Adds per-voice stereo state, rate/profile resets and independent session versioning.

No external Rings binary, firmware resource tables, VCV Rack runtime, plugin
wrapper, extra dependency or unlisted GitHub project is required. This is an
adapted modal resonator, not a complete unmodified Rings instrument.

The original MIT notices are retained in compiled headers, reference sources,
LICENSE.txt and the existing Licenses/Rings-MIT.txt artifact notice.
