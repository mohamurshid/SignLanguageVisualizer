#pragma once
// TransformScene: pages that show each 2D transformation on its own,
// applied to a finger-shaped object.
//
//   Page 1  Basic transformations : translation, rotation about the origin,
//                                   rotation about a pivot (the knuckle),
//                                   scaling about a fixed point, reflection,
//                                   shear
//   Page 2  Composite transformations and the matrix stack : the order of
//                                   matrices matters; rotation about a pivot
//                                   step by step; a finger built with
//                                   push / rotate / translate and its frames
//
// Dashed outline = the object before; filled = after the transformation.

#include <string>
#include <vector>

#include "Framebuffer.h"
#include "HandRenderer.h"
#include "TestScene.h"  // PageLabel

int transformPageCount();
const char* transformPageTitle(int page);
const char* transformPageFileName(int page);
void drawTransformPage(Framebuffer& fb, int page, const RenderSettings& settings);
std::vector<PageLabel> transformPageLabels(int page);
