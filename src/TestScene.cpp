#include "TestScene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Clipping.h"
#include "Curves.h"
#include "Geometry.h"
#include "Hand.h"
#include "Raster.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

// Results kept for the labels and the tests.
int gDifferences = 0;
ClipStats gClipStats;
double gPalmArea = 0.0;
bool gPalmConvex = false;
double gCurveT = 0.5;
int gLastW = 480;
int gLastH = 360;

constexpr Color kBoth{250, 220, 60};
constexpr Color kOnlyDDA{240, 70, 70};
constexpr Color kOnlyBresenham{70, 230, 110};
constexpr Color kGreen{90, 230, 120};
constexpr Color kRed{240, 90, 90};
constexpr Color kOrange{255, 150, 60};
constexpr Color kCyan{80, 210, 255};
constexpr Color kMagenta{230, 110, 240};
constexpr Color kWhite{245, 245, 255};

// Page layout shared by the drawing code and the labels.
struct Layout {
    int w, h, cy;
    int s2;  // size of a shape when the page has 2 panels
    int s3;  // size of a shape when the page has 3 panels
};
Layout layoutFor(int w, int h) {
    Layout L;
    L.w = w;
    L.h = h;
    L.cy = h * 45 / 100;
    L.s2 = std::min(w / 4, h * 2 / 5) * 85 / 100;
    L.s3 = std::min(w / 6, h * 2 / 5) * 85 / 100;
    return L;
}

Vec2 V(double x, double y) { return {x, y}; }
IPoint P(Vec2 v) { return toPixel(v); }

void dot(Framebuffer& fb, Vec2 p, int r, Color c) {
    const IPoint q = P(p);
    raster::fillCircle(fb, q.x, q.y, r, c);
}

// A small deterministic random number generator (the same lines every run).
struct Lcg {
    std::uint32_t state;
    double next() {  // in [-1, 1]
        state = state * 1664525u + 1013904223u;
        return (state >> 8) / static_cast<double>(1u << 24) * 2.0 - 1.0;
    }
};

std::vector<Vec2> starPolygon(Vec2 c, double outer, double inner, int points) {
    std::vector<Vec2> pts;
    for (int i = 0; i < 2 * points; ++i) {
        const double a = kPi / 2.0 + i * kPi / points;
        const double r = (i % 2 == 0) ? outer : inner;
        pts.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
    return pts;
}

// ===========================================================================
// Page 1: lines
// ===========================================================================
// Draws the same starburst with both line algorithms into two scratch
// buffers, then colours each pixel by which algorithm(s) chose it.
void lineComparison(Framebuffer& fb, int cx, int cy, int radius) {
    const Color white{255, 255, 255};
    Framebuffer dda(fb.width(), fb.height());
    Framebuffer bres(fb.width(), fb.height());

    const int rays = 24;  // every 15 degrees: covers all 8 octants
    for (int i = 0; i < rays; ++i) {
        const double a = 2.0 * kPi * i / rays + 0.12;  // small offset avoids only "perfect" slopes
        const int x1 = cx + static_cast<int>(std::lround(radius * std::cos(a)));
        const int y1 = cy + static_cast<int>(std::lround(radius * std::sin(a)));
        raster::lineDDA(dda, cx, cy, x1, y1, white);
        raster::lineBresenham(bres, cx, cy, x1, y1, white);
    }

    gDifferences = 0;
    for (int y = 0; y < fb.height(); ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            const bool inDDA = dda.getPixel(x, y).r != 0;
            const bool inBres = bres.getPixel(x, y).r != 0;
            if (inDDA && inBres) {
                fb.setPixel(x, y, kBoth);
            } else if (inDDA) {
                fb.setPixel(x, y, kOnlyDDA);
                ++gDifferences;
            } else if (inBres) {
                fb.setPixel(x, y, kOnlyBresenham);
                ++gDifferences;
            }
        }
    }
}

struct LineStyleRow {
    const char* label;
    unsigned pattern;  // 0 = solid
    int width;
};
const LineStyleRow kLineStyles[] = {
    {"solid (Bresenham)", 0, 1},   {"dashed  0x0F0F", 0x0F0F, 1}, {"dotted  0x3333", 0x3333, 1},
    {"dash-dot  0x19FF", 0x19FF, 1}, {"thick: width 3", 0, 3},      {"thick: width 6", 0, 6},
};
constexpr int kLineStyleCount = static_cast<int>(sizeof(kLineStyles) / sizeof(kLineStyles[0]));

double lineStyleRowY(const Layout& L, int i) {
    return L.cy + L.s2 * 0.80 - i * (1.6 * L.s2 / (kLineStyleCount - 1));
}

void drawLinesPage(Framebuffer& fb, const Layout& L) {
    lineComparison(fb, L.w / 4, L.cy, L.s2);

    const int cx = L.w * 3 / 4;
    for (int i = 0; i < kLineStyleCount; ++i) {
        const LineStyleRow& row = kLineStyles[i];
        const int y = static_cast<int>(std::lround(lineStyleRowY(L, i)));
        const int x0 = cx - L.s2;
        const int x1 = cx + L.s2;
        const int rise = std::max(1, L.s2 / 16);  // a small slope, so the steps show
        if (row.width > 1) {
            raster::lineThick(fb, x0, y - rise, x1, y + rise, row.width, palette::joint);
        } else if (row.pattern != 0) {
            raster::lineDashed(fb, x0, y - rise, x1, y + rise, palette::joint, row.pattern);
        } else {
            raster::lineBresenham(fb, x0, y - rise, x1, y + rise, palette::joint);
        }
    }
}

// ===========================================================================
// Page 2: circles and ellipses
// ===========================================================================
void drawCirclesPage(Framebuffer& fb, const Layout& L, const RenderSettings& s) {
    const int s3 = L.s3;

    // Panel 1: concentric midpoint circles.
    const int cx1 = L.w / 6;
    for (int r = s3; r > 2; r -= std::max(2, s3 / 6)) raster::circleMidpoint(fb, cx1, L.cy, r, palette::joint);
    if (s.filled) raster::fillCircle(fb, cx1, L.cy, std::max(1, s3 / 8), palette::root);

    // Panel 2: 8-way symmetry. Dashed guide lines mark the octant borders.
    const int cx2 = L.w / 2;
    const int g = s3 + s3 / 6;
    raster::lineDashed(fb, cx2 - g, L.cy, cx2 + g, L.cy, palette::dim, 0x3333);
    raster::lineDashed(fb, cx2, L.cy - g, cx2, L.cy + g, palette::dim, 0x3333);
    raster::lineDashed(fb, cx2 - g * 7 / 10, L.cy - g * 7 / 10, cx2 + g * 7 / 10, L.cy + g * 7 / 10, palette::dim, 0x3333);
    raster::lineDashed(fb, cx2 - g * 7 / 10, L.cy + g * 7 / 10, cx2 + g * 7 / 10, L.cy - g * 7 / 10, palette::dim, 0x3333);
    const Color octants[8] = {kRed, kOrange, palette::root, kGreen, kCyan, palette::bone, kMagenta, kWhite};
    raster::circleOctants(fb, cx2, L.cy, s3, octants);
    raster::circleOctants(fb, cx2, L.cy, s3 * 6 / 10, octants);

    // Panel 3: midpoint ellipses. The big one shows region 1 (green) and
    // region 2 (orange); they meet where the slope of the curve is -1.
    const int cx3 = L.w * 5 / 6;
    raster::ellipseRegions(fb, cx3, L.cy, s3, s3 * 62 / 100, kGreen, kOrange);
    if (s.filled) raster::fillEllipse(fb, cx3, L.cy, s3 * 30 / 100, s3 * 48 / 100, palette::skin);
    raster::ellipseMidpoint(fb, cx3, L.cy, s3 * 75 / 100, s3 * 22 / 100, palette::joint);
}

// ===========================================================================
// Page 3: polygons
// ===========================================================================
void drawPolygonsPage(Framebuffer& fb, const Layout& L, const RenderSettings& s) {
    const int s3 = L.s3;

    // Panel 1: concave star, scan-line filled, with its centroid.
    const Vec2 c1 = V(L.w / 6.0, L.cy);
    const std::vector<Vec2> star = starPolygon(c1, s3, s3 * 0.4, 5);
    if (s.filled) raster::scanlineFill(fb, geom::toPixels(star), palette::skin);
    raster::polygonOutline(fb, geom::toPixels(star), s.lineAlgo, s.filled ? palette::skinOutline : palette::wireframe);
    dot(fb, geom::centroid(star), 2, palette::root);

    // Panel 2: a self-intersecting star (pentagram). Scan-line fill uses the
    // even-odd rule, so the middle pentagon (crossed twice) stays empty.
    const Vec2 c2 = V(L.w / 2.0, L.cy);
    std::vector<Vec2> pentagram;
    for (int i = 0; i < 5; ++i) {
        const double a = kPi / 2.0 + (i * 2 % 5) * 2.0 * kPi / 5.0;
        pentagram.push_back({c2.x + s3 * std::cos(a), c2.y + s3 * std::sin(a)});
    }
    if (s.filled) raster::scanlineFill(fb, geom::toPixels(pentagram), palette::skin);
    raster::polygonOutline(fb, geom::toPixels(pentagram), s.lineAlgo, palette::joint);

    // Panel 3: point-in-polygon on the palm shape from our hand model.
    Hand hand;
    const double k = 1.7 * s3 / 1.12;
    std::vector<Vec2> palm;
    for (const Vec2& p : hand.palmOutline()) palm.push_back({L.w * 5.0 / 6.0 + p.x * k, L.cy + (p.y - 0.54) * k});
    gPalmArea = std::fabs(geom::signedArea(palm));
    gPalmConvex = geom::isConvex(palm);

    double xmin = palm[0].x, xmax = palm[0].x, ymin = palm[0].y, ymax = palm[0].y;
    for (const Vec2& p : palm) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    const int step = std::max(4, s3 / 9);
    for (int y = static_cast<int>(ymin) - step; y <= static_cast<int>(ymax) + step; y += step) {
        for (int x = static_cast<int>(xmin) - step; x <= static_cast<int>(xmax) + step; x += step) {
            const bool in = geom::pointInPolygon(V(x, y), palm);
            raster::fillCircle(fb, x, y, 1, in ? kGreen : kRed);
        }
    }
    raster::polygonOutline(fb, geom::toPixels(palm), s.lineAlgo, palette::wireframe);
}

// ===========================================================================
// Page 4: clipping
// ===========================================================================
ClipRect clipWindowAt(const Layout& L, int cx) {
    return {cx - L.s2 * 6 / 10, L.cy - L.s2 * 45 / 100, cx + L.s2 * 6 / 10, L.cy + L.s2 * 45 / 100};
}

void drawClippingPage(Framebuffer& fb, const Layout& L, const RenderSettings& s) {
    gClipStats = ClipStats{};

    // Panel 1: Cohen-Sutherland. Dashed = the original line,
    // solid = what survives clipping, white rings = moved end points.
    const int cx1 = L.w / 4;
    const ClipRect win1 = clipWindowAt(L, cx1);
    Lcg rng{2024u};
    std::vector<std::pair<Vec2, Vec2>> lines;
    for (int i = 0; i < 18; ++i) {
        const Vec2 a{cx1 + rng.next() * L.s2 * 1.1, L.cy + rng.next() * L.s2 * 0.9};
        const Vec2 b{cx1 + rng.next() * L.s2 * 1.1, L.cy + rng.next() * L.s2 * 0.9};
        lines.push_back({a, b});
    }
    // Two lines that must be accepted / rejected without any intersection maths.
    lines.push_back({{cx1 - L.s2 * 0.3, L.cy - L.s2 * 0.1}, {cx1 + L.s2 * 0.25, L.cy + L.s2 * 0.2}});
    lines.push_back({{cx1 - L.s2 * 1.05, L.cy + L.s2 * 0.6}, {cx1 + L.s2 * 1.0, L.cy + L.s2 * 0.85}});

    for (const auto& ln : lines) {
        Vec2 a = ln.first;
        Vec2 b = ln.second;
        clip::LineResult result;
        const bool visible = clip::cohenSutherland(a, b, win1, &result);
        raster::lineDashed(fb, P(ln.first).x, P(ln.first).y, P(ln.second).x, P(ln.second).y,
                           visible ? palette::dim : Color{150, 60, 70}, 0x3333);
        if (!visible) {
            ++gClipStats.rejected;
            continue;
        }
        const Color c = result == clip::LineResult::AcceptedWhole ? kGreen : palette::root;
        raster::line(fb, s.lineAlgo, P(a).x, P(a).y, P(b).x, P(b).y, c);
        if (result == clip::LineResult::AcceptedWhole) {
            ++gClipStats.whole;
        } else {
            ++gClipStats.clipped;
            if (P(a).x != P(ln.first).x || P(a).y != P(ln.first).y) raster::circleMidpoint(fb, P(a).x, P(a).y, 2, kWhite);
            if (P(b).x != P(ln.second).x || P(b).y != P(ln.second).y) raster::circleMidpoint(fb, P(b).x, P(b).y, 2, kWhite);
        }
    }
    raster::polygonOutline(fb, {{win1.xmin, win1.ymin}, {win1.xmax, win1.ymin}, {win1.xmax, win1.ymax}, {win1.xmin, win1.ymax}},
                           LineAlgo::Bresenham, kWhite);

    // Panel 2: Sutherland-Hodgman on a concave star that sticks out of the window.
    const int cx2 = L.w * 3 / 4;
    const ClipRect win2 = clipWindowAt(L, cx2);
    const std::vector<Vec2> star = starPolygon(V(cx2 + L.s2 * 0.3, L.cy + L.s2 * 0.22), L.s2 * 0.95, L.s2 * 0.42, 7);
    const std::vector<Vec2> clipped = clip::sutherlandHodgman(star, win2);
    gClipStats.polygonIn = star.size();
    gClipStats.polygonOut = clipped.size();

    for (size_t i = 0; i < star.size(); ++i) {
        const IPoint a = P(star[i]);
        const IPoint b = P(star[(i + 1) % star.size()]);
        raster::lineDashed(fb, a.x, a.y, b.x, b.y, palette::dim, 0x3333);
    }
    if (s.filled && clipped.size() >= 3) raster::scanlineFill(fb, geom::toPixels(clipped), palette::skin);
    raster::polygonOutline(fb, geom::toPixels(clipped), s.lineAlgo, palette::skinOutline);
    raster::polygonOutline(fb, {{win2.xmin, win2.ymin}, {win2.xmax, win2.ymin}, {win2.xmax, win2.ymax}, {win2.xmin, win2.ymax}},
                           LineAlgo::Bresenham, kWhite);
    for (const Vec2& v : clipped) dot(fb, v, 1, palette::controlPoint);
}

// ===========================================================================
// Page 5: curves
// ===========================================================================
std::vector<Vec2> bezierControls(const Layout& L) {
    const double cx = L.w / 4.0;
    const double s = L.s2;
    return {{cx - 0.95 * s, L.cy - 0.55 * s}, {cx - 0.55 * s, L.cy + 0.85 * s},
            {cx + 0.75 * s, L.cy + 0.65 * s}, {cx + 0.95 * s, L.cy - 0.45 * s}};
}

std::vector<Vec2> splineControls(const Layout& L) {
    const double cx = L.w * 3.0 / 4.0;
    const double s = L.s2;
    return {{cx - 0.95 * s, L.cy - 0.40 * s}, {cx - 0.62 * s, L.cy + 0.60 * s}, {cx - 0.30 * s, L.cy - 0.20 * s},
            {cx + 0.00 * s, L.cy + 0.75 * s}, {cx + 0.32 * s, L.cy - 0.55 * s}, {cx + 0.62 * s, L.cy + 0.35 * s},
            {cx + 0.95 * s, L.cy - 0.10 * s}};
}

void thickPolyline(Framebuffer& fb, const std::vector<Vec2>& pts, int width, Color c) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        raster::lineThick(fb, P(pts[i]).x, P(pts[i]).y, P(pts[i + 1]).x, P(pts[i + 1]).y, width, c);
    }
}

void drawCurvesPage(Framebuffer& fb, const Layout& L, double t) {
    // Panel 1: cubic Bezier curve and the de Casteljau construction at t.
    const std::vector<Vec2> ctrl = bezierControls(L);
    for (size_t i = 0; i + 1 < ctrl.size(); ++i) {
        raster::lineDashed(fb, P(ctrl[i]).x, P(ctrl[i]).y, P(ctrl[i + 1]).x, P(ctrl[i + 1]).y, palette::dim);
    }
    const int segments = curves::segmentsForLength(curves::polylineLength(ctrl), 3.0);
    thickPolyline(fb, curves::sampleBezier(ctrl, segments), 2, palette::root);

    const std::vector<std::vector<Vec2>> levels = curves::deCasteljauLevels(ctrl, t);
    const Color levelColors[] = {palette::dim, kGreen, kCyan};
    for (size_t lv = 1; lv + 1 < levels.size(); ++lv) {
        const std::vector<Vec2>& pts = levels[lv];
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            raster::lineBresenham(fb, P(pts[i]).x, P(pts[i]).y, P(pts[i + 1]).x, P(pts[i + 1]).y, levelColors[lv]);
        }
        for (const Vec2& p : pts) dot(fb, p, 2, levelColors[lv]);
    }
    dot(fb, levels.back()[0], 3, kWhite);
    for (const Vec2& p : ctrl) dot(fb, p, 3, palette::controlPoint);

    // Panel 2: the same points joined by a Catmull-Rom spline (through every
    // point) and a uniform cubic B-spline (near the points, smoother).
    const std::vector<Vec2> pts = splineControls(L);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        raster::lineDashed(fb, P(pts[i]).x, P(pts[i]).y, P(pts[i + 1]).x, P(pts[i + 1]).y, palette::dim);
    }
    thickPolyline(fb, curves::sampleBSpline(pts, 16, false), 2, kMagenta);
    thickPolyline(fb, curves::sampleCatmullRom(pts, 16, false), 2, kGreen);
    for (const Vec2& p : pts) dot(fb, p, 3, palette::controlPoint);
}

std::string fmt(const char* f, double a) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), f, a);
    return buf;
}

}  // namespace

// ===========================================================================
// Public interface
// ===========================================================================
int algorithmPageCount() { return 5; }

const char* algorithmPageTitle(int page) {
    switch (page) {
        case 0: return "Lines: DDA vs Bresenham, line attributes";
        case 1: return "Circles and ellipses: midpoint algorithms";
        case 2: return "Polygons: scan-line fill and polygon operations";
        case 3: return "Clipping: Cohen-Sutherland and Sutherland-Hodgman";
        default: return "Curves: Bezier (de Casteljau) and splines";
    }
}

const char* algorithmPageFileName(int page) {
    switch (page) {
        case 0: return "lines";
        case 1: return "circles_ellipses";
        case 2: return "polygons";
        case 3: return "clipping";
        default: return "curves";
    }
}

void drawAlgorithmPage(Framebuffer& fb, int page, const RenderSettings& settings, double t) {
    fb.clear(palette::background);
    fb.resetScissor();
    gLastW = fb.width();
    gLastH = fb.height();
    gCurveT = t;
    const Layout L = layoutFor(fb.width(), fb.height());
    switch (page) {
        case 0: drawLinesPage(fb, L); break;
        case 1: drawCirclesPage(fb, L, settings); break;
        case 2: drawPolygonsPage(fb, L, settings); break;
        case 3: drawClippingPage(fb, L, settings); break;
        default: drawCurvesPage(fb, L, t); break;
    }
}

std::vector<PageLabel> algorithmPageLabels(int page) {
    const Layout L = layoutFor(gLastW, gLastH);
    const double top = 0.86;
    const double bottom = 0.09;
    std::vector<PageLabel> labels;
    switch (page) {
        case 0: {
            labels.push_back({0.25, top, "DDA vs Bresenham"});
            labels.push_back({0.25, bottom, fmt("%.0f pixels differ: yellow both, red DDA only, green Bresenham only",
                                                static_cast<double>(lastLineDifferenceCount()))});
            labels.push_back({0.75, top, "Line attributes: style and width"});
            for (int i = 0; i < kLineStyleCount; ++i) {
                labels.push_back({0.75, (lineStyleRowY(L, i) + L.s2 * 0.13) / L.h, kLineStyles[i].label});
            }
            break;
        }
        case 1:
            labels.push_back({1.0 / 6.0, top, "Midpoint circle"});
            labels.push_back({0.5, top, "8-way symmetry: 1 colour per octant"});
            labels.push_back({5.0 / 6.0, top, "Midpoint ellipse"});
            labels.push_back({5.0 / 6.0, bottom, "green = region 1, orange = region 2"});
            break;
        case 2:
            labels.push_back({1.0 / 6.0, top, "Scan-line fill (concave)"});
            labels.push_back({1.0 / 6.0, bottom, "dot = centroid"});
            labels.push_back({0.5, top, "Even-odd rule"});
            labels.push_back({0.5, bottom, "self-intersecting: centre stays empty"});
            labels.push_back({5.0 / 6.0, top, "Point-in-polygon (palm)"});
            labels.push_back({5.0 / 6.0, bottom, fmt(gPalmConvex ? "convex, area %.0f px^2" : "concave, area %.0f px^2", gPalmArea)});
            break;
        case 3: {
            const ClipStats st = lastClipStats();
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%d whole (green), %d clipped (yellow), %d rejected (red)", st.whole,
                          st.clipped, st.rejected);
            labels.push_back({0.25, top, "Cohen-Sutherland line clipping"});
            labels.push_back({0.25, bottom, buf});
            std::snprintf(buf, sizeof(buf), "%zu vertices in, %zu vertices out", st.polygonIn, st.polygonOut);
            labels.push_back({0.75, top, "Sutherland-Hodgman polygon clipping"});
            labels.push_back({0.75, bottom, buf});
            break;
        }
        default:
            labels.push_back({0.25, top, "Cubic Bezier"});
            labels.push_back({0.25, bottom, fmt("de Casteljau construction at t = %.2f", gCurveT)});
            labels.push_back({0.75, top, "Catmull-Rom (green) vs B-spline (pink)"});
            labels.push_back({0.75, bottom, "Catmull-Rom passes through the points"});
            break;
    }
    return labels;
}

int lastLineDifferenceCount() { return gDifferences; }
ClipStats lastClipStats() { return gClipStats; }
