// MaskRowTest.cpp
// Proves buildMaskRow gives the same answers as the plugin's current per-pixel maths,
// then times both on a 1920x1080 frame with a 128-edge shape.
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include "../src/MaskRow.h"

struct Pt { double x, y; };

// ---- reference: copies of the plugin's current maths ----
static double refDistSqToSegment(const Pt &p, const Pt &a, const Pt &b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double len2 = dx * dx + dy * dy;
    double t = (len2 > 0.0) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    double cx = a.x + t * dx - p.x;
    double cy = a.y + t * dy - p.y;
    return cx * cx + cy * cy;
}
static double refDistSqToPolygon(const std::vector<Pt> &poly, double px, double py)
{
    Pt p = { px, py };
    double best = 1e300;
    for (size_t i = 0; i < poly.size(); ++i) {
        double d2 = refDistSqToSegment(p, poly[i], poly[(i + 1) % poly.size()]);
        if (d2 < best) best = d2;
    }
    return best;
}
static bool refInside(const std::vector<Pt> &poly, double px, double py)
{
    if (poly.size() < 3)
        return false;
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > py) != (poly[j].y > py) &&
            px < (poly[j].x - poly[i].x) * (py - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    }
    return in;
}
static float refMask(const std::vector<Pt> &pts, double px, double py, double feather, bool invert)
{
    if (pts.size() < 3)
        return 1.0f;
    bool inside = refInside(pts, px, py);
    float a;
    if (feather <= 0.0) {
        a = inside ? 1.0f : 0.0f;
    } else {
        double d = std::sqrt(refDistSqToPolygon(pts, px, py));
        double sd = inside ? d : -d;
        a = (float) std::max(0.0, std::min(1.0, 0.5 + sd / feather));
    }
    return invert ? 1.0f - a : a;
}

int main()
{
    std::mt19937 rng(12345);
    auto rnd = [&](double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); };

    const double featherChoices[] = { 0.0, 0.5, 5.0, 40.0, 300.0 };
    const double bandChoices[]    = { 0.0, 0.0, 3.0, 25.0 };
    const double scaleChoices[]   = { 1.0, 0.5 };

    int checks = 0, fails = 0;
    for (int trial = 0; trial < 600; ++trial) {
        int n = (trial % 10 == 0) ? 3 + (int) (rng() % 130) : 1 + (int) (rng() % 20);
        std::vector<Pt> pts;
        for (int i = 0; i < n; ++i)
            pts.push_back(Pt{ rnd(0.0, 1920.0), rnd(0.0, 1080.0) });
        std::vector<PolyEdge> E;
        preparePoly(pts, E);

        double feather = featherChoices[rng() % 5];
        double band    = bandChoices[rng() % 4];
        double rscale  = scaleChoices[rng() % 2];
        bool invert    = (rng() & 1u) != 0;
        double offX = rnd(-200.0, 200.0), offY = rnd(-200.0, 200.0);

        for (int r = 0; r < 5; ++r) {
            int y  = (int) (rng() % 1080);
            int x1 = (int) (rng() % 500);
            int count = 1 + (int) (rng() % 1400);
            double py = (y + 0.5) / rscale - offY;
            double px0 = (x1 + 0.5) / rscale - offX;
            double pxStep = 1.0 / rscale;

            std::vector<float> mask((size_t) count), dist((size_t) count);
            std::vector<unsigned char> ins((size_t) count);
            buildMaskRow(E, py, px0, pxStep, count, feather, invert, band, mask.data(), dist.data(), ins.data());

            for (int k = 0; k < count; ++k) {
                double px = px0 + (double) k * pxStep;
                float want = refMask(pts, px, py, feather, invert);
                if (n >= 3) {
                    ++checks;
                    if ((ins[k] != 0) != refInside(pts, px, py)) {
                        if (fails < 10)
                            printf("INSIDE MISMATCH trial %d n %d px %.3f py %.3f\n", trial, n, px, py);
                        ++fails;
                    }
                }
                ++checks;
                if (std::fabs(mask[k] - want) > 1e-4f) {
                    if (fails < 10)
                        printf("MASK MISMATCH trial %d n %d feather %.1f px %.3f py %.3f got %.5f want %.5f\n",
                               trial, n, feather, px, py, mask[k], want);
                    ++fails;
                }
                if (n >= 3) {
                    double ref = std::sqrt(refDistSqToPolygon(pts, px, py));
                    double reach = std::max(feather * 0.5, band);
                    ++checks;
                    bool ok;
                    if (ref <= reach)
                        ok = (dist[k] >= 0.0f) && std::fabs(dist[k] - ref) <= 1e-3;
                    else
                        ok = (dist[k] < 0.0f) || (dist[k] >= reach - 1e-3);
                    if (!ok) {
                        if (fails < 10)
                            printf("DIST MISMATCH trial %d n %d feather %.1f band %.1f px %.3f py %.3f got %.4f ref %.4f\n",
                                   trial, n, feather, band, px, py, dist[k], ref);
                        ++fails;
                    }
                }
            }
        }
    }
    printf("%d checks, %d failures\n", checks, fails);

    // ---- speed: a 128-edge circle on a 1920x1080 frame, feather 20 ----
    {
        std::vector<Pt> circle;
        for (int i = 0; i < 128; ++i) {
            double a = 6.283185307179586 * i / 128.0;
            circle.push_back(Pt{ 960.0 + 400.0 * std::cos(a), 540.0 + 400.0 * std::sin(a) });
        }
        std::vector<PolyEdge> E;
        preparePoly(circle, E);
        const int W = 1920, H = 1080;
        const double feather = 20.0;

        typedef std::chrono::steady_clock clk;
        auto t0 = clk::now();
        double sumRef = 0.0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                sumRef += refMask(circle, x + 0.5, y + 0.5, feather, false);
        auto t1 = clk::now();

        std::vector<float> mask(W), dist(W);
        double sumNew = 0.0;
        for (int y = 0; y < H; ++y) {
            buildMaskRow(E, y + 0.5, 0.5, 1.0, W, feather, false, 0.0, mask.data(), dist.data());
            for (int k = 0; k < W; ++k) sumNew += mask[k];
        }
        auto t2 = clk::now();

        double msRef = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double msNew = std::chrono::duration<double, std::milli>(t2 - t1).count();
        printf("old per-pixel: %.0f ms (sum %.1f)\n", msRef, sumRef);
        printf("new per-row:   %.0f ms (sum %.1f)\n", msNew, sumNew);
        if (msNew > 0.0)
            printf("speed-up: %.1fx\n", msRef / msNew);
    }

    printf(fails == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return fails == 0 ? 0 : 1;
}
