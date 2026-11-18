# Reference UI upgrade

The default editor follows the supplied reference at 1024 x 765 pixels.
Collapsed height is 562 pixels. All source text and UI labels remain ASCII.

## Interface

- Embedded Barlow Condensed typeface (SIL Open Font License in Assets/OFL.txt).
- Embedded SVG impact plate, fractured paper, screen tones and five firearm icons.
- Cached static illustration with audio-driven recoil, debris and output shading.
- Six engraved macro knobs and paired piano-key strips.
- Click a piano key to toggle its existing pitch-class parameter. Repeated keys
  represent the same pitch class. Active notes carry a small contrasting dot.
- Click the letter markers or right-click the keyboard for root, octave, voice
  count, MIDI, arp, motion and PANIC controls. CLOSE returns to the illustration.
- TRIGGER previews the visual impact. It does not generate an audio test signal.
- A / VOICES opens four large linked track/knob controls. 12 VOICES opens the
  original per-voice level, pan, detune and decay editor; MACROS returns.
- B, C and D retain the existing resonator, modulation matrix and global controls.
- INVERT updates illustrations, controls, menus and editable value boxes.

## Voice macros

These controls are UI operations on existing parameters, not new DSP parameters.

| Control | Existing target | Behavior |
| --- | --- | --- |
| DETUNE | vDetune_0..11 | Shift mean tuning while preserving offsets, limited to +/-50 cents. |
| SPREAD | vPan_0..11 | Set alternating left/right spread, increasing across voices. |
| HUMAN | vDecay_0..11 | Apply repeatable variation around a decay multiplier of 1. |
| COLOR | tone | Same parameter as the main TONE knob. |

Per-voice overrides remain available in the detail editor. Macro readouts show
mean detune, maximum absolute pan and maximum decay deviation respectively.
Drag gestures notify the host for each affected parameter. Host automation and
state restoration update the controls without generating parameter writes.

## Compatibility and build

The initial UI-only delivery kept PluginProcessor.cpp and PluginProcessor.h
byte-identical to the supplied baseline. The subsequent spatial DSP stage is
documented in SPATIAL_UPGRADE.md, including its engine-version compatibility
behavior. Parameter IDs, order, ranges, defaults, version hints, state tree name
and plugin codes remain unchanged. No artwork or font files
are required beside the installed plugin; CMake embeds them through BinaryData.

Build using the existing CMake/GitHub Actions workflow. Include the new Assets
folder, MangaControls.h and updated CMakeLists.txt when copying this upgrade.

Optional UI/session smoke test:

```sh
cmake -S . -B build -DBREACH_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target BreachUiSmoke --config Debug
./build/BreachUiSmoke /tmp/breach-ui-snapshots
```

On Linux the test requires an available display and the normal JUCE development
dependencies. On multi-configuration generators the executable is in Debug/.
The test checks visible bounds in all drawer views, both themes, view-only state
preservation, detune offset preservation and serialized state restoration. It
also exports actual JUCE component snapshots for visual review.
