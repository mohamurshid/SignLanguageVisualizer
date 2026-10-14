#include "Geometry.h"

namespace geom {

std::vector<Vec2> toVec2(const std::vector<IPoint>& poly) {
    std::vector<Vec2> out;
    out.reserve(poly.size());
    for (const IPoint& p : poly) out.push_back(::toVec2(p));
    return out;
}

std::vector<IPoint> toPixels(const std::vector<Vec2>& poly) {
    std::vector<IPoint> out;
    out.reserve(poly.size());
    for (const Vec2& p : poly) out.push_back(toPixel(p));
    return out;
}

double signedArea(const std::vector<Vec2>& poly) {
    const size_t n = poly.size();
    if (n < 3) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % n];
        sum += a.x * b.y - b.x * a.y;
    }
    return sum / 2.0;
}

bool isConvex(const std::vector<Vec2>& poly) {
    const size_t n = poly.size();
    if (n < 3) return false;
    bool sawLeft = false;
    bool sawRight = false;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % n];
        const Vec2& c = poly[(i + 2) % n];
        // z-component of the cross product (b - a) x (c - b):
        // positive = left turn, negative = right turn, zero = straight on.
        const double cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
        if (cross > 1e-9) sawLeft = true;
        if (cross < -1e-9) sawRight = true;
        if (sawLeft && sawRight) return false;
    }
    return true;
}

bool pointInPolygon(Vec2 p, const std::vector<Vec2>& poly) {
    const size_t n = poly.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[j];
        // Does edge a-b cross the horizontal ray going right from p?
        // (a.y > p.y) != (b.y > p.y) counts each vertex once, like the
        // "exclude the top row" rule in scan-line fill.
        if ((a.y > p.y) != (b.y > p.y)) {
            const double xCross = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (p.x < xCross) inside = !inside;
        }
    }
    return inside;
}

Vec2 centroid(const std::vector<Vec2>& poly) {
    const size_t n = poly.size();
    const double area = signedArea(poly);
    if (n < 3 || std::fabs(area) < 1e-12) {
        // Degenerate: fall back to the average of the vertices.
        Vec2 sum{0.0, 0.0};
        for (const Vec2& p : poly) sum = sum + p;
        return n ? sum * (1.0 / static_cast<double>(n)) : sum;
    }
    double cx = 0.0;
    double cy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % n];
        const double cross = a.x * b.y - b.x * a.y;
        cx += (a.x + b.x) * cross;
        cy += (a.y + b.y) * cross;
    }
    return {cx / (6.0 * area), cy / (6.0 * area)};
}

}  // namespace geom
