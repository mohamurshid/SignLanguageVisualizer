#pragma once
// Clipping: keep only the part of a shape that lies inside a clip window.
//
//   Cohen-Sutherland   : clips a LINE against a rectangle using 4-bit
//                        region codes ("outcodes")
//   Sutherland-Hodgman : clips a POLYGON against a rectangle, one window
//                        edge at a time
//
// Clipping happens BEFORE scan conversion, so the line and fill algorithms
// never produce pixels outside the window.
//
// Used in the hand:
//   - every line and polygon is clipped to the screen before it is drawn
//   - the "detail view" inset magnifies part of the hand into a small
//     viewport; everything drawn there is clipped to that viewport

#include <vector>

#include "Geometry.h"

namespace clip {

// Region codes: one bit for each side of the window the point is beyond.
//      1001 | 1000 | 1010          TOP    = 8
//      -----+------+-----          BOTTOM = 4
//      0001 | 0000 | 0010          RIGHT  = 2
//      -----+------+-----          LEFT   = 1
//      0101 | 0100 | 0110
enum Outcode : int { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

int outcode(double x, double y, const ClipRect& r);

// Result of clipping a line, for statistics and the test page.
enum class LineResult { AcceptedWhole, Clipped, Rejected };

// Cohen-Sutherland. Moves a and b onto the visible part of the line.
// Returns false (and leaves a, b unspecified) if nothing is visible.
bool cohenSutherland(Vec2& a, Vec2& b, const ClipRect& r, LineResult* result = nullptr);

// Sutherland-Hodgman. Returns the clipped polygon (empty if nothing is
// inside). Concave polygons are handled, but if the window cuts a concave
// polygon into separate pieces, the pieces stay joined by edges running
// along the window border.
std::vector<Vec2> sutherlandHodgman(const std::vector<Vec2>& poly, const ClipRect& r);

}  // namespace clip
