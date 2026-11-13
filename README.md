# BREACH // 貫通 — 和弦共振器音频效果插件 (Chord Resonator)

**BREACH // 貫通** 是一款基于 **JUCE (C++17)** 框架开发的立体声和弦共振器（Harmonic Chord Resonator）音频效果插件，支持 **VST3 / AU / Standalone** 格式，采用高对比极简二次元黑白漫画分镜视觉风格。

## 核心原理

```
立体声输入 ──► [瞬态检测 & 电平表] ──► (干声通路) ──────────────────────────┐
     │                                                                       ▼
     ├─► [包络跟随器] ──┐                                              [等功率干湿混合] ──► 立体声输出
     │                  ▼                                                    ▲
[MIDI / 和弦引擎] ──► [调制矩阵 (2 LFO + Env)]                               │
     │                  │                                                    │
     ▼                  ▼                                                    │
[BPM 步进琶音] ──► [8 声部并行共振器组 (20ms 模式平滑淡化)] ──► [全通扩散涂抹] ──► [Hilbert 移频] ──► [湿声滤波 & 软限幅]
```

输入信号同时激励最多 **8 个并行调谐的立体声共振器**，每个共振器调谐至当前设定音阶、和弦或实时按下的 MIDI 音符音高。即使是无明确音高的打击乐、鼓组、白噪音或环境采样，也能被瞬间“染上”富有动态与空间感的和弦色彩。

## 五种枪械效果模式（Resonator Modes）

底部武器切换栏可一键切换 5 种核心共振物理模型（内置 **20ms 等功率交叉淡化**，实时切换无爆音）：

| 模式代号 | 武器图标 | 共振器类型 | 听感与音色特征 |
| --- | --- | --- | --- |
| **Mode A** | **PISTOL** | 干净带通 (`Clean Bandpass`) | 零延迟反馈 TPT 状态变量高 Q 带通滤波，清脆通透、颗粒感强 |
| **Mode B** | **SHOTGUN** | 梳状弦 (`Comb String`) | 3 阶拉格朗日分数延迟 Karplus-Strong 模型，带有拨弦与金属琴弦质感 |
| **Mode C** | **SMG** | 双峰共振峰 (`Dual Formant`) | 双谐振峰串联/并联结构，模拟人声声道元音共振，赋予素材合唱团般的嗓音色彩 |
| **Mode D** | **SNIPER** | 失真共振 (`Distorted Res`) | 高 Q 共振后接非对称 `tanh` 软削波饱和，具有强劲的谐波过载与穿透力 |
| **Mode E** | **REVOLVER** | 长尾扩散 (`Long-Tail Pad`) | 极长衰减反馈配合多级质数全通扩散网络，生成宏大空灵的氛围和弦尾音 |

## 九大功能模块与参数

1. **Chord Panel（和弦与音高面板）**
   - 支持 **12 个根音**、多种常用音阶（大调、自然小调、和声小调、Dorian、Mixolydian、五声音阶等）与和弦类型（`Maj` / `Min` / `Maj7` / `Min7` / `Dom7` / `Sus2` / `Sus4` / `Add9` / `Dim7` 等）。
   - 支持 **4–8 个声部**同时发声，可跨 **1–3 个八度**分布。
   - **MIDI 实时接管**：开启后，直接通过 MIDI 键盘演奏即可实时决定各共振器的目标音高。
2. **Resonator Bank（共振器组核心）**
   - `DECAY`：控制共振衰减时间与反馈深度（内置自激保护与自动增益补偿）。
   - `DAMPING`：控制高频阻尼衰减速率，模拟真实物理材质的高频吸收。
   - `TONE`：调节共振谐波明亮度与双峰共振峰比例。
3. **Spacing（声场与音高分布）**
   - 控制各声部的跨八度展开比例（`Octave Spread`）、对称微失谐量（`Detune`）与等功率立体声声像宽度（`Stereo Width`）。
4. **Drift（频率漂移与涂抹）**
   - 为每个声部注入独立的缓慢准周期频率偏移，并串联全通扩散器（Allpass Diffuser），使共振边界向外柔和晕开。
5. **Shift（单边带移频器）**
   - 基于宽带 IIR **Hilbert 变换**实现单边带频谱平移（`-500 Hz` 至 `+500 Hz`），改变泛音比例以制造金属/非谐波音色（非传统变调）。
6. **Arp（BPM 同步步进琶音器）**
   - 自动读取宿主（DAW）工程速度，按设定的音符时值（`1/1` 至 `1/32`）、行进图案（上行、下行、折返、随机、和弦脉冲）与门限长度（`Gate`）循环点亮各声部激励门。
7. **Motion（调制矩阵）**
   - 内置 **2 个多波形 LFO**（正弦、三角、锯齿、方波、采样保持）与 **1 个包络跟随器**，可双向路由调制 `Decay`、`Drift`、`Shift` 与滤波截止频率。
8. **Filter（湿声带宽滤波）**
   - 湿信号独立的 `HPF`（高通）与 `LPF`（低通）滤波器，精准修剪低频浑浊与高频毛刺。
9. **Depth（干湿混合）**
   - `0% – 100%` 等功率（Equal-Power）干湿比混合，确保混合过程中响度平滑自然。

## 漫画分镜交互与工程特性

- **实时声画遥测联动**：DSP 引擎通过无锁原子结构以 60Hz 向界面传递输入 RMS 电平、瞬态冲击脉冲与 8 个声部的独立实时能量，驱动中央分镜的子弹震动、速度线拉伸、墙面放射状裂纹扩展与 1 帧黑白反色闪光。
- **黑白反色切换（`INVERT`）**：支持一键切换“白底黑线原稿模式”与“黑底白线暗夜反色模式”。
- **工程级实时安全**：所有参数均托管于 `AudioProcessorValueTreeState`（支持完整 DAW 自动化与工程状态保存），音频线程零内存分配、内置 `ScopedNoDenormals` 防非规格化数处理与输出安全软限幅。

## 安装与编译说明

### 方式一：直接下载 GitHub Actions 云端编译产物

1. 进入本仓库顶部的 **`Actions`** 标签页，打开最新运行成功的 **`Build BREACH Resonator VST3 & AU`** 工作流。
2. 在页面最底部的 **`Artifacts`** 区域下载对应操作系统的压缩包：
   - **Windows**：解压后将 `BREACH Resonator.vst3` 复制到 `C:\Program Files\Common Files\VST3\`
   - **macOS**：解压后将 `BREACH Resonator.vst3` 复制到 `~/Library/Audio/Plug-Ins/VST3/`（AU 格式复制到 `~/Library/Audio/Plug-Ins/Components/`）
3. 打开你的宿主软件（Ableton Live、Cubase、FL Studio、Logic Pro、Reaper 等）重新扫描插件即可使用。

### 方式二：本地 CMake 编译（需 CMake 3.22+ 及 Xcode / Visual Studio 2022）

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
# BREACH // 貫通 — Harmonic Chord Resonator

**BREACH // 貫通** is a stereo Harmonic Chord Resonator audio effect plugin built with **JUCE (C++17)**, supporting **VST3 / AU / Standalone** formats. It features a high-contrast, minimalist monochrome Japanese manga aesthetic with real-time audio-reactive impact visuals.

## Core Architecture & Signal Flow

```
Stereo Input ──► [Transient & RMS Meter] ──► (Dry Path) ─────────────────────────────┐
     │                                                                               ▼
     ├─► [Envelope Follower] ──┐                                         [Equal-Power Dry/Wet] ──► Stereo Out
     │                         ▼                                                     ▲
[MIDI / Chord Engine] ──► [Modulation Matrix (2 LFO + Env)]                          │
     │                         │                                                     │
     ▼                         ▼                                                     │
[BPM Step Arp] ──► [8-Voice Parallel Resonator Bank (20ms Crossfade)] ──► [Allpass Smear] ──► [Hilbert Shift] ──► [Wet Filter & Limiter]
```

The incoming audio simultaneously excites up to **8 parallel tuned stereo resonators**, each pitched to a note within the selected scale, chord voicing, or live MIDI input. Even unpitched material—such as drums, percussion, white noise, or foley—is instantly infused with rich harmonic resonance and spatial depth.

## Five Firearm Resonator Modes

Switch seamlessly between 5 distinct physical/spectral resonator topologies via the bottom firearm dock (engineered with a **20ms equal-power crossfade** to prevent clicks or pops):

| Mode | Weapon Icon | Resonator Topology | Sonic Character |
| --- | --- | --- | --- |
| **Mode A** | **PISTOL** | Clean Bandpass | Zero-delay feedback TPT State-Variable high-Q bandpass filters; crisp, glassy, and articulate |
| **Mode B** | **SHOTGUN** | Comb String | 3rd-order Lagrange fractional-delay Karplus-Strong waveguides; plucked string and metallic textures |
| **Mode C** | **SMG** | Dual Formant | Dual-peak vocal tract formant filters; imparts lush choir-like vowel resonances |
| **Mode D** | **SNIPER** | Distorted Res | High-Q resonators driven into asymmetric `tanh` soft-clipping; aggressive harmonic saturation |
| **Mode E** | **REVOLVER** | Long-Tail Pad | Extended feedback decay routed through multi-stage prime allpass diffusers; vast ambient chord tails |

## Nine Core Modules & Parameters

1. **Chord Panel (Pitch & Voicing)**
   - **12 Root Notes**, multiple scales (Major, Natural Minor, Harmonic Minor, Dorian, Mixolydian, Pentatonic, etc.), and chord types (`Maj`, `Min`, `Maj7`, `Min7`, `Dom7`, `Sus2`, `Sus4`, `Add9`, `Dim7`).
   - **4 to 8 active voices** distributed across **1 to 3 octaves**.
   - **MIDI Pitch Override**: Play notes directly on a MIDI keyboard to dynamically dictate resonator pitches in real time.
2. **Resonator Bank**
   - `DECAY`: Controls resonance tail length and feedback depth (with built-in stability limiting and gain compensation).
   - `DAMPING`: Controls high-frequency energy absorption over time.
   - `TONE`: Shapes harmonic brightness and dual-formant vowel ratios.
3. **Spacing**
   - Adjusts cross-octave distribution (`Octave Spread`), symmetrical micro-detuning (`Detune`), and equal-power stereo panorama (`Stereo Width`).
4. **Drift**
   - Introduces slow, independent quasi-random pitch drift per voice alongside stereo allpass diffusion to smear transient edges.
5. **Shift (Frequency Shifter)**
   - Broadband IIR **Hilbert Transform** single-sideband frequency shifter (`-500 Hz` to `+500 Hz`), shifting spectral partials linearly to create metallic and inharmonic timbres.
6. **Arp (BPM-Synced Step Sequencer)**
   - Syncs to host DAW tempo (`1/1` to `1/32`) with selectable patterns (`Up`, `Down`, `Up-Down`, `Random`, `Chord Pulse`) and `Gate` length to rhythmically excite individual voices.
7. **Motion (Modulation Matrix)**
   - **2 Multi-Waveform LFOs** (Sine, Triangle, Saw, Square, Sample & Hold) + **1 Envelope Follower**, routable to `Decay`, `Drift`, `Shift`, and Filter Cutoff.
8. **Filter**
   - Dedicated wet-signal High-Pass (`HPF`) and Low-Pass (`LPF`) filters to sculpt the resonant bandwidth.
9. **Depth (Dry/Wet Mix)**
   - `0% – 100%` equal-power dry/wet blend for consistent loudness across the entire sweep.

## Manga Visuals & Engineering Highlights

- **60Hz Lock-Free Visual Telemetry**: The DSP engine streams input RMS, transient triggers, and individual per-voice energies to the UI via atomic snapshots—driving bullet vibration, speed lines, radial wall cracks, and 1-frame impact inversions.
- **Polarity Inversion (`INVERT`)**: Toggle instantly between classic black-on-white manga ink and inverted white-on-black night mode.
- **Real-Time Safe C++17**: Full `AudioProcessorValueTreeState` (APVTS) parameter automation, parameter smoothing (`SmoothedValue`), zero heap allocations on the audio thread, `ScopedNoDenormals` protection, and output safety soft-limiting.

## Installation & Build Instructions

### Option 1: Install Pre-Built GitHub Actions Binaries

1. Navigate to the **`Actions`** tab in this repository and open the latest successful **`Build BREACH Resonator VST3 & AU`** workflow run.
2. Download the archive for your OS under **`Artifacts`**:
   - **Windows**: Extract and copy `BREACH Resonator.vst3` into `C:\Program Files\Common Files\VST3\`
   - **macOS**: Extract and copy `BREACH Resonator.vst3` into `~/Library/Audio/Plug-Ins/VST3/` (and `.component` into `~/Library/Audio/Plug-Ins/Components/`)
3. Rescan plugins in your DAW (Ableton Live, Cubase, FL Studio, Logic Pro, Reaper, etc.) and load **BREACH Resonator**.

### Option 2: Local CMake Build (CMake 3.22+ & Xcode / Visual Studio 2022)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
