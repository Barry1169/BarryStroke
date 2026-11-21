# BarryStroke

A stroke-only outline effect for DaVinci Resolve (OpenFX image effect).
It draws a stroke around a polygon outline over the source image.

Ported from [BarryShape](https://github.com/Barry1169/BarryShape): only the
stroke functionality and its Inspector UI, plus the minimal outline
infrastructure the stroke needs (points, presets, overlay editing,
corner radius, transform). Fill, gradients, morphing and shape keyframes
are intentionally not included.

## Inspector

- **Shape** — pick a preset outline (Triangle, Rectangle, Circle, Star,
  Heart, Arrow) or Custom, then edit points in the viewer.
- **Stroke** — on/off, width (%), position (Inside / Center / Outside),
  color, opacity (%).
- **Corner radius** — rounds the outline the stroke follows.
- **Transform** — Feather (also the stroke softness), Offset, Scale, Rotation.
- **Points** — the outline points (advanced), Auto Key.

A new effect starts as a Triangle outline with the stroke off. Turn
**Stroke** on to see the stroke.

## Build

Requires the OpenFX SDK and MSVC (x64 Native Tools prompt):

```
cmake -S . -B build -DOFX_SDK_DIR="..\openfx"
cmake --build build --config Release
```

Or run `build-install.bat` (adjust `PROJ` to your checkout path) to build,
stage into `bundle\BarryStroke.ofx.bundle`, and install into
`C:\Program Files\Common Files\OFX\Plugins\`.

## Tests

Standalone checks of the pure maths (no OpenFX, no Resolve):

```
cd tests
cl /EHsc /std:c++17 StrokeTest.cpp /Fe:StrokeTest.exe
cl /EHsc /std:c++17 MaskRowTest.cpp /Fe:MaskRowTest.exe
cl /EHsc /std:c++17 ShapeTemplatesTest.cpp /Fe:ShapeTemplatesTest.exe
cl /EHsc /std:c++17 SmoothingTest.cpp /Fe:SmoothingTest.exe
```

## Layout

```
src/Plugin.cpp      plugin description + action routing
src/Host.*          host suites + logger
src/Inspector.*     parameters + show/hide rules
src/Overlay.*       viewer overlay (draw, click, drag points)
src/ShapeData.*     outline points, order, presets
src/FrameParams.*   per-frame parameter snapshot
src/Render.cpp      pixel loop (stroke over source)
src/Stroke.h        stroke band / alpha / blend maths
src/MaskRow.h       per-row edge distance maths
src/Geometry.h      polygon helpers
src/Rounding.h      corner rounding
src/ShapeTemplates.h preset outlines
```
