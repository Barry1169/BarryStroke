// ShapeData.cpp
#include "ShapeData.h"
#include "ShapeTemplates.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

bool readSlotPoints(OfxParamSetHandle paramSet, double time, std::vector<OfxPointD> &out,
                    std::vector<int> *slotsOut)
{
    out.clear();
    if (slotsOut) slotsOut->clear();
    OfxParamHandle oh = nullptr;
    char *ot = nullptr;
    if (gParamHost->paramGetHandle(paramSet, "order", &oh, nullptr) != kOfxStatOK || !oh)
        return false;
    gParamHost->paramGetValueAtTime(oh, time, &ot);
    if (!ot || !*ot)
        return false;


    const char *s = ot;
    while (*s) {
        char *end = nullptr;
        long idx = strtol(s, &end, 10);
        if (end == s) { ++s; continue; }   // skip anything that is not a number
        s = end;
        if (idx >= 0 && idx < kMaxPts) {
            char nm[16];
            sprintf_s(nm, sizeof(nm), "pt%d", (int) idx);
            OfxParamHandle ph = nullptr;
            double v[2] = { 0.5, 0.5 };
            if (gParamHost->paramGetHandle(paramSet, nm, &ph, nullptr) == kOfxStatOK && ph) {
                gParamHost->paramGetValueAtTime(ph, time, &v[0], &v[1]);
                out.push_back(OfxPointD{ v[0], v[1] });
                if (slotsOut) slotsOut->push_back((int) idx);
            }
        }
    }
    return true;
}

void setShapeKindCustom(OfxParamSetHandle paramSet)
{
    OfxParamHandle h = nullptr;
    if (gParamHost->paramGetHandle(paramSet, "shapeKind", &h, nullptr) != kOfxStatOK || !h)
        return;
    int v = 0;
    gParamHost->paramGetValue(h, &v);
    if (v != 0)
        gParamHost->paramSetValue(h, 0);

    // Forget which preset was applied last, so picking the same preset again works.
    OfxParamHandle dh = nullptr;
    if (gParamHost->paramGetHandle(paramSet, "shapeDone", &dh, nullptr) == kOfxStatOK && dh) {
        int d = 0;
        gParamHost->paramGetValue(dh, &d);
        if (d != 0)
            gParamHost->paramSetValue(dh, 0);
    }
}

void clearParamKeys(OfxParamHandle ph)
{
    unsigned int nKeys = 0, guard = 0;
    if (gParamHost->paramGetNumKeys(ph, &nKeys) != kOfxStatOK)
        return;
    while (nKeys > 0 && guard++ < 1000) {
        OfxTime kt = 0.0;
        if (gParamHost->paramGetKeyTime(ph, 0, &kt) != kOfxStatOK)
            break;
        if (gParamHost->paramDeleteKey(ph, kt) != kOfxStatOK)
            break;
        if (gParamHost->paramGetNumKeys(ph, &nKeys) != kOfxStatOK)
            break;
    }
}

void writeSlotPoint(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs,
                    int slot, const OfxPointD &p, const char *newOrder)
{
    OfxPropertySetHandle eprops = nullptr;
    double ext[2] = { 0.0, 0.0 };
    gEffectHost->getPropertySet(effect, &eprops);
    gPropHost->propGetDouble(eprops, kOfxImageEffectPropProjectExtent, 0, &ext[0]);
    gPropHost->propGetDouble(eprops, kOfxImageEffectPropProjectExtent, 1, &ext[1]);
    if (ext[0] <= 0.0 || ext[1] <= 0.0 || slot < 0 || slot >= kMaxPts)
        return;

    double t = 0.0;
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &t);

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);

    char nm[16];
    sprintf_s(nm, sizeof(nm), "pt%d", slot);
    OfxParamHandle ph = nullptr;
    if (gParamHost->paramGetHandle(paramSet, nm, &ph, nullptr) != kOfxStatOK || !ph)
        return;

    OfxParamHandle ordp = nullptr;
    if (newOrder)
        gParamHost->paramGetHandle(paramSet, "order", &ordp, nullptr);

    gParamHost->paramEditBegin(paramSet, newOrder ? "Add point" : "Edit point");

    if (newOrder) {
        // A newly added point must not inherit keys left behind by a deleted point.
        clearParamKeys(ph);
    }

    // A point that already has keyframes gets a key at this time; otherwise one constant.
    unsigned int slotKeys = 0;
    gParamHost->paramGetNumKeys(ph, &slotKeys);
    OfxStatus st = (slotKeys > 0)
        ? gParamHost->paramSetValueAtTime(ph, t, p.x / ext[0], p.y / ext[1])
        : gParamHost->paramSetValue(ph, p.x / ext[0], p.y / ext[1]);
    if (newOrder && ordp)
        gParamHost->paramSetValue(ordp, newOrder);
    setShapeKindCustom(paramSet);   // the user is editing by hand: the dropdown says Custom
    gParamHost->paramEditEnd(paramSet);

    if (st != kOfxStatOK) {
        char line[96];
        sprintf_s(line, sizeof(line), "slot write FAILED: slot %d status %d", slot, (int) st);
        logLine(line);
    }
}

void writeOrderOnly(OfxImageEffectHandle effect, const char *newOrder)
{
    OfxParamSetHandle paramSet = nullptr;
    OfxParamHandle ordp = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);
    if (gParamHost->paramGetHandle(paramSet, "order", &ordp, nullptr) != kOfxStatOK || !ordp)
        return;

    gParamHost->paramEditBegin(paramSet, "Delete point");
    OfxStatus st = gParamHost->paramSetValue(ordp, newOrder);
    setShapeKindCustom(paramSet);
    gParamHost->paramEditEnd(paramSet);

    if (st != kOfxStatOK) {
        char line[96];
        sprintf_s(line, sizeof(line), "order write FAILED: status %d", (int) st);
        logLine(line);
    }
}

void applyShapeTemplate(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs, int kind)
{
    OfxPropertySetHandle eprops = nullptr;
    double ext[2] = { 0.0, 0.0 };
    gEffectHost->getPropertySet(effect, &eprops);
    gPropHost->propGetDouble(eprops, kOfxImageEffectPropProjectExtent, 0, &ext[0]);
    gPropHost->propGetDouble(eprops, kOfxImageEffectPropProjectExtent, 1, &ext[1]);
    if (ext[0] <= 0.0 || ext[1] <= 0.0)
        return;

    // The preset points come from ShapeTemplates.h (pure maths, tested on its own).
    ShapeTemplate<OfxPointD> tpl = makeTemplate<OfxPointD>(kind, ext[1] / ext[0]);
    if (tpl.pts.empty())
        return;
    const std::vector<OfxPointD> &f = tpl.pts;   // fractions of the frame
    const double radiusPct = tpl.radiusPct;      // Corner radius to set, percent of frame height

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);

    gParamHost->paramEditBegin(paramSet, "Apply shape");

    int ptFails = 0, crFails = 0;
    std::string ord;
    for (size_t i = 0; i < f.size(); ++i) {
        char nm[16];
        sprintf_s(nm, sizeof(nm), "pt%d", (int) i);
        OfxParamHandle ph = nullptr;
        if (gParamHost->paramGetHandle(paramSet, nm, &ph, nullptr) != kOfxStatOK || !ph)
            continue;
        clearParamKeys(ph);
        if (gParamHost->paramSetValue(ph, f[i].x, f[i].y) != kOfxStatOK) ++ptFails;
        if (!ord.empty()) ord += ",";
        ord += std::to_string(i);
    }

    OfxParamHandle crh = nullptr;
    if (gParamHost->paramGetHandle(paramSet, "cornerRadius", &crh, nullptr) == kOfxStatOK && crh) {
        clearParamKeys(crh);
        if (gParamHost->paramSetValue(crh, radiusPct) != kOfxStatOK) ++crFails;
    }

    // Remember which preset is applied, so a repeated change event for the same preset is ignored.
    OfxParamHandle doneh = nullptr;
    if (gParamHost->paramGetHandle(paramSet, "shapeDone", &doneh, nullptr) == kOfxStatOK && doneh)
        gParamHost->paramSetValue(doneh, kind + 1);

    OfxParamHandle ordp = nullptr;
    OfxStatus ost = kOfxStatFailed;
    if (gParamHost->paramGetHandle(paramSet, "order", &ordp, nullptr) == kOfxStatOK && ordp)
        ost = gParamHost->paramSetValue(ordp, ord.c_str());

    gParamHost->paramEditEnd(paramSet);

    {
        char dl[128];
        sprintf_s(dl, sizeof(dl), "template %d: point write fails %d, radius write fails %d, order status %d",
                  kind, ptFails, crFails, (int) ost);
        logLine(dl);
    }
    if (ost != kOfxStatOK) {
        char tl[96];
        sprintf_s(tl, sizeof(tl), "template write FAILED: kind %d status %d", kind, (int) ost);
        logLine(tl);
    }
}

void ensureDefaultShape(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs)
{
    OfxParamSetHandle ps = nullptr;
    gEffectHost->getParamSet(effect, &ps);
    logLine("ensureDefaultShape: called");
    OfxParamHandle oh = nullptr, kh = nullptr, dh = nullptr;
    if (gParamHost->paramGetHandle(ps, "order", &oh, nullptr) != kOfxStatOK || !oh)
        return;
    char *ot = nullptr;
    gParamHost->paramGetValue(oh, &ot);
    if (ot && *ot)
    { logLine("ensureDefaultShape: order already has points"); return; }
    int kind = 0, done = 0;
    if (gParamHost->paramGetHandle(ps, "shapeKind", &kh, nullptr) == kOfxStatOK && kh)
        gParamHost->paramGetValue(kh, &kind);
    if (gParamHost->paramGetHandle(ps, "shapeDone", &dh, nullptr) == kOfxStatOK && dh)
        gParamHost->paramGetValue(dh, &done);
    {
        char dl[128];
        sprintf_s(dl, sizeof(dl), "default shape check: order empty, kind %d, done %d", kind, done);
        logLine(dl);
    }
    if (kind != 1 || done != 0)                   // only: dropdown on Triangle, nothing loaded yet
        return;
    applyShapeTemplate(effect, inArgs, 0);
}
