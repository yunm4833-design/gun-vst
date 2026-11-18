# Voices, modulation routes and ring time

## Voice controls

A // VOICES has two controls. TUNE SPREAD adds up to 20 cents of relative tuning
between voices. The lowest voice retains its individual tuning, and a single
voice retains its pitch. PAN SPREAD adds alternating stereo offsets. At zero,
each macro adds zero offset. These controls have their own host automation and
do not overwrite individual voice settings.

VOICE EDIT selects V1 through V12 and exposes LEVEL, PAN, TUNE and RING TRIM for
that slot. Those controls use the same per-voice parameters as earlier builds.
TONE on the main panel supplies the sound shaping previously duplicated by COLOR.

## Modulation

C // MODULATION lists the routes with a nonzero amount. Select a row to adjust
its signed AMOUNT. SOURCE and DESTINATION choose a route to add. ADD starts a
new route at +25% and selects an existing route without writing its amount.
CLEAR sets the selected route to zero. The two LFOs, envelope controls and all
18 original route parameters remain available. Route automation updates the list.

## Ring time

RING TIME on the main panel is the existing `decay` parameter. The new mapping
converts the knob position to a logarithmically spaced nominal decay time in
seconds for the active topology. RING TRIM multiplies this physical time per
voice. The readout is a nominal design value before voice trim, damping, and
modulation. Reverb DECAY controls only the separate post-resonator reverb.

| Profile | Nominal base range |
| --- | ---: |
| PISTOL | 25 ms to 1.55 s |
| SHOTGUN | 60 ms to 4 s |
| SMG | 25 ms to 1.45 s |
| SNIPER | 24 ms to 1.8 s |
| REVOLVER | 70 ms to 3.8 s |

The banks receive the trimmed time in seconds. Modal and pitched filter poles
use that time to set their loss. The waveguide uses the period and decay target
to set feedback loss. STK formant bandwidth follows the duration while preserving
its four vowel bands. Legacy topology overrides target decay through their own
feedback or SVF damping. Actual tails depend on the input and on the model's
material damping, pitch, and modulation. Physical per-voice targets are bounded
to 6 ms through 10 s. Manual topology selection uses its corresponding range.

Peak-normalized SMG and SNIPER paths use a bounded gain correction to retain
short-strike energy across the range. A small fixed strike correction further
reduces level changes as ring time varies. The correction depends on the knob
and voice trim values. Sustained excitation can still get louder at long ring
times as resonant energy accumulates.

## Session compatibility

New instances use physical ring time. Older saved states keep their original
decay mapping. B // RESONATOR switches between RING: NEW and RING: LEGACY.
Use this switch with playback stopped because changing the decay model resets
resonator states. The old state, parameter IDs, ranges, defaults, order, and
host automation remain valid. `voiceTuneSpread` and `voicePanSpread` are appended
at version hint 3 so they follow the EQ and earlier parameters in AUv2.

`BreachRingTimeIntegration` measures a 20 ms pitched excitation at 20%, 50% and
80% with 1x and 4x processing. Measurements use a T20 estimate extrapolated to
60 dB and the peak in the first 30 ms. The test exercises upper-range behavior
with 2x voice trim, automation across block sizes, and state recall.
`Stage8SessionRecall` uses pre-edit full-processor fixtures for older projects.
`BreachUiSmoke` checks control navigation, voices, route add/edit/clear, and
state preservation in both themes. Results are in Docs/Stage9/VALIDATION.txt.
