#pragma once
// TransformedShapes: drawing primitives that have been through a matrix.
//
// Lines and polygons survive any affine transformation: transform their
// vertices and draw them as before. Circles and ellipses are different:
//   - the midpoint ellipse algorithm only draws AXIS-ALIGNED ellipses,
//   - a rotated (or sheared) ellipse is still an ellipse, but tilted.
// So an ellipse given in local coordinates is drawn like this:
//   matrix has no rotation/shear -> midpoint ellipse with scaled radii
//   otherwise                    -> sample points on the ellipse
//                                   (cx + rx cos t, cy + ry sin t),
//                                   transform them, and scan-line fill the
//                                   polygon (or draw its outline)
//
// Also here: drawing a matrix's local coordinate frame (its x and y axes),
// which shows the matrix stack at work.

#include "Canvas.h"
#include "Geometry.h"
#include "Transform.h"

namespace shapes {

// Ellipse with centre and radii in LOCAL coordinates, drawn through 'toScreen'.
void ellipse(Canvas& canvas, const Mat3& toScreen, Vec2 centre, double rx, double ry, Color c, bool filled);

// The ellipse as a polygon in local coordinates (used when it is rotated).
std::vector<Vec2> ellipsePolygon(Vec2 centre, double rx, double ry, int segments);

// Arrow from a to b (screen pixels), with a small V-shaped head.
void arrow(Canvas& canvas, Vec2 a, Vec2 b, Color c);

// The local x-axis (red) and y-axis (green) of 'frame', drawn through
// 'toScreen'. 'length' is in local units.
void frameAxes(Canvas& canvas, const Mat3& toScreen, const Mat3& frame, double length);

}  // namespace shapes
