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
    { "feather",      [](const UiState &s) { return s.strokeOn; } },
};

UiState readUiState(OfxParamSetHandle ps)
{
    UiState s;
    s.strokeOn = readInt(ps, "strokeOn", 1) != 0;
    return s;
}

} // namespace

void applyVisibility(OfxParamSetHandle paramSet)
{
    const UiState s = readUiState(paramSet);
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

void syncVisibility(OfxParamSetHandle paramSet, const void *instKey)
{
    const UiState s = readUiState(paramSet);
    std::string sig = std::to_string((unsigned long long)(size_t) instKey) + "|"
                    + (s.strokeOn ? "S" : "s");
    if (sig == gVisSig)
        return;
    gVisSig = sig;
    applyVisibility(paramSet);
}

// ---------------------------------------------------------------------------
// DescribeInContext. Inspector order, top to bottom: Stroke only.
// The stroke follows the input's alpha edge automatically; no shape to set up.
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

    // ---- Stroke ---------------------------------------------------------
    // Always active as soon as the effect is applied.
    defineGroup (ps, "strokeGroup", "Stroke settings", 1, false);
    defineDouble(ps, "strokeWidth",   "Stroke width (%)", 1.0, 0.0, 100.0, 0.0, 5.0, "strokeGroup", false);
    defineChoice(ps, "strokeAlign",   "Stroke position", { "Inside", "Center", "Outside" }, 1, false, "strokeGroup");
    defineDouble(ps, "strokeGap",     "Gap / Offset (%)", 0.0, -50.0, 50.0, -5.0, 5.0, "strokeGroup", false);
    defineDouble(ps, "strokeOpacity", "Stroke opacity (%)", 100.0, 0.0, 100.0, 0.0, 100.0, "strokeGroup", false);
    defineDouble(ps, "feather",       "Feather (%)", 0.0, 0.0, 100.0, 0.0, 20.0, "strokeGroup", false);
    defineChoice(ps, "blendMode",     "Blend mode", { "Normal", "Add", "Screen", "Multiply", "Overlay" }, 0, false, "strokeGroup");

    // ---- Style (Solid / Dashed / Dotted) --------------------------------
    defineGroup (ps, "styleGroup", "Style", 1, false);
    defineChoice(ps, "strokeStyle", "Stroke style", { "Solid", "Dashed", "Dotted" }, 0, false, "styleGroup");
    defineDouble(ps, "dashCount",   "Dash / Dot count", 24.0, 2.0, 200.0, 2.0, 80.0, "styleGroup", false);
    defineDouble(ps, "dashRatio",   "Dash length (%)", 50.0, 10.0, 90.0, 10.0, 90.0, "styleGroup", false);

    // ---- Color (Solid / Gradient / Animated) ----------------------------
    defineGroup (ps, "colorGroup", "Color", 1, false);
    defineChoice(ps, "colorMode",   "Color mode", { "Solid", "Gradient", "Animated" }, 0, false, "colorGroup");
    defineRGB   (ps, "strokeColor", "Color 1", 1.0, 1.0, 1.0, "colorGroup", false);
    defineRGB   (ps, "strokeColor2","Color 2", 0.0, 0.6, 1.0, "colorGroup", false);
    defineDouble(ps, "gradAngle",   "Gradient angle (deg)", 0.0, -360.0, 360.0, 0.0, 360.0, "colorGroup", false);
    defineDouble(ps, "colorSpeed",  "Animation speed", 1.0, -10.0, 10.0, 0.0, 5.0, "colorGroup", false);

    // ---- Glow -----------------------------------------------------------
    defineGroup (ps, "glowGroup", "Glow", 1, false);
    defineBool  (ps, "glowOn",        "Enable glow", 0, "glowGroup", false);
    defineDouble(ps, "glowSize",      "Glow size (%)", 2.0, 0.0, 50.0, 0.0, 10.0, "glowGroup", false);
    defineDouble(ps, "glowIntensity", "Glow intensity (%)", 60.0, 0.0, 100.0, 0.0, 100.0, "glowGroup", false);

    return kOfxStatOK;
}
