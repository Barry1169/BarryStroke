// Inspector.h
// The Inspector: defining every parameter, and showing or hiding them while the plugin runs.
#pragma once
#include <string>
#include "Host.h"

// DescribeInContext: clips and all parameters, in Inspector order.
OfxStatus describeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs);

// What the Inspector was last arranged for. Empty = arrange it again at the next chance.
extern std::string gVisSig;

// Show or hide every conditional parameter, from the current checkbox values.
void applyVisibility(OfxParamSetHandle paramSet);

// Touches the Inspector only when something changed.
void syncVisibility(OfxParamSetHandle paramSet, const void *instKey);

// Open or close a folder while the plugin runs.
void setGroupOpen(OfxParamSetHandle paramSet, const char *name, bool open);
