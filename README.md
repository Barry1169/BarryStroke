# BarryStroke

A standalone stroke effect for DaVinci Resolve (OpenFX image effect).
It draws a stroke around the input's visible boundaries automatically —
no shapes to create, no points to edit.

The stroke follows the input's alpha edge (alpha > 50% counts as visible),
so it works on transparent PNGs, graphics, and any node with alpha.
Inputs with no alpha edge (e.g. fully opaque video) have no visible
boundary, so no stroke is drawn.

Stroke maths ported from [BarryShape](https://github.com/Barry1169/BarryShape).

## Inspector

- **Stroke** — on/off (on by default, so the stroke appears immediately)
- **Stroke width (%)** — percent of frame height
- **Stroke position** — Inside / Center / Outside
- **Stroke color**
- **Stroke opacity (%)**
- **Feather (%)** — softness of the stroke edge

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
cl /EHsc /std:c++17 AlphaDistTest.cpp /Fe:AlphaDistTest.exe
```

## Layout

```
src/Plugin.cpp      plugin description + action routing
src/Host.*          host suites + logger
src/Inspector.*     parameters + show/hide rules
src/FrameParams.*   per-frame parameter snapshot
src/Render.cpp      pixel loop (stroke over source)
src/Stroke.h        stroke band / alpha / blend maths
src/AlphaDist.h     distance to the alpha edge
```
