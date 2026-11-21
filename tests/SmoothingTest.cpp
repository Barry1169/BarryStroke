// SmoothingTest.cpp
// Standalone check of roundCornersSmooth in Rounding.h. No OpenFX, no Resolve.
// Compile in the x64 Native Tools prompt:
//   cd /d D:\DavinciCoding\OpenfxProject\BarryStroke\tests
//   cl /EHsc /std:c++17 SmoothingTest.cpp /Fe:SmoothingTest.exe
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include "../src/Rounding.h"

struct Pt { double x, y; };

static int gFails = 0, gChecks = 0;
static void check(const char *name, bool ok)
{
    ++gChecks;
    if (!ok) { ++gFails; printf("FAIL %s\n", name); }
}

static double distToPoly(const std::vector<Pt> &poly, const Pt &p)
{
    double best = 1e300;
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Pt &a = poly[i], &b = poly[(i + 1) % n];
        double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
        double t = (l2 > 0) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0;
        t = std::max(0.0, std::min(1.0, t));
        double cx = a.x + t * dx - p.x, cy = a.y + t * dy - p.y;
        best = std::min(best, std::sqrt(cx * cx + cy * cy));
    }
    return best;
}

static bool finite(const std::vector<Pt> &v)
{
    for (const Pt &p : v)
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    return true;
}

static bool hasPoint(const std::vector<Pt> &v, double x, double y, double tol)
{
    for (const Pt &p : v)
        if (std::fabs(p.x - x) < tol && std::fabs(p.y - y) < tol) return true;
    return false;
}

// Distance from the corner (0,0) of the square to its rounded outline's nearest point.
static double cornerGap(const std::vector<Pt> &outline) { return distToPoly(outline, Pt{ 0, 0 }); }

int main()
{
    std::vector<Pt> sq = { { 0, 0 }, { 400, 0 }, { 400, 400 }, { 0, 400 } };

    // ---- smoothing 0 is exactly the normal rounding ----
    {
        auto a = roundCornersSmooth(sq, 60.0, 0.0);
        auto b = roundCorners(sq, 60.0);
        bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); ++i)
            same = (a[i].x == b[i].x && a[i].y == b[i].y);
        check("smoothing 0 equals roundCorners", same);
        auto c = roundCornersSmooth(sq, 0.0, 0.8);
        check("radius 0 returns the polygon", c.size() == 4);
    }

    // ---- the curve starts farther along the edges, by a known amount ----
    {
        // Square corner, radius 60: t = 60. K = 4 (r / sin45 - r) / cos45. At smoothing s the start is t + s (0.9 K - t).
        const double t = 60.0;
        const double K = 4.0 * (t / std::sin(3.14159265358979323846 / 4.0) - t) / std::cos(3.14159265358979323846 / 4.0);
        double L5 = t + 0.5 * (0.9 * K - t), L10 = t + 1.0 * (0.9 * K - t);
        auto m = roundCornersSmooth(sq, 60.0, 0.5);
        check("smooth 0.5: starts at the expected distance on the bottom edge", hasPoint(m, L5, 0.0, 1e-6));
        check("smooth 0.5: starts at the expected distance on the left edge", hasPoint(m, 0.0, L5, 1e-6));
        check("smooth 0.5: finite", finite(m));
        auto full = roundCornersSmooth(sq, 60.0, 1.0);
        check("smooth 1.0: starts at the expected distance", hasPoint(full, L10, 0.0, 1e-6));
        check("smooth 1.0: starts about 2.1 x the arc's start", L10 > 2.0 * t && L10 < 2.2 * t);
    }

    // ---- sharper corners get a bigger boost ----
    {
        // A 36 degree tip (a star's point), long edges. Compare the start at smoothing 1 with the arc's start.
        std::vector<Pt> tri = { { 0, 0 }, { 1000, 0 }, { 500, 163.7 } };   // a flat triangle: sharp corners at both ends of the bottom edge
        auto a = roundCornersSmooth(tri, 10.0, 0.0);
        auto b = roundCornersSmooth(tri, 10.0, 1.0);
        // The first point after the left corner's arc on the bottom edge tells where the curve of the corner at (1000, 0) starts.
        double startArc = 0.0, startSm = 0.0;
        for (const Pt &p : a) if (std::fabs(p.y) < 1e-9 && p.x > 500) startArc = std::max(startArc, 1000.0 - p.x);
        for (const Pt &p : b) if (std::fabs(p.y) < 1e-9 && p.x > 500) startSm = std::max(startSm, 1000.0 - p.x);
        check("sharp corner: smoothing 1 starts more than 2.2 x farther", startArc > 0.0 && startSm > 2.2 * startArc);
    }

    // ---- the curve is symmetric about the corner's bisector ----
    {
        auto m = roundCornersSmooth(sq, 60.0, 0.7);
        bool sym = true;
        for (const Pt &p : m) {
            if (p.x < 200 && p.y < 200) {            // only the bottom-left corner's curve
                Pt mirror = { p.y, p.x };
                if (distToPoly(m, mirror) > 1e-6) sym = false;
            }
        }
        check("smooth 0.7: symmetric about the bisector", sym);
    }

    // ---- the tip of the corner stays where the circular arc puts it ----
    {
        double base = cornerGap(roundCornersSmooth(sq, 60.0, 0.0));
        bool same = true, inside = true;
        for (int k = 1; k <= 10; ++k) {
            auto m = roundCornersSmooth(sq, 60.0, k / 10.0);
            if (std::fabs(cornerGap(m) - base) > 0.05) same = false;
            for (const Pt &p : m)
                if (p.x < -1e-9 || p.x > 400 + 1e-9 || p.y < -1e-9 || p.y > 400 + 1e-9) inside = false;
        }
        check("the corner tip does not move when smoothing grows", same);
        check("smoothed outline stays inside the square", inside);
    }

    // ---- the bend is spread out: the curve leaves the edge earlier than the arc does ----
    {
        auto arc = roundCornersSmooth(sq, 60.0, 0.0);
        auto sm = roundCornersSmooth(sq, 60.0, 1.0);
        Pt at90 = { 90.0, 0.0 };
        check("arc is on the edge at 90", distToPoly(arc, at90) < 1e-9);
        check("smooth curve has left the edge by 90", distToPoly(sm, at90) > 0.05);
    }

    // ---- the start is clamped to half of the short edge ----
    {
        std::vector<Pt> thin = { { 0, 0 }, { 400, 0 }, { 400, 100 }, { 0, 100 } };
        auto m = roundCornersSmooth(thin, 60.0, 1.0);
        bool ok = true;
        for (const Pt &p : m)
            if (p.y > 50 + 1e-6 && p.y < 100 - 50 - 1e-6) ok = false;   // the curves of the two sides never overlap
        // The bottom-left curve starts at most 50 up the left edge.
        check("thin rectangle: curve start clamped to half the short edge", ok && hasPoint(m, 0.0, 50.0, 1e-6));
    }

    // ---- per-corner radii work ----
    {
        const double kPi = 3.14159265358979323846;
        auto startFor = [&](double r, double smv) {
            double K = 4.0 * (r / std::sin(kPi / 4.0) - r) / std::cos(kPi / 4.0);
            return r + smv * (0.9 * K - r);
        };
        std::vector<double> mult = { 80.0, 0.0, 40.0, 0.0 };
        auto m = roundCornersSmooth(sq, 100.0, 0.5, &mult);
        check("per-corner: sharp corner stays a point", hasPoint(m, 400.0, 0.0, 1e-9) && hasPoint(m, 0.0, 400.0, 1e-9));
        check("per-corner: corner 0 starts at its own distance", hasPoint(m, startFor(80.0, 0.5), 0.0, 1e-6));
        check("per-corner: corner 2 starts at its own distance", hasPoint(m, 400.0, 400.0 - startFor(40.0, 0.5), 1e-6));
    }

    // ---- fuzz ----
    {
        std::mt19937 rng(4242);
        auto rnd = [&](double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); };
        int bad = 0;
        for (int trial = 0; trial < 500; ++trial) {
            int n = 3 + (int) (rng() % 14);
            std::vector<Pt> pts;
            double minx = 1e300, maxx = -1e300, miny = 1e300, maxy = -1e300;
            for (int i = 0; i < n; ++i) {
                Pt p = { rnd(0, 1920), rnd(0, 1080) };
                pts.push_back(p);
                minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
                miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
            }
            double radius = rnd(0, 150), sm = rnd(0, 1);
            auto m = roundCornersSmooth(pts, radius, sm);
            bool ok = m.size() >= 3 && finite(m);
            for (const Pt &p : m)
                if (p.x < minx - 1e-6 || p.x > maxx + 1e-6 || p.y < miny - 1e-6 || p.y > maxy + 1e-6) ok = false;
            if (!ok) { ++bad; if (bad < 5) printf("fuzz trial %d n %d\n", trial, n); }
        }
        check("fuzz: 500 random shapes, finite and inside the bounding box", bad == 0);
    }

    printf("%d checks, %d failures\n", gChecks, gFails);
    printf(gFails == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return gFails == 0 ? 0 : 1;
}
