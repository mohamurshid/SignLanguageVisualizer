// Circles: midpoint circle algorithm (outline, filled, and coloured by octant).
#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace raster {

// =========================================================================
// Midpoint circle algorithm
// =========================================================================
// A circle is symmetric in 8 ways, so we only compute the arc from the top
// of the circle (0, r) clockwise to the 45-degree line where x == y, and
// mirror every point into the other seven octants.
//
// On that arc x always steps by 1; y either stays or steps down by 1. We test
// the MIDPOINT between those two candidate pixels against the circle function
//      f(x, y) = x^2 + y^2 - r^2   (negative inside, positive outside)
// Starting value: p0 = 1 - r  (the integer form of 5/4 - r)
//      p <  0 : midpoint is inside, keep y      p = p + 2x + 1
//      p >= 0 : midpoint is outside, y = y - 1  p = p + 2x + 1 - 2y
// (x and y here are the NEW values after stepping.)
//
// midpointCircleWalk runs the algorithm and calls visit(x, y) for every
// point of the first octant; the callers decide how to mirror it.
namespace {
template <typename Visit>
void midpointCircleWalk(int r, Visit visit) {
    int x = 0;
    int y = r;
    int p = 1 - r;
    visit(x, y);
    while (x < y) {
        ++x;
        if (p < 0) {
            p += 2 * x + 1;
        } else {
            --y;
            p += 2 * x + 1 - 2 * y;
        }
        visit(x, y);
    }
}
}  // namespace

void circleMidpoint(Framebuffer& fb, int xc, int yc, int r, Color c) {
    if (r <= 0) {
        fb.setPixel(xc, yc, c);
        return;
    }
    midpointCircleWalk(r, [&](int x, int y) {
        fb.setPixel(xc + x, yc + y, c);
        fb.setPixel(xc - x, yc + y, c);
        fb.setPixel(xc + x, yc - y, c);
        fb.setPixel(xc - x, yc - y, c);
        fb.setPixel(xc + y, yc + x, c);
        fb.setPixel(xc - y, yc + x, c);
        fb.setPixel(xc + y, yc - x, c);
        fb.setPixel(xc - y, yc - x, c);
    });
}

void circleOctants(Framebuffer& fb, int xc, int yc, int r, const Color oc[8]) {
    if (r <= 0) {
        fb.setPixel(xc, yc, oc[0]);
        return;
    }
    // Octants numbered anticlockwise from the positive x axis.
    midpointCircleWalk(r, [&](int x, int y) {
        fb.setPixel(xc + y, yc + x, oc[0]);
        fb.setPixel(xc + x, yc + y, oc[1]);  // the octant we actually compute
        fb.setPixel(xc - x, yc + y, oc[2]);
        fb.setPixel(xc - y, yc + x, oc[3]);
        fb.setPixel(xc - y, yc - x, oc[4]);
        fb.setPixel(xc - x, yc - y, oc[5]);
        fb.setPixel(xc + x, yc - y, oc[6]);
        fb.setPixel(xc + y, yc - x, oc[7]);
    });
}

void fillCircle(Framebuffer& fb, int xc, int yc, int r, Color c) {
    if (r <= 0) {
        fb.setPixel(xc, yc, c);
        return;
    }
    // Instead of 8 single points, join the mirrored points with 4 spans.
    midpointCircleWalk(r, [&](int x, int y) {
        hspan(fb, xc - x, xc + x, yc + y, c);
        hspan(fb, xc - x, xc + x, yc - y, c);
        hspan(fb, xc - y, xc + y, yc + x, c);
        hspan(fb, xc - y, xc + y, yc - x, c);
    });
}

}  // namespace raster
