// Plugin.cpp
// BarryStroke: a stroke-only outline effect for DaVinci Resolve (OpenFX image effect).
// It draws a stroke around a polygon outline over the source image.
// This file only describes the plugin and sends each action to the right place.
//   Host.*        host pointers and the logger
//   Inspector.*   parameters, and showing / hiding them
//   Overlay.*     the viewer overlay (draw, click, drag)
//   ShapeData.*   reading and writing the points, order and presets
//   FrameParams.* every plain parameter, read once per frame
//   Render.*      the pixel loop
//   Stroke.h MaskRow.h Geometry.h Rounding.h   pure maths, no OpenFX types
#include <cstdio>
#include <cstring>
#include <vector>
#include "Host.h"
#include "Inspector.h"
#include "Overlay.h"
#include "Render.h"
#include "ShapeData.h"
#include "ShapeTemplates.h"

// Called by the host first, before anything else. It may only store the
// pointer; the header forbids calling any OFX function here.
static void setHostFunc(OfxHost *hostStruct)
{
    gHost = hostStruct;
}

// Load: fetch the suites we need from the host.
static OfxStatus onLoad(void)
{
    if (!gHost)
        return kOfxStatErrMissingHostFeature;

    gEffectHost = (OfxImageEffectSuiteV1 *) gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    gPropHost   = (OfxPropertySuiteV1 *)    gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    gParamHost  = (OfxParameterSuiteV1 *)   gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);

    // The draw suite is optional, so it is not part of the checks below.
    gDrawHost = (OfxDrawSuiteV1 *) gHost->fetchSuite(gHost->host, kOfxDrawSuite, 1);
    logLine(gDrawHost ? "draw suite: available" : "draw suite: NOT available");

    if (!gEffectHost || !gPropHost || !gParamHost)
        return kOfxStatErrMissingHostFeature;

    return kOfxStatOK;
}

// Describe stage 1: what the plugin is. Runs once.
static OfxStatus describe(OfxImageEffectHandle effect)
{
    OfxPropertySetHandle effectProps;
    gEffectHost->getPropertySet(effect, &effectProps);

    // Name and category the host shows to the user.
    gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "BarryStroke");
    gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "BarryOFXPlugin");

    // A filter: one source clip in, one output clip out.
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

    // Declare three depths. The host picks one; input and output share it.
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
    gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthByte);

    gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);

    // Ask the host to show an overlay. V2 means we draw with the DrawSuite.
    gPropHost->propSetPointer(effectProps, kOfxImageEffectPluginPropOverlayInteractV2, 0, (void *) overlayMainV2);

    return kOfxStatOK;
}

// A parameter changed: the Shape dropdown, the Clear button, and the Stroke checkbox.
static OfxStatus onInstanceChanged(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs)
{
    char *changed = nullptr;
    gPropHost->propGetString(inArgs, kOfxPropName, 0, &changed);
    logLine(changed ? changed : "(instance changed: no name)");
    if (!changed)
        return kOfxStatReplyDefault;

    OfxParamSetHandle ps = nullptr;
    gEffectHost->getParamSet(effect, &ps);

    // Shape dropdown: 0 = Custom (do nothing), 1..6 = Triangle, Rectangle, Circle, Star, Heart, Arrow.
    // A preset is loaded only when the dropdown really moves to a DIFFERENT shape than the last one loaded.
    if (strcmp(changed, "shapeKind") == 0) {
        OfxParamHandle h = nullptr, dh = nullptr;
        int kind = 0, done = 0;
        if (gParamHost->paramGetHandle(ps, "shapeKind", &h, nullptr) == kOfxStatOK && h)
            gParamHost->paramGetValue(h, &kind);
        if (gParamHost->paramGetHandle(ps, "shapeDone", &dh, nullptr) == kOfxStatOK && dh)
            gParamHost->paramGetValue(dh, &done);
        char kl[96];
        sprintf_s(kl, sizeof(kl), "shapeKind event: kind %d, last loaded %d", kind, done);
        logLine(kl);

        if (kind == 0) {
            if (done != 0 && dh)
                gParamHost->paramSetValue(dh, 0);
            return kOfxStatOK;
        }
        if (kind >= 1 && kind <= kTemplateCount && kind != done) {
            applyShapeTemplate(effect, inArgs, kind - 1);
        }
        return kOfxStatOK;
    }

    // Stroke checkbox: turning Stroke on with width 0 would show nothing: give it a small starting width.
    if (strcmp(changed, "strokeOn") == 0) {
        OfxParamHandle on = nullptr, w = nullptr;
        int isOn = 0;
        double width = 0.0;
        if (gParamHost->paramGetHandle(ps, "strokeOn", &on, nullptr) == kOfxStatOK && on)
            gParamHost->paramGetValue(on, &isOn);
        if (gParamHost->paramGetHandle(ps, "strokeWidth", &w, nullptr) == kOfxStatOK && w)
            gParamHost->paramGetValue(w, &width);
        if (isOn && w && width <= 0.0)
            gParamHost->paramSetValue(w, 0.4);
        std::vector<OfxPointD> vpts;
        std::vector<int> vslots;
        readSlotPoints(ps, 0.0, vpts, &vslots);
        gVisSig.clear();
        applyVisibility(ps, vslots);
        return kOfxStatOK;
    }

    if (strcmp(changed, "clearShape") == 0)     { writeOrderOnly(effect, ""); return kOfxStatOK; }
    return kOfxStatReplyDefault;
}

// The single entry point: the host sends every action through here.
static OfxStatus pluginMain(const char *action, const void *handle,
                            OfxPropertySetHandle inArgs, OfxPropertySetHandle /*outArgs*/)
{
    try {
        OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;
        if (strcmp(action, kOfxActionLoad) == 0 ||
            strcmp(action, kOfxActionDescribe) == 0 ||
            strcmp(action, kOfxImageEffectActionDescribeInContext) == 0 ||
            strcmp(action, kOfxActionCreateInstance) == 0 ||
            strcmp(action, kOfxActionDestroyInstance) == 0)
            logLine(action);

        if (strcmp(action, kOfxActionLoad) == 0)
            return onLoad();
        if (strcmp(action, kOfxActionDescribe) == 0)
            return describe(effect);
        if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0)
            return describeInContext(effect, inArgs);
        if (strcmp(action, kOfxActionCreateInstance) == 0) {
            gVisSig.clear();   // a new instance: the Inspector must be arranged again
            return kOfxStatOK;
        }
        if (strcmp(action, kOfxActionDestroyInstance) == 0)
            return kOfxStatOK;
        if (strcmp(action, kOfxImageEffectActionRender) == 0)
            return render(effect, inArgs);
        if (strcmp(action, kOfxActionInstanceChanged) == 0)
            return onInstanceChanged(effect, inArgs);
    }
    catch (...) {
        // Never let an exception cross into the host.
        return kOfxStatErrUnknown;
    }

    // Every action we don't handle: let the host use its default.
    return kOfxStatReplyDefault;
}

// The plugin's identity card, handed to the host by OfxGetPlugin.
// Do NOT change the identifier: saved projects find the plugin by it.
static OfxPlugin gPlugin =
{
    kOfxImageEffectPluginApi,           // pluginApi
    kOfxImageEffectPluginApiVersion,    // apiVersion
    "com.BarryOFXPlugin.BarryStroke",   // pluginIdentifier: unique, no spaces
    1,                                  // pluginVersionMajor
    0,                                  // pluginVersionMinor
    setHostFunc,                        // setHost
    pluginMain                          // mainEntry
};

// The two mandatory exports the host looks up by name.
OfxExport OfxPlugin *OfxGetPlugin(int nth)
{
    logLine("OfxGetPlugin called");
    if (nth == 0)
        return &gPlugin;
    return nullptr;
}

OfxExport int OfxGetNumberOfPlugins(void)
{
    logLine("OfxGetNumberOfPlugins called");
    return 1;
}
