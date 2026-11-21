// AlphaDistTest.cpp
// Standalone check of AlphaDist.h (distance to the alpha edge).
// No OpenFX, no Resolve.
// Compile in the x64 Native Tools prompt:
//   cd /d D:\DavinciCoding\OpenfxProject\BarryStroke\tests
//   cl /EHsc /std:c++17 AlphaDistTest.cpp /Fe:AlphaDistTest.exe
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/AlphaDist.h"

static int gFails = 0, gChecks = 0;
static void check(const char *name, bool ok)
{
    ++gChecks;
    if (!ok) { ++gFails; printf("FAIL %s\n", name); }
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main()
{
    // ---- square 10x10 inside a 20x20 field (inside = x,y in [5,15)) ----
    const int W = 20, H = 20;
    std::vector<unsigned char> mask((size_t) W * H, 0);
    for (int y = 5; y < 15; ++y)
        for (int x = 5; x < 15; ++x)
            mask[(size_t) y * W + x] = 1;
    std::vector<float> dist((size_t) W * H, 0.0f);
    alphaDistance(mask.data(), W, H, 100.0f, dist.data());
    auto at = [&](int x, int y) { return (double) dist[(size_t) y * W + x]; };

    check("inside edge = 1", near(at(5, 10), 1.0, 1e-6));
    check("inside 2 deep = 2", near(at(6, 10), 2.0, 1e-6));
    check("inside centre = 5", near(at(9, 10), 5.0, 1e-6));
    check("outside edge = 1", near(at(4, 10), 1.0, 1e-6));
    check("outside 2 away = 2", near(at(3, 10), 2.0, 1e-6));
    check("outside far = 5", near(at(0, 10), 5.0, 1e-6));
    // Diagonal from the corner: chamfer approximates sqrt(2) per step.
    check("diag ~1.41", near(at(4, 4), 1.4142, 0.05));
    check("diag 2 ~2.83", near(at(3, 3), 2.8284, 0.1));

    // ---- cap: distances stop at maxDist ----
    {
        std::vector<float> d2((size_t) W * H, 0.0f);
        alphaDistance(mask.data(), W, H, 3.0f, d2.data());
        check("capped centre", near((double) d2[(size_t) 10 * W + 9], 3.0, 1e-6));
        check("capped far outside", near((double) d2[(size_t) 10 * W + 0], 3.0, 1e-6));
        check("uncapped near", near((double) d2[(size_t) 10 * W + 5], 1.0, 1e-6));
    }

    // ---- all inside: no edge, everything capped ----
    {
        std::vector<unsigned char> all((size_t) W * H, 1);
        std::vector<float> d3((size_t) W * H, 0.0f);
        alphaDistance(all.data(), W, H, 7.0f, d3.data());
        check("all inside capped", near((double) d3[(size_t) 10 * W + 10], 7.0, 1e-6));
    }

    // ---- all outside: no edge, everything capped ----
    {
        std::vector<unsigned char> none((size_t) W * H, 0);
        std::vector<float> d4((size_t) W * H, 0.0f);
        alphaDistance(none.data(), W, H, 7.0f, d4.data());
        check("all outside capped", near((double) d4[(size_t) 10 * W + 10], 7.0, 1e-6));
    }

    // ---- single inside pixel ----
    {
        std::vector<unsigned char> one((size_t) W * H, 0);
        one[(size_t) 10 * W + 10] = 1;
        std::vector<float> d5((size_t) W * H, 0.0f);
        alphaDistance(one.data(), W, H, 100.0f, d5.data());
        check("single centre = 1", near((double) d5[(size_t) 10 * W + 10], 1.0, 1e-6));
        check("single neighbour = 1", near((double) d5[(size_t) 10 * W + 11], 1.0, 1e-6));
        check("single 2 away = 2", near((double) d5[(size_t) 10 * W + 12], 2.0, 1e-6));
    }

    // ---- empty input: no crash, no output ----
    {
        std::vector<float> d6(4, 0.0f);
        alphaDistance(nullptr, 0, 0, 10.0f, d6.data());
        check("null safe", true);
    }

    printf("%d checks, %d failures\n", gChecks, gFails);
    printf(gFails == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return gFails == 0 ? 0 : 1;
}
