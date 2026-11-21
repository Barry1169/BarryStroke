// Render.cpp
#include "Render.h"
#include "FrameParams.h"
#include "AlphaDist.h"
#include "Stroke.h"
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

    // Units: percents become frame units. The render scale maps them to pixels.
    double feather = fp.feather;
    double strokeW = fp.strokeW;
    if (ext[0] > 0.0 && ext[1] > 0.0) {
        feather *= ext[1] / 100.0;   // Feather is percent of the frame height
    }
    strokeW *= ext[1] / 100.0;       // stroke width: percent of frame height

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
            // Stroke geometry, in frame units. The stroke follows the alpha edge.
            const double strokeSoft = std::max(feather, 1.0 / rscale[0]);
            const double band = strokeBand(strokeW, fp.strokePos, strokeSoft, true);
            const double maxDistFrame = band + strokeSoft;   // beyond this the stroke alpha is 0
            const double maxDistPx = maxDistFrame * rscale[0];

            // Distance field over the render window expanded by the stroke reach,
            // so pixels near the window edge still get a correct distance.
            std::vector<unsigned char> mask;
            std::vector<float> alphaBuf;
            std::vector<float> dist;
            int ex1 = x1, ey1 = y1, ew = 0;
            bool haveDist = false;
            if (bpp == 16 && band > 0.0 && x2 > x1 && y2 > y1) {
                const int margin = (int) std::ceil(maxDistPx) + 2;
                ex1 = std::max(srcB[0], x1 - margin);
                ey1 = std::max(srcB[1], y1 - margin);
                int ex2 = std::min(srcB[2], x2 + margin);
                int ey2 = std::min(srcB[3], y2 + margin);
                ew = ex2 - ex1;
                int eh = ey2 - ey1;
                if (ew > 0 && eh > 0) {
                    mask.assign((size_t) ew * (size_t) eh, 0);
                    alphaBuf.assign((size_t) ew * (size_t) eh, 0.0f);
                    bool anyIn = false, anyOut = false;
                    for (int y = ey1; y < ey2; ++y) {
                        const float *sp = (const float *) ((const char *) srcData
                            + (std::ptrdiff_t)(y - srcB[1]) * srcRow
                            + (std::ptrdiff_t)(ex1 - srcB[0]) * bpp);
                        unsigned char *mp = &mask[(size_t)(y - ey1) * (size_t) ew];
                        float *ap = &alphaBuf[(size_t)(y - ey1) * (size_t) ew];
                        for (int x = 0; x < ew; ++x) {
                            float a = sp[3];
                            bool inside = a > 0.5f;
                            mp[x] = inside ? 1 : 0;
                            ap[x] = a;
                            if (inside) anyIn = true; else anyOut = true;
                            sp += 4;
                        }
                    }
                    // No alpha edge in view (fully visible or fully empty): no stroke.
                    if (anyIn && anyOut) {
                        dist.assign((size_t) ew * (size_t) eh, 0.0f);
                        alphaDistanceSmooth(alphaBuf.data(), ew, eh, (float) (maxDistPx + 1.0), dist.data());
                        haveDist = true;
                    }
                }
            }

            for (int y = y1; y < y2; ++y) {
                const char *s = (const char *) srcData
                    + (std::ptrdiff_t)(y - srcB[1]) * srcRow
                    + (std::ptrdiff_t)(x1 - srcB[0]) * bpp;
                char *d = (char *) dstData
                    + (std::ptrdiff_t)(y - dstB[1]) * dstRow
                    + (std::ptrdiff_t)(x1 - dstB[0]) * bpp;
                if (x2 > x1) {
                    if (bpp == 16) {
                        // Float RGBA, premultiplied. The stroke is drawn over the picture.
                        const float *sp = (const float *) s;
                        float *dp = (float *) d;
                        const float *distRow = haveDist
                            ? &dist[(size_t)(y - ey1) * (size_t) ew + (size_t)(x1 - ex1)] : nullptr;
                        const unsigned char *maskRow = haveDist
                            ? &mask[(size_t)(y - ey1) * (size_t) ew + (size_t)(x1 - ex1)] : nullptr;
                        for (int x = 0; x < x2 - x1; ++x) {
                            dp[0] = sp[0];
                            dp[1] = sp[1];
                            dp[2] = sp[2];
                            dp[3] = sp[3];
                            if (distRow && distRow[x] < (float) maxDistPx) {
                                // Signed distance to the alpha edge: positive inside, negative outside.
                                double sdist = (maskRow[x] ? (double) distRow[x] : -(double) distRow[x]) / rscale[0];
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
