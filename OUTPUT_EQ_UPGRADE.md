# Output EQ

The old `excHpf` parameter controls a one-pole filter on the resonator excitation
path. The dry branch bypasses that filter, and the resonator can produce energy
below the excitation cutoff. The **EXC INPUT CUT** label now describes its job.
Use **G // EQ** for a low cut across the entire BREACH mix.

```text
INPUT -> OTT-style dynamics -> BREACH resonator + dry/wet MIX
      -> OUTPUT EQ -> reverb -> output gain / soft limiter
```

The output EQ also filters what feeds the reverb. A reverb tail already ringing
can continue after an EQ change. Output gain and the limiter follow the reverb.

Open **G // EQ**, enable **EQ OFF** to turn the module on, then select a band.
Band 1 is ready with an 80 Hz, 24 dB/oct low cut. The other bands start disabled.
Move a node horizontally to set frequency. Vertical movement changes gain for
bells and shelves, or Q for cuts and notches. Alt-drag adjusts a bell's Q.
Double-click a node to toggle its band. Numbered buttons select any band, and a
small dot shows which bands are enabled. The selected band's inspector offers
keyboard-editable frequency, gain, and Q values and a filter-type selector.
Shelves use a fixed monotonic slope, so their Q control is disabled.

The graph plots the combined EQ transfer curve from the actual filter design at
the processing sample rate. It shows the settled response of the current EQ settings. It is a response
plot. Live spectrum analysis is outside this version's scope. While bypassed, it shows the flat
applied response and a dashed preview of the stored settings.

- Eight stereo-linked bands, each with on/off, shape, frequency, gain and Q.
- Low/high cut at 12, 24, or 48 dB/oct, low/high shelves, bell, and notch.
- 10 Hz to 20 kHz control range, +/-18 dB gain, Q .25 to 8. The effective upper
  frequency is limited to 49% of the base sample rate.
- Double-precision biquads and cascaded Butterworth sections for cut filters at
  Q .707. Changing the Q of steep cuts adjusts the most resonant section.
- Smoothed frequency, gain and Q with short fades for band on/off and type changes.
  Inactive bands skip sample processing. No look-ahead or added reported latency.

Existing parameter IDs and defaults are preserved. The 41 EQ parameters are
appended to the host layout with JUCE version hint 2 to preserve the ordering of
existing version 1 parameters in AUv2. New and earlier sessions start with EQ bypassed.
States without EQ parameters restore the defaults when loaded into a used instance.

The interaction uses the public EQ Eight documentation as a reference. BREACH
uses its existing manga theme and a compact selected-band inspector. Filter
coefficients follow the published Audio EQ Cookbook equations, with an original
C++17 implementation and no additional runtime dependency.

- Ableton EQ Eight reference: https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/#eq-eight
- W3C Audio EQ Cookbook: https://www.w3.org/TR/audio-eq-cookbook/

The test executables `BreachOutputEqTests` and `BreachOutputEqIntegration` check
filter responses, stereo matching, bypass, low-cut rejection through the full
plugin, wet/dry routing, recall and buffer-size-independent automation. The
existing UI smoke test exercises the graph drag and selected-band inspector.
Implementation and platform test results are included in `Docs/Stage8`.
