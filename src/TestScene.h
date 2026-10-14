#pragma once
// TestScene: algorithm pages that show each technique on its own.
//
//   Page 1  Lines     : DDA vs Bresenham (pixel comparison), line attributes
//   Page 2  Circles   : midpoint circle, 8-way symmetry, midpoint ellipse regions
//   Page 3  Polygons  : scan-line fill, even-odd rule, point-in-polygon
//   Page 4  Clipping  : Cohen-Sutherland lines, Sutherland-Hodgman polygon
//   Page 5  Curves    : Bezier with de Casteljau construction, Catmull-Rom vs B-spline

#include <string>
#include <vector>

#include "Framebuffer.h"
#include "HandRenderer.h"

int algorithmPageCount();
const char* algorithmPageTitle(int page);
const char* algorithmPageFileName(int page);  // e.g. "lines"

// Draws one page. 't' is the parameter shown by the de Casteljau
// construction on the curves page.
void drawAlgorithmPage(Framebuffer& fb, int page, const RenderSettings& settings, double t = 0.5);

// Text labels for the page, positioned as fractions of the width/height
// (fx, fy = centre of the text). Only the optional viewer draws text.
struct PageLabel {
    double fx, fy;
    std::string text;
};
std::vector<PageLabel> algorithmPageLabels(int page);

// Results from the last time the pages were drawn (shown on screen and
// checked by the tests).
int lastLineDifferenceCount();
struct ClipStats {
    int whole = 0, clipped = 0, rejected = 0;
    size_t polygonIn = 0, polygonOut = 0;
};
ClipStats lastClipStats();
