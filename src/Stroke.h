// Stroke.h
// Pure maths for drawing a stroke around a shape outline. No OpenFX types, so a
// standalone test program can include it. The numbers match BarryShape's stroke:
// the stroke is a band centred on the outline (or pushed inside / outside it),
// with a soft edge whose width is the Feather (or one pixel, whichever is larger).
#pragma once
#include <algorithm>
#include <cmath>

// Stroke position, maths numbers (the Inspector order is Inside, Center, Outside;
// FrameParams maps it to these numbers).
enum StrokePos { kStrokeCenter = 0, kStrokeInside = 1, kStrokeOutside = 2 };

// Middle of the stroke band, as a signed distance from the outline edge.
// 0 = on the edge, +w/2 = inside, -w/2 = outside.
static inline double strokeCentre(double strokeW, int strokePos)
{
    return (strokePos == kStrokeInside)  ?  strokeW * 0.5
         : (strokePos == kStrokeOutside) ? -strokeW * 0.5
         : 0.0;
}

// How far from the outline the distance maths must reach, so the whole stroke
// (plus half its softness) is covered. 0 = no stroke (width 0 or no shape).
static inline double strokeBand(double strokeW, int strokePos, double strokeSoft, bool hasShape)
{
    if (strokeW <= 0.0 || !hasShape)
        return 0.0;
    return (strokePos == kStrokeCenter ? strokeW * 0.5 : strokeW) + strokeSoft * 0.5;
}

// Stroke coverage 0..1 for one pixel.
//   sdist = signed distance to the outline: positive inside, negative outside.
//   strokeW / strokeSoft are in the same units as sdist. opacity is 0..1.
static inline float strokeAlpha(double sdist, double strokeW, int strokePos,
                                double strokeSoft, float opacity)
{
    if (strokeW <= 0.0 || strokeSoft <= 0.0)
        return 0.0f;
    double centre = strokeCentre(strokeW, strokePos);
    double a = 0.5 + (strokeW * 0.5 - std::fabs(sdist - centre)) / strokeSoft;
    a = std::max(0.0, std::min(1.0, a));
    return (float) (a * (double) opacity);
}

// Blend the stroke colour over one premultiplied RGBA pixel (4 floats).
// strokeRGB is straight (not premultiplied), 3 floats. sa is 0..1.
static inline void compositeStroke(float *dp, const float *strokeRGB, float sa)
{
    if (sa <= 0.0f)
        return;
    float inv = 1.0f - sa;
    dp[0] = dp[0] * inv + strokeRGB[0] * sa;
    dp[1] = dp[1] * inv + strokeRGB[1] * sa;
    dp[2] = dp[2] * inv + strokeRGB[2] * sa;
    dp[3] = dp[3] * inv + sa;
}
