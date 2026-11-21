// ShapeData.h
// Reading and writing the shape outline the stroke follows: the point slots
// pt0..pt15, the "order" string, and the preset shapes.
#pragma once
#include <vector>
#include "Host.h"

// Parse "order" and read each point slot at the given time. Points come back as FRACTIONS of the frame.
// Returns false when "order" is empty. slotsOut (optional) gets the slot number of each point.
bool readSlotPoints(OfxParamSetHandle paramSet, double time, std::vector<OfxPointD> &out,
                    std::vector<int> *slotsOut = nullptr);

// Set the Shape dropdown back to Custom (call inside an edit group when the user changes the shape by hand).
void setShapeKindCustom(OfxParamSetHandle paramSet);

// Remove every keyframe of one parameter.
void clearParamKeys(OfxParamHandle ph);

// Write one point into its slot (Auto Key decides keyframe or constant). newOrder (optional) adds the point.
void writeSlotPoint(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs,
                    int slot, const OfxPointD &p, const char *newOrder = nullptr);

// Write only the "order" string (delete a point, clear the shape).
void writeOrderOnly(OfxImageEffectHandle effect, const char *newOrder);

// kind: 0 = Triangle, 1 = Rectangle, 2 = Circle, 3 = Star, 4 = Heart, 5 = Arrow (see ShapeTemplates.h).
void applyShapeTemplate(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs, int kind);

// A new effect starts as a Triangle: if the shape is empty, the dropdown says Triangle and nothing was loaded yet, load it.
void ensureDefaultShape(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs);
