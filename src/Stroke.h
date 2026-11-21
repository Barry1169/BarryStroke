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

enum BlendMode {
    kBlendNormal = 0,
    kBlendAdd = 1,
    kBlendScreen = 2,
    kBlendMultiply = 3,
    kBlendOverlay = 4
};

static inline void compositeStrokeBlend(float *dp, const float *strokeRGB, float sa, int mode)
{
    if (sa <= 0.0f)
        return;
    if (mode == kBlendNormal) {
        compositeStroke(dp, strokeRGB, sa);
        return;
    }
    float da = dp[3];
    float inv = 1.0f - sa;
    for (int c = 0; c < 3; ++c) {
        float bg = (da > 1e-5f) ? std::min(1.0f, std::max(0.0f, dp[c] / da)) : 0.0f;
        float fg = strokeRGB[c];
        float blended = fg;
        if (mode == kBlendAdd) {
            blended = std::min(1.0f, bg + fg);
        } else if (mode == kBlendScreen) {
            blended = 1.0f - (1.0f - bg) * (1.0f - fg);
        } else if (mode == kBlendMultiply) {
            blended = bg * fg;
        } else if (mode == kBlendOverlay) {
            blended = (bg < 0.5f) ? (2.0f * bg * fg) : (1.0f - 2.0f * (1.0f - bg) * (1.0f - fg));
        }
        // Composite blended over premultiplied background
        float outStraight = bg * (1.0f - sa) + blended * sa;
        float outAlpha = da * inv + sa;
        dp[c] = (da > 1e-5f) ? (dp[c] * inv + blended * sa * da + fg * sa * (1.0f - da))
                             : (fg * sa);
        (void) outStraight;
        (void) outAlpha;
    }
    dp[3] = da * inv + sa;
}

// Shift hue of an RGB color [0..1] by angle in turns [0..1].
static inline void hueShiftRGB(const float *inRGB, float turns, float *outRGB)
{
    float r = inRGB[0], g = inRGB[1], b = inRGB[2];
    float cmax = std::max(r, std::max(g, b));
    float cmin = std::min(r, std::min(g, b));
    float delta = cmax - cmin;
    float h = 0.0f;
    if (delta > 1e-6f) {
        if (cmax == r)      h = std::fmod((g - b) / delta, 6.0f);
        else if (cmax == g) h = (b - r) / delta + 2.0f;
        else                h = (r - g) / delta + 4.0f;
        h /= 6.0f;
        if (h < 0.0f) h += 1.0f;
    }
    float s = (cmax > 1e-6f) ? (delta / cmax) : 0.0f;
    float v = cmax;
    h = std::fmod(h + turns, 1.0f);
    if (h < 0.0f) h += 1.0f;
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r1 = 0, g1 = 0, b1 = 0;
    int hi = (int)(h * 6.0f) % 6;
    if      (hi == 0) { r1 = c; g1 = x; b1 = 0; }
    else if (hi == 1) { r1 = x; g1 = c; b1 = 0; }
    else if (hi == 2) { r1 = 0; g1 = c; b1 = x; }
    else if (hi == 3) { r1 = 0; g1 = x; b1 = c; }
    else if (hi == 4) { r1 = x; g1 = 0; b1 = c; }
    else              { r1 = c; g1 = 0; b1 = x; }
    outRGB[0] = r1 + m;
    outRGB[1] = g1 + m;
    outRGB[2] = b1 + m;
}
