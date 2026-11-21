// AlphaDist.h
// Exact 2D Euclidean distance from every pixel to the nearest alpha edge
// (the boundary between "inside" and "outside" pixels).
// Uses Felzenszwalb & Huttenlocher's O(W*H) 2-pass 1D distance transform.
// Pure maths, no OpenFX types, so a standalone test program can include it.
//
//   mask  = 1 where the input is visible (alpha > threshold), 0 where not.
//   dist  = Euclidean distance to the nearest pixel of the opposite class,
//           capped at maxDist. A pixel touching the opposite class has distance 1.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace detail {

// 1D squared Euclidean distance transform of f[0..n-1] (stride = step).
// Uses the lower envelope of parabolas (Felzenszwalb & Huttenlocher 2012).
static void edt1d(const float *f, float *d, int n, int step,
                  std::vector<int> &v, std::vector<float> &z)
{
    int k = 0;
    v[0] = 0;
    z[0] = -1e20f;
    z[1] =  1e20f;

    for (int q = 1; q < n; ++q) {
        float fq = f[q * step];
        float s;
        while (true) {
            int r = v[k];
            float fr = f[r * step];
            s = ((fq + (float)(q * q)) - (fr + (float)(r * r))) / (float)(2 * q - 2 * r);
            if (s > z[k])
                break;
            --k;
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = 1e20f;
    }

    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < (float) q)
            ++k;
        int r = v[k];
        float dx = (float)(q - r);
        d[q * step] = dx * dx + f[r * step];
    }
}

} // namespace detail

static void alphaDistance(const unsigned char *mask, int w, int h,
                          float maxDist, float *distOut, bool sharpCorners = false)
{
    if (!mask || !distOut || w <= 0 || h <= 0)
        return;

    const float cap = (maxDist > 0.0f) ? maxDist : 1.0f;
    const size_t N = (size_t) w * (size_t) h;

    if (sharpCorners) {
        // Exact Chebyshev distance: max(|dx|, |dy|) gives sharp 90-degree corners.
        std::vector<float> buf(N);
        auto chamfer = [&](bool zeroIsInside) {
            for (size_t i = 0; i < N; ++i)
                buf[i] = ((mask[i] != 0) == zeroIsInside) ? 0.0f : cap;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    float d = buf[(size_t) y * w + x];
                    if (x > 0)               d = std::min(d, buf[(size_t) y * w + x - 1] + 1.0f);
                    if (y > 0)               d = std::min(d, buf[(size_t)(y - 1) * w + x] + 1.0f);
                    if (x > 0 && y > 0)      d = std::min(d, buf[(size_t)(y - 1) * w + x - 1] + 1.0f);
                    if (x + 1 < w && y > 0)  d = std::min(d, buf[(size_t)(y - 1) * w + x + 1] + 1.0f);
                    buf[(size_t) y * w + x] = std::min(d, cap);
                }
            }
            for (int y = h - 1; y >= 0; --y) {
                for (int x = w - 1; x >= 0; --x) {
                    float d = buf[(size_t) y * w + x];
                    if (x + 1 < w)               d = std::min(d, buf[(size_t) y * w + x + 1] + 1.0f);
                    if (y + 1 < h)               d = std::min(d, buf[(size_t)(y + 1) * w + x] + 1.0f);
                    if (x + 1 < w && y + 1 < h)  d = std::min(d, buf[(size_t)(y + 1) * w + x + 1] + 1.0f);
                    if (x > 0 && y + 1 < h)      d = std::min(d, buf[(size_t)(y + 1) * w + x - 1] + 1.0f);
                    buf[(size_t) y * w + x] = std::min(d, cap);
                }
            }
        };
        chamfer(false);
        for (size_t i = 0; i < N; ++i)
            distOut[i] = mask[i] ? buf[i] : 0.0f;
        chamfer(true);
        for (size_t i = 0; i < N; ++i)
            if (!mask[i])
                distOut[i] = buf[i];
        return;
    }

    const float inf = cap * cap + 1.0f;

    std::vector<float> f(N), d(N);
    int maxDim = std::max(w, h);
    std::vector<int> v(maxDim);
    std::vector<float> z(maxDim + 1);

    auto runEDT = [&](bool zeroIsInside) {
        for (size_t i = 0; i < N; ++i) {
            bool inside = mask[i] != 0;
            f[i] = (inside == zeroIsInside) ? 0.0f : inf;
        }
        // Pass 1: along columns (vertical)
        for (int x = 0; x < w; ++x)
            detail::edt1d(&f[x], &d[x], h, w, v, z);
        // Pass 2: along rows (horizontal)
        for (int y = 0; y < h; ++y)
            detail::edt1d(&d[(size_t) y * w], &f[(size_t) y * w], w, 1, v, z);
    };

    // Distance to the nearest outside pixel (for inside pixels) ...
    runEDT(false);
    for (size_t i = 0; i < N; ++i)
        distOut[i] = mask[i] ? std::min(cap, std::sqrt(f[i])) : 0.0f;

    // ... and to the nearest inside pixel (for outside pixels).
    runEDT(true);
    for (size_t i = 0; i < N; ++i)
        if (!mask[i])
            distOut[i] = std::min(cap, std::sqrt(f[i]));
}
