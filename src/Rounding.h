// Rounding.h
// Pure maths for rounding the corners of a closed polygon. No OpenFX types,
// so a standalone test program can include it.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Replace every corner of the closed polygon by a short arc.
// radius = the base radius, in the same units as the points.
// mult   = optional, one value per input point: that corner's radius is radius * mult[i] / 100.
//          A vector of the wrong length is ignored (every corner then uses 100).
// Corners that are straight, have a zero-length edge, or end up with radius 0 stay sharp.
// Each radius is clamped so an arc uses at most half of each adjacent edge.
// Each arc is cut into enough pieces to stay within about 0.1 unit of a true curve.
template <class T>
static std::vector<T> roundCorners(const std::vector<T> &in, double radius,
                                   const std::vector<double> *mult = nullptr)
{
    const double kPi = 3.14159265358979323846;
    const size_t n = in.size();
    if (n < 3 || radius <= 0.0)
        return in;
    const bool useMult = (mult != nullptr && mult->size() == n);

    std::vector<T> out;
    out.reserve(n * 9);
    for (size_t i = 0; i < n; ++i) {
        const T &prev = in[(i + n - 1) % n];
        const T &cur  = in[i];
        const T &next = in[(i + 1) % n];

        // This corner's own radius.
        double rad = radius * (useMult ? std::max(0.0, (*mult)[i]) : 100.0) / 100.0;
        if (rad <= 0.0) { out.push_back(cur); continue; }

        // Unit vectors from the corner toward its two neighbours.
        double d1x = prev.x - cur.x, d1y = prev.y - cur.y;
        double d2x = next.x - cur.x, d2y = next.y - cur.y;
        double len1 = std::sqrt(d1x * d1x + d1y * d1y);
        double len2 = std::sqrt(d2x * d2x + d2y * d2y);
        if (len1 < 1e-9 || len2 < 1e-9) { out.push_back(cur); continue; }
        d1x /= len1; d1y /= len1;
        d2x /= len2; d2y /= len2;

        // theta = the angle at the corner between the two edges.
        double cosT = std::max(-1.0, std::min(1.0, d1x * d2x + d1y * d2y));
        double theta = std::acos(cosT);
        if (theta < 1e-3 || theta > kPi - 1e-3) { out.push_back(cur); continue; }

        // t = distance from the corner to where the arc touches each edge.
        double tanHalf = std::tan(theta * 0.5);
        double t = rad / tanHalf;
        double tMax = 0.5 * std::min(len1, len2);
        if (t > tMax) t = tMax;
        double r = t * tanHalf;               // the radius actually used after clamping

        // The arc centre sits on the bisector of the corner.
        double bx = d1x + d2x, by = d1y + d2y;
        double bl = std::sqrt(bx * bx + by * by);
        bx /= bl; by /= bl;
        double cdist = r / std::sin(theta * 0.5);
        double ccx = cur.x + bx * cdist, ccy = cur.y + by * cdist;

        // Walk the short way round from one touch point to the other.
        double a1 = std::atan2(cur.y + d1y * t - ccy, cur.x + d1x * t - ccx);
        double a2 = std::atan2(cur.y + d2y * t - ccy, cur.x + d2x * t - ccx);
        double da = a2 - a1;
        while (da > kPi)   da -= 2.0 * kPi;
        while (da <= -kPi) da += 2.0 * kPi;

        double stepAng = (r > 0.2) ? 2.0 * std::acos(1.0 - 0.1 / r) : kPi;
        int nSeg = (int) std::ceil(std::fabs(da) / stepAng);
        nSeg = std::max(4, std::min(64, nSeg));
        for (int k = 0; k <= nSeg; ++k) {
            double a = a1 + da * (double) k / (double) nSeg;
            out.push_back(T{ ccx + r * std::cos(a), ccy + r * std::sin(a) });
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Corner smoothing. Each corner becomes ONE cubic Bezier curve instead of a circular arc.
// The curve starts farther from the corner along the edges, so the bend ramps up gradually
// (a "squircle-like" corner). It looks similar to Figma's smoothing but is NOT its exact formula.
//
//   smoothing = 0..1. 0 = the normal circular arc (this function then returns roundCorners exactly).
//   The curve starts farther from the corner than the circular arc does: at smoothing 1 about 2.1 times
//   as far on a square corner and up to about 2.7 times on a sharp one, but never farther than half of
//   an adjacent edge, so neighbouring corners never overlap.
//   The tip of the corner stays where the circular arc puts it: smoothing changes how the bend
//   ramps up, it does not shrink the corner.
//   radius and mult work exactly as in roundCorners.
// ---------------------------------------------------------------------------
namespace roundingDetail {
struct Bez { double x[4], y[4]; };
static inline void bezPoint(const Bez &b, double t, double &px, double &py)
{
    double u = 1.0 - t;
    double w0 = u * u * u, w1 = 3.0 * u * u * t, w2 = 3.0 * u * t * t, w3 = t * t * t;
    px = w0 * b.x[0] + w1 * b.x[1] + w2 * b.x[2] + w3 * b.x[3];
    py = w0 * b.y[0] + w1 * b.y[1] + w2 * b.y[2] + w3 * b.y[3];
}
// Fewest pieces that keep the curve within about 0.1 unit of the straight pieces.
static inline int bezSegments(const Bez &b)
{
    static const int cand[] = { 4, 6, 8, 12, 16, 24, 32, 48, 64 };
    for (int c : cand) {
        double worst = 0.0;
        for (int k = 0; k < c; ++k) {
            double ax, ay, bx, by, mx, my;
            bezPoint(b, (double) k / c, ax, ay);
            bezPoint(b, (double) (k + 1) / c, bx, by);
            bezPoint(b, ((double) k + 0.5) / c, mx, my);
            double dx = mx - 0.5 * (ax + bx), dy = my - 0.5 * (ay + by);
            worst = std::max(worst, std::sqrt(dx * dx + dy * dy));
        }
        if (worst <= 0.1) return c;
    }
    return 64;
}
} // namespace roundingDetail

template <class T>
static std::vector<T> roundCornersSmooth(const std::vector<T> &in, double radius, double smoothing,
                                         const std::vector<double> *mult = nullptr)
{
    if (smoothing <= 0.0)
        return roundCorners(in, radius, mult);

    const double kPi = 3.14159265358979323846;
    const size_t n = in.size();
    if (n < 3 || radius <= 0.0)
        return in;
    const double sm = std::min(1.0, smoothing);
    const bool useMult = (mult != nullptr && mult->size() == n);

    std::vector<T> out;
    out.reserve(n * 9);
    for (size_t i = 0; i < n; ++i) {
        const T &prev = in[(i + n - 1) % n];
        const T &cur  = in[i];
        const T &next = in[(i + 1) % n];

        double rad = radius * (useMult ? std::max(0.0, (*mult)[i]) : 100.0) / 100.0;
        if (rad <= 0.0) { out.push_back(cur); continue; }

        double d1x = prev.x - cur.x, d1y = prev.y - cur.y;
        double d2x = next.x - cur.x, d2y = next.y - cur.y;
        double len1 = std::sqrt(d1x * d1x + d1y * d1y);
        double len2 = std::sqrt(d2x * d2x + d2y * d2y);
        if (len1 < 1e-9 || len2 < 1e-9) { out.push_back(cur); continue; }
        d1x /= len1; d1y /= len1;
        d2x /= len2; d2y /= len2;

        double cosT = std::max(-1.0, std::min(1.0, d1x * d2x + d1y * d2y));
        double theta = std::acos(cosT);
        if (theta < 1e-3 || theta > kPi - 1e-3) { out.push_back(cur); continue; }

        // The circular arc: it touches each edge t away from the corner, with radius r.
        double tanHalf = std::tan(theta * 0.5);
        double t = rad / tanHalf;
        double tMax = 0.5 * std::min(len1, len2);
        if (t > tMax) t = tMax;
        double r = t * tanHalf;

        // The tip of the corner must stay where the circular arc puts it, so smoothing changes
        // the SHAPE of the bend and does not shrink the corner. The middle of the Bezier is
        // (L + 3 cd) cos(theta/2) / 4 away from the corner; the arc's middle is r / sin(theta/2) - r.
        // K is the longest start (L) for which a control distance cd >= 0 still keeps the tip in place.
        double apex = r / std::sin(theta * 0.5) - r;
        double K = 4.0 * apex / std::cos(theta * 0.5);
        // At 0 the curve starts where the arc does (t). At 1 it starts 90% of the way to K:
        // about 2.1 x t on a square corner and up to about 2.7 x t on a sharp one.
        // It never starts beyond half of an adjacent edge, so neighbouring corners never overlap.
        double L = t + sm * std::max(0.0, 0.9 * K - t);
        L = std::min(L, tMax);
        double cd = std::max(0.0, (K - L) / 3.0);

        roundingDetail::Bez b;
        b.x[0] = cur.x + d1x * L;  b.y[0] = cur.y + d1y * L;
        b.x[1] = cur.x + d1x * cd; b.y[1] = cur.y + d1y * cd;
        b.x[2] = cur.x + d2x * cd; b.y[2] = cur.y + d2y * cd;
        b.x[3] = cur.x + d2x * L;  b.y[3] = cur.y + d2y * L;

        int nSeg = roundingDetail::bezSegments(b);
        for (int k = 0; k <= nSeg; ++k) {
            double px, py;
            roundingDetail::bezPoint(b, (double) k / (double) nSeg, px, py);
            out.push_back(T{ px, py });
        }
    }
    return out;
}
