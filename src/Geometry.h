#pragma once
// Geometry: the basic 2D types shared by every module, plus polygon operations.
//
//   IPoint   : a pixel position (whole numbers)
//   Vec2     : an exact position (real numbers), used before rounding to pixels
//   ClipRect : an axis-aligned rectangle used as a clip window or viewport
//
// Polygon operations (namespace geom):
//   signedArea     : shoelace formula (also tells us the vertex order)
//   isConvex       : checks the turn direction at every vertex
//   pointInPolygon : even-odd ray-casting test
//   centroid       : centre of mass of the polygon's area

#include <cmath>
#include <vector>

struct IPoint {
    int x, y;
};

struct Vec2 {
    double x, y;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double k) { return {a.x * k, a.y * k}; }
inline Vec2 operator*(double k, Vec2 a) { return {a.x * k, a.y * k}; }

// Rounds an exact position to the nearest pixel.
inline IPoint toPixel(Vec2 p) {
    return {static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y))};
}
inline Vec2 toVec2(IPoint p) { return {static_cast<double>(p.x), static_cast<double>(p.y)}; }

// Inclusive rectangle: covers the pixels xmin..xmax and ymin..ymax.
struct ClipRect {
    int xmin, ymin, xmax, ymax;

    bool contains(int x, int y) const { return x >= xmin && x <= xmax && y >= ymin && y <= ymax; }
    int width() const { return xmax - xmin + 1; }
    int height() const { return ymax - ymin + 1; }
};

namespace geom {

std::vector<Vec2> toVec2(const std::vector<IPoint>& poly);
std::vector<IPoint> toPixels(const std::vector<Vec2>& poly);

// Shoelace formula: A = 1/2 * sum(x_i * y_(i+1) - x_(i+1) * y_i).
// Positive when the vertices go anticlockwise, negative when clockwise.
double signedArea(const std::vector<Vec2>& poly);

// True if every turn along the boundary goes the same way.
bool isConvex(const std::vector<Vec2>& poly);

// Even-odd rule: cast a ray to the right of p and count edge crossings.
// An odd count means p is inside. (The same rule scan-line fill uses.)
bool pointInPolygon(Vec2 p, const std::vector<Vec2>& poly);

// Area centroid of a simple (non-self-intersecting) polygon.
Vec2 centroid(const std::vector<Vec2>& poly);

}  // namespace geom
