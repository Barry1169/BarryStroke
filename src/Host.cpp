// Host.cpp
#include "Host.h"
#include <cstdio>

// Debug log. OFF by default, so nobody else's machine ever gets a file written to a D: path.
// To debug, change the 0 below to 1, rebuild, and log.txt is written to your project folder.
#ifndef OFX_DEBUG_LOG
#define OFX_DEBUG_LOG 0
#endif

OfxHost               *gHost       = nullptr;
OfxImageEffectSuiteV1 *gEffectHost = nullptr;
OfxPropertySuiteV1    *gPropHost   = nullptr;
OfxParameterSuiteV1   *gParamHost  = nullptr;
OfxDrawSuiteV1        *gDrawHost   = nullptr;

void logLine(const char *text)
{
#if OFX_DEBUG_LOG
    FILE *f = nullptr;
    if (fopen_s(&f, "D:\\DavinciCoding\\OpenfxProject\\BarryStroke\\log.txt", "a") == 0 && f) {
        fprintf(f, "%s\n", text);
        fclose(f);
    }
#else
    (void) text;
#endif
}
