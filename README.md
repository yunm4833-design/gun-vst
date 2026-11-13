**English** | [简体中文](./README_CN.md)


# BREACH // 貫通 — Polyphonic Chord Resonator (v1.1.1)

**BREACH // 貫通** is a stereo polyphonic chord resonator audio effect plugin built with **JUCE 8.0.4 (C++17)**, supporting **VST3**, **AU**, and **Standalone** formats. Designed around a high-contrast Japanese manga split-panel aesthetic, it transforms incoming transients, drums, synths, and guitars into pitched harmonic chords, metallic waveguides, and kinetic textures.
English Manual：
https://drive.google.com/file/d/1rrfJUIrgdXrz3JxT974NPWXPIvj5ykrY/view?usp=sharing
---

## Key Features (Stage 2 / v1.1.1)

- **12-Voice Polyphonic Resonator Engine**: Supports 4, 6, 8, or 12 simultaneous resonant voices with a 12-semitone pitch-class selector, 10 built-in chord presets (`Major/Minor Triad`, `Maj7`, `Min7`, `Dom7`, `Sus2`, `Sus4`, `Dim7`, `Aug`, `Pentatonic`), octave spread (`1–4 Octaves`), and real-time **MIDI Input Mode**.
- **5 Firearm Resonator Topologies** *(with 20 ms equal-power zero-click crossfading)*:
  - **PISTOL (`TRANSIENT`)**: Zero-delay feedback TPT State-Variable Bandpass filter bank for tight, punchy pitched percussion.
  - **SHOTGUN (`SCATTER`)**: Fractional-delay waveguide comb filters with 4-point Lagrange interpolation and allpass string dispersion.
  - **SMG (`BURST`)**: Coupled dual-formant SVF resonators for vocal-like and aggressive metallic bursts.
  - **SNIPER (`PIERCE`)**: High-Q resonant SVF driven into asymmetric `tanh` saturation for piercing harmonic leads.
  - **REVOLVER (`CRUSH`)**: High-Q SVF cascaded into dual Schroeder allpass diffusers for lush, sustained ambient pads.
- **Kinetic Arpeggiator & Motion Engine**: Host-BPM-synced or free-running resonator voice arpeggiator (`Up`, `Down`, `Up-Down`, `Random`, `Simultaneous`) paired with a multi-phase LFO motion modulator.
- **Fold-Out `ADVANCED` 4-Tab Drawer (115 Automatable Parameters)**:
  - **Tab A // VOICES**: Global voice cap (`4 / 6 / 8 / 12`) plus independent per-voice **Level**, **Pan**, **Detune (`±50 ct`)**, and **Decay Multiplier (`0.25x–2.0x`)**.
  - **Tab B // RESONATOR**: Resonator topology override (`Auto`, `Bandpass SVF`, `Comb String`, `Dual Formant`), **Attack (`0.5–500 ms`)**, **Feedback Damping**, **Stiffness** (inharmonic partial dispersion), **Excitation HPF**, and **Excitation Drive**.
  - **Tab C // MODULATION**: Dual independent LFOs (`LFO 1` & `LFO 2` with BPM sync & phase offset), **Envelope Follower**, **8-Stage Hilbert Frequency Shifter (`±500 Hz`)**, **Stereo Unison**, and a **3 × 6 Modulation Matrix** (`LFO 1 / LFO 2 / Env` → `Warp / Drift / Shift / Q / Tilt / Depth`).
  - **Tab D // GLOBAL**: Input/Output Gain (`±24 dB`), Stereo Width (`0–200%`), Dry/Wet Crossfade Curves (`Equal Power`, `Linear`, `S-Curve`), **1x / 2x / 4x Polyphase IIR Oversampling**, Output **Soft Limiter**, and configurable **Parameter Smoothing (`2–100 ms`)**.
- **Reactive Manga Split-Panel UI**: Lock-free 60 FPS audio-to-UI telemetry driving transient flash inversion, spring-physics bullet impact animations, dynamic katakana SFX, radial voice fracture lines, debris particles, and an **INVERT** light/dark ink theme toggle.

---

## Signal Flow

```text
Stereo Input ─► Input Gain ─► [Transient Detector & Envelope Follower]
                   │
                   ├─► (Dry Path) ───────────────────────────────────────────┐
                   │                                                         ▼
                   └─► Excitation HPF & Drive ─► 12-Voice Resonator Bank ─► Dry/Wet Mix
                                                      │                      │
                                  ┌───────────────────┴───────────────────┐  │
                                  ▼                                       ▼  ▼
                        Allpass Drift Smear ─► Hilbert Shifter ─► Tilt EQ & Bandpass ─► Stereo Width ─► Soft Limiter ─► Output
```

---

## Download & Build

### Pre-built Binaries (GitHub Actions)
1. Go to the [**Actions**](https://github.com/yunm4833-design/gun-vst/actions) tab of this repository.
2. Select the latest completed workflow run (`1.1.1`).
3. Download the artifact for your OS under **Artifacts**:
   - `BreachResonator-macOS-VST3-AU` (Universal `arm64` + `x86_64` `.vst3` & `.component`)
   - `BreachResonator-Windows-VST3` (Windows x64 `.vst3`)
  - **macOS**:
  - **VST3**: `/Library/Audio/Plug-Ins/VST3/BREACH Resonator.vst3`
  - **AU (Logic Pro)**: `/Library/Audio/Plug-Ins/Components/BREACH Resonator.component`
  - *Note for unsigned builds on macOS*: Run `sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/BREACH Resonator.vst3"` (and the `.component` path) in Terminal if macOS Gatekeeper blocks loading.
- **Windows**:
  - **VST3**: `C:\Program Files\Common Files\VST3\BREACH Resonator.vst3`
### Build Locally from Source
Requires **CMake 3.22+** and a C++17 compiler (Xcode on macOS or Visual Studio 2022 on Windows). JUCE `8.0.4` is fetched automatically via CMake `FetchContent`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

*(Optional)* To also build VST2, pass `-DVST2_SDK_PATH="/path/to/vst2sdk"` during configuration.
