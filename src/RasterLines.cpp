// Lines: DDA, Bresenham, and line attributes (dashed and thick lines).
#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

const char* lineAlgoName(LineAlgo algo) {
    return algo == LineAlgo::DDA ? "DDA" : "Bresenham";
}

namespace raster {

// =========================================================================
// DDA (Digital Differential Analyzer)
// =========================================================================
// The line from (x0,y0) to (x1,y1) has dx = x1-x0 and dy = y1-y0.
// We take  steps = max(|dx|, |dy|)  so that we move exactly ONE pixel per
// step along the longer axis (otherwise a steep line would have gaps).
// Each step adds  xInc = dx/steps  and  yInc = dy/steps , one of which is
// +-1 and the other a fraction, then rounds to the nearest pixel.
// Works for every direction because the signs are carried in xInc/yInc.
void lineDDA(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0) {  // start and end are the same pixel
        fb.setPixel(x0, y0, c);
        return;
    }

    const double xInc = static_cast<double>(dx) / steps;
    const double yInc = static_cast<double>(dy) / steps;

    double x = x0;
    double y = y0;
    for (int i = 0; i <= steps; ++i) {
        fb.setPixel(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), c);
        x += xInc;
        y += yInc;
    }
}

// =========================================================================
// Bresenham line algorithm
// =========================================================================
// Case 1: gentle slope (|dx| >= |dy|). We step x by one pixel each time and
// must decide whether y also steps. The decision parameter
//      p0 = 2|dy| - |dx|
// tells us which of the two candidate pixels is closer to the true line:
//      p <  0 : stay on the same row         p = p + 2|dy|
//      p >= 0 : step y as well               p = p + 2|dy| - 2|dx|
// Case 2: steep slope (|dy| > |dx|). Identical, with the roles of x and y
// swapped. sx and sy (+1 or -1) handle lines going left or down, so all
// eight octants are covered. Only integer addition is used.
//
// bresenhamWalk runs the algorithm and calls plot(x, y, i, steep) for each
// pixel it chooses: i is the step number and steep says which axis drives.
// The plain line, the dashed line and the thick line all share it.
namespace {
template <typename Plot>
void bresenhamWalk(int x0, int y0, int x1, int y1, Plot plot) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = (x1 >= x0) ? 1 : -1;
    const int sy = (y1 >= y0) ? 1 : -1;

    int x = x0;
    int y = y0;

    if (dx >= dy) {
        // Gentle slope: x is the driving axis.
        int p = 2 * dy - dx;
        for (int i = 0; i <= dx; ++i) {
            plot(x, y, i, false);
            x += sx;
            if (p < 0) {
                p += 2 * dy;
            } else {
                y += sy;
                p += 2 * dy - 2 * dx;
            }
        }
    } else {
        // Steep slope: y is the driving axis.
        int p = 2 * dx - dy;
        for (int i = 0; i <= dy; ++i) {
            plot(x, y, i, true);
            y += sy;
            if (p < 0) {
                p += 2 * dx;
            } else {
                x += sx;
                p += 2 * dx - 2 * dy;
            }
        }
    }
}
}  // namespace

void lineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c) {
    bresenhamWalk(x0, y0, x1, y1, [&](int x, int y, int, bool) { fb.setPixel(x, y, c); });
}

void line(Framebuffer& fb, LineAlgo algo, int x0, int y0, int x1, int y1, Color c) {
    if (algo == LineAlgo::DDA) {
        lineDDA(fb, x0, y0, x1, y1, c);
    } else {
        lineBresenham(fb, x0, y0, x1, y1, c);
    }
}

void polyline(Framebuffer& fb, const std::vector<IPoint>& pts, LineAlgo algo, Color c) {
    if (pts.size() == 1) fb.setPixel(pts[0].x, pts[0].y, c);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        line(fb, algo, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, c);
    }
}

// =========================================================================
// Line attributes
// =========================================================================
// Line style (dashed / dotted): walk the line with Bresenham as normal, but
// only plot pixel i when bit (i mod 16) of the pattern is 1.
//      0x0F0F = 1111000011110000 read from bit 0 : 4 on, 4 off (dashed)
//      0x3333 = 2 on, 2 off (dotted)
void lineDashed(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c, unsigned pattern) {
    bresenhamWalk(x0, y0, x1, y1, [&](int x, int y, int i, bool) {
        if ((pattern >> (i % 16)) & 1u) fb.setPixel(x, y, c);
    });
}

// Line width by pixel replication: for each pixel Bresenham chooses, plot a
// short run of pixels ACROSS the driving axis (a vertical run for a gentle
// line, a horizontal run for a steep one).
// A run of n pixels measured along an axis is only n * cos(angle) thick when
// measured at right angles to the line, so diagonal lines would come out
// thinner. We correct this by stretching the run:
//      run = width * length / max(|dx|, |dy|)
void lineThick(Framebuffer& fb, int x0, int y0, int x1, int y1, int width, Color c) {
    if (width <= 1) {
        lineBresenham(fb, x0, y0, x1, y1, c);
        return;
    }
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double major = std::max(std::fabs(dx), std::fabs(dy));
    const double length = std::sqrt(dx * dx + dy * dy);
    const int run = (major > 0.0) ? static_cast<int>(std::lround(width * length / major)) : width;
    const int below = (run - 1) / 2;  // pixels on one side of the centre line
    const int above = run - 1 - below;

    bresenhamWalk(x0, y0, x1, y1, [&](int x, int y, int, bool steep) {
        for (int k = -below; k <= above; ++k) {
            if (steep) {
                fb.setPixel(x + k, y, c);
            } else {
                fb.setPixel(x, y + k, c);
            }
        }
    });
}

}  // namespace raster
