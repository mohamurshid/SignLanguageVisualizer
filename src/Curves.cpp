#include "Curves.h"

#include <algorithm>
#include <cmath>

namespace curves {

namespace {
Vec2 lerp(Vec2 a, Vec2 b, double t) { return a + (b - a) * t; }
}  // namespace

// ===========================================================================
// Bezier curves
// ===========================================================================
// For control points P0..Pn, de Casteljau's algorithm at parameter t:
//   level 0:  P0, P1, ..., Pn
//   level 1:  lerp(P0,P1,t), lerp(P1,P2,t), ..., lerp(Pn-1,Pn,t)
//   ...
//   level n:  one point = P(t)
// where lerp(A, B, t) = A + t (B - A) = (1 - t) A + t B.
// For a cubic (4 points) that is 3 + 2 + 1 = 6 interpolations per point.
std::vector<std::vector<Vec2>> deCasteljauLevels(const std::vector<Vec2>& ctrl, double t) {
    std::vector<std::vector<Vec2>> levels;
    if (ctrl.empty()) return levels;
    levels.push_back(ctrl);
    while (levels.back().size() > 1) {
        const std::vector<Vec2>& prev = levels.back();
        std::vector<Vec2> next;
        next.reserve(prev.size() - 1);
        for (size_t i = 0; i + 1 < prev.size(); ++i) next.push_back(lerp(prev[i], prev[i + 1], t));
        levels.push_back(next);
    }
    return levels;
}

Vec2 deCasteljau(const std::vector<Vec2>& ctrl, double t) {
    if (ctrl.empty()) return {0.0, 0.0};
    std::vector<Vec2> pts = ctrl;
    // Overwrite in place: after round k, pts[0..n-k] hold level k.
    for (size_t level = pts.size() - 1; level > 0; --level) {
        for (size_t i = 0; i < level; ++i) pts[i] = lerp(pts[i], pts[i + 1], t);
    }
    return pts[0];
}

// Bernstein form. For a cubic this is the familiar
//   P(t) = (1-t)^3 P0 + 3t(1-t)^2 P1 + 3t^2(1-t) P2 + t^3 P3
// The weights are never negative and always add up to 1, which is why the
// curve stays inside the convex hull of its control points.
Vec2 bezierBernstein(const std::vector<Vec2>& ctrl, double t) {
    if (ctrl.empty()) return {0.0, 0.0};
    const int n = static_cast<int>(ctrl.size()) - 1;
    Vec2 sum{0.0, 0.0};
    double binom = 1.0;  // C(n, i), updated as i increases
    for (int i = 0; i <= n; ++i) {
        const double weight = binom * std::pow(t, i) * std::pow(1.0 - t, n - i);
        sum = sum + ctrl[static_cast<size_t>(i)] * weight;
        binom = binom * (n - i) / (i + 1);
    }
    return sum;
}

std::vector<Vec2> sampleBezier(const std::vector<Vec2>& ctrl, int segments) {
    std::vector<Vec2> out;
    if (ctrl.empty()) return out;
    segments = std::max(1, segments);
    out.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        out.push_back(deCasteljau(ctrl, static_cast<double>(i) / segments));
    }
    return out;
}

// ===========================================================================
// Splines
// ===========================================================================
// Both splines below are built from cubic pieces ("spans"). Each span uses
// four neighbouring control points and a 4x4 basis matrix M:
//      P(t) = [t^3 t^2 t 1] * M * [p0 p1 p2 p3]^T
//
// Catmull-Rom (an interpolating cardinal spline, tension 0.5):
//            | -1  3 -3  1 |
//      1/2 * |  2 -5  4 -1 |    P(0) = p1, P(1) = p2, and the tangent at
//            | -1  0  1  0 |    p1 is (p2 - p0) / 2, so neighbouring spans
//            |  0  2  0  0 |    join smoothly (C1 continuity).
//
// Uniform cubic B-spline (approximating):
//            | -1  3 -3  1 |
//      1/6 * |  3 -6  3  0 |    P(0) = (p0 + 4 p1 + p2) / 6, so the curve
//            | -3  0  3  0 |    does NOT pass through p1. In exchange it is
//            |  1  4  1  0 |    smoother (C2) and stays inside the convex
//                               hull of its four control points.
Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double t) {
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double b0 = 0.5 * (-t3 + 2.0 * t2 - t);
    const double b1 = 0.5 * (3.0 * t3 - 5.0 * t2 + 2.0);
    const double b2 = 0.5 * (-3.0 * t3 + 4.0 * t2 + t);
    const double b3 = 0.5 * (t3 - t2);
    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

Vec2 uniformBSpline(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double t) {
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double b0 = (-t3 + 3.0 * t2 - 3.0 * t + 1.0) / 6.0;
    const double b1 = (3.0 * t3 - 6.0 * t2 + 4.0) / 6.0;
    const double b2 = (-3.0 * t3 + 3.0 * t2 + 3.0 * t + 1.0) / 6.0;
    const double b3 = t3 / 6.0;
    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

namespace {
// Shared sampling loop for both spline types.
// Closed curves wrap around the point list. Open curves repeat the end points
// (once for Catmull-Rom, twice for the B-spline) so that the curve starts at
// the first point and finishes at the last.
template <typename Basis>
std::vector<Vec2> sampleSpline(const std::vector<Vec2>& input, int segmentsPerSpan, bool closed, int endRepeats,
                               Basis basis) {
    std::vector<Vec2> out;
    const int n = static_cast<int>(input.size());
    if (n == 0) return out;
    if (n == 1) return input;
    segmentsPerSpan = std::max(1, segmentsPerSpan);

    std::vector<Vec2> pts;
    if (closed) {
        pts = input;
    } else {
        for (int k = 0; k < endRepeats; ++k) pts.push_back(input.front());
        pts.insert(pts.end(), input.begin(), input.end());
        for (int k = 0; k < endRepeats; ++k) pts.push_back(input.back());
    }
    const int m = static_cast<int>(pts.size());
    auto at = [&](int i) { return pts[static_cast<size_t>(((i % m) + m) % m)]; };

    // Closed: one span per point. Open: one span per group of 4 neighbours.
    const int spans = closed ? m : m - 3;
    for (int s = 0; s < spans; ++s) {
        const int first = closed ? s - 1 : s;
        // Skip t = 1 except on the very last open span, because it equals
        // t = 0 of the next span (closed curves are closed by the polygon).
        const bool lastOpenSpan = !closed && s == spans - 1;
        const int steps = lastOpenSpan ? segmentsPerSpan + 1 : segmentsPerSpan;
        for (int i = 0; i < steps; ++i) {
            const double t = static_cast<double>(i) / segmentsPerSpan;
            out.push_back(basis(at(first), at(first + 1), at(first + 2), at(first + 3), t));
        }
    }
    return out;
}
}  // namespace

std::vector<Vec2> sampleCatmullRom(const std::vector<Vec2>& pts, int segmentsPerSpan, bool closed) {
    return sampleSpline(pts, segmentsPerSpan, closed, 1, catmullRom);
}

std::vector<Vec2> sampleBSpline(const std::vector<Vec2>& pts, int segmentsPerSpan, bool closed) {
    return sampleSpline(pts, segmentsPerSpan, closed, 2, uniformBSpline);
}

// ===========================================================================
// Helpers
// ===========================================================================
double polylineLength(const std::vector<Vec2>& pts) {
    double total = 0.0;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const Vec2 d = pts[i + 1] - pts[i];
        total += std::sqrt(d.x * d.x + d.y * d.y);
    }
    return total;
}

int segmentsForLength(double pixelLength, double pixelsPerSegment) {
    const int n = static_cast<int>(std::ceil(pixelLength / std::max(1.0, pixelsPerSegment)));
    return std::min(200, std::max(4, n));
}

}  // namespace curves
