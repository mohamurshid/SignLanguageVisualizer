#include "Clipping.h"

namespace clip {

int outcode(double x, double y, const ClipRect& r) {
    int code = INSIDE;
    if (x < r.xmin) {
        code |= LEFT;
    } else if (x > r.xmax) {
        code |= RIGHT;
    }
    if (y < r.ymin) {
        code |= BOTTOM;
    } else if (y > r.ymax) {
        code |= TOP;
    }
    return code;
}

// ===========================================================================
// Cohen-Sutherland line clipping
// ===========================================================================
// 1. Give each end point its 4-bit outcode.
// 2. Both codes 0000                 -> the whole line is inside: ACCEPT.
// 3. (code_a AND code_b) != 0000     -> both ends are beyond the SAME side:
//                                       the whole line is outside: REJECT.
// 4. Otherwise pick an end point that is outside, find where the line meets
//    the window edge named by one of its bits, move the point there, recompute
//    its outcode and go back to step 2.
// Intersections, using the line's slope:
//    with x = xmin or xmax:  y = y0 + (y1 - y0) * (x - x0) / (x1 - x0)
//    with y = ymin or ymax:  x = x0 + (x1 - x0) * (y - y0) / (y1 - y0)
// Each pass clears at least one bit, so it finishes in at most 4 passes.
bool cohenSutherland(Vec2& a, Vec2& b, const ClipRect& r, LineResult* result) {
    int codeA = outcode(a.x, a.y, r);
    int codeB = outcode(b.x, b.y, r);
    bool moved = false;

    while (true) {
        if ((codeA | codeB) == 0) {  // trivially accept
            if (result) *result = moved ? LineResult::Clipped : LineResult::AcceptedWhole;
            return true;
        }
        if ((codeA & codeB) != 0) {  // trivially reject
            if (result) *result = LineResult::Rejected;
            return false;
        }

        // At least one end is outside: work on that one.
        const int out = codeA != 0 ? codeA : codeB;
        double x = 0.0;
        double y = 0.0;
        if (out & TOP) {
            x = a.x + (b.x - a.x) * (r.ymax - a.y) / (b.y - a.y);
            y = r.ymax;
        } else if (out & BOTTOM) {
            x = a.x + (b.x - a.x) * (r.ymin - a.y) / (b.y - a.y);
            y = r.ymin;
        } else if (out & RIGHT) {
            y = a.y + (b.y - a.y) * (r.xmax - a.x) / (b.x - a.x);
            x = r.xmax;
        } else {  // LEFT
            y = a.y + (b.y - a.y) * (r.xmin - a.x) / (b.x - a.x);
            x = r.xmin;
        }
        // (No division by zero: e.g. a TOP bit on one end and not on the
        // other means b.y != a.y.)

        if (out == codeA) {
            a = {x, y};
            codeA = outcode(a.x, a.y, r);
        } else {
            b = {x, y};
            codeB = outcode(b.x, b.y, r);
        }
        moved = true;
    }
}

// ===========================================================================
// Sutherland-Hodgman polygon clipping
// ===========================================================================
// Clip the whole polygon against ONE window edge, then feed the result into
// the next edge: left, right, bottom, top. For each polygon edge S -> P
// (S = previous vertex, P = current vertex) there are four cases:
//
//      S inside,  P inside  : output P
//      S inside,  P outside : output the intersection I
//      S outside, P inside  : output I, then P
//      S outside, P outside : output nothing
//
// After four passes, every vertex left is inside the window.
namespace {
enum class Edge { Left, Right, Bottom, Top };

bool insideEdge(Vec2 p, Edge e, const ClipRect& r) {
    switch (e) {
        case Edge::Left: return p.x >= r.xmin;
        case Edge::Right: return p.x <= r.xmax;
        case Edge::Bottom: return p.y >= r.ymin;
        default: return p.y <= r.ymax;
    }
}

// Where segment s -> p crosses the window edge (only called when it does).
Vec2 intersectEdge(Vec2 s, Vec2 p, Edge e, const ClipRect& r) {
    if (e == Edge::Left || e == Edge::Right) {
        const double x = (e == Edge::Left) ? r.xmin : r.xmax;
        const double t = (x - s.x) / (p.x - s.x);
        return {x, s.y + t * (p.y - s.y)};
    }
    const double y = (e == Edge::Bottom) ? r.ymin : r.ymax;
    const double t = (y - s.y) / (p.y - s.y);
    return {s.x + t * (p.x - s.x), y};
}

std::vector<Vec2> clipAgainstEdge(const std::vector<Vec2>& input, Edge e, const ClipRect& r) {
    std::vector<Vec2> output;
    if (input.empty()) return output;
    Vec2 s = input.back();  // start with the edge from the last vertex to the first
    for (const Vec2& p : input) {
        const bool sIn = insideEdge(s, e, r);
        const bool pIn = insideEdge(p, e, r);
        if (sIn && pIn) {
            output.push_back(p);
        } else if (sIn && !pIn) {
            output.push_back(intersectEdge(s, p, e, r));
        } else if (!sIn && pIn) {
            output.push_back(intersectEdge(s, p, e, r));
            output.push_back(p);
        }
        s = p;
    }
    return output;
}
}  // namespace

std::vector<Vec2> sutherlandHodgman(const std::vector<Vec2>& poly, const ClipRect& r) {
    std::vector<Vec2> out = poly;
    for (Edge e : {Edge::Left, Edge::Right, Edge::Bottom, Edge::Top}) {
        out = clipAgainstEdge(out, e, r);
        if (out.empty()) break;
    }
    return out;
}

}  // namespace clip
