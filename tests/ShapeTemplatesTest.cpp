// ShapeTemplatesTest.cpp
// Standalone check of ShapeTemplates.h. No OpenFX, no Resolve.
// Compile in the x64 Native Tools prompt:
//   cd /d D:\DavinciCoding\OpenfxProject\BarryStroke\tests
//   cl /EHsc /std:c++17 ShapeTemplatesTest.cpp /Fe:ShapeTemplatesTest.exe
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/ShapeTemplates.h"

struct Pt { double x, y; };

static int gFails = 0, gChecks = 0;
static void check(const char *name, bool ok)
{
    ++gChecks;
    if (!ok) { ++gFails; printf("FAIL %s\n", name); }
}

static double area(const std::vector<Pt> &p)
{
    double s = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        const Pt &a = p[i], &b = p[(i + 1) % p.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

// Do segments p1-p2 and p3-p4 cross (not just touch)?
static bool cross(const Pt &p1, const Pt &p2, const Pt &p3, const Pt &p4)
{
    auto o = [](const Pt &a, const Pt &b, const Pt &c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); };
    double d1 = o(p3, p4, p1), d2 = o(p3, p4, p2), d3 = o(p1, p2, p3), d4 = o(p1, p2, p4);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
}

static bool simple(const std::vector<Pt> &p)
{
    const size_t n = p.size();
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) continue;   // neighbours through the closing edge
            if (cross(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n])) return false;
        }
    return true;
}

int main()
{
    const size_t wantCount[kTemplateCount]  = { 3, 4, 4, 10, 12, 7 };
    const double wantRadius[kTemplateCount] = { 0, 0, 25, 0, 14, 0 };
    const char *names[kTemplateCount] = { "triangle", "rectangle", "circle", "star", "heart", "arrow" };
    const double aspects[3] = { 9.0 / 16.0, 1.0, 1080.0 / 1440.0 };   // 16:9, square, 4:3 frames

    for (int a = 0; a < 3; ++a)
        for (int k = 0; k < kTemplateCount; ++k) {
            char nm[96];
            ShapeTemplate<Pt> t = makeTemplate<Pt>(k, aspects[a]);
            sprintf(nm, "%s aspect %d: point count", names[k], a);
            check(nm, t.pts.size() == wantCount[k]);
            sprintf(nm, "%s aspect %d: corner radius", names[k], a);
            check(nm, t.radiusPct == wantRadius[k]);
            bool inside = true;
            for (const Pt &p : t.pts)
                if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) inside = false;
            sprintf(nm, "%s aspect %d: inside the frame", names[k], a);
            check(nm, inside);
            sprintf(nm, "%s aspect %d: counter-clockwise", names[k], a);
            check(nm, area(t.pts) > 0.0);
            sprintf(nm, "%s aspect %d: does not cross itself", names[k], a);
            check(nm, simple(t.pts));
        }

    // The four old presets must keep exactly the numbers they had.
    {
        ShapeTemplate<Pt> r = makeTemplate<Pt>(kTplRectangle, 9.0 / 16.0);
        check("rectangle exact", r.pts[0].x == 0.25 && r.pts[0].y == 0.25 && r.pts[2].x == 0.75 && r.pts[2].y == 0.75);
        ShapeTemplate<Pt> c = makeTemplate<Pt>(kTplCircle, 9.0 / 16.0);
        check("circle exact", std::fabs(c.pts[0].x - (0.5 - 0.25 * 9.0 / 16.0)) < 1e-12 && c.pts[0].y == 0.25);
        ShapeTemplate<Pt> t = makeTemplate<Pt>(kTplTriangle, 9.0 / 16.0);
        check("triangle tip is at the top", std::fabs(t.pts[0].x - 0.5) < 1e-12 && std::fabs(t.pts[0].y - 0.8) < 1e-12);
        ShapeTemplate<Pt> s = makeTemplate<Pt>(kTplStar, 9.0 / 16.0);
        check("star tip is at the top", std::fabs(s.pts[0].x - 0.5) < 1e-12 && std::fabs(s.pts[0].y - 0.8) < 1e-12);
    }

    // Unknown kind gives an empty shape.
    check("unknown kind is empty", makeTemplate<Pt>(99, 0.5625).pts.empty());

    printf("%d checks, %d failures\n", gChecks, gFails);
    printf(gFails == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return gFails == 0 ? 0 : 1;
}
