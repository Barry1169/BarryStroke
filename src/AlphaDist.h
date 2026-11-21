// AlphaDist.h
// Distance from every pixel to the nearest alpha edge (the boundary between
// "inside" and "outside" pixels). Pure maths, no OpenFX types, so a standalone
// test program can include it.
//
//   mask  = 1 where the input is visible (alpha > threshold), 0 where not.
//   dist  = distance to the nearest pixel of the opposite class, capped at
//           maxDist (values at maxDist mean "at least maxDist away").
//           A pixel touching the opposite class has distance 1.
//
// The render turns this into a signed distance (positive inside, negative
// outside) and feeds it to Stroke.h, exactly like the old polygon distance.
#pragma once
#include <algorithm>
#include <vector>

static void alphaDistance(const unsigned char *mask, int w, int h,
                          float maxDist, float *distOut)
{
    if (!mask || !distOut || w <= 0 || h <= 0)
        return;
    const float kSqrt2 = 1.41421356f;
    const float cap = (maxDist > 0.0f) ? maxDist : 1.0f;

    // One chamfer pass pair: dist starts at 0 on the "zero" class and INF on
    // the other, then propagates. zeroIsInside selects which class starts at 0.
    std::vector<float> buf((size_t) w * (size_t) h);
    auto chamfer = [&](bool zeroIsInside) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                bool inside = mask[(size_t) y * w + x] != 0;
                buf[(size_t) y * w + x] = (inside == zeroIsInside) ? 0.0f : cap;
            }
        }
        // Forward pass: neighbours already done (left, up, up-left, up-right).
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float d = buf[(size_t) y * w + x];
                if (x > 0)         d = std::min(d, buf[(size_t) y * w + x - 1] + 1.0f);
                if (y > 0)         d = std::min(d, buf[(size_t) (y - 1) * w + x] + 1.0f);
                if (x > 0 && y > 0) d = std::min(d, buf[(size_t) (y - 1) * w + x - 1] + kSqrt2);
                if (x + 1 < w && y > 0) d = std::min(d, buf[(size_t) (y - 1) * w + x + 1] + kSqrt2);
                buf[(size_t) y * w + x] = std::min(d, cap);
            }
        }
        // Backward pass: neighbours not yet done (right, down, down-right, down-left).
        for (int y = h - 1; y >= 0; --y) {
            for (int x = w - 1; x >= 0; --x) {
                float d = buf[(size_t) y * w + x];
                if (x + 1 < w)     d = std::min(d, buf[(size_t) y * w + x + 1] + 1.0f);
                if (y + 1 < h)     d = std::min(d, buf[(size_t) (y + 1) * w + x] + 1.0f);
                if (x + 1 < w && y + 1 < h) d = std::min(d, buf[(size_t) (y + 1) * w + x + 1] + kSqrt2);
                if (x > 0 && y + 1 < h)     d = std::min(d, buf[(size_t) (y + 1) * w + x - 1] + kSqrt2);
                buf[(size_t) y * w + x] = std::min(d, cap);
            }
        }
    };

    // Distance to the nearest outside pixel (for inside pixels) ...
    chamfer(false);
    for (size_t i = 0; i < (size_t) w * (size_t) h; ++i)
        distOut[i] = mask[i] ? buf[i] : 0.0f;
    // ... and to the nearest inside pixel (for outside pixels).
    chamfer(true);
    for (size_t i = 0; i < (size_t) w * (size_t) h; ++i)
        if (!mask[i])
            distOut[i] = buf[i];
}
