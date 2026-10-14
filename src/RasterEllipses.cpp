// Ellipses: midpoint ellipse algorithm (outline, filled, and coloured by region).
#include "Raster.h"

#include <cmath>

namespace raster {

// =========================================================================
// Midpoint ellipse algorithm
// =========================================================================
// An axis-aligned ellipse centred on (xc, yc) with radii rx and ry is
// symmetric in 4 ways, so we only compute the quarter from the top (0, ry)
// clockwise to the right (rx, 0) and mirror each point into the other three
// quadrants. (A circle has 8-way symmetry; an ellipse only 4, because x and
// y are no longer interchangeable.)
//
// Ellipse function: f(x, y) = ry^2 x^2 + rx^2 y^2 - rx^2 ry^2
//                   (negative inside, zero on, positive outside)
//
// The quarter is split where the slope of the curve is -1, which is where
//      2 ry^2 x  =  2 rx^2 y        (px = py below)
//
// Region 1 (slope between 0 and -1): x steps by 1 every time; y may step
// down. Test the midpoint (x + 1, y - 1/2):
//      p1_0 = ry^2 - rx^2 ry + rx^2 / 4
//      p1 <  0 : keep y         p1 = p1 + 2 ry^2 x + ry^2
//      p1 >= 0 : y = y - 1      p1 = p1 + 2 ry^2 x - 2 rx^2 y + ry^2
//
// Region 2 (slope steeper than -1): y steps down by 1 every time; x may
// step right. Test the midpoint (x + 1/2, y - 1):
//      p2_0 = ry^2 (x + 1/2)^2 + rx^2 (y - 1)^2 - rx^2 ry^2
//      p2 >  0 : keep x         p2 = p2 - 2 rx^2 y + rx^2
//      p2 <= 0 : x = x + 1      p2 = p2 + 2 ry^2 x - 2 rx^2 y + rx^2
// (x and y are the NEW values after stepping.)
//
// px = 2 ry^2 x and py = 2 rx^2 y are kept up to date by adding constants,
// so every step uses only integer additions. The two starting values contain
// quarters, so they are rounded to the nearest integer (as in Hearn & Baker).
namespace {
template <typename Visit>
void midpointEllipseWalk(int rx, int ry, Visit visit) {
    const long long rx2 = static_cast<long long>(rx) * rx;
    const long long ry2 = static_cast<long long>(ry) * ry;
    const long long twoRx2 = 2 * rx2;
    const long long twoRy2 = 2 * ry2;

    long long x = 0;
    long long y = ry;
    long long px = 0;           // 2 ry^2 x
    long long py = twoRx2 * y;  // 2 rx^2 y
    visit(static_cast<int>(x), static_cast<int>(y), 1);

    // Region 1
    long long p = std::llround(static_cast<double>(ry2) - static_cast<double>(rx2) * ry + 0.25 * static_cast<double>(rx2));
    while (px < py) {
        ++x;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            --y;
            py -= twoRx2;
            p += ry2 + px - py;
        }
        visit(static_cast<int>(x), static_cast<int>(y), 1);
    }

    // Region 2
    const double hx = static_cast<double>(x) + 0.5;
    const double ym1 = static_cast<double>(y) - 1.0;
    p = std::llround(static_cast<double>(ry2) * hx * hx + static_cast<double>(rx2) * ym1 * ym1 -
                     static_cast<double>(rx2) * static_cast<double>(ry2));
    while (y > 0) {
        --y;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            ++x;
            px += twoRy2;
            p += rx2 - py + px;
        }
        visit(static_cast<int>(x), static_cast<int>(y), 2);
    }
}

// Ellipses with a zero radius are straight lines; the algorithm above
// would not draw them correctly (with ry = 0 it stops at the first point).
bool degenerateEllipse(Framebuffer& fb, int xc, int yc, int rx, int ry, Color c) {
    if (rx < 0) rx = -rx;
    if (ry < 0) ry = -ry;
    if (rx == 0 && ry == 0) {
        fb.setPixel(xc, yc, c);
        return true;
    }
    if (ry == 0) {
        hspan(fb, xc - rx, xc + rx, yc, c);
        return true;
    }
    if (rx == 0) {
        for (int y = yc - ry; y <= yc + ry; ++y) fb.setPixel(xc, y, c);
        return true;
    }
    return false;
}
}  // namespace

void ellipseRegions(Framebuffer& fb, int xc, int yc, int rx, int ry, Color region1, Color region2) {
    if (degenerateEllipse(fb, xc, yc, rx, ry, region1)) return;
    midpointEllipseWalk(rx, ry, [&](int x, int y, int region) {
        const Color c = region == 1 ? region1 : region2;
        fb.setPixel(xc + x, yc + y, c);
        fb.setPixel(xc - x, yc + y, c);
        fb.setPixel(xc + x, yc - y, c);
        fb.setPixel(xc - x, yc - y, c);
    });
}

void ellipseMidpoint(Framebuffer& fb, int xc, int yc, int rx, int ry, Color c) {
    ellipseRegions(fb, xc, yc, rx, ry, c, c);
}

void fillEllipse(Framebuffer& fb, int xc, int yc, int rx, int ry, Color c) {
    if (degenerateEllipse(fb, xc, yc, rx, ry, c)) return;
    // Join each pair of mirrored points with a horizontal span.
    midpointEllipseWalk(rx, ry, [&](int x, int y, int) {
        hspan(fb, xc - x, xc + x, yc + y, c);
        hspan(fb, xc - x, xc + x, yc - y, c);
    });
}

}  // namespace raster
