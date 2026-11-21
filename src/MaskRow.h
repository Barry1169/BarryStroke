// MaskRow.h
// Fast mask maths for one row of pixels. No OpenFX types, so a standalone
// test program can include it.
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// One edge of the closed polygon, with everything precomputed once per frame.
struct PolyEdge {
    double ax, ay, bx, by;          // the two end points
    double dx, dy, inv2;            // b - a, and 1 / length squared (0 for a zero-length edge)
    double xmin, xmax, ymin, ymax;  // bounding box of the edge
};

// Build the edge list from any point type that has .x and .y. Fewer than 3 points = empty list.
template <class P>
static void preparePoly(const std::vector<P> &p, std::vector<PolyEdge> &E)
{
    E.clear();
    size_t n = p.size();
    if (n < 3)
        return;
    E.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const P &a = p[i];
        const P &b = p[(i + 1) % n];
        PolyEdge e;
        e.ax = a.x; e.ay = a.y; e.bx = b.x; e.by = b.y;
        e.dx = b.x - a.x; e.dy = b.y - a.y;
        double len2 = e.dx * e.dx + e.dy * e.dy;
        e.inv2 = (len2 > 0.0) ? 1.0 / len2 : 0.0;
        e.xmin = std::min(a.x, b.x); e.xmax = std::max(a.x, b.x);
        e.ymin = std::min(a.y, b.y); e.ymax = std::max(a.y, b.y);
        E.push_back(e);
    }
}

// Squared distance from (px, py) to one edge.
static inline double edgeDistSq(const PolyEdge &e, double px, double py)
{
    double t = ((px - e.ax) * e.dx + (py - e.ay) * e.dy) * e.inv2;
    t = std::max(0.0, std::min(1.0, t));
    double cx = e.ax + t * e.dx - px;
    double cy = e.ay + t * e.dy - py;
    return cx * cx + cy * cy;
}

// Fill maskOut[0..count) for one row. Pixel k sits at shape-space x = px0 + k * pxStep, y = py.
// maskOut = the keep-amount 0..1, same result as the old maskAmount (fewer than 3 points = 1).
// distOut (may be null) = distance to the nearest edge for pixels within the reach of the
//   feather / stroke band, and -1 for pixels farther away than that.
// band = the stroke's reach: half the stroke width plus half its softness (0 = no stroke).
static void buildMaskRow(const std::vector<PolyEdge> &E, double py, double px0, double pxStep,
                         int count, double feather, bool invert, double band,
                         float *maskOut, float *distOut, unsigned char *insideOut = nullptr)
{
    if (count <= 0)
        return;
    const size_t n = E.size();
    if (n < 3) {
        for (int k = 0; k < count; ++k) {
            maskOut[k] = 1.0f;
            if (distOut) distOut[k] = -1.0f;
            if (insideOut) insideOut[k] = 0;
        }
        return;
    }

    // Pass 1: where do the edges cross this row? Sorted, they give the inside spans (even-odd).
    std::vector<double> xs;
    xs.reserve(16);
    for (size_t i = 0; i < n; ++i) {
        const PolyEdge &e = E[i];
        if ((e.ay > py) != (e.by > py))
            xs.push_back((e.ax - e.bx) * (py - e.by) / (e.ay - e.by) + e.bx);
    }
    std::sort(xs.begin(), xs.end());
    const size_t m = xs.size();

    // Every pixel gets the hard answer: 1 inside, 0 outside. Far pixels keep it for good.
    size_t ci = 0;
    for (int k = 0; k < count; ++k) {
        double px = px0 + (double) k * pxStep;
        while (ci < m && xs[ci] <= px) ++ci;
        bool inside = ((m - ci) & 1u) != 0;
        float a = inside ? 1.0f : 0.0f;
        maskOut[k] = invert ? 1.0f - a : a;
        if (distOut) distOut[k] = -1.0f;
        if (insideOut) insideOut[k] = inside ? 1 : 0;
    }

    // Pass 2: only pixels near an edge need a real distance (soft edge and/or stroke).
    if (feather <= 0.0 && (band <= 0.0 || !distOut))
        return;
    const double reach = std::max(feather * 0.5, band) + 1.0;

    // For each edge that can reach this row: the range of pixels it can reach.
    struct Iv { int k0, k1, ei; };
    std::vector<Iv> iv;
    for (size_t i = 0; i < n; ++i) {
        const PolyEdge &e = E[i];
        if (py < e.ymin - reach || py > e.ymax + reach)
            continue;
        double f0 = std::ceil((e.xmin - reach - px0) / pxStep);
        double f1 = std::floor((e.xmax + reach - px0) / pxStep);
        if (f1 < 0.0 || f0 > (double) (count - 1))
            continue;
        int k0 = (f0 < 0.0) ? 0 : (int) f0;
        int k1 = (f1 > (double) (count - 1)) ? count - 1 : (int) f1;
        if (k0 > k1)
            continue;
        iv.push_back(Iv{ k0, k1, (int) i });
    }
    std::sort(iv.begin(), iv.end(), [](const Iv &a, const Iv &b) { return a.k0 < b.k0; });

    // Merge overlapping ranges into groups. Each group knows which edges can reach it.
    size_t s = 0;
    while (s < iv.size()) {
        int g0 = iv[s].k0, g1 = iv[s].k1;
        size_t t = s + 1;
        while (t < iv.size() && iv[t].k0 <= g1 + 1) {
            g1 = std::max(g1, iv[t].k1);
            ++t;
        }
        for (int k = g0; k <= g1; ++k) {
            double px = px0 + (double) k * pxStep;
            double best = 1e300;
            for (size_t q = s; q < t; ++q)
                best = std::min(best, edgeDistSq(E[iv[q].ei], px, py));
            double d = std::sqrt(best);

            size_t idx = (size_t) (std::upper_bound(xs.begin(), xs.end(), px) - xs.begin());
            bool inside = ((m - idx) & 1u) != 0;
            float a;
            if (feather <= 0.0) {
                a = inside ? 1.0f : 0.0f;
            } else {
                double sd = inside ? d : -d;   // positive inside, negative outside
                a = (float) std::max(0.0, std::min(1.0, 0.5 + sd / feather));
            }
            maskOut[k] = invert ? 1.0f - a : a;
            if (distOut) distOut[k] = (float) d;
        }
        s = t;
    }
}