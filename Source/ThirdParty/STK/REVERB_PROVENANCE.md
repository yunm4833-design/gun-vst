# STK reverb and octave layer incorporated in stage 7

Approved project: https://github.com/thestk/stk
Pinned revision: 6aacd357d76250bb7da2b1ddf675651828784bbc (same as stage 4).
Copyright (c) 1995-2023 Perry R. Cook and Gary P. Scavone.
Full permissive STK license: LICENSE.txt and the top of StkReverb.h.
The existing Licenses/STK.txt is bundled into plugin/app artifacts by CMake.

New upstream reference copies are src/JCRev.cpp, include/JCRev.h, src/NRev.cpp,
include/NRev.h, src/PitShift.cpp and include/PitShift.h. These are not separately
compiled. REVERB_UPSTREAM_SHA256.txt records the fetched contents' SHA-256 hashes.
No full STK runtime, rawwaves, global sample-rate state, Faust compiler, external
binary plugin, impulse response pack or unlisted GitHub repository is added.

Compiled adaptation: StkReverb.h. The JCRev three-allpass/four-comb network,
feedback lowpasses and unequal stereo output delays; NRev six parallel combs,
serial allpasses, lowpass and two allpass stereo outputs; and PitShift's moving
two-head delays, rate=1-shift and half-window-spaced Hann crossfade execute in the
live audio path. The upstream library base classes, exceptions and frame API
are replaced by per-instance double states, fixed arrays and delay vectors.
Vectors allocate only in prepare. Delay lengths are scaled to the actual rate,
rounded to primes, and exposed to a physical T60 feedback coefficient design.

Port/integration changes:
- Wet-only network outputs; dry/wet mixing is performed once by BREACH.
- Comb read-before-write uses the configured physical delay directly, rather
  than STK's previous lastOut feedback sample. T60 gains use that physical length.
- Unity-DC loop lowpasses with rate-correct damping are used in both networks.
  NRev originally had its lowpass only after its comb/allpass group.
- Comb sums are normalized by 1/4 or 1/6. Two separately excited cores retain
  stereo/anti-phase content, with asymmetric crossfeed at their wet outputs.
- HALL uses JCRev with longer delays. PLATE uses compact NRev; SPACE uses longer
  NRev; SHIMMER uses a medium NRev and an octave-up late-field layer. PLATE is a
  dense plate-style voicing, not a Dattorro/physical plate model or an emulation
  of ValhallaVintageVerb. Different profiles are not different full libraries.
- PitShift's integer table reads and fixed 5000-sample window become interpolated
  reads, a physical 80 ms window and continuous complementary Hann weights.
  This also avoids the original constructor's inclusive window-table loop.
- Eight-pole lowpass conditioning precedes the octave-up layer to reduce aliasing.
  The shifter is granular/time-varying: sidebands/chorusing remain and pitch is
  nominally +12 semitones, not a phase-vocoder-quality perfect transposition.
- Shimmer is a single feed-forward octave layer, not an octave feedback cascade.
  It adds no regenerative runaway path. No claim of equivalence to a commercial
  shimmer plugin or a hardware reverberator is made.

Source/ReverbDSP.h supplies smoothed controls, predelay, stereo width, bypass,
mode-change fades/resets and 1x/2x/4x banks. It routes after BREACH's main mix and
before output gain/soft limiter. No fixed latency is added; the musical reverb
and granular-window delays are part of the wet response. Source/DynamicsDSP.h is
an independent BREACH implementation of generic multiband upward/downward
compression. It contains no Xfer OTT source, presets or binary integration.
