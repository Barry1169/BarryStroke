// StrokeTest.cpp
// Standalone check of Stroke.h (the stroke band / alpha / blend maths) and of
// how it uses MaskRow.h distances. No OpenFX, no Resolve.
// Compile in the x64 Native Tools prompt:
//   cd /d D:\DavinciCoding\OpenfxProject\BarryStroke\tests
//   cl /EHsc /std:c++17 StrokeTest.cpp /Fe:StrokeTest.exe
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/Stroke.h"
#include "../src/MaskRow.h"

struct Pt { double x, y; };

static int gFails = 0, gChecks = 0;
static void check(const char *name, bool ok)
{
    ++gChecks;
    if (!ok) { ++gFails; printf("FAIL %s\n", name); }
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main()
{
    // ---- band: how far the distance maths must reach ----
    check("band center",  near(strokeBand(10.0, kStrokeCenter, 2.0, true), 6.0, 1e-9));   // 5 + 1
    check("band inside",  near(strokeBand(10.0, kStrokeInside, 2.0, true), 11.0, 1e-9));  // 10 + 1
    check("band outside", near(strokeBand(10.0, kStrokeOutside, 2.0, true), 11.0, 1e-9));
    check("band zero width", strokeBand(0.0, kStrokeCenter, 2.0, true) == 0.0);
    check("band no shape",   strokeBand(10.0, kStrokeCenter, 2.0, false) == 0.0);

    // ---- centre: middle of the band as a signed distance ----
    check("centre center",  near(strokeCentre(10.0, kStrokeCenter), 0.0, 1e-9));
    check("centre inside",  near(strokeCentre(10.0, kStrokeInside), 5.0, 1e-9));
    check("centre outside", near(strokeCentre(10.0, kStrokeOutside), -5.0, 1e-9));

    // ---- alpha: full on the band middle, fading to 0 at the soft edge ----
    // Width 10, soft 2, opacity 1, Center: middle at sdist 0 -> 1.
    check("alpha middle", near(strokeAlpha(0.0, 10.0, kStrokeCenter, 2.0, 1.0f), 1.0, 1e-6));
    // At the band edge (|sdist| = 5) -> 0.5.
    check("alpha edge", near(strokeAlpha(5.0, 10.0, kStrokeCenter, 2.0, 1.0f), 0.5, 1e-6));
    check("alpha edge out", near(strokeAlpha(-5.0, 10.0, kStrokeCenter, 2.0, 1.0f), 0.5, 1e-6));
    // One soft-width beyond the band edge -> 0.
    check("alpha beyond", strokeAlpha(6.0, 10.0, kStrokeCenter, 2.0, 1.0f) == 0.0f);
    check("alpha beyond out", strokeAlpha(-6.0, 10.0, kStrokeCenter, 2.0, 1.0f) == 0.0f);
    // Inside: middle at +5.
    check("alpha inside mid", near(strokeAlpha(5.0, 10.0, kStrokeInside, 2.0, 1.0f), 1.0, 1e-6));
    check("alpha inside on edge", near(strokeAlpha(0.0, 10.0, kStrokeInside, 2.0, 1.0f), 0.5, 1e-6));
    check("alpha inside outside", strokeAlpha(-2.0, 10.0, kStrokeInside, 2.0, 1.0f) == 0.0f);
    // Outside: middle at -5.
    check("alpha outside mid", near(strokeAlpha(-5.0, 10.0, kStrokeOutside, 2.0, 1.0f), 1.0, 1e-6));
    check("alpha outside on edge", near(strokeAlpha(0.0, 10.0, kStrokeOutside, 2.0, 1.0f), 0.5, 1e-6));
    check("alpha outside inside", strokeAlpha(2.0, 10.0, kStrokeOutside, 2.0, 1.0f) == 0.0f);
    // Opacity scales the result.
    check("alpha opacity", near(strokeAlpha(0.0, 10.0, kStrokeCenter, 2.0, 0.5f), 0.5, 1e-6));
    // Zero width / zero softness: no stroke.
    check("alpha zero width", strokeAlpha(0.0, 0.0, kStrokeCenter, 2.0, 1.0f) == 0.0f);
    check("alpha zero soft", strokeAlpha(0.0, 10.0, kStrokeCenter, 0.0, 1.0f) == 0.0f);

    // ---- blend: premultiplied over-composite ----
    {
        float dp[4] = { 0.2f, 0.4f, 0.6f, 0.8f };
        float col[3] = { 1.0f, 0.0f, 0.0f };
        compositeStroke(dp, col, 1.0f);
        check("blend full r", near(dp[0], 1.0, 1e-6));
        check("blend full g", near(dp[1], 0.0, 1e-6));
        check("blend full a", near(dp[3], 1.0, 1e-6));
    }
    {
        float dp[4] = { 0.2f, 0.4f, 0.6f, 0.8f };
        float col[3] = { 1.0f, 1.0f, 1.0f };
        compositeStroke(dp, col, 0.5f);
        check("blend half r", near(dp[0], 0.6, 1e-6));   // 0.2*0.5 + 1*0.5
        check("blend half a", near(dp[3], 0.9, 1e-6));   // 0.8*0.5 + 0.5
    }
    {
        float dp[4] = { 0.2f, 0.4f, 0.6f, 0.8f };
        float col[3] = { 1.0f, 1.0f, 1.0f };
        compositeStroke(dp, col, 0.0f);
        check("blend zero", near(dp[0], 0.2, 1e-6) && near(dp[3], 0.8, 1e-6));
    }

    // ---- integration: MaskRow distances feed the stroke alpha ----
    // Square 0..100. A pixel 2 units inside the edge with width 10 Center soft 2 -> alpha 1.
    // A pixel 8 units inside -> beyond the band -> alpha 0 (and no distance computed).
    {
        std::vector<Pt> sq = { { 0, 0 }, { 100, 0 }, { 100, 100 }, { 0, 100 } };
        std::vector<PolyEdge> E;
        preparePoly(sq, E);
        const double strokeW = 10.0, soft = 2.0;
        const double band = strokeBand(strokeW, kStrokeCenter, soft, true);
        float mask[1], dist[1];
        unsigned char ins[1];
        // Row y = 50, pixel x = 2 (2 units inside the left edge).
        buildMaskRow(E, 50.0, 2.0, 1.0, 1, 0.0, false, band, mask, dist, ins);
        check("row inside flag", ins[0] == 1);
        check("row dist", near(dist[0], 2.0, 1e-6));
        double sdist = ins[0] ? (double) dist[0] : -(double) dist[0];
        check("row alpha inside", near(strokeAlpha(sdist, strokeW, kStrokeCenter, soft, 1.0f), 1.0, 1e-6));
        // Pixel x = 50 (50 units from the nearest edge): no distance, no stroke.
        buildMaskRow(E, 50.0, 50.0, 1.0, 1, 0.0, false, band, mask, dist, ins);
        check("row far no dist", dist[0] < 0.0f);
        // Pixel x = -2 (2 units outside): signed distance -2 -> alpha 1 (Center band covers it).
        buildMaskRow(E, 50.0, -2.0, 1.0, 1, 0.0, false, band, mask, dist, ins);
        check("row outside flag", ins[0] == 0);
        sdist = ins[0] ? (double) dist[0] : -(double) dist[0];
        check("row alpha outside", near(strokeAlpha(sdist, strokeW, kStrokeCenter, soft, 1.0f), 1.0, 1e-6));
    }

    printf("%d checks, %d failures\n", gChecks, gFails);
    printf(gFails == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return gFails == 0 ? 0 : 1;
}
