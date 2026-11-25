# BarryStroke

A fast, standalone auto-stroke OpenFX plugin for DaVinci Resolve.
Drop it onto any transparent PNG, graphic, or alpha node and it automatically outlines the visible boundaries — no shapes to draw, no points to edit.

## Features

- **Automatic Alpha Edge Detection** — instantly outlines any transparent image or alpha-enabled node
- **Stroke Settings** — Width (%), Position (**Inside / Center / Outside**), Opacity (%), Feather (%), and **Blend Mode** (**Normal, Add, Screen, Multiply, Overlay**)
- **Style** — **Solid** or **Dashed** with integer **Dash count**, **Dash length (%)** (0–100% for write-on effects), and **Dash angle (deg)**
- **Color** — **Solid** or **Gradient** (2-color linear gradient with **Gradient angle**)
- **Glow** — optional outer bloom (**Glow size (%)** and **Glow intensity (%)**) that follows dashed strokes
- **Sharp Corners** — separable Chebyshev distance transform keeps 90° corners crisp with zero per-frame heap allocations

## Install (Windows)

1. Download `BarryStroke_Setup.exe` from the [Releases](../../releases) page.
2. Run `BarryStroke_Setup.exe` (accept the Administrator prompt).
3. Restart DaVinci Resolve — **BarryStroke** appears under **Effects → Open FX → BarryOFXPlugin**.

*(No C++, Python, or Visual Studio required — the C++ runtime is statically linked.)*

## Build from Source

Requires the [OpenFX SDK](https://github.com/ofxa/openfx) and MSVC (x64 Native Tools Command Prompt):

```bat
cmake -S . -B build -DOFX_SDK_DIR="..\openfx"
cmake --build build --config Release
build-install.bat
```

## License

[MIT](LICENSE)
