#include "TransformedShapes.h"

#include <algorithm>
#include <cmath>

#include "Curves.h"

namespace shapes {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr Color kAxisX{240, 90, 90};
constexpr Color kAxisY{90, 230, 120};
}  // namespace

std::vector<Vec2> ellipsePolygon(Vec2 c, double rx, double ry, int segments) {
    std::vector<Vec2> pts;
    segments = std::max(8, segments);
    for (int i = 0; i < segments; ++i) {
        const double t = 2.0 * kPi * i / segments;
        pts.push_back({c.x + rx * std::cos(t), c.y + ry * std::sin(t)});
    }
    return pts;
}

void ellipse(Canvas& canvas, const Mat3& toScreen, Vec2 centre, double rx, double ry, Color c, bool filled) {
    if (toScreen.isAxisAligned(1e-6)) {
        // Only scaling and translation: still axis-aligned, so the midpoint
        // ellipse algorithm can draw it. |a| and |e| are the x and y scale
        // factors (absolute value, because a mirror makes them negative).
        const Vec2 p = toScreen.apply(centre);
        const double sx = std::fabs(toScreen.m[0][0]);
        const double sy = std::fabs(toScreen.m[1][1]);
        if (filled) {
            canvas.fillEllipse(p, rx * sx, ry * sy, c);
        } else {
            canvas.ellipse(p, rx * sx, ry * sy, c);
        }
        return;
    }
    // Rotated or sheared: transform a polygon approximation instead.
    const double screenRadius = std::max(rx, ry) * toScreen.uniformScale();
    const int segments = curves::segmentsForLength(2.0 * kPi * screenRadius, 3.0);
    const std::vector<Vec2> poly = xf::apply(toScreen, ellipsePolygon(centre, rx, ry, segments));
    if (filled) {
        canvas.fillPolygon(poly, c);
    } else {
        canvas.polygonOutline(poly, c);
    }
}

void arrow(Canvas& canvas, Vec2 a, Vec2 b, Color c) {
    canvas.line(a, b, c);
    const Vec2 d = b - a;
    const double len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1.0) return;
    // The head: the direction back along the arrow, rotated +-25 degrees.
    const double head = std::min(6.0, len * 0.35);
    const Vec2 back = d * (-head / len);
    canvas.line(b, b + xf::rotate(25).applyVector(back), c);
    canvas.line(b, b + xf::rotate(-25).applyVector(back), c);
}

void frameAxes(Canvas& canvas, const Mat3& toScreen, const Mat3& frame, double length) {
    const Mat3 m = toScreen * frame;
    const Vec2 o = m.apply({0.0, 0.0});
    arrow(canvas, o, m.apply({length, 0.0}), kAxisX);
    arrow(canvas, o, m.apply({0.0, length}), kAxisY);
}

}  // namespace shapes
