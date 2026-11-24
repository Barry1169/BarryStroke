// FrameParams.h
// Every plain parameter the render needs, read ONCE per frame into one struct.
#pragma once
#include "Host.h"
#include "Stroke.h"

struct FrameParams {
    // Stroke
    double strokeW       = 1.0;            // percent of frame height
    int    strokePos     = kStrokeCenter;  // maths numbers: 0 Center, 1 Inside, 2 Outside
    double strokeOpacity = 100.0;          // percent
    double feather       = 0.0;            // percent of frame height (the stroke softness)
    int    blendMode     = kBlendNormal;   // 0 Normal, 1 Add, 2 Screen, 3 Multiply, 4 Overlay
    double roundness     = 0.0;            // 0 = Sharp (Chebyshev), 100 = Round (Euclidean)

    // Style (Solid / Dashed)
    int    strokeStyle   = 0;              // 0 Solid, 1 Dashed
    int    dashCount     = 24;             // number of dashes around the subject
    double dashRatio     = 50.0;           // duty cycle percent (0..100)
    double dashAngle     = 0.0;            // rotation angle in degrees

    // Color (Solid / Gradient)
    int    colorMode     = 0;              // 0 Solid, 1 Gradient
    double strokeRGB[3]  = { 1.0, 1.0, 1.0 };
    double strokeRGB2[3] = { 0.0, 0.6, 1.0 };
    double gradAngle     = 0.0;            // degrees

    // Glow
    int    glowOn        = 0;
    double glowSize      = 2.0;            // percent of frame height
    double glowIntensity = 60.0;           // percent
};

FrameParams readFrameParams(OfxParamSetHandle paramSet, double time);
