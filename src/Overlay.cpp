// Overlay.cpp
#define NOMINMAX
#include <windows.h>
#include "Overlay.h"
#include "ShapeData.h"
#include "Inspector.h"
#include "Geometry.h"
#include "Rounding.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Overlay state. Refreshed at the start of every Draw and PenDown.
// (Shared by all instances for now. A per-instance version comes in a later step.)
// ---------------------------------------------------------------------------
static int  gDragIndex = -1;                  // point being dragged, -1 = none
static std::vector<int> gSlots;               // gSlots[i] = slot number of gPoints[i]
static bool gSlotMode = false;
static std::vector<OfxPointD> gPoints;        // control points, canonical pixels (before transform)

static double gOffX = 0.0, gOffY = 0.0;                   // shape offset
static double gScale = 1.0, gCenX = 0.0, gCenY = 0.0;     // shape scale and the frame center it scales around
static double gRotCos = 1.0, gRotSin = 0.0;               // shape rotation
static double gCornerR = 0.0;                             // global corner radius, canonical pixels
static double gCornerSmooth = 0.0;                        // corner smoothing, 0..1

// Fake a filled polygon: horizontal lines between pairs of edge crossings.
static void drawScanlineFill(OfxDrawContextHandle ctx, const std::vector<OfxPointD> &poly, double step)
{
    if (poly.size() < 3 || step <= 0.0)
        return;

    double minY = poly[0].y, maxY = poly[0].y;
    for (const OfxPointD &p : poly) {
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    // Safety: never more than 3000 scanlines, whatever the zoom.
    if ((maxY - minY) / step > 3000.0)
        step = (maxY - minY) / 3000.0;

    std::vector<OfxPointD> segs;
    std::vector<double> xs;
    for (double y = minY + step * 0.5; y < maxY; y += step) {
        xs.clear();
        for (size_t i = 0; i < poly.size(); ++i) {
            const OfxPointD &a = poly[i];
            const OfxPointD &b = poly[(i + 1) % poly.size()];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                double t = (y - a.y) / (b.y - a.y);
                xs.push_back(a.x + t * (b.x - a.x));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            segs.push_back(OfxPointD{ xs[k], y });
            segs.push_back(OfxPointD{ xs[k + 1], y });
        }
    }
    if (!segs.empty())
        gDrawHost->draw(ctx, kOfxDrawPrimitiveLines, segs.data(), (int) segs.size());
}

// Refresh gPoints, gSlots and the transform values from this effect instance's parameters.
static bool loadPolygonParam(OfxPropertySetHandle inArgs)
{
    void *inst = nullptr;
    gPropHost->propGetPointer(inArgs, kOfxPropEffectInstance, 0, &inst);
    if (!inst)
        return false;

    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet((OfxImageEffectHandle) inst, &paramSet);

    ensureDefaultShape((OfxImageEffectHandle) inst, inArgs);   // a new effect starts as a Triangle

    double t = 0.0;
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &t);

    // Points always come from the slots, as fractions of the frame. No order = an empty shape.
    gPoints.clear();
    gSlots.clear();
    std::vector<OfxPointD> sp;
    if (readSlotPoints(paramSet, t, sp, &gSlots))
        gPoints = sp;
    gSlotMode = true;

    gScale = 1.0;
    gRotCos = 1.0;
    gRotSin = 0.0;
    gCornerR = 0.0;

    OfxPropertySetHandle iprops = nullptr;
    double ext[2] = { 0.0, 0.0 };
    gEffectHost->getPropertySet((OfxImageEffectHandle) inst, &iprops);
    gPropHost->propGetDouble(iprops, kOfxImageEffectPropProjectExtent, 0, &ext[0]);
    gPropHost->propGetDouble(iprops, kOfxImageEffectPropProjectExtent, 1, &ext[1]);
    if (ext[0] > 0.0 && ext[1] > 0.0) {
        for (OfxPointD &p : gPoints) { p.x *= ext[0]; p.y *= ext[1]; }
    } else {
        gPoints.clear();
    }

    // Offsets: percent of the frame.
    OfxParamHandle ox = nullptr, oy = nullptr;
    gOffX = 0.0;
    gOffY = 0.0;
    if (gParamHost->paramGetHandle(paramSet, "offsetX", &ox, nullptr) == kOfxStatOK && ox)
        gParamHost->paramGetValueAtTime(ox, t, &gOffX);
    if (gParamHost->paramGetHandle(paramSet, "offsetY", &oy, nullptr) == kOfxStatOK && oy)
        gParamHost->paramGetValueAtTime(oy, t, &gOffY);
    gOffX *= ext[0] / 100.0;
    gOffY *= ext[1] / 100.0;

    OfxParamHandle scp = nullptr;
    double sv = 100.0;
    if (gParamHost->paramGetHandle(paramSet, "shapeScale", &scp, nullptr) == kOfxStatOK && scp)
        gParamHost->paramGetValueAtTime(scp, t, &sv);
    gScale = (sv > 0.0) ? sv / 100.0 : 1.0;

    OfxParamHandle rtp = nullptr;
    double rdeg = 0.0;
    if (gParamHost->paramGetHandle(paramSet, "shapeRotation", &rtp, nullptr) == kOfxStatOK && rtp)
        gParamHost->paramGetValueAtTime(rtp, t, &rdeg);
    double rrad = rdeg * 3.14159265358979323846 / 180.0;
    gRotCos = std::cos(rrad);
    gRotSin = std::sin(rrad);
    gCenX = ext[0] * 0.5;
    gCenY = ext[1] * 0.5;

    // Corner radius (percent of frame height) at this time, in canonical pixels.
    OfxParamHandle crp = nullptr;
    double crv = 0.0;
    if (gParamHost->paramGetHandle(paramSet, "cornerRadius", &crp, nullptr) == kOfxStatOK && crp)
        gParamHost->paramGetValueAtTime(crp, t, &crv);
    gCornerR = (crv > 0.0) ? crv * ext[1] / 100.0 : 0.0;

    OfxParamHandle smp = nullptr;
    double smv = 0.0;
    if (gParamHost->paramGetHandle(paramSet, "cornerSmoothing", &smp, nullptr) == kOfxStatOK && smp)
        gParamHost->paramGetValueAtTime(smp, t, &smv);
    gCornerSmooth = std::max(0.0, std::min(1.0, smv / 100.0));

    syncVisibility(paramSet, inst, gSlots);
    return true;
}

// V2 overlay entry point: draws with the host's DrawSuite instead of OpenGL.
OfxStatus overlayMainV2(const char *action, const void * /*handle*/,
                        OfxPropertySetHandle inArgs, OfxPropertySetHandle /*outArgs*/)
{
    if (strcmp(action, kOfxActionCreateInstance) == 0 ||
        strcmp(action, kOfxActionDestroyInstance) == 0) {
        char actionLine[256];
        sprintf_s(actionLine, sizeof(actionLine), "overlayV2: %s", action);
        logLine(actionLine);
    }

    if (strcmp(action, kOfxActionDescribe) == 0)        return kOfxStatOK;
    if (strcmp(action, kOfxActionCreateInstance) == 0)  return kOfxStatOK;
    if (strcmp(action, kOfxActionDestroyInstance) == 0) return kOfxStatOK;

    // ---------------------------------------------------------------- Draw
    if (strcmp(action, kOfxInteractActionDraw) == 0 && gDrawHost) {
        // The draw context is passed to us as a property of the Draw action.
        void *ctxPtr = nullptr;
        gPropHost->propGetPointer(inArgs, kOfxInteractPropDrawContext, 0, &ctxPtr);
        OfxDrawContextHandle ctx = (OfxDrawContextHandle) ctxPtr;

        static bool drawNoInst = false;
        if (!loadPolygonParam(inArgs) && !drawNoInst) {
            logLine("draw: no effect instance in inArgs");
            drawNoInst = true;
        }

        // Draw the shape where the render puts it. The guard moves the points now and
        // moves them back automatically when Draw ends, at any return.
        struct ShiftGuard {
            std::vector<OfxPointD> saved;
            ShiftGuard() {
                saved = gPoints;
                for (OfxPointD &p : gPoints) {
                    double dx = (p.x - gCenX) * gScale;
                    double dy = (p.y - gCenY) * gScale;
                    p.x = gCenX + dx * gRotCos - dy * gRotSin + gOffX;
                    p.y = gCenY + dx * gRotSin + dy * gRotCos + gOffY;
                }
            }
            ~ShiftGuard() { gPoints = saved; }
        } shiftGuard;

        // Mark every point with a small cross.
        OfxRGBAColourF cyan = { 0.0f, 1.0f, 1.0f, 1.0f };
        gDrawHost->setColour(ctx, &cyan);
        gDrawHost->setLineWidth(ctx, 3.0f);
        for (const OfxPointD &p : gPoints) {
            OfxPointD cross[4] = {
                { p.x - 15.0, p.y }, { p.x + 15.0, p.y },
                { p.x, p.y - 15.0 }, { p.x, p.y + 15.0 }
            };
            gDrawHost->draw(ctx, kOfxDrawPrimitiveLines, cross, 4);
        }

        double pscale[2] = { 1.0, 1.0 };
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPixelScale, 0, &pscale[0]);
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPixelScale, 1, &pscale[1]);

        if (gPoints.size() >= 3) {
            // The rounded polygon, the same shape the render uses. The crosses stay on the control points.
            std::vector<OfxPointD> shown = roundCornersSmooth(gPoints, gCornerR, gCornerSmooth);
            OfxRGBAColourF fill = { 0.0f, 1.0f, 1.0f, 0.3f };
            gDrawHost->setColour(ctx, &fill);
            gDrawHost->setLineWidth(ctx, 2.0f);
            drawScanlineFill(ctx, shown, 2.0 * pscale[1]);
            gDrawHost->setColour(ctx, &cyan);
            gDrawHost->setLineWidth(ctx, 3.0f);
            gDrawHost->draw(ctx, kOfxDrawPrimitiveLineLoop, shown.data(), (int) shown.size());
        } else if (gPoints.size() == 2) {
            gDrawHost->draw(ctx, kOfxDrawPrimitiveLineStrip, gPoints.data(), 2);
        }

        return kOfxStatOK;
    }

    // ------------------------------------------------------------- PenDown
    if (strcmp(action, kOfxInteractActionPenDown) == 0) {
        loadPolygonParam(inArgs);

        double pen[2] = { 0.0, 0.0 };
        double scale[2] = { 1.0, 1.0 };
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPenPosition, 0, &pen[0]);
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPenPosition, 1, &pen[1]);
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPixelScale, 0, &scale[0]);
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPixelScale, 1, &scale[1]);

        // The shape is drawn transformed, so move the click back into shape space.
        {
            double qx = pen[0] - gOffX - gCenX;
            double qy = pen[1] - gOffY - gCenY;
            pen[0] = gCenX + (qx * gRotCos + qy * gRotSin) / gScale;
            pen[1] = gCenY + (-qx * gRotSin + qy * gRotCos) / gScale;
        }

        // Grab radius: 10 screen pixels, converted to canonical units.
        double radius = 10.0 * scale[0] / gScale;
        gDragIndex = -1;
        for (size_t i = 0; i < gPoints.size(); ++i) {
            double dx = gPoints[i].x - pen[0];
            double dy = gPoints[i].y - pen[1];
            if (dx * dx + dy * dy <= radius * radius) {
                gDragIndex = (int) i;
                break;
            }
        }

        if (gSlotMode) {
            if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
                // Alt held: delete the grabbed point by removing its slot from order. Never add one.
                if (gDragIndex >= 0 && gDragIndex < (int) gSlots.size()) {
                    std::vector<int> rest = gSlots;
                    rest.erase(rest.begin() + gDragIndex);
                    std::string restOrd;
                    for (size_t i = 0; i < rest.size(); ++i) {
                        if (i) restOrd += ",";
                        restOrd += std::to_string(rest[i]);
                    }
                    void *dinst = nullptr;
                    gPropHost->propGetPointer(inArgs, kOfxPropEffectInstance, 0, &dinst);
                    if (dinst)
                        writeOrderOnly((OfxImageEffectHandle) dinst, restOrd.c_str());
                    gDragIndex = -1;   // the point is gone, nothing to drag
                    char dl[64];
                    sprintf_s(dl, sizeof(dl), "slot point deleted: %d left", (int) rest.size());
                    logLine(dl);
                }
                return kOfxStatOK;
            }
            if (gDragIndex >= 0)
                return kOfxStatOK;   // a click on a point only grabs it

            // A click on empty space adds a point. Find the first free slot.
            bool used[kMaxPts] = {};
            for (int s : gSlots)
                if (s >= 0 && s < kMaxPts) used[s] = true;
            int freeSlot = -1;
            for (int s = 0; s < kMaxPts; ++s)
                if (!used[s]) { freeSlot = s; break; }
            if (freeSlot < 0) {
                logLine("add point: all slots used");
                return kOfxStatOK;
            }

            // Insert on the nearest edge within the grab radius, else append.
            size_t n = gPoints.size();
            size_t insertAt = n;
            if (n >= 2) {
                double best = radius * radius;
                OfxPointD click = { pen[0], pen[1] };
                for (size_t i = 0; i < n; ++i) {
                    if (n == 2 && i == 1) break;   // two points: only one edge
                    double d2 = distSqToSegment(click, gPoints[i], gPoints[(i + 1) % n]);
                    if (d2 <= best) { best = d2; insertAt = i + 1; }
                }
            }

            std::vector<int> newSlots = gSlots;
            newSlots.insert(newSlots.begin() + insertAt, freeSlot);
            std::string ord;
            for (size_t i = 0; i < newSlots.size(); ++i) {
                if (i) ord += ",";
                ord += std::to_string(newSlots[i]);
            }

            void *inst = nullptr;
            gPropHost->propGetPointer(inArgs, kOfxPropEffectInstance, 0, &inst);
            if (inst)
                writeSlotPoint((OfxImageEffectHandle) inst, inArgs, freeSlot,
                               OfxPointD{ pen[0], pen[1] }, ord.c_str());
            return kOfxStatOK;
        }
    }

    // ----------------------------------------------------------- PenMotion
    if (strcmp(action, kOfxInteractActionPenMotion) == 0 && gDragIndex >= 0 && gDragIndex < (int) gPoints.size()) {
        double pen[2] = { 0.0, 0.0 };
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPenPosition, 0, &pen[0]);
        gPropHost->propGetDouble(inArgs, kOfxInteractPropPenPosition, 1, &pen[1]);

        {
            double qx = pen[0] - gOffX - gCenX;
            double qy = pen[1] - gOffY - gCenY;
            gPoints[gDragIndex].x = gCenX + (qx * gRotCos + qy * gRotSin) / gScale;
            gPoints[gDragIndex].y = gCenY + (-qx * gRotSin + qy * gRotCos) / gScale;
        }

        void *inst = nullptr;
        gPropHost->propGetPointer(inArgs, kOfxPropEffectInstance, 0, &inst);
        if (inst && gSlotMode && gDragIndex < (int) gSlots.size())
            writeSlotPoint((OfxImageEffectHandle) inst, inArgs, gSlots[gDragIndex], gPoints[gDragIndex]);

        return kOfxStatOK;
    }

    // --------------------------------------------------------------- PenUp
    if (strcmp(action, kOfxInteractActionPenUp) == 0 && gDragIndex >= 0) {
        gDragIndex = -1;
        return kOfxStatOK;
    }
    return kOfxStatReplyDefault;
}
