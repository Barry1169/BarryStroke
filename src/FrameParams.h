// FrameParams.h
// Every plain parameter the render needs, read ONCE per frame into one struct.
// Adding a new effect parameter = add a field here and one line in FrameParams.cpp.
#pragma once
#include "Host.h"
#include "Stroke.h"

struct FrameParams {
    // Stroke
    double strokeW    = 1.0;            // percent of frame height
    double strokeRGB[3] = { 1.0, 1.0, 1.0 };
    double strokeOpacity = 100.0;       // percent
    int    strokePos  = kStrokeCenter;  // maths numbers: 0 Center, 1 Inside, 2 Outside
    double feather    = 0.0;            // percent of frame height (the stroke softness)
};

FrameParams readFrameParams(OfxParamSetHandle paramSet, double time);
