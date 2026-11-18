# BREACH // RESONATOR Build Guide

Use CMake 3.22 or newer, a C++17 compiler, and an internet connection for the initial JUCE 8.0.4 FetchContent download. Use Xcode on macOS or Visual Studio with the Desktop development with C++ workload on Windows.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The project configures universal `arm64;x86_64` binaries on macOS. Under `build/BreachResonator_artefacts/Release/`, look in `VST3/` for `BREACH Resonator.vst3`, `AU/` for the macOS `.component`, and `Standalone/` for the app or EXE.

The GitHub Actions workflow builds Windows and macOS packages on pushes to the default branch and by manual dispatch. Each platform artifact contains a ready-to-download ZIP. See [Releases](https://github.com/yunm4833-design/gun-vst/releases) for published versions and installation instructions.

## Local checks

```sh
cmake -S . -B build-tests -DBREACH_BUILD_DSP_TESTS=ON -DBREACH_BUILD_UI_TESTS=ON
cmake --build build-tests --config Release --parallel
ctest --test-dir build-tests -C Release --output-on-failure
```

Run the built `BreachUiSmoke` executable with a screenshot output directory to check the editor and session recall. The executable is under `build-tests/` for single-config builds or under `build-tests/Release/` for multi-config builds.

## Optional VST2

If you already have the legacy Steinberg VST2 SDK, pass `-DVST2_SDK_PATH=/path/to/vst2sdk` at configuration time.
