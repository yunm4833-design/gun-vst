[English](./README.md) | **简体中文**

# BREACH // 貫通 — 多复音和弦共振器插件 (v1.1.1)

**BREACH // 貫通** 是一款基于 **JUCE 8.0.4 (C++17)** 开发的立体声多复音和弦共振器（Polyphonic Chord Resonator）音频效果插件，支持 **VST3**、**AU** 与 **Standalone** 格式。插件采用高对比度黑白日漫分镜视觉风格，可将鼓组瞬态、合成器、吉他及各类音频信号实时转化为带有和声色彩的共振弦音、金属波导与动态铺底质感。
说明书：
https://drive.google.com/file/d/1wn8hUrbyw5LvqqEyvzh2DUZ687c5WVTc/view?usp=sharing
---

## 核心特性（Stage 2 / v1.1.1）

- **12 复音和弦共振引擎**：支持 `4 / 6 / 8 / 12` 复音上限切换，内置 12 半音音级开关、10 种常用和弦预设（大小三和弦、大小七和弦、属七、挂二、挂四、减七、增三、五声音阶）、`1–4` 八度跨度扩展，以及 **MIDI 实时演奏模式**。
- **5 种枪械谐振拓扑** *（切换时自带 20ms 等功率零爆音交叉淡入）*：
  - **PISTOL（`TRANSIENT` 瞬态）**：零延迟反馈 TPT 状态变量带通滤波器组，适合紧致、颗粒感强的音高打击乐。
  - **SHOTGUN（`SCATTER` 散射）**：采用四点拉格朗日分数延迟插值与全通色散滤波器的波导梳状弦振模型。
  - **SMG（`BURST` 连射）**：双共振峰耦合 SVF 滤波器组，呈现类似人声共振峰与金属爆发质感。
  - **SNIPER（`PIERCE` 贯通）**：高 Q 值谐振搭配非对称 `tanh` 饱和削波，产生极具穿透力的谐波泛音。
  - **REVOLVER（`CRUSH` 轰鸣）**：高 Q 值谐振级联双 Schroeder 全通扩散器，打造绵长饱满的氛围铺底。
- **动态琶音器与多相调制**：支持宿主 BPM 速度同步或自由频率的共振声部琶音器（`Up`、`Down`、`Up-Down`、`Random`、`Simultaneous`），配合多相位 LFO 调制引擎。
- **底部折叠式 `ADVANCED` 四标签页高级面板（共 115 个可自动化参数）**：
  - **Tab A // VOICES（声部独立控制）**：全局复音上限（`4 / 6 / 8 / 12`），以及全部 12 个声部各自的独立 **音量（Level）**、**声像（Pan）**、**微调音分（Detune ±50 ct）** 与 **衰减倍率（Decay Mul 0.25x–2.0x）**。
  - **Tab B // RESONATOR（物理建模与激励）**：谐振物理模型强制覆盖（`自动跟随枪械模式` / `Bandpass SVF` / `Comb String` / `Dual Formant`）、**起音时间（Attack 0.5–500 ms）**、**反馈高频阻尼（FB Damping）**、**弦刚度非谐波色散（Stiffness）**、**激励信号高通滤波（Exc HPF）** 与 **激励饱和驱动（Exc Drive）**。
  - **Tab C // MODULATION（调制中心与矩阵）**：双独立 LFO（`LFO 1` & `LFO 2`，支持 5 种波形、BPM 同步及 `0–360°` 相位偏移）、**输入包络跟随器（Envelope Follower）**、**8 阶 Hilbert 移频器（`±500 Hz`）**、**立体声齐奏（Unison）**，以及 **3 × 6 自由调制矩阵**（`LFO 1 / LFO 2 / Env` → `Warp / Drift / Shift / Q / Tilt / Depth`）。
  - **Tab D // GLOBAL（全局母线与引擎设置）**：输入/输出增益（`±24 dB`）、立体声宽度（`0–200%`）、干湿比混合曲线（`Equal Power` / `Linear` / `S-Curve`）、**1x / 2x / 4x 多相 IIR 过采样**、输出端 **软限幅器（Soft Limiter）** 与全局 **参数平滑时间（2–100 ms）**。
- **实时音频驱动漫画分镜 UI**：基于无锁遥测（Lock-Free Telemetry）的 60 FPS 画面响应——瞬态触发全屏黑白反色闪烁、弹簧物理子弹冲击位移、动态拟声词特效、随声部能量与琶音步进延伸的放射状裂纹、飞溅碎石粒子，以及一键 **INVERT** 黑白主题切换。

---

## 信号处理链路

```text
立体声输入 ─► 输入增益 ─► [瞬态检测 & 包络跟随器]
               │
               ├─► (干声通路) ───────────────────────────────────────────────────────┐
               │                                                                     ▼
               └─► 激励高通 & 饱和驱动 ─► 12 声部和弦共振器组 ──────────────────────► 干湿比混合
                                              │                                      │
                              ┌───────────────┴───────────────┐                      │
                              ▼                               ▼                      ▼
                    全通立体声扩散 ─► Hilbert 移频器 ─► 倾斜均衡 & 带通滤波 ─► 立体声宽度 ─► 软限幅 ─► 输出
```

---

## 获取与编译安装

### 方式一：直接下载 GitHub Actions 云端编译成品
1. 打开本仓库顶部的 [**Actions**](https://github.com/yunm4833-design/gun-vst/actions) 页面。
2. 点击最新一条已完成（绿色对勾）的 `1.1.1` 工作流记录。
3. 滑到页面底部的 **Artifacts** 区域下载对应系统的安装包：
   - `BreachResonator-macOS-VST3-AU`（包含 Apple Silicon + Intel 通用版 `.vst3` 与 `.component`）
   - `BreachResonator-Windows-VST3`（包含 Windows 64 位 `.vst3`）
- **macOS**:
  - **VST3**: `/Library/Audio/Plug-Ins/VST3/BREACH Resonator.vst3`
  - **AU (Logic Pro)**: `/Library/Audio/Plug-Ins/Components/BREACH Resonator.component`
  - *Note for unsigned builds on macOS*: Run `sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/BREACH Resonator.vst3"` (and the `.component` path) in Terminal if macOS Gatekeeper blocks loading.
- **Windows**:
  - **VST3**: `C:\Program Files\Common Files\VST3\BREACH Resonator.vst3`
### 方式二：本地源码编译
本地需安装 **CMake 3.22+** 及 C++17 编译器（macOS 需安装 Xcode，Windows 需安装 Visual Studio 2022），CMake 会自动拉取 JUCE `8.0.4` 框架源码进行编译：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

*(可选)* 如需同时编译 VST2 格式，可在配置阶段追加参数：`-DVST2_SDK_PATH="/你的/vst2sdk/路径"`。
