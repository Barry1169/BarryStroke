// Host.h
// The host's function tables (suites) and the debug logger. Shared by every .cpp file.
#pragma once
#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxDrawSuite.h"

extern OfxHost               *gHost;        // stored in setHost
extern OfxImageEffectSuiteV1 *gEffectHost;  // fetched in Load
extern OfxPropertySuiteV1    *gPropHost;
extern OfxParameterSuiteV1   *gParamHost;
extern OfxDrawSuiteV1        *gDrawHost;    // optional suite

// Maximum number of points. One keyframeable parameter pair per point.
static const int kMaxPts = 16;

// Debug log. Off unless OFX_DEBUG_LOG is 1 (see Host.cpp).
void logLine(const char *text);
