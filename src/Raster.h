#pragma once
// Raster: the scan-conversion algorithms.
//
// Each function turns a shape (a line, a circle, an ellipse, a polygon) into
// individual pixels, using only Framebuffer::setPixel(). No graphics library
// is used anywhere in these files.
//
//   RasterLines.cpp    : DDA, Bresenham, line attributes (dashed, thick)
//   RasterCircles.cpp  : midpoint circle (outline, filled, 8 octants)
//   RasterEllipses.cpp : midpoint ellipse (outline, filled, 2 regions)
//   RasterPolygons.cpp : polygon outline, scan-line fill (Edge Table +
//                        Active Edge Table)
//
// Curves (Curves.h) and clipping (Clipping.h) are geometry: they produce
// points and polygons that are then drawn with these functions.

#include <vector>

#include "Framebuffer.h"
#include "Geometry.h"

enum class LineAlgo { DDA, Bresenham };
const char* lineAlgoName(LineAlgo algo);

namespace raster {

// ---- Lines ---------------------------------------------------------------
// DDA: steps along the longer axis, adding a fractional increment to the
// other axis each step and rounding to the nearest pixel (uses floats).
void lineDDA(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c);

// Bresenham: same idea, but decides "step or not" with an integer decision
// parameter, so it needs no floating point or rounding at all.
void lineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c);

// Calls whichever line algorithm is selected.
void line(Framebuffer& fb, LineAlgo algo, int x0, int y0, int x1, int y1, Color c);

// Joins consecutive points with lines (used to draw sampled curves).
void polyline(Framebuffer& fb, const std::vector<IPoint>& pts, LineAlgo algo, Color c);

// ---- Line attributes -------------------------------------------------------
// Line style: bit (i mod 16) of 'pattern' says whether the i-th pixel along
// the line is drawn. 0x0F0F = 4 on, 4 off. Uses Bresenham.
void lineDashed(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c, unsigned pattern = 0x0F0F);

// Line width: Bresenham, but each pixel is replaced by a short run of
// pixels across the line (pixel replication).
void lineThick(Framebuffer& fb, int x0, int y0, int x1, int y1, int width, Color c);

// ---- Circles -------------------------------------------------------------
// Midpoint circle: computes one 1/8 arc with an integer decision parameter
// and mirrors each point into all eight octants.
void circleMidpoint(Framebuffer& fb, int xc, int yc, int r, Color c);

// Same circle, but each of the 8 mirrored copies gets its own colour,
// to show the 8-way symmetry.
void circleOctants(Framebuffer& fb, int xc, int yc, int r, const Color octantColors[8]);

// Filled circle: same midpoint loop, but each pair of mirrored points is
// joined with a horizontal span instead of being plotted as single points.
void fillCircle(Framebuffer& fb, int xc, int yc, int r, Color c);

// ---- Ellipses ------------------------------------------------------------
// Midpoint ellipse: two regions (slope > -1, then slope < -1) with their own
// decision parameters; each point is mirrored into the four quadrants.
void ellipseMidpoint(Framebuffer& fb, int xc, int yc, int rx, int ry, Color c);

// Same ellipse with region 1 and region 2 in different colours.
void ellipseRegions(Framebuffer& fb, int xc, int yc, int rx, int ry, Color region1, Color region2);

// Filled ellipse: horizontal spans between mirrored points.
void fillEllipse(Framebuffer& fb, int xc, int yc, int rx, int ry, Color c);

// ---- Polygons ------------------------------------------------------------
// Outline: draws each edge (v0-v1, v1-v2, ..., vN-v0) with a line algorithm.
void polygonOutline(Framebuffer& fb, const std::vector<IPoint>& pts, LineAlgo algo, Color c);

// Scan-line fill: works for convex AND concave polygons (even-odd rule).
void scanlineFill(Framebuffer& fb, const std::vector<IPoint>& pts, Color c);

// Horizontal run of pixels from x0 to x1 (inclusive) on row y.
void hspan(Framebuffer& fb, int x0, int x1, int y, Color c);

}  // namespace raster
