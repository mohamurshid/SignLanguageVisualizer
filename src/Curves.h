#pragma once
// Curves: Bezier curves and splines.
//
// A curve is a formula P(t) that gives a point for every t. To draw it we
// evaluate it at many values of t ("sampling") and join the points with
// lines, so the result is a polyline that the line algorithms can draw, or a
// polygon that scan-line fill can fill.
//
//   Bezier (de Casteljau)    : repeated linear interpolation of control points
//   Bezier (Bernstein form)  : the same curve written as a weighted sum
//   Catmull-Rom spline       : passes THROUGH every control point
//   Uniform cubic B-spline   : smoother, passes NEAR the control points
//
// Used in the hand:
//   Bezier      -> palm crease lines (heart, head and life lines)
//   Catmull-Rom -> the smooth palm outline, and fingertip motion trails
//   B-spline    -> compared with Catmull-Rom on the curves test page

#include <vector>

#include "Geometry.h"

namespace curves {

// ---- Bezier ----------------------------------------------------------------
// de Casteljau: interpolate between neighbouring points at t, repeat on the
// new (one shorter) list until one point is left. Works for any degree.
Vec2 deCasteljau(const std::vector<Vec2>& ctrl, double t);

// Every level of the de Casteljau construction (level 0 = the control
// points, last level = the single curve point). Used to draw the steps.
std::vector<std::vector<Vec2>> deCasteljauLevels(const std::vector<Vec2>& ctrl, double t);

// Bernstein form: P(t) = sum C(n,i) t^i (1-t)^(n-i) P_i. Gives exactly the
// same point as de Casteljau.
Vec2 bezierBernstein(const std::vector<Vec2>& ctrl, double t);

// 'segments' + 1 points along the curve, from t = 0 to t = 1.
std::vector<Vec2> sampleBezier(const std::vector<Vec2>& ctrl, int segments);

// ---- Splines ---------------------------------------------------------------
// One span of a spline is controlled by four neighbouring points p0..p3.
// Catmull-Rom: the span goes from p1 (t = 0) to p2 (t = 1).
Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double t);
// Uniform cubic B-spline: the span runs near p1 and p2 but not through them.
Vec2 uniformBSpline(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double t);

// Sample a whole spline through a list of points.
//   closed = true : the curve loops back to the first point (an outline)
//   closed = false: the curve starts at the first point and ends at the last
std::vector<Vec2> sampleCatmullRom(const std::vector<Vec2>& pts, int segmentsPerSpan, bool closed);
std::vector<Vec2> sampleBSpline(const std::vector<Vec2>& pts, int segmentsPerSpan, bool closed);

// ---- Helpers ---------------------------------------------------------------
double polylineLength(const std::vector<Vec2>& pts);
// Picks how many line segments a curve needs so that each one is about
// 'pixelsPerSegment' long on screen (longer curves get more segments).
int segmentsForLength(double pixelLength, double pixelsPerSegment = 4.0);

}  // namespace curves
