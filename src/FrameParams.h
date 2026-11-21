// FrameParams.h
// Every plain parameter the render needs, read ONCE per frame into one struct.
// Adding a new effect parameter = add a field here and one line in FrameParams.cpp.
#pragma once
#include "Host.h"
#include "Stroke.h"

struct FrameParams {
    // Stroke
    int    strokeOn   = 0;              // Stroke checkbox: off = no stroke
    double strokeW    = 0.0;            // percent of frame height
    double strokeRGB[3] = { 1.0, 1.0, 1.0 };
    double strokeOpacity = 100.0;       // percent
    int    strokePos  = kStrokeCenter;  // maths numbers: 0 Center, 1 Inside, 2 Outside

    // Transform
    double feather    = 0.0;            // percent of frame height (also the stroke softness)
    double offX       = 0.0;            // percent of frame width
    double offY       = 0.0;            // percent of frame height
    double scalePct   = 100.0;
    double rotDeg     = 0.0;
    double cornerPct  = 0.0;            // percent of frame height
    double cornerSmooth = 0.0;          // percent: 0 = circular arcs, 100 = the smoothest corner
};

FrameParams readFrameParams(OfxParamSetHandle paramSet, double time);
