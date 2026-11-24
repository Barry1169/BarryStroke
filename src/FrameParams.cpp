// FrameParams.cpp
#include "FrameParams.h"

namespace {

void getDouble(OfxParamSetHandle ps, const char *name, double t, double &v)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(ps, name, &h, nullptr) == kOfxStatOK && h)
        gParamHost->paramGetValueAtTime(h, t, &v);
}

void getInt(OfxParamSetHandle ps, const char *name, double t, int &v)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(ps, name, &h, nullptr) == kOfxStatOK && h)
        gParamHost->paramGetValueAtTime(h, t, &v);
}

void getRGB(OfxParamSetHandle ps, const char *name, double t, double *v)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(ps, name, &h, nullptr) == kOfxStatOK && h)
        gParamHost->paramGetValueAtTime(h, t, &v[0], &v[1], &v[2]);
}

} // namespace

FrameParams readFrameParams(OfxParamSetHandle ps, double t)
{
    FrameParams p;

    getDouble(ps, "strokeWidth",   t, p.strokeW);
    int align = 1;
    getInt(ps, "strokeAlign", t, align);
    p.strokePos = (align == 0) ? kStrokeInside : (align == 1) ? kStrokeCenter : kStrokeOutside;
    getDouble(ps, "strokeOpacity", t, p.strokeOpacity);
    getDouble(ps, "feather",       t, p.feather);
    getInt   (ps, "blendMode",     t, p.blendMode);
    getDouble(ps, "roundness",     t, p.roundness);

    getInt   (ps, "strokeStyle",   t, p.strokeStyle);
    getInt   (ps, "dashCount",     t, p.dashCount);
    getDouble(ps, "dashRatio",     t, p.dashRatio);
    getDouble(ps, "dashAngle",     t, p.dashAngle);
    getInt   (ps, "dashCap",       t, p.dashCap);

    getInt   (ps, "colorMode",     t, p.colorMode);
    getRGB   (ps, "strokeColor",   t, p.strokeRGB);
    getRGB   (ps, "strokeColor2",  t, p.strokeRGB2);
    getDouble(ps, "gradAngle",     t, p.gradAngle);

    getInt   (ps, "glowOn",        t, p.glowOn);
    getDouble(ps, "glowSize",      t, p.glowSize);
    getDouble(ps, "glowIntensity", t, p.glowIntensity);

    return p;
}
