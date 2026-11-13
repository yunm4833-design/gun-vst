# BREACH // Chord Resonator — VST3 / AU Build Guide

## 1. Local Build (macOS / Windows)
Ensure you have **CMake (>= 3.22)** and a C++17 compiler (**Xcode** on macOS, or **Visual Studio 2022** on Windows) installed. CMake will automatically fetch JUCE 8.0.4:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

- **Windows VST3 output**: `build/BreachResonator_artefacts/Release/VST3/BREACH Resonator.vst3`
  - Copy into `C:\Program Files\Common Files\VST3\`
- **macOS VST3 & AU output**:
  - `build/BreachResonator_artefacts/Release/VST3/BREACH Resonator.vst3` -> `~/Library/Audio/Plug-Ins/VST3/`
  - `build/BreachResonator_artefacts/Release/AU/BREACH Resonator.component` -> `~/Library/Audio/Plug-Ins/Components/`

## 2. Optional Legacy VST2 (`.dll` / `.vst`)
Steinberg discontinued the VST2 SDK in 2018. If you own a copy of the legacy VST2 SDK, pass its path during configuration:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVST2_SDK_PATH=/path/to/vstsdk2.4
cmake --build build --config Release --parallel
```

## 3. Zero-Setup Cloud Build (GitHub Actions)
Push this repository to GitHub and open the **Actions** tab to download pre-compiled Windows `.vst3` and macOS `.vst3` / `.component` binaries automatically.
