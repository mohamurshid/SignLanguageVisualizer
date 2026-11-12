#include "TransformScene.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Canvas.h"
#include "Transform.h"
#include "TransformedShapes.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr Color kGreen{90, 230, 120};
constexpr Color kOrange{255, 150, 60};
constexpr Color kCyan{80, 210, 255};
constexpr Color kWhite{245, 245, 255};

// The object: a finger in local units. Its base (the knuckle) is at (0, 0)
// and it points up the y-axis, 1 unit long with a rounded tip.
std::vector<Vec2> fingerShape() {
    std::vector<Vec2> pts = {{0.16, 0.0}, {0.14, 0.82}};
    for (int i = 0; i <= 10; ++i) {  // rounded tip: a half circle
        const double a = kPi * i / 10.0;
        pts.push_back({0.14 * std::cos(a), 0.82 + 0.14 * std::sin(a)});
    }
    pts.push_back({-0.16, 0.0});
    return pts;
}

// One panel of a page: a viewport with its own local coordinate system.
struct Panel {
    ClipRect viewport;
    Mat3 toScreen;  // local units -> pixels (window-to-viewport)
    double unit;    // pixels per local unit
};

Panel makePanel(const Framebuffer& fb, int col, int row, int cols, int rows, double originX = 0.5,
                double originY = 0.2, double unitFactor = 0.46) {
    const int top = fb.height() * 82 / 100;  // leave room for the title text
    const int bottom = fb.height() * 6 / 100;
    const int cellW = fb.width() / cols;
    const int cellH = (top - bottom) / rows;
    const int x0 = col * cellW;
    const int y0 = bottom + (rows - 1 - row) * cellH;
    Panel p;
    p.viewport = {x0 + 3, y0 + 3, x0 + cellW - 4, y0 + cellH - 4};
    p.unit = std::min(cellH, cellW) * unitFactor;
    p.toScreen = xf::translate(x0 + cellW * originX, y0 + cellH * originY) * xf::scale(p.unit, p.unit);
    return p;
}

void drawAxes(Canvas& c, const Panel& p) {
    const Vec2 o = p.toScreen.apply({0, 0});
    const ClipRect& v = p.viewport;
    c.dashedLine({static_cast<double>(v.xmin), o.y}, {static_cast<double>(v.xmax), o.y}, palette::dim, 0x3333);
    c.dashedLine({o.x, static_cast<double>(v.ymin)}, {o.x, static_cast<double>(v.ymax)}, palette::dim, 0x3333);
    c.fillCircle(o, 2, palette::dim);
    c.rectOutline(v, Color{40, 52, 84});
}

// Draws the object before (dashed) and after (filled) a transformation.
void drawBeforeAfter(Canvas& c, const Panel& p, const Mat3& before, const Mat3& transform, Color after) {
    const std::vector<Vec2> shape = fingerShape();
    c.dashedPolygonOutline(xf::apply(p.toScreen * before, shape), palette::wireframe, 0x3333);
    const std::vector<Vec2> moved = xf::apply(p.toScreen * transform * before, shape);
    c.fillPolygon(moved, palette::skin);
    c.polygonOutline(moved, after);
    // Mark the tip so rotations and reflections are easy to follow.
    c.fillCircle((p.toScreen * transform * before).apply({0, 0.95}), 2, after);
}

// ---- Page 1 --------------------------------------------------------------------
void drawBasicsPage(Framebuffer& fb, const RenderSettings& s) {
    const Mat3 upright = xf::translate(0.35, 0.0);  // the object, a little right of the origin

    {  // Translation
        const Panel p = makePanel(fb, 0, 0, 3, 2);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Mat3 t = xf::translate(-0.9, 0.35);
        drawBeforeAfter(c, p, upright, t, kGreen);
        shapes::arrow(c, p.toScreen.apply({0.35, 0.0}), p.toScreen.apply({-0.55, 0.35}), kWhite);
    }
    {  // Rotation about the origin
        const Panel p = makePanel(fb, 1, 0, 3, 2);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        drawBeforeAfter(c, p, xf::translate(0.7, 0.0), xf::rotate(50), kGreen);
        c.fillCircle(p.toScreen.apply({0, 0}), 3, palette::root);
    }
    {  // Rotation about a pivot: the knuckle stays still
        const Panel p = makePanel(fb, 2, 0, 3, 2);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Vec2 knuckle{0.35, 0.0};
        drawBeforeAfter(c, p, upright, xf::rotateAbout(knuckle, 40), kGreen);
        c.fillCircle(p.toScreen.apply(knuckle), 3, palette::root);
    }
    {  // Scaling about a fixed point (the base)
        const Panel p = makePanel(fb, 0, 1, 3, 2, 0.5, 0.1, 0.4);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Vec2 base{0.35, 0.0};
        drawBeforeAfter(c, p, upright, xf::scaleAbout(base, 1.5, 1.5), kGreen);
        c.fillCircle(p.toScreen.apply(base), 3, palette::root);
    }
    {  // Reflection in the y-axis: right hand -> left hand
        const Panel p = makePanel(fb, 1, 1, 3, 2);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Mat3 tilted = xf::translate(0.5, 0.0) * xf::rotate(-25);
        drawBeforeAfter(c, p, tilted, xf::reflectY(), kGreen);
    }
    {  // Shear
        const Panel p = makePanel(fb, 2, 1, 3, 2);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        drawBeforeAfter(c, p, xf::translate(-0.1, 0.0), xf::shear(0.7, 0.0), kGreen);
    }
}

// ---- Page 2 --------------------------------------------------------------------
void drawCompositePage(Framebuffer& fb, const RenderSettings& s) {
    {  // The order of matrices matters
        const Panel p = makePanel(fb, 0, 0, 3, 1, 0.3, 0.38, 0.33);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Mat3 r = xf::rotate(-60);
        const Mat3 t = xf::translate(0.9, 0.4);
        drawBeforeAfter(c, p, Mat3::identity(), t * r, kGreen);   // rotate first, then translate
        drawBeforeAfter(c, p, Mat3::identity(), r * t, kOrange);  // translate first, then rotate
    }
    {  // Rotation about a pivot, one step at a time
        const Panel p = makePanel(fb, 1, 0, 3, 1, 0.45, 0.3, 0.36);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const Vec2 pivot{0.6, 0.35};
        const Mat3 start = xf::translate(pivot.x, pivot.y);
        const std::vector<Vec2> shape = fingerShape();
        const Mat3 step1 = xf::translate(-pivot.x, -pivot.y);
        const Mat3 step2 = xf::rotate(60) * step1;
        const Mat3 step3 = xf::translate(pivot.x, pivot.y) * step2;
        c.dashedPolygonOutline(xf::apply(p.toScreen * start, shape), palette::wireframe, 0x3333);
        c.polygonOutline(xf::apply(p.toScreen * step1 * start, shape), kCyan);    // 1. pivot to the origin
        c.polygonOutline(xf::apply(p.toScreen * step2 * start, shape), kOrange);  // 2. rotate
        const std::vector<Vec2> done = xf::apply(p.toScreen * step3 * start, shape);
        c.fillPolygon(done, palette::skin);                                       // 3. move back
        c.polygonOutline(done, kGreen);
        c.fillCircle(p.toScreen.apply(pivot), 3, palette::root);
    }
    {  // The matrix stack: a three-bone finger
        const Panel p = makePanel(fb, 2, 0, 3, 1, 0.25, 0.15, 0.6);
        Canvas c(fb, p.viewport, s.lineAlgo);
        drawAxes(c, p);
        const double lengths[3] = {0.55, 0.38, 0.30};
        const double radius[4] = {0.10, 0.09, 0.08, 0.07};
        // Two states: knuckle at 0 degrees (dim) and at 35 degrees (bright):
        // the middle and end bones follow the knuckle without being told to.
        for (int state = 0; state < 2; ++state) {
            const double bends[3] = {state ? -35.0 : 0.0, -25.0, -20.0};
            MatrixStack stack;
            stack.load(p.toScreen);
            stack.rotate(90);  // the finger points up
            std::vector<Mat3> frames;
            std::vector<Vec2> joints = {stack.top().apply({0, 0})};
            for (int b = 0; b < 3; ++b) {
                stack.rotate(bends[b]);
                frames.push_back(stack.top());
                stack.translate(lengths[b], 0);
                joints.push_back(stack.top().apply({0, 0}));
            }
            const Color bone = state ? palette::bone : palette::ghost;
            for (int b = 0; b < 3; ++b) {
                c.thickLine(joints[b], joints[b + 1], state ? 2 : 1, bone);
                c.circle(joints[b], radius[b] * p.unit, state ? palette::joint : palette::ghost);
            }
            c.circle(joints[3], radius[3] * p.unit, state ? palette::joint : palette::ghost);
            if (state) {
                for (const Mat3& f : frames) shapes::frameAxes(c, Mat3::identity(), f, 0.22);
            }
        }
    }
}

}  // namespace

int transformPageCount() { return 2; }

const char* transformPageTitle(int page) {
    return page == 0 ? "Transformations: translate, rotate, scale, reflect, shear"
                     : "Composite transformations and the matrix stack";
}

const char* transformPageFileName(int page) { return page == 0 ? "transform_basics" : "transform_composite"; }

void drawTransformPage(Framebuffer& fb, int page, const RenderSettings& settings) {
    fb.clear(palette::background);
    fb.resetScissor();
    if (page == 0) {
        drawBasicsPage(fb, settings);
    } else {
        drawCompositePage(fb, settings);
    }
}

std::vector<PageLabel> transformPageLabels(int page) {
    // Panel title positions as fractions of the window (centre of the text).
    std::vector<PageLabel> labels;
    if (page == 0) {
        const double c[3] = {1.0 / 6.0, 0.5, 5.0 / 6.0};
        labels = {{c[0], 0.795, "Translation T(-0.9, 0.35)"},     {c[1], 0.795, "Rotation 50 deg about the origin"},
                  {c[2], 0.795, "Rotation 40 deg about the knuckle"}, {c[0], 0.415, "Scaling x1.5 about the base"},
                  {c[1], 0.415, "Reflection in the y-axis"},       {c[2], 0.415, "Shear: x' = x + 0.7 y"},
                  {0.5, 0.025, "dashed = before   filled = after   yellow dot = fixed point (pivot)"}};
    } else {
        labels = {{1.0 / 6.0, 0.795, "Order matters"},
                  {1.0 / 6.0, 0.09, "green: T*R (rotate, then move)   orange: R*T"},
                  {0.5, 0.795, "Rotation about a pivot"},
                  {0.5, 0.09, "T(-p) cyan -> R orange -> T(p) green"},
                  {5.0 / 6.0, 0.795, "Matrix stack: a 3-bone finger"},
                  {5.0 / 6.0, 0.09, "knuckle turned 35 deg: children follow"}};
    }
    return labels;
}
