#include "Canvas.h"

#include <algorithm>
#include <cmath>

#include "Clipping.h"

namespace {
int roundRadius(double r) { return std::max(0, static_cast<int>(std::lround(r))); }
}  // namespace

Canvas::Canvas(Framebuffer& fb, ClipRect clip, LineAlgo algo) : fb_(fb), clip_(clip), algo_(algo) {
    // Never clip to more than the framebuffer itself.
    const ClipRect b = fb.bounds();
    clip_.xmin = std::max(clip_.xmin, b.xmin);
    clip_.ymin = std::max(clip_.ymin, b.ymin);
    clip_.xmax = std::min(clip_.xmax, b.xmax);
    clip_.ymax = std::min(clip_.ymax, b.ymax);
}

template <typename Draw>
void Canvas::withScissor(Draw draw) {
    const ClipRect previous = fb_.scissor();
    fb_.setScissor(clip_);
    draw();
    fb_.setScissor(previous);
}

// ---- Lines: Cohen-Sutherland, then the selected line algorithm ---------------
void Canvas::line(Vec2 a, Vec2 b, Color c) {
    if (!clip::cohenSutherland(a, b, clip_)) return;
    const IPoint p = toPixel(a);
    const IPoint q = toPixel(b);
    raster::line(fb_, algo_, p.x, p.y, q.x, q.y, c);
}

void Canvas::dashedLine(Vec2 a, Vec2 b, Color c, unsigned pattern) {
    if (!clip::cohenSutherland(a, b, clip_)) return;
    const IPoint p = toPixel(a);
    const IPoint q = toPixel(b);
    raster::lineDashed(fb_, p.x, p.y, q.x, q.y, c, pattern);
}

void Canvas::thickLine(Vec2 a, Vec2 b, int width, Color c) {
    // Clip the centre line first, then let the scissor trim the width.
    if (!clip::cohenSutherland(a, b, clip_)) return;
    const IPoint p = toPixel(a);
    const IPoint q = toPixel(b);
    withScissor([&] { raster::lineThick(fb_, p.x, p.y, q.x, q.y, width, c); });
}

void Canvas::polyline(const std::vector<Vec2>& pts, Color c) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) line(pts[i], pts[i + 1], c);
}

void Canvas::dashedPolyline(const std::vector<Vec2>& pts, Color c, unsigned pattern) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) dashedLine(pts[i], pts[i + 1], c, pattern);
}

// ---- Polygons -------------------------------------------------------------------
// An outline is clipped edge by edge (Cohen-Sutherland). Clipping it with
// Sutherland-Hodgman instead would add new edges along the window border.
void Canvas::polygonOutline(const std::vector<Vec2>& poly, Color c) {
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) line(poly[i], poly[(i + 1) % n], c);
}

void Canvas::dashedPolygonOutline(const std::vector<Vec2>& poly, Color c, unsigned pattern) {
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) dashedLine(poly[i], poly[(i + 1) % n], c, pattern);
}

// A filled polygon is clipped as a whole (Sutherland-Hodgman), then filled.
// Scan-line fill leaves out the top row of every polygon (so that shared
// edges are not drawn twice), so the window is made one row taller here:
// the filled pixels then cover exactly the rows ymin..ymax.
void Canvas::fillPolygon(const std::vector<Vec2>& poly, Color c) {
    ClipRect fillWindow = clip_;
    fillWindow.ymax += 1;
    const std::vector<Vec2> clipped = clip::sutherlandHodgman(poly, fillWindow);
    if (clipped.size() < 3) return;
    raster::scanlineFill(fb_, geom::toPixels(clipped), c);
}

// ---- Circles and ellipses: point clipping -----------------------------------
void Canvas::circle(Vec2 centre, double r, Color c) {
    const IPoint p = toPixel(centre);
    withScissor([&] { raster::circleMidpoint(fb_, p.x, p.y, roundRadius(r), c); });
}

void Canvas::fillCircle(Vec2 centre, double r, Color c) {
    const IPoint p = toPixel(centre);
    withScissor([&] { raster::fillCircle(fb_, p.x, p.y, roundRadius(r), c); });
}

void Canvas::ellipse(Vec2 centre, double rx, double ry, Color c) {
    const IPoint p = toPixel(centre);
    withScissor([&] { raster::ellipseMidpoint(fb_, p.x, p.y, roundRadius(rx), roundRadius(ry), c); });
}

void Canvas::fillEllipse(Vec2 centre, double rx, double ry, Color c) {
    const IPoint p = toPixel(centre);
    withScissor([&] { raster::fillEllipse(fb_, p.x, p.y, roundRadius(rx), roundRadius(ry), c); });
}

// ---- Rectangles -------------------------------------------------------------------
void Canvas::fillClipRect(Color c) {
    for (int y = clip_.ymin; y <= clip_.ymax; ++y) raster::hspan(fb_, clip_.xmin, clip_.xmax, y, c);
}

void Canvas::rectOutline(const ClipRect& r, Color c, bool dashed) {
    const std::vector<Vec2> corners = {
        {static_cast<double>(r.xmin), static_cast<double>(r.ymin)},
        {static_cast<double>(r.xmax), static_cast<double>(r.ymin)},
        {static_cast<double>(r.xmax), static_cast<double>(r.ymax)},
        {static_cast<double>(r.xmin), static_cast<double>(r.ymax)},
    };
    if (dashed) {
        dashedPolygonOutline(corners, c);
    } else {
        polygonOutline(corners, c);
    }
}
