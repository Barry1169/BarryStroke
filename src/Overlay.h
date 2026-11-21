// Overlay.h
// The viewer overlay: draws the shape and handles clicks and drags.
#pragma once
#include "Host.h"

OfxStatus overlayMainV2(const char *action, const void *handle,
                        OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
