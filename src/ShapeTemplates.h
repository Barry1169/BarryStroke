// ShapeTemplates.h
// The preset shapes as pure maths. No OpenFX types.
// Points are FRACTIONS of the frame (0..1), y up, listed counter-clockwise.
#pragma once
#include <vector>
#include <cmath>

enum TemplateKind {
    kTplTriangle  = 0,
    kTplRectangle = 1,
    kTplCircle    = 2,
    kTplStar      = 3,
    kTplHeart     = 4,
    kTplArrow     = 5
};
static const int kTemplateCount = 6;

template <class P>
struct ShapeTemplate {
    std::vector<P> pts;     // fractions of the frame
    double radiusPct;       // Corner radius to use, percent of frame height
};

// hw = frame height / frame width. It turns "frame height units" into fractions of the frame width,
// so the shapes are not stretched on a wide frame.
template <class P>
static ShapeTemplate<P> makeTemplate(int kind, double hw)
{
    const double kPi = 3.14159265358979323846;
    ShapeTemplate<P> t;
    t.radiusPct = 0.0;

    // A point given in frame-height units around the frame centre.
    auto at = [hw](double ux, double uy) { return P{ 0.5 + ux * hw, 0.5 + uy }; };

    if (kind == kTplTriangle) {
        // Equilateral triangle, centred, point up.
        const double R = 0.3;
        for (int i = 0; i < 3; ++i) {
            double a = kPi * (0.5 + 2.0 * i / 3.0);
            t.pts.push_back(at(R * std::cos(a), R * std::sin(a)));
        }
    } else if (kind == kTplRectangle) {
        // The middle half of the frame. Slots: 0 bottom-left, 1 bottom-right, 2 top-right, 3 top-left.
        t.pts.push_back(P{ 0.25, 0.25 });
        t.pts.push_back(P{ 0.75, 0.25 });
        t.pts.push_back(P{ 0.75, 0.75 });
        t.pts.push_back(P{ 0.25, 0.75 });
    } else if (kind == kTplCircle) {
        // A square of side 0.5 frame heights, with a corner radius of half its side.
        const double s = 0.25;
        t.pts.push_back(at(-s, -s));
        t.pts.push_back(at( s, -s));
        t.pts.push_back(at( s,  s));
        t.pts.push_back(at(-s,  s));
        t.radiusPct = 25.0;
    } else if (kind == kTplStar) {
        // Five-point star: 10 points, outer and inner radius alternating, point up.
        for (int i = 0; i < 10; ++i) {
            double R = (i % 2 == 0) ? 0.3 : 0.12;
            double a = kPi * (0.5 + i / 5.0);
            t.pts.push_back(at(R * std::cos(a), R * std::sin(a)));
        }
    } else if (kind == kTplHeart) {
        // Heart, 12 points. The two lobes are corners that the Corner radius rounds into curves.
        // The short edges next to the tip and the dip limit the rounding there, so those two stay sharp.
        const double k = 0.6;   // overall size, frame heights
        static const double h[12][2] = {
            {  0.00, -0.52 },   // bottom tip
            {  0.08, -0.42 },   // tip, right
            {  0.52,  0.02 },   // right side
            {  0.52,  0.38 },   // right lobe, outer corner
            {  0.30,  0.52 },   // right lobe, top
            {  0.06,  0.38 },   // right of the dip
            {  0.00,  0.24 },   // dip
            { -0.06,  0.38 },   // left of the dip
            { -0.30,  0.52 },   // left lobe, top
            { -0.52,  0.38 },   // left lobe, outer corner
            { -0.52,  0.02 },   // left side
            { -0.08, -0.42 }    // tip, left
        };
        for (int i = 0; i < 12; ++i)
            t.pts.push_back(at(h[i][0] * k, h[i][1] * k));
        t.radiusPct = 14.0;
    } else if (kind == kTplArrow) {
        // Arrow pointing up: head and shaft, 7 points.
        const double k = 0.6;
        t.pts.push_back(at( 0.00 * k,  0.50 * k));   // tip
        t.pts.push_back(at(-0.30 * k,  0.10 * k));   // head, left
        t.pts.push_back(at(-0.10 * k,  0.10 * k));   // shaft top, left
        t.pts.push_back(at(-0.10 * k, -0.50 * k));   // shaft bottom, left
        t.pts.push_back(at( 0.10 * k, -0.50 * k));   // shaft bottom, right
        t.pts.push_back(at( 0.10 * k,  0.10 * k));   // shaft top, right
        t.pts.push_back(at( 0.30 * k,  0.10 * k));   // head, right
    }
    return t;
}
