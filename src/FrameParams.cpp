// FrameParams.cpp
#include "FrameParams.h"

namespace {

// Read one parameter at the given time. If the parameter is missing, the value is left as it was.
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

    getInt   (ps, "strokeOn",     t, p.strokeOn);
    getDouble(ps, "strokeWidth",  t, p.strokeW);
    getRGB   (ps, "strokeColor",  t, p.strokeRGB);
    getDouble(ps, "strokeOpacity", t, p.strokeOpacity);
    // strokeAlign order is Inside, Center, Outside. The stroke maths uses 0 Center, 1 Inside, 2 Outside.
    int align = 1;
    getInt(ps, "strokeAlign", t, align);
    p.strokePos = (align == 0) ? kStrokeInside : (align == 1) ? kStrokeCenter : kStrokeOutside;

    getDouble(ps, "feather",       t, p.feather);
    getDouble(ps, "offsetX",       t, p.offX);
    getDouble(ps, "offsetY",       t, p.offY);
    getDouble(ps, "shapeScale",    t, p.scalePct);
    getDouble(ps, "shapeRotation", t, p.rotDeg);
    getDouble(ps, "cornerRadius",  t, p.cornerPct);
    getDouble(ps, "cornerSmoothing", t, p.cornerSmooth);

    // The checkbox switches the whole feature off.
    if (!p.strokeOn) p.strokeW = 0.0;
    return p;
}
