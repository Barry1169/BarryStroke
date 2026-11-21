// Inspector.cpp
#include "Inspector.h"
#include <cstdio>
#include <initializer_list>

std::string gVisSig;

// ---------------------------------------------------------------------------
// Small helpers: one line per parameter instead of ten.
// The ORDER of the calls is the order in the Inspector. A group must be defined before its children.
// "secret" = hidden at the start. Anything the visibility table can hide must start hidden
// when its default state says so, otherwise Reset leaves it showing.
// ---------------------------------------------------------------------------
namespace {

OfxPropertySetHandle defineBase(OfxParamSetHandle ps, const char *type, const char *name,
                                const char *label, const char *parent, bool secret)
{
    OfxPropertySetHandle p = nullptr;
    gParamHost->paramDefine(ps, type, name, &p);
    gPropHost->propSetString(p, kOfxPropLabel, 0, label);
    if (parent) gPropHost->propSetString(p, kOfxParamPropParent, 0, parent);
    if (secret) gPropHost->propSetInt(p, kOfxParamPropSecret, 0, 1);
    return p;
}

void defineDouble(OfxParamSetHandle ps, const char *name, const char *label, double def,
                  double mn, double mx, double dmn, double dmx, const char *parent = nullptr,
                  bool secret = false)
{
    OfxPropertySetHandle p = defineBase(ps, kOfxParamTypeDouble, name, label, parent, secret);
    gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
    gPropHost->propSetDouble(p, kOfxParamPropMin, 0, mn);
    gPropHost->propSetDouble(p, kOfxParamPropMax, 0, mx);
    gPropHost->propSetDouble(p, kOfxParamPropDisplayMin, 0, dmn);
    gPropHost->propSetDouble(p, kOfxParamPropDisplayMax, 0, dmx);
}

void defineBool(OfxParamSetHandle ps, const char *name, const char *label, int def,
                const char *parent = nullptr, bool secret = false)
{
    OfxPropertySetHandle p = defineBase(ps, kOfxParamTypeBoolean, name, label, parent, secret);
    gPropHost->propSetInt(p, kOfxParamPropDefault, 0, def);
}

void defineChoice(OfxParamSetHandle ps, const char *name, const char *label,
                  std::initializer_list<const char *> options, int def, bool secret = false,
                  const char *parent = nullptr)
{
    OfxPropertySetHandle p = defineBase(ps, kOfxParamTypeChoice, name, label, parent, secret);
    int i = 0;
    for (const char *o : options)
        gPropHost->propSetString(p, kOfxParamPropChoiceOption, i++, o);
    gPropHost->propSetInt(p, kOfxParamPropDefault, 0, def);
}

void defineRGB(OfxParamSetHandle ps, const char *name, const char *label, double r, double g, double b,
               const char *parent = nullptr, bool secret = false)
{
    OfxPropertySetHandle p = defineBase(ps, kOfxParamTypeRGB, name, label, parent, secret);
    gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, r);
    gPropHost->propSetDouble(p, kOfxParamPropDefault, 1, g);
    gPropHost->propSetDouble(p, kOfxParamPropDefault, 2, b);
}

void defineButton(OfxParamSetHandle ps, const char *name, const char *label,
                  const char *parent = nullptr, bool secret = false)
{
    defineBase(ps, kOfxParamTypePushButton, name, label, parent, secret);
}

void defineGroup(OfxParamSetHandle ps, const char *name, const char *label, int open, bool secret = false)
{
    OfxPropertySetHandle p = defineBase(ps, kOfxParamTypeGroup, name, label, nullptr, secret);
    gPropHost->propSetInt(p, kOfxParamPropGroupOpen, 0, open);
}

// Show or hide one parameter while the plugin runs (the Secret property).
void setParamHidden(OfxParamSetHandle paramSet, const char *name, bool hidden)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(paramSet, name, &h, nullptr) != kOfxStatOK || !h)
        return;
    OfxPropertySetHandle props = nullptr;
    if (gParamHost->paramGetPropertySet(h, &props) != kOfxStatOK || !props)
        return;
    gPropHost->propSetInt(props, kOfxParamPropSecret, 0, hidden ? 1 : 0);
}

int readInt(OfxParamSetHandle ps, const char *name, int def)
{
    OfxParamHandle h = nullptr;
    int v = def;
    if (gParamHost->paramGetHandle(ps, name, &h, nullptr) == kOfxStatOK && h)
        gParamHost->paramGetValueAtTime(h, 0.0, &v);
    return v;
}

} // namespace

// ---------------------------------------------------------------------------
// Visibility rules. ONE table: "this parameter is visible when ...".
// To hide or show a new control, add one row.
// ---------------------------------------------------------------------------
namespace {

struct UiState {
    bool strokeOn;
};

struct Rule {
    const char *param;
    bool (*visible)(const UiState &);
};

const Rule kRules[] = {
    // Stroke
    { "strokeGroup",  [](const UiState &s) { return s.strokeOn; } },
    { "strokeWidth",  [](const UiState &s) { return s.strokeOn; } },
    { "strokeAlign",  [](const UiState &s) { return s.strokeOn; } },
    { "strokeColor",  [](const UiState &s) { return s.strokeOn; } },
    { "strokeOpacity", [](const UiState &s) { return s.strokeOn; } },
};

UiState readUiState(OfxParamSetHandle ps, const std::vector<int> &/*slots*/)
{
    UiState s;
    s.strokeOn = readInt(ps, "strokeOn", 0) != 0;
    return s;
}

} // namespace

void applyVisibility(OfxParamSetHandle paramSet, const std::vector<int> &slots)
{
    const UiState s = readUiState(paramSet, slots);
    for (const Rule &r : kRules)
        setParamHidden(paramSet, r.param, !r.visible(s));
}

void setGroupOpen(OfxParamSetHandle paramSet, const char *name, bool open)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(paramSet, name, &h, nullptr) != kOfxStatOK || !h)
        return;
    OfxPropertySetHandle props = nullptr;
    if (gParamHost->paramGetPropertySet(h, &props) != kOfxStatOK || !props)
        return;
    gPropHost->propSetInt(props, kOfxParamPropGroupOpen, 0, open ? 1 : 0);
}

void syncVisibility(OfxParamSetHandle paramSet, const void *instKey, const std::vector<int> &slots)
{
    const UiState s = readUiState(paramSet, slots);
    std::string sig = std::to_string((unsigned long long)(size_t) instKey) + "|"
                    + (s.strokeOn ? "S" : "s");
    if (sig == gVisSig)
        return;
    gVisSig = sig;
    applyVisibility(paramSet, slots);
}

// ---------------------------------------------------------------------------
// DescribeInContext. Inspector order, top to bottom:
//   Shape, Stroke, Corner radius, Transform, Points, then hidden.
// ---------------------------------------------------------------------------
OfxStatus describeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle /*inArgs*/)
{
    OfxPropertySetHandle clipProps;

    // The output clip: we produce RGBA.
    gEffectHost->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps);
    gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

    // The source clip: we accept RGBA.
    gEffectHost->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &clipProps);
    gPropHost->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

    OfxParamSetHandle ps = nullptr;
    gEffectHost->getParamSet(effect, &ps);

    // ---- Shape ----------------------------------------------------------
    // Picking a shape applies it at once. Editing a point by hand sets this back to Custom.
    defineChoice(ps, "shapeKind", "Shape", { "Custom", "Triangle", "Rectangle", "Circle", "Star", "Heart", "Arrow" }, 1);   // default: Triangle
    defineButton(ps, "clearShape", "Clear shape");

    // ---- Stroke ---------------------------------------------------------
    // The Stroke folder is hidden until the Stroke checkbox is on.
    defineBool  (ps, "strokeOn", "Stroke", 0);
    defineGroup (ps, "strokeGroup", "Stroke settings", 1, true);
    defineDouble(ps, "strokeWidth", "Stroke width (%)", 0.0, 0.0, 100.0, 0.0, 5.0, "strokeGroup", true);
    defineChoice(ps, "strokeAlign", "Stroke position", { "Inside", "Center", "Outside" }, 1, true, "strokeGroup");
    defineRGB   (ps, "strokeColor", "Stroke color", 1.0, 1.0, 1.0, "strokeGroup", true);
    defineDouble(ps, "strokeOpacity", "Stroke opacity (%)", 100.0, 0.0, 100.0, 0.0, 100.0, "strokeGroup", true);

    // ---- Corner radius --------------------------------------------------
    defineGroup (ps, "cornerGroup", "Corner radius", 1);
    defineDouble(ps, "cornerRadius", "Corner radius (%)", 0.0, 0.0, 100.0, 0.0, 20.0, "cornerGroup");
    defineDouble(ps, "cornerSmoothing", "Corner smoothing (%)", 0.0, 0.0, 100.0, 0.0, 100.0, "cornerGroup");

    // ---- Transform ------------------------------------------------------
    defineGroup(ps, "transformGroup", "Transform", 1);
    defineDouble(ps, "feather",       "Feather (%)",   0.0,   0.0,  100.0,    0.0,  20.0, "transformGroup");
    defineDouble(ps, "offsetX",       "Offset X (%)",  0.0, -1000.0, 1000.0, -100.0, 100.0, "transformGroup");
    defineDouble(ps, "offsetY",       "Offset Y (%)",  0.0, -1000.0, 1000.0, -100.0, 100.0, "transformGroup");
    defineDouble(ps, "shapeScale",    "Scale (%)",   100.0,    1.0, 1000.0,   10.0, 300.0, "transformGroup");
    defineDouble(ps, "shapeRotation", "Rotation (deg)", 0.0, -3600.0, 3600.0, -180.0, 180.0, "transformGroup");

    // ---- Points ---------------------------------------------------------
    defineGroup(ps, "pointsGroup", "Points (advanced)", 0);
    for (int i = 0; i < kMaxPts; ++i) {
        char nm[16], lb[24];
        sprintf_s(nm, sizeof(nm), "pt%d", i);
        sprintf_s(lb, sizeof(lb), "Point %d", i);
        OfxPropertySetHandle sp = defineBase(ps, kOfxParamTypeDouble2D, nm, lb, "pointsGroup", false);
        gPropHost->propSetString(sp, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
        gPropHost->propSetDouble(sp, kOfxParamPropDefault, 0, 0.5);
        gPropHost->propSetDouble(sp, kOfxParamPropDefault, 1, 0.5);
    }

    // Auto Key: on = overlay edits write a keyframe at the current frame, off = one constant.
    defineBool(ps, "autoKey", "Auto Key", 1);

    // ---- Hidden: the overlay manages these ------------------------------
    // Order: which slots make the outline, in order, for example "0,1,2".
    {
        OfxPropertySetHandle op = defineBase(ps, kOfxParamTypeString, "order", "Order", nullptr, true);
        gPropHost->propSetString(op, kOfxParamPropStringMode, 0, kOfxParamStringIsSingleLine);
        gPropHost->propSetString(op, kOfxParamPropDefault, 0, "");
        gPropHost->propSetInt(op, kOfxParamPropPersistant, 0, 1);
    }

    // Which preset was applied last (0 = none). Stops a repeated change event from re-applying it.
    {
        OfxPropertySetHandle dp = defineBase(ps, kOfxParamTypeInteger, "shapeDone", "Shape applied", nullptr, true);
        gPropHost->propSetInt(dp, kOfxParamPropDefault, 0, 0);
    }

    return kOfxStatOK;
}
