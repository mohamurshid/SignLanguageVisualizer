#pragma once
// Canvas: draws shapes into a Framebuffer, CLIPPED to a rectangle.
//
// This is where clipping meets scan conversion:
//   line, dashed line, polyline, polygon outline
//        -> Cohen-Sutherland, then DDA or Bresenham
//   filled polygon
//        -> Sutherland-Hodgman, then scan-line fill
//   circle, ellipse, thick line
//        -> point clipping (the framebuffer scissor test), because
//           the clipping algorithms above work on straight edges only
//
// The hand renderer and the test pages draw everything through a Canvas, so
// the same drawing code works for the full screen and for a small viewport.

#include <vector>

#include "Framebuffer.h"
#include "Geometry.h"
#include "Raster.h"

class Canvas {
public:
    Canvas(Framebuffer& fb, ClipRect clip, LineAlgo algo);

    Framebuffer& framebuffer() { return fb_; }
    const ClipRect& clipRect() const { return clip_; }
    LineAlgo lineAlgo() const { return algo_; }

    void line(Vec2 a, Vec2 b, Color c);
    void dashedLine(Vec2 a, Vec2 b, Color c, unsigned pattern = 0x0F0F);
    void thickLine(Vec2 a, Vec2 b, int width, Color c);
    void polyline(const std::vector<Vec2>& pts, Color c);
    void dashedPolyline(const std::vector<Vec2>& pts, Color c, unsigned pattern = 0x0F0F);

    void polygonOutline(const std::vector<Vec2>& poly, Color c);
    void dashedPolygonOutline(const std::vector<Vec2>& poly, Color c, unsigned pattern = 0x0F0F);
    void fillPolygon(const std::vector<Vec2>& poly, Color c);

    void circle(Vec2 centre, double r, Color c);
    void fillCircle(Vec2 centre, double r, Color c);
    void ellipse(Vec2 centre, double rx, double ry, Color c);
    void fillEllipse(Vec2 centre, double rx, double ry, Color c);

    // Fills the whole clip rectangle (used to clear a viewport).
    void fillClipRect(Color c);
    // Draws the border of a rectangle (inside this canvas's clip rectangle).
    void rectOutline(const ClipRect& r, Color c, bool dashed = false);

private:
    // Runs 'draw' with the framebuffer scissor set to the clip rectangle.
    template <typename Draw>
    void withScissor(Draw draw);

    Framebuffer& fb_;
    ClipRect clip_;
    LineAlgo algo_;
};
