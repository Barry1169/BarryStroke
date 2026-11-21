// Render.cpp
#include "Render.h"
#include "FrameParams.h"
#include "ShapeData.h"
#include "MaskRow.h"
#include "Stroke.h"
#include "Rounding.h"
#include "ShapeTemplates.h"
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <vector>

OfxStatus render(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs)
{
    // What to render: the time, and the window (x1, y1, x2, y2) in pixels.
    double time = 0.0;
    int win[4] = { 0, 0, 0, 0 };
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    for (int i = 0; i < 4; ++i)
        gPropHost->propGetInt(inArgs, kOfxImageEffectPropRenderWindow, i, &win[i]);

    // Proxy scale: pixels per canonical unit (1.0 = full size).
    double rscale[2] = { 1.0, 1.0 };
    gPropHost->propGetDouble(inArgs, kOfxImageEffectPropRenderScale, 0, &rscale[0]);
    gPropHost->propGetDouble(inArgs, kOfxImageEffectPropRenderScale, 1, &rscale[1]);
    if (rscale[0] <= 0.0) rscale[0] = 1.0;   // never divide by zero
    if (rscale[1] <= 0.0) rscale[1] = 1.0;

    // This instance's two clips, then one image from each.
    OfxImageClipHandle srcClip = nullptr, dstClip = nullptr;
    gEffectHost->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &srcClip, nullptr);
    gEffectHost->clipGetHandle(effect, kOfxImageEffectOutputClipName, &dstClip, nullptr);

    // All plain parameters, read once for this frame.
    OfxParamSetHandle paramSet = nullptr;
    gEffectHost->getParamSet(effect, &paramSet);
    const FrameParams fp = readFrameParams(paramSet, time);

    // Frame size in canonical units (Resolve shrinks it in proxy modes).
    OfxPropertySetHandle effProps = nullptr;
    double ext[2] = { 0.0, 0.0 };
    gEffectHost->getPropertySet(effect, &effProps);
    gPropHost->propGetDouble(effProps, kOfxImageEffectPropProjectExtent, 0, &ext[0]);
    gPropHost->propGetDouble(effProps, kOfxImageEffectPropProjectExtent, 1, &ext[1]);

    OfxPropertySetHandle srcImg = nullptr, dstImg = nullptr;
    OfxStatus srcSt = gEffectHost->clipGetImage(srcClip, time, nullptr, &srcImg);
    OfxStatus dstSt = gEffectHost->clipGetImage(dstClip, time, nullptr, &dstImg);
    OfxStatus result = kOfxStatFailed;

    // The outline: points from the slots (fractions of the frame).
    std::vector<OfxPointD> pts;
    readSlotPoints(paramSet, time, pts, nullptr);

    // A new effect shows a Triangle before any points exist: empty shape, dropdown on Triangle, nothing loaded yet.
    if (pts.empty() && ext[0] > 0.0 && ext[1] > 0.0) {
        int kindNow = 0, doneNow = 0;
        OfxParamHandle kh2 = nullptr, dh2 = nullptr;
        if (gParamHost->paramGetHandle(paramSet, "shapeKind", &kh2, nullptr) == kOfxStatOK && kh2)
            gParamHost->paramGetValueAtTime(kh2, time, &kindNow);
        if (gParamHost->paramGetHandle(paramSet, "shapeDone", &dh2, nullptr) == kOfxStatOK && dh2)
            gParamHost->paramGetValueAtTime(dh2, time, &doneNow);
        if (kindNow == 1 && doneNow == 0) {
            ShapeTemplate<OfxPointD> tt = makeTemplate<OfxPointD>(kTplTriangle, ext[1] / ext[0]);
            pts = tt.pts;
        }
    }

    // Units: fractions and percents become frame units. The render scale maps them to pixels.
    double feather = fp.feather;
    double offX = fp.offX, offY = fp.offY;
    double strokeW = fp.strokeW;
    if (ext[0] > 0.0 && ext[1] > 0.0) {
        for (OfxPointD &p : pts) { p.x *= ext[0]; p.y *= ext[1]; }
        offX    *= ext[0] / 100.0;   // Offset X / Y are percent of the frame
        offY    *= ext[1] / 100.0;
        feather *= ext[1] / 100.0;   // Feather is percent of the frame height
    } else {
        pts.clear();
    }
    strokeW *= ext[1] / 100.0;       // stroke width: percent of frame height

    // Scale and rotate the outline around the frame center.
    const double sc = (fp.scalePct > 0.0) ? fp.scalePct / 100.0 : 1.0;
    const double rotDeg = fp.rotDeg;
    const double rotRad = rotDeg * 3.14159265358979323846 / 180.0;
    const double rcos = std::cos(rotRad), rsin = std::sin(rotRad);
    const double ccx = ext[0] * 0.5, ccy = ext[1] * 0.5;
    if (sc != 1.0 || rotDeg != 0.0) {
        for (OfxPointD &p : pts) {
            double dx = (p.x - ccx) * sc;
            double dy = (p.y - ccy) * sc;
            p.x = ccx + dx * rcos - dy * rsin;
            p.y = ccy + dx * rsin + dy * rcos;
        }
    }

    // Corner radius: percent of frame height -> frame units, then round the outline.
    // The stroke follows the rounded outline.
    if (fp.cornerPct > 0.0 && pts.size() >= 3 && ext[1] > 0.0) {
        pts = roundCornersSmooth(pts, fp.cornerPct * ext[1] / 100.0, fp.cornerSmooth / 100.0);
    }

    const float strokeOpac = (float) std::max(0.0, std::min(1.0, fp.strokeOpacity / 100.0));
    const float strokeF[3] = { (float) fp.strokeRGB[0], (float) fp.strokeRGB[1], (float) fp.strokeRGB[2] };

    if (srcSt == kOfxStatOK && dstSt == kOfxStatOK) {
        // Read the pixel pointer, bounds, row stride and depth of both images.
        void *srcData = nullptr, *dstData = nullptr;
        int srcB[4] = { 0, 0, 0, 0 }, dstB[4] = { 0, 0, 0, 0 };
        int srcRow = 0, dstRow = 0;
        char *srcDepth = nullptr, *dstDepth = nullptr;

        gPropHost->propGetPointer(srcImg, kOfxImagePropData, 0, &srcData);
        gPropHost->propGetPointer(dstImg, kOfxImagePropData, 0, &dstData);
        for (int i = 0; i < 4; ++i) {
            gPropHost->propGetInt(srcImg, kOfxImagePropBounds, i, &srcB[i]);
            gPropHost->propGetInt(dstImg, kOfxImagePropBounds, i, &dstB[i]);
        }
        gPropHost->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRow);
        gPropHost->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstRow);
        gPropHost->propGetString(srcImg, kOfxImageEffectPropPixelDepth, 0, &srcDepth);
        gPropHost->propGetString(dstImg, kOfxImageEffectPropPixelDepth, 0, &dstDepth);

        // Bytes per pixel: 4 channels (RGBA) times the size of one channel.
        int bpp = 0;
        if (srcDepth && strcmp(srcDepth, kOfxBitDepthFloat) == 0) bpp = 16;
        else if (srcDepth && strcmp(srcDepth, kOfxBitDepthShort) == 0) bpp = 8;
        else if (srcDepth && strcmp(srcDepth, kOfxBitDepthByte) == 0) bpp = 4;

        // Work only where the render window and both buffers overlap.
        int x1 = std::max(win[0], std::max(srcB[0], dstB[0]));
        int y1 = std::max(win[1], std::max(srcB[1], dstB[1]));
        int x2 = std::min(win[2], std::min(srcB[2], dstB[2]));
        int y2 = std::min(win[3], std::min(srcB[3], dstB[3]));

        if (srcData && dstData && bpp > 0 && srcDepth && dstDepth && strcmp(srcDepth, dstDepth) == 0) {
            // Once per frame: the edge list. Once per row: the edge distance and inside flag.
            std::vector<PolyEdge> edges;
            preparePoly(pts, edges);
            const int rowCount = (x2 > x1) ? (x2 - x1) : 0;
            std::vector<float> maskRow((size_t) rowCount), distRow((size_t) rowCount);
            std::vector<unsigned char> insideRow((size_t) rowCount);
            const double strokeSoft = std::max(feather, 1.0 / rscale[0]);
            const double band = strokeBand(strokeW, fp.strokePos, strokeSoft, pts.size() >= 3);
            for (int y = y1; y < y2; ++y) {
                if (rowCount > 0)
                    buildMaskRow(edges, (y + 0.5) / rscale[1] - offY, (x1 + 0.5) / rscale[0] - offX,
                                 1.0 / rscale[0], rowCount, feather, false, band,
                                 maskRow.data(), distRow.data(), insideRow.data());
                const char *s = (const char *) srcData
                    + (std::ptrdiff_t)(y - srcB[1]) * srcRow
                    + (std::ptrdiff_t)(x1 - srcB[0]) * bpp;
                char *d = (char *) dstData
                    + (std::ptrdiff_t)(y - dstB[1]) * dstRow
                    + (std::ptrdiff_t)(x1 - dstB[0]) * bpp;
                if (x2 > x1) {
                    if (bpp == 16) {
                        // Float RGBA, premultiplied. The stroke is drawn over the picture.
                        // Fewer than 3 points = no outline yet, so nothing is stroked.
                        const float *sp = (const float *) s;
                        float *dp = (float *) d;
                        for (int x = 0; x < x2 - x1; ++x) {
                            dp[0] = sp[0];
                            dp[1] = sp[1];
                            dp[2] = sp[2];
                            dp[3] = sp[3];
                            if (band > 0.0 && distRow[x] >= 0.0f) {
                                // Signed distance to the edge: positive inside the outline, negative outside.
                                double sdist = insideRow[x] ? (double) distRow[x] : -(double) distRow[x];
                                float sa = strokeAlpha(sdist, strokeW, fp.strokePos, strokeSoft, strokeOpac);
                                compositeStroke(dp, strokeF, sa);
                            }
                            sp += 4;
                            dp += 4;
                        }
                    } else {
                        // Short / Byte RGBA: Resolve renders Float, so this is a plain copy.
                        memcpy(d, s, (size_t)(x2 - x1) * (size_t) bpp);
                    }
                }
            }
            result = kOfxStatOK;
        }
    }

    // Every image we fetched must be released before Render returns.
    if (srcSt == kOfxStatOK) gEffectHost->clipReleaseImage(srcImg);
    if (dstSt == kOfxStatOK) gEffectHost->clipReleaseImage(dstImg);
    return result;
}
