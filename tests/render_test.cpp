// render_test: automated checks for every algorithm (pure C++, no window).
// The 3D checks of the final project are in final_tests.cpp.
//
// Run it after changing any algorithm. It prints PASS/FAIL for each check
// and ends with "ALL CHECKS PASSED" when everything is correct.

#include <cmath>
#include <cstdio>
#include <functional>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Animation.h"
#include "GifWriter.h"
#include "Transform.h"
#include "TransformedShapes.h"

#include "Canvas.h"
#include "Clipping.h"
#include "Curves.h"
#include "Framebuffer.h"
#include "Geometry.h"
#include "Hand.h"
#include "HandRenderer.h"
#include "Poses.h"
#include "Raster.h"
#include "TestScene.h"

namespace {
int failures = 0;
int passes = 0;

const Color kWhite{255, 255, 255};
const Color kBlack{0, 0, 0};

void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (ok) {
        ++passes;
    } else {
        ++failures;
    }
}

int countColour(const Framebuffer& fb, Color c) {
    int n = 0;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x)
            if (fb.getPixel(x, y) == c) ++n;
    return n;
}

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
bool near(Vec2 a, Vec2 b, double eps = 1e-9) { return near(a.x, b.x, eps) && near(a.y, b.y, eps); }

// ---------------------------------------------------------------------------
void lineTests() {
    std::printf("Lines\n");
    for (LineAlgo algo : {LineAlgo::DDA, LineAlgo::Bresenham}) {
        const std::string name = lineAlgoName(algo);
        Framebuffer fb(50, 50);
        raster::line(fb, algo, 10, 10, 10, 10, kWhite);
        check(countColour(fb, kWhite) == 1, name + ": zero-length line draws 1 pixel");

        fb.clear(kBlack);
        raster::line(fb, algo, 5, 20, 40, 20, kWhite);
        check(countColour(fb, kWhite) == 36, name + ": horizontal line has no gaps");

        fb.clear(kBlack);
        raster::line(fb, algo, 20, 40, 20, 5, kWhite);
        check(countColour(fb, kWhite) == 36, name + ": vertical line (drawn downwards) has no gaps");

        fb.clear(kBlack);
        raster::line(fb, algo, 40, 3, 3, 40, kWhite);
        check(countColour(fb, kWhite) == 38, name + ": diagonal right-to-left");

        fb.clear(kBlack);
        raster::line(fb, algo, 2, 2, 12, 45, kWhite);
        check(countColour(fb, kWhite) == 44, name + ": steep line, one pixel per row");

        fb.clear(kBlack);
        raster::line(fb, algo, -30, 25, 80, 25, kWhite);
        check(countColour(fb, kWhite) == 50, name + ": line off both sides is cut at the edges");
    }

    // Bresenham worked example from the write-up: (20,10) -> (30,18).
    Framebuffer fb(50, 50);
    raster::lineBresenham(fb, 20, 10, 30, 18, kWhite);
    const IPoint expected[] = {{20, 10}, {21, 11}, {22, 12}, {23, 12}, {24, 13}, {25, 14},
                               {26, 15}, {27, 16}, {28, 16}, {29, 17}, {30, 18}};
    bool all = countColour(fb, kWhite) == 11;
    for (const IPoint& p : expected) all = all && fb.getPixel(p.x, p.y) == kWhite;
    check(all, "Bresenham: matches the worked example (20,10) -> (30,18)");

    fb.clear(kBlack);
    raster::lineDashed(fb, 0, 5, 15, 5, kWhite, 0x0F0F);
    check(countColour(fb, kWhite) == 8, "dashed 0x0F0F: 8 of 16 pixels drawn");
    check(fb.getPixel(0, 5) == kWhite && fb.getPixel(4, 5) == kBlack, "dashed 0x0F0F: starts with 4 on, then off");

    fb.clear(kBlack);
    raster::lineThick(fb, 5, 20, 24, 20, 3, kWhite);
    check(countColour(fb, kWhite) == 60, "thick line, width 3: 20 x 3 pixels");

    fb.clear(kBlack);
    raster::lineThick(fb, 5, 5, 25, 25, 3, kWhite);
    check(countColour(fb, kWhite) == 21 * 4, "thick 45-degree line: run stretched to 4 (3 x sqrt 2)");
}

// ---------------------------------------------------------------------------
void circleAndEllipseTests() {
    std::printf("Circles and ellipses\n");
    Framebuffer fb(60, 60);
    raster::fillCircle(fb, 30, 30, 10, kWhite);
    const int area = countColour(fb, kWhite);
    // Boundary pixels are included, so the area is about pi * (r + 0.5)^2 = 346.
    check(area > 335 && area < 360, "filled circle r=10: area close to pi*(r+0.5)^2 = 346");

    fb.clear(kBlack);
    raster::circleMidpoint(fb, 30, 30, 0, kWhite);
    check(countColour(fb, kWhite) == 1, "circle radius 0 draws 1 pixel");

    fb.clear(kBlack);
    raster::circleMidpoint(fb, 30, 30, 10, kWhite);
    check(fb.getPixel(30, 40) == kWhite && fb.getPixel(40, 30) == kWhite && fb.getPixel(20, 30) == kWhite &&
              fb.getPixel(30, 20) == kWhite,
          "circle r=10 passes through its 4 extreme points");
    // Midpoint circle worked example (r = 10): first-octant points.
    const IPoint octant[] = {{0, 10}, {1, 10}, {2, 10}, {3, 10}, {4, 9}, {5, 9}, {6, 8}, {7, 7}};
    bool all = true;
    for (const IPoint& p : octant) all = all && fb.getPixel(30 + p.x, 30 + p.y) == kWhite;
    check(all, "circle r=10: matches the worked example (0,10) ... (7,7)");

    // Ellipse
    Framebuffer e(80, 60);
    raster::ellipseMidpoint(e, 40, 30, 8, 6, kWhite);
    const IPoint quad[] = {{0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 5}, {5, 5}, {6, 4}, {7, 3}, {8, 2}, {8, 1}, {8, 0}};
    all = true;
    for (const IPoint& p : quad) all = all && e.getPixel(40 + p.x, 30 + p.y) == kWhite;
    check(all, "ellipse rx=8, ry=6: matches the worked example (0,6) ... (8,0)");

    bool symmetric = true;
    for (int y = 0; y < e.height(); ++y)
        for (int x = 0; x < e.width(); ++x)
            if ((e.getPixel(x, y) == kWhite) != (e.getPixel(80 - x, y) == kWhite) && 80 - x < e.width())
                symmetric = false;
    check(symmetric, "ellipse is mirror-symmetric");

    e.clear(kBlack);
    raster::fillEllipse(e, 40, 30, 20, 10, kWhite);
    const int eArea = countColour(e, kWhite);
    check(eArea > 640 && eArea < 715, "filled ellipse 20x10: area close to pi*(rx+0.5)(ry+0.5) = 676");
    check(e.getPixel(61, 30) == kBlack && e.getPixel(40, 41) == kBlack, "filled ellipse stays inside its radii");

    e.clear(kBlack);
    raster::ellipseMidpoint(e, 40, 30, 10, 0, kWhite);
    check(countColour(e, kWhite) == 21, "ellipse with ry=0 is a 21-pixel horizontal line");
    e.clear(kBlack);
    raster::ellipseMidpoint(e, 40, 30, 0, 7, kWhite);
    check(countColour(e, kWhite) == 15, "ellipse with rx=0 is a 15-pixel vertical line");
}

// ---------------------------------------------------------------------------
void polygonTests() {
    std::printf("Polygons\n");
    Framebuffer fb(60, 60);
    raster::scanlineFill(fb, {{10, 10}, {40, 10}, {40, 40}, {10, 40}}, kWhite);
    check(countColour(fb, kWhite) == 31 * 30, "square fill: 31 columns x 30 rows (top row excluded)");

    fb.clear(kBlack);
    const std::vector<IPoint> u = {{5, 5}, {50, 5}, {50, 50}, {38, 50}, {38, 20}, {17, 20}, {17, 50}, {5, 50}};
    raster::scanlineFill(fb, u, kWhite);
    check(fb.getPixel(27, 35) == kBlack && fb.getPixel(10, 35) == kWhite, "concave U-shape: notch empty, arms filled");

    fb.clear(kBlack);
    raster::scanlineFill(fb, {{10, 10}, {40, 10}, {25, 10}}, kWhite);
    check(countColour(fb, kWhite) == 0, "zero-height polygon fills nothing");

    fb.clear(kBlack);
    raster::scanlineFill(fb, {{-20, -20}, {100, -20}, {100, 100}, {-20, 100}}, kWhite);
    check(countColour(fb, kWhite) == 60 * 60, "polygon larger than the screen fills the whole screen");

    // Polygon operations
    const std::vector<Vec2> square = {{0, 0}, {20, 0}, {20, 20}, {0, 20}};
    check(near(geom::signedArea(square), 400.0), "shoelace: square area = 400 (positive = anticlockwise)");
    const std::vector<Vec2> clockwise = {{0, 0}, {0, 20}, {20, 20}, {20, 0}};
    check(near(geom::signedArea(clockwise), -400.0), "shoelace: clockwise order gives a negative area");
    check(geom::isConvex(square), "square is convex");
    check(!geom::isConvex(geom::toVec2(u)), "U-shape is concave");
    Hand hand;
    check(!geom::isConvex(hand.palmOutline()), "palm outline is concave (web of the thumb)");
    check(!geom::pointInPolygon({27, 35}, geom::toVec2(u)) && geom::pointInPolygon({10, 35}, geom::toVec2(u)),
          "point-in-polygon: notch is outside, arm is inside");
    check(near(geom::centroid(square), Vec2{10, 10}), "centroid of the square is (10, 10)");
}

// ---------------------------------------------------------------------------
void clippingTests() {
    std::printf("Clipping\n");
    const ClipRect r{10, 10, 50, 40};
    check(clip::outcode(5, 45, r) == (clip::LEFT | clip::TOP), "outcode above-left = 1001");
    check(clip::outcode(30, 20, r) == clip::INSIDE, "outcode inside = 0000");

    Vec2 a{15, 15}, b{40, 30};
    clip::LineResult res;
    check(clip::cohenSutherland(a, b, r, &res) && res == clip::LineResult::AcceptedWhole && near(a, Vec2{15, 15}),
          "Cohen-Sutherland: line inside is accepted unchanged");

    a = {0, 0};
    b = {5, 60};
    check(!clip::cohenSutherland(a, b, r, &res) && res == clip::LineResult::Rejected,
          "Cohen-Sutherland: line left of the window is rejected");

    a = {0, 25};
    b = {60, 25};
    check(clip::cohenSutherland(a, b, r) && near(a, Vec2{10, 25}) && near(b, Vec2{50, 25}),
          "Cohen-Sutherland: horizontal line cut to x = 10 .. 50");

    a = {0, 0};
    b = {60, 60};
    check(clip::cohenSutherland(a, b, r) && near(a, Vec2{10, 10}) && near(b, Vec2{40, 40}),
          "Cohen-Sutherland: diagonal cut to (10,10) .. (40,40)");

    a = {0, 35};
    b = {20, 60};  // no shared outcode bit, but it misses the window's top-left corner
    check(!clip::cohenSutherland(a, b, r, &res) && res == clip::LineResult::Rejected,
          "Cohen-Sutherland: line past the corner is rejected after one clip step");

    const std::vector<Vec2> sq = {{0, 0}, {30, 0}, {30, 30}, {0, 30}};
    const std::vector<Vec2> c1 = clip::sutherlandHodgman(sq, r);
    check(near(geom::signedArea(c1), 20.0 * 20.0), "Sutherland-Hodgman: overlapping square clipped to 20 x 20");

    const std::vector<Vec2> inside = {{20, 20}, {30, 20}, {25, 30}};
    const std::vector<Vec2> c2 = clip::sutherlandHodgman(inside, r);
    check(c2.size() == 3 && near(geom::signedArea(c2), geom::signedArea(inside)),
          "Sutherland-Hodgman: polygon inside is unchanged");

    check(clip::sutherlandHodgman({{60, 60}, {70, 60}, {65, 70}}, r).empty(),
          "Sutherland-Hodgman: polygon outside gives nothing");

    const std::vector<Vec2> big = {{-100, -100}, {200, -100}, {50, 200}};
    check(near(geom::signedArea(clip::sutherlandHodgman(big, r)), 40.0 * 30.0),
          "Sutherland-Hodgman: triangle covering the window gives the window");

    // Canvas: draw lots of lines and polygons clipped to a viewport and make
    // sure nothing lands outside it. Lines and polygons are NOT protected by
    // the scissor, so this tests Cohen-Sutherland and Sutherland-Hodgman.
    Framebuffer fb(100, 80);
    Canvas canvas(fb, {20, 15, 69, 54}, LineAlgo::Bresenham);
    for (int i = 0; i < 40; ++i) {
        const double t = i * 0.37;
        canvas.line({50 + 70 * std::cos(t), 40 + 60 * std::sin(1.3 * t)},
                    {50 - 65 * std::cos(0.7 * t), 40 + 55 * std::sin(t)}, kWhite);
        canvas.fillPolygon({{50 + 60 * std::cos(t), 40 + 50 * std::sin(t)},
                            {50 + 60 * std::cos(t + 2), 40 + 50 * std::sin(t + 2)},
                            {50 + 60 * std::cos(t + 4), 40 + 50 * std::sin(t + 4)}},
                           kWhite);
    }
    canvas.circle({50, 40}, 45, kWhite);  // circles use the scissor (point clipping)
    bool leaked = false;
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x)
            if (fb.getPixel(x, y) == kWhite && !canvas.clipRect().contains(x, y)) leaked = true;
    check(!leaked && countColour(fb, kWhite) > 0,
          "Canvas: 40 lines + 40 triangles + a circle, no pixel outside the viewport");
    check(fb.scissor().xmin == 0 && fb.scissor().xmax == 99, "Canvas restores the scissor after drawing circles");
}

// ---------------------------------------------------------------------------
void curveTests() {
    std::printf("Curves\n");
    const std::vector<Vec2> ctrl = {{0, 0}, {10, 30}, {40, 30}, {50, 0}};
    check(near(curves::deCasteljau(ctrl, 0.0), ctrl[0]) && near(curves::deCasteljau(ctrl, 1.0), ctrl[3]),
          "Bezier passes through its first and last control points");
    bool same = true;
    for (double t = 0.0; t <= 1.0; t += 0.05) same = same && near(curves::deCasteljau(ctrl, t), curves::bezierBernstein(ctrl, t), 1e-9);
    check(same, "de Casteljau and Bernstein form give the same points");
    check(near(curves::deCasteljau(ctrl, 0.5), Vec2{25.0, 22.5}), "worked example: P(0.5) = (25, 22.5)");
    check(curves::deCasteljauLevels(ctrl, 0.5).size() == 4, "cubic: 4 levels in the de Casteljau construction");

    const Vec2 p0{0, 0}, p1{10, 10}, p2{20, 0}, p3{30, 10};
    check(near(curves::catmullRom(p0, p1, p2, p3, 0.0), p1) && near(curves::catmullRom(p0, p1, p2, p3, 1.0), p2),
          "Catmull-Rom span goes from p1 to p2");
    check(near(curves::uniformBSpline(p0, p1, p2, p3, 0.0), (p0 + p1 * 4.0 + p2) * (1.0 / 6.0)),
          "B-spline starts at (p0 + 4 p1 + p2) / 6, not at p1");

    Hand hand;
    const std::vector<Vec2>& palm = hand.palmOutline();
    const std::vector<Vec2> outline = curves::sampleCatmullRom(palm, 6, true);
    bool through = outline.size() == palm.size() * 6;
    for (size_t i = 0; i < palm.size(); ++i) through = through && near(outline[i * 6], palm[i]);
    check(through, "closed Catmull-Rom palm outline passes through all 14 control points");

    const std::vector<Vec2> open = {{0, 0}, {10, 20}, {20, 0}, {30, 20}};
    const std::vector<Vec2> cr = curves::sampleCatmullRom(open, 8, false);
    const std::vector<Vec2> bs = curves::sampleBSpline(open, 8, false);
    check(near(cr.front(), open.front()) && near(cr.back(), open.back()) && near(bs.front(), open.front()) &&
              near(bs.back(), open.back()),
          "open splines start at the first point and end at the last");
}

// ---------------------------------------------------------------------------
void sceneTests() {
    std::printf("Hand scenes\n");
    Framebuffer fb(240, 180);
    Hand hand;
    RenderSettings s;
    bool allDrawn = true;
    for (const HandPose& pose : signLibrary()) {
        hand.setPose(pose);
        drawHandScene(fb, hand, s);
        allDrawn = allDrawn && countColour(fb, palette::skin) > 500;
    }
    check(allDrawn, "all " + std::to_string(signLibrary().size()) + " signs render with skin pixels");

    // Detail view: magnified hand must stay inside its viewport.
    hand.setPose(signLibrary()[0]);
    drawHandScene(fb, hand, s);
    Framebuffer before = fb;
    drawDetailView(fb, hand, s);
    const DetailView dv = detailViewFor(fb, hand);
    bool outsideUnchanged = true;
    for (int y = 0; y < fb.height(); ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            const bool onWindowBorder = dv.window.contains(x, y) && (x == dv.window.xmin || x == dv.window.xmax ||
                                                                     y == dv.window.ymin || y == dv.window.ymax);
            if (!dv.viewport.contains(x, y) && !onWindowBorder && fb.getPixel(x, y) != before.getPixel(x, y))
                outsideUnchanged = false;
        }
    }
    check(outsideUnchanged, "detail view: nothing drawn outside the viewport (except the window marker)");

    for (int page = 0; page < algorithmPageCount(); ++page) drawAlgorithmPage(fb, page, s);
    drawAlgorithmPage(fb, 3, s);
    const ClipStats cs = lastClipStats();
    check(cs.whole + cs.clipped + cs.rejected == 20 && cs.polygonOut > 0, "clipping page: 20 lines classified");
}

// ===========================================================================
// Checkpoint 2
// ===========================================================================
bool nearM(const Mat3& a, const Mat3& b, double eps = 1e-9) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (std::fabs(a.m[r][c] - b.m[r][c]) > eps) return false;
    return true;
}

void transformTests() {
    std::printf("Transformations\n");
    check(near(xf::translate(3, -2).apply({1, 1}), Vec2{4, -1}), "translate (1,1) by (3,-2) gives (4,-1)");
    check(near(xf::translate(3, -2).applyVector({1, 1}), Vec2{1, 1}), "translation does not move a direction (w = 0)");
    check(near(xf::rotate(90).apply({1, 0}), Vec2{0, 1}), "rotate (1,0) by 90 deg gives (0,1)");
    check(near(xf::rotate(30).apply({2, 0}), Vec2{std::sqrt(3.0), 1.0}), "worked example: rotate (2,0) by 30 deg = (1.732, 1)");
    const Vec2 pivot{2, 1};
    check(near(xf::rotateAbout(pivot, 73).apply(pivot), pivot), "rotation about a pivot leaves the pivot fixed");
    check(near(xf::rotateAbout(pivot, 90).apply({3, 1}), Vec2{2, 2}), "worked example: (3,1) rotated 90 deg about (2,1) = (2,2)");
    check(near(xf::scaleAbout({1, 1}, 2, 3).apply({1, 1}), Vec2{1, 1}) &&
              near(xf::scaleAbout({1, 1}, 2, 3).apply({2, 2}), Vec2{3, 4}),
          "scaling about a fixed point keeps that point; (2,2) -> (3,4)");
    check(near(xf::reflectY().apply({2, 5}), Vec2{-2, 5}) && xf::reflectY().determinant() < 0,
          "reflection in the y-axis: (2,5) -> (-2,5), determinant negative");
    check(near(xf::shear(0.5, 0).apply({0, 2}), Vec2{1, 2}), "shear x' = x + 0.5 y: (0,2) -> (1,2)");

    const Mat3 t = xf::translate(1, 0), r = xf::rotate(90);
    check(near((t * r).apply({1, 0}), Vec2{1, 1}) && near((r * t).apply({1, 0}), Vec2{0, 2}),
          "order matters: T*R(1,0) = (1,1) but R*T(1,0) = (0,2)");
    const Mat3 m = xf::translate(4, -1) * xf::rotate(37) * xf::scale(2, 0.5) * xf::shear(0.3, 0);
    check(nearM(m * m.inverse(), Mat3::identity(), 1e-9), "M * inverse(M) = identity");
    check(near(m.inverse().apply(m.apply({0.7, -2.3})), Vec2{0.7, -2.3}, 1e-9), "inverse undoes the transformation");
    check(std::fabs(xf::scale(2, 3).determinant() - 6.0) < 1e-12, "det of scale(2,3) = 6 (area grows 6 times)");
    check(xf::scale(2, 2).isAxisAligned() && !xf::rotate(10).isAxisAligned(), "rotation is detected (for ellipses)");

    MatrixStack stack;
    stack.translate(5, 0);
    stack.push();
    stack.rotate(90);
    const Vec2 child = stack.top().apply({1, 0});
    stack.pop();
    check(near(child, Vec2{5, 1}) && near(stack.top().apply({1, 0}), Vec2{6, 0}) && stack.depth() == 1,
          "matrix stack: child rotated inside push/pop, parent restored");

    // The matrix-stack kinematics must match the CP1 trigonometry exactly.
    Hand hand;
    double worst = 0.0;
    const double kPi = 3.14159265358979323846;
    for (const HandPose& pose : signLibrary()) {
        hand.setPose(pose);
        for (int f = 0; f < kFingerCount; ++f) {
            const FingerSpec& sp = hand.spec(f);
            Vec2 p = sp.base;
            double dir = sp.restDirection, flex = 0;
            for (int b = 0; b < kBones; ++b) {
                dir += pose.fingers[f].bend[b];
                flex += pose.fingers[f].flex[b];
                const double len = sp.length[b] * std::cos(flex * kPi / 180);
                p = p + Vec2{std::cos(dir * kPi / 180), std::sin(dir * kPi / 180)} * len;
                const Vec2 q = hand.chain(f).point[b + 1];
                worst = std::fmax(worst, std::fmax(std::fabs(q.x - p.x), std::fabs(q.y - p.y)));
            }
        }
    }
    check(worst < 1e-12, "matrix-stack kinematics = CP1 trigonometry for all " + std::to_string(signLibrary().size()) + " signs");

    hand.setPose(signByName("5"));
    const FingerChain& c = hand.chain(Index);
    check(near(c.boneFrame[0].apply({0, 0}), c.point[0]) && near(c.jointFrame[1].apply({0, 0}), c.point[1]),
          "joint frames sit on the joints");

    Placement pl;
    pl.wristAngle = 30;
    pl.position = {0.5, 0.2};
    check(near(pl.handMatrix().apply({0, 0}), Vec2{0.5, 0.2}) && near(pl.forearmMatrix().apply({0, 0}), Vec2{0.5, 0.2}),
          "wrist rotation turns the hand about the wrist (wrist stays put)");
    pl.mirror = true;
    check(pl.handMatrix().determinant() < 0, "mirrored placement is a reflection (left hand)");

    // Build mode idea: the inverse joint matrix turns a target point into a bend angle.
    HandPose pose = hand.pose();
    const Vec2 target = c.point[0] + Vec2{0.4, 0.3};
    const Vec2 local = c.jointFrame[0].inverse().apply(target);
    pose.fingers[Index].bend[0] = std::atan2(local.y, local.x) * 180.0 / kPi;
    hand.setPose(pose);
    const Vec2 dir = hand.chain(Index).point[1] - hand.chain(Index).point[0];
    check(std::fabs(dir.x * 0.3 - dir.y * 0.4) < 1e-9 && dir.x > 0, "inverse matrix picking: bone now points at the target");

    // A rotated ellipse is drawn as a transformed polygon, inside its bounds.
    Framebuffer fb(80, 80);
    Canvas canvas(fb, fb.bounds(), LineAlgo::Bresenham);
    shapes::ellipse(canvas, xf::translate(40, 40) * xf::rotate(45), {0, 0}, 30, 8, kWhite, true);
    check(fb.getPixel(40, 40) == kWhite && fb.getPixel(60, 60) == kWhite && fb.getPixel(60, 20) == kBlack,
          "rotated ellipse lies along the 45-degree diagonal");
}

void animationTests() {
    std::printf("Animation\n");
    const auto& lib = animationLibrary();
    check(lib.size() >= 6, "6 animated signs");
    bool keysOk = true;
    for (const SignAnimation& a : lib) {
        for (size_t i = 0; i + 1 < a.keys.size(); ++i) keysOk = keysOk && a.keys[i].time < a.keys[i + 1].time;
        for (const Keyframe& k : a.keys) {
            const AnimationFrame f = sampleAnimation(a, k.time);
            keysOk = keysOk && near(f.placement.position, k.placement.position, 1e-9) &&
                     std::fabs(f.placement.wristAngle - k.placement.wristAngle) < 1e-9 &&
                     std::fabs(f.pose.fingers[Index].flex[1] - k.pose.fingers[Index].flex[1]) < 1e-9;
        }
    }
    check(keysOk, "every animation passes exactly through its keyframes");

    const SignAnimation& wave = lib[1];  // GOODBYE
    const AnimationFrame mid = sampleAnimation(wave, (wave.keys[1].time + wave.keys[2].time) / 2);
    check(std::fabs(mid.placement.wristAngle) < 1e-9, "halfway between +22 and -22 deg the wrist is at 0 deg (eased)");
    const double early = sampleAnimation(wave, wave.keys[0].time + 0.05 * (wave.keys[1].time - wave.keys[0].time)).placement.wristAngle;
    check(early > 0 && early < 22 * 0.05, "easing: the motion starts slower than linear");
    check(near(sampleAnimation(wave, -1).placement.position, wave.keys.front().placement.position) &&
              near(sampleAnimation(wave, 99).placement.position, wave.keys.back().placement.position),
          "times before the start / after the end are clamped");

    const SignAnimation& hello = lib[0];
    const std::vector<Vec2> path = motionPath(hello);
    const std::vector<Vec2> keys = keyframePositions(hello);
    check(near(path.front(), keys.front()) && near(path.back(), keys.back()) && keys.size() == 3,
          "HELLO motion path: Catmull-Rom through its 3 keyframe positions");
    const AnimationFrame hold = sampleAnimation(hello, (hello.keys[0].time + hello.keys[1].time) / 2);
    check(near(hold.placement.position, hello.keys[0].placement.position), "a held keyframe does not drift");

    AnimationPlayer player;
    player.start(&wave);
    player.setSpeed(2.0);
    player.advance(0.25);
    check(std::fabs(player.time() - 0.5) < 1e-12, "speed 2x: 0.25 s of real time = 0.5 s of animation");
    player.advance(10.0);
    check(player.time() >= 0 && player.time() < wave.duration() + 1.0, "playback loops back to the start");
}

void gifTests() {
    std::printf("GIF writer\n");
    // LZW on a run of one colour: few codes, much smaller than the input.
    const std::vector<std::uint8_t> run(10000, 3);
    check(gifLzwEncode(run, 8).size() < 300, "LZW compresses 10000 equal pixels to under 300 bytes");

    Framebuffer fb(16, 8);
    fb.clear({10, 20, 30});
    fb.setPixel(3, 2, {200, 100, 50});
    GifWriter gif;
    const std::string path = "render_test_tmp.gif";
    bool ok = gif.open(path, 16, 8, 1) && gif.addFrame(fb, 10) && gif.addFrame(fb, 10) && gif.close();
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::remove(path.c_str());
    ok = ok && bytes.size() > 20 && std::string(bytes.begin(), bytes.begin() + 6) == "GIF89a" && bytes.back() == 0x3B &&
         static_cast<unsigned char>(bytes[6]) == 16 && static_cast<unsigned char>(bytes[8]) == 8;
    check(ok && gif.frameCount() == 2, "GIF file: GIF89a header, 16 x 8, 2 frames, trailer 0x3B");
}
}  // namespace

void finalTests(const std::function<void(bool, const std::string&)>& check);  // final_tests.cpp

int main() {
    lineTests();
    circleAndEllipseTests();
    polygonTests();
    clippingTests();
    curveTests();
    sceneTests();
    transformTests();
    animationTests();
    gifTests();
    finalTests(check);
    std::printf("\n%d passed, %d failed\n", passes, failures);
    std::printf("%s\n", failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
