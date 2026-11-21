// Geometry.h
// Small polygon maths on OfxPointD. Used by the overlay (click maths) and by the
// old per-pixel path for Short and Byte images.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "ofxCore.h"

// Squared distance from point p to the line segment a-b.
static inline double distSqToSegment(const OfxPointD &p, const OfxPointD &a, const OfxPointD &b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double len2 = dx * dx + dy * dy;
    double t = (len2 > 0.0) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
    t = std::max(0.0, std::min(1.0, t));   // stay on the segment, not the infinite line
    double cx = a.x + t * dx - p.x;
    double cy = a.y + t * dy - p.y;
    return cx * cx + cy * cy;
}

// Squared distance from a point to the nearest edge of the closed polygon.
static inline double distSqToPolygon(const std::vector<OfxPointD> &poly, double px, double py)
{
    OfxPointD p = { px, py };
    double best = 1e300;
    for (size_t i = 0; i < poly.size(); ++i) {
        double d2 = distSqToSegment(p, poly[i], poly[(i + 1) % poly.size()]);
        if (d2 < best) best = d2;
    }
    return best;
}

// Even-odd point-in-polygon test (ray crossing).
static inline bool insidePolygon(const std::vector<OfxPointD> &poly, double px, double py)
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

// How much of a pixel to keep: 1 = keep it, 0 = cut it. Only the Short and Byte paths use it now.
static inline float maskAmount(const std::vector<OfxPointD> &pts, double px, double py,
                               double feather, bool invert)
{
    if (pts.size() < 3)
        return 1.0f;                       // no shape yet: keep everything

    bool inside = insidePolygon(pts, px, py);
    float a;
    if (feather <= 0.0) {
        a = inside ? 1.0f : 0.0f;
    } else {
        double d = std::sqrt(distSqToPolygon(pts, px, py));
        double sd = inside ? d : -d;       // positive inside, negative outside
        a = (float) std::max(0.0, std::min(1.0, 0.5 + sd / feather));
    }
    return invert ? 1.0f - a : a;
}
