# STK code incorporated in BREACH formant stage 4

Project: The Synthesis ToolKit in C++ (STK)
https://github.com/thestk/stk

Pinned commit: 6aacd357d76250bb7da2b1ddf675651828784bbc
Copyright (c) 1995-2023 Perry R. Cook and Gary P. Scavone.
The permissive STK license is reproduced in LICENSE.txt and in the adapted header.

## Upstream reference copies

- src/FormSwep.cpp: setResonance coefficient design and parameter sweep setters.
- include/FormSwep.h: parameter sweep and tick difference equation.
- src/Phonemes.cpp: measured formant frequency/radius/gain data.
- src/VoicForm.cpp: reference for using four FormSwep filters with phoneme data.
- src/Stk.cpp: reference for the original global sample-rate mechanism.
- LICENSE: unmodified upstream license.

Original files are retained in upstream/ and hashed in UPSTREAM_SHA256.txt. They
are reference sources, not separately compiled. StkFormant.h is the compiled port.

## Ported material and changes

StkFormant.h retains the FormSwep setResonance formulas and the tick difference
equation, with STK's peak-normalizing zeros at +1/-1. Its five vowel datasets are
copied exactly from Phonemes.cpp: ahh, ehh, eee, ohh and ooo, exposed as A/E/I/O/U.
The source notices are preserved and the imported code is exercised in tests.

- Namespace becomes breach::stkport; the STK Filter base, vectors, callbacks,
  exceptions and global sample-rate mechanism are replaced with fixed arrays
  and an explicit per-instance rate.
- Double-precision arithmetic/state is retained for narrow resonances at high SR.
- STK's parameter sweep step is moved from every audio sample to a physical
  2 kHz control cadence, with an 8 ms target sweep. Coefficients remain fixed
  between control updates. Changing targets frequently behaves like smoothing.
- Inputs are constrained to supported frequency and radius bounds.
- Five selected datasets are interpolated across frequency, log radius and dB
  gain by the BREACH adapter. Radii are interpreted at a 44.1 kHz reference and
  converted to the actual processing rate; no STK global rate is changed.

Source/FormantDSP.h is the BREACH integration layer: partial pitch tracking of
four parallel formants, normalized gain weights, damping/decay/Q bandwidth
mapping, near-Nyquist attenuation, and a separate pitched STK resonance anchor.
The anchor is a BREACH design choice, not a component of STK VoicForm.

The full VoicForm instrument, singing wavetable, rawwave files, vibrato, noise
source and MIDI controllers are NOT bundled or run. This is an external-input
formant resonator adaptation, not a full vocal synthesizer or neural model.

Faust was evaluated during selection; no Faust source, generated code, data,
compiler/runtime or LGPL components were added. No unlisted GitHub model/library
was introduced. Existing Rings integration is independent of this STK stage.
