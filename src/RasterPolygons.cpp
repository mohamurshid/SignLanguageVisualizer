// Polygons: outline and scan-line fill (Edge Table + Active Edge Table).
#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace raster {

// =========================================================================
// Polygons
// =========================================================================
void hspan(Framebuffer& fb, int x0, int x1, int y, Color c) {
    if (y < 0 || y >= fb.height()) return;
    if (x0 > x1) std::swap(x0, x1);
    x0 = std::max(x0, 0);  // clip the span to the screen
    x1 = std::min(x1, fb.width() - 1);
    for (int x = x0; x <= x1; ++x) fb.setPixel(x, y, c);
}

void polygonOutline(Framebuffer& fb, const std::vector<IPoint>& pts, LineAlgo algo, Color c) {
    const size_t n = pts.size();
    if (n == 0) return;
    for (size_t i = 0; i < n; ++i) {
        const IPoint& a = pts[i];
        const IPoint& b = pts[(i + 1) % n];  // last vertex joins back to the first
        line(fb, algo, a.x, a.y, b.x, b.y, c);
    }
}

// -------------------------------------------------------------------------
// Scan-line polygon fill (Edge Table + Active Edge Table)
// -------------------------------------------------------------------------
// Idea: sweep a horizontal line from the bottom of the polygon to the top.
// On each row, find where the row crosses the polygon's edges, sort those
// crossings by x, and fill BETWEEN pairs of crossings (1st-2nd, 3rd-4th...).
// The inside/outside switches at every crossing (the "odd-even rule"), which
// is why concave shapes are filled correctly.
//
// Edge Table (ET): every non-horizontal edge, bucketed by its lowest y.
//   For each edge we store: yMax, x at the bottom, and 1/m = dx/dy.
// Active Edge Table (AET): the edges crossing the current row.
//   Each row: add edges that start here, drop edges that end here,
//   sort by x, fill the pairs, then move every edge's x up by 1/m.
//
// Edges cover the rows ymin <= y < ymax (top row excluded). This makes a
// vertex shared by two edges count exactly once, so spans pair up correctly.
// Horizontal edges are skipped: the edges around them already bound the span.
namespace {
struct Edge {
    int yMax;         // the row where this edge stops being active
    double x;         // where the edge crosses the current row
    double invSlope;  // 1/m: how much x changes when y goes up by 1
};
}  // namespace

void scanlineFill(Framebuffer& fb, const std::vector<IPoint>& pts, Color c) {
    const size_t n = pts.size();
    if (n < 3) return;

    int yMin = pts[0].y;
    int yMax = pts[0].y;
    for (const IPoint& p : pts) {
        yMin = std::min(yMin, p.y);
        yMax = std::max(yMax, p.y);
    }
    if (yMin == yMax) return;  // zero-height polygon: nothing to fill

    // 1. Build the Edge Table.
    std::vector<std::vector<Edge>> edgeTable(static_cast<size_t>(yMax - yMin + 1));
    for (size_t i = 0; i < n; ++i) {
        IPoint a = pts[i];
        IPoint b = pts[(i + 1) % n];
        if (a.y == b.y) continue;   // skip horizontal edges
        if (a.y > b.y) std::swap(a, b);  // make 'a' the lower end
        Edge e;
        e.yMax = b.y;
        e.x = a.x;
        e.invSlope = static_cast<double>(b.x - a.x) / (b.y - a.y);
        edgeTable[static_cast<size_t>(a.y - yMin)].push_back(e);
    }

    // 2. Sweep upwards, one scan line at a time.
    std::vector<Edge> active;
    const int yStart = yMin;
    const int yEnd = std::min(yMax, fb.height());  // rows above the screen are skipped
    for (int y = yStart; y < yEnd; ++y) {
        // a) Add edges whose bottom is on this row.
        for (const Edge& e : edgeTable[static_cast<size_t>(y - yMin)]) active.push_back(e);

        // b) Remove edges that have finished (top row is excluded).
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [y](const Edge& e) { return e.yMax <= y; }),
                     active.end());

        // c) Sort crossings left to right and fill between pairs.
        std::sort(active.begin(), active.end(),
                  [](const Edge& a, const Edge& b) { return a.x < b.x; });
        if (y >= 0) {
            for (size_t i = 0; i + 1 < active.size(); i += 2) {
                const int xLeft = static_cast<int>(std::ceil(active[i].x));
                const int xRight = static_cast<int>(std::floor(active[i + 1].x));
                if (xLeft <= xRight) hspan(fb, xLeft, xRight, y, c);
            }
        }

        // d) Move every active edge up to the next row: x = x + 1/m.
        for (Edge& e : active) e.x += e.invSlope;
    }
}

}  // namespace raster
