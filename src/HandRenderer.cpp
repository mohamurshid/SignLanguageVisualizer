#include "HandRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Curves.h"

HandView HandView::fit(int width, int height) {
    return {height * 0.34, width * 0.47, height * 0.17};
}

namespace {

// The colours used for one drawing pass (normal, or all-dim for a ghost).
struct Style {
    Color skin, skinFolded, skinShade, outline, wire, crease, nail, bone, joint, root, control;
};

Style styleFor(const RenderSettings& s) {
    if (s.ghost) {
        const Color g = palette::ghost;
        return {g, g, g, g, g, g, g, g, g, g, g};
    }
    return {palette::skin,  palette::skinFolded, palette::skinShade, palette::skinOutline,
            palette::wireframe, palette::crease, palette::nail,      palette::bone,
            palette::joint, palette::root,       palette::controlPoint};
}

std::vector<Vec2> toPixels(const std::vector<Vec2>& shape, const HandView& view) {
    std::vector<Vec2> out;
    out.reserve(shape.size());
    for (const Vec2& p : shape) out.push_back(view.toPixels(p));
    return out;
}

// One finger bone drawn as a "capsule": a 4-sided polygon between two joints
// (wider at one end than the other) with a circle on each end. The circles
// round off the knuckles and the fingertip.
//   a, b   : joint positions in pixels
//   ra, rb : half the skin width at each joint, in pixels
void drawBone(Canvas& canvas, Vec2 a, Vec2 b, double ra, double rb, Color color, bool filled) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::sqrt(dx * dx + dy * dy);

    // A bone pointing straight at the viewer has (almost) no length on screen;
    // then only the circles are drawn.
    if (len > 0.75) {
        // Unit vector perpendicular to the bone: rotate (dx, dy) by 90 degrees.
        const Vec2 n{-dy / len, dx / len};
        const std::vector<Vec2> quad = {a + n * ra, b + n * rb, b - n * rb, a - n * ra};
        if (filled) {
            canvas.fillPolygon(quad, color);
        } else {
            canvas.polygonOutline(quad, color);
        }
    }
    if (filled) {
        canvas.fillCircle(a, ra, color);
        canvas.fillCircle(b, rb, color);
    } else {
        canvas.circle(a, ra, color);
        canvas.circle(b, rb, color);
    }
}

// Fingernail: only visible when the end of the finger is folded over the
// palm (we then see the back of the finger). Drawn as a midpoint ellipse.
void drawNail(Canvas& canvas, const Hand& hand, int f, const HandView& view, const Style& st, bool filled) {
    if (f == Thumb) return;
    const FingerChain& chain = hand.chain(f);
    if (chain.totalFlex[kBones - 1] <= 90.0) return;
    const Vec2 tip = view.toPixels(chain.point[kPoints - 1]);
    const Vec2 dip = view.toPixels(chain.point[kPoints - 2]);
    const Vec2 centre = tip + (dip - tip) * 0.30;
    const double w = hand.spec(f).width[kPoints - 1] * view.scale;
    if (filled) {
        canvas.fillEllipse(centre, w * 0.30, w * 0.36, st.nail);
        canvas.ellipse(centre, w * 0.30, w * 0.36, st.outline);
    } else {
        canvas.ellipse(centre, w * 0.30, w * 0.36, st.wire);
    }
}

void drawFingerSkin(Canvas& canvas, const Hand& hand, int f, const HandView& view, const RenderSettings& s,
                    const Style& st) {
    const FingerSpec& spec = hand.spec(f);
    const FingerChain& chain = hand.chain(f);

    std::vector<Vec2> pts;
    std::vector<double> radius;
    for (int i = 0; i < kPoints; ++i) {
        pts.push_back(view.toPixels(chain.point[i]));
        radius.push_back(spec.width[i] * 0.5 * view.scale);
    }

    if (!s.filled) {
        for (int b = 0; b < kBones; ++b) drawBone(canvas, pts[b], pts[b + 1], radius[b], radius[b + 1], st.wire, false);
    } else {
        // Pass 1: the whole finger one pixel fatter, in the outline colour.
        // Pass 2: the finger at its real size on top, leaving a 1-pixel border.
        for (int b = 0; b < kBones; ++b) {
            drawBone(canvas, pts[b], pts[b + 1], radius[b] + 1.0, radius[b + 1] + 1.0, st.outline, true);
        }
        for (int b = 0; b < kBones; ++b) {
            // Past 90 degrees of flex we are looking at the back of the bone.
            const Color c = chain.totalFlex[b] > 90.0 ? st.skinFolded : st.skin;
            drawBone(canvas, pts[b], pts[b + 1], radius[b], radius[b + 1], c, true);
        }
    }
    if (s.showDetails) drawNail(canvas, hand, f, view, st, s.filled);
}

void drawPalm(Canvas& canvas, const Hand& hand, const HandView& view, const RenderSettings& s, const Style& st) {
    // The palm outline is a closed Catmull-Rom spline through the control
    // points, sampled into a many-sided polygon and then scan-line filled.
    const std::vector<Vec2> control = toPixels(hand.palmOutline(), view);
    const std::vector<Vec2> outline = curves::sampleCatmullRom(control, 6, true);

    if (s.filled) {
        canvas.fillPolygon(outline, st.skin);
        canvas.polygonOutline(outline, st.outline);
    } else {
        canvas.polygonOutline(outline, st.wire);
        // Show the spline's control points in wireframe mode.
        const double r = std::max(1.0, view.scale * 0.018);
        for (const Vec2& p : control) canvas.circle(p, r, st.control);
    }

    if (!s.showDetails) return;

    // Pads at the base of the thumb and little finger: midpoint ellipses.
    for (const Hand::EllipseSpec& pad : hand.palmPads()) {
        const Vec2 c = view.toPixels(pad.centre);
        if (s.filled) {
            canvas.fillEllipse(c, pad.rx * view.scale, pad.ry * view.scale, st.skinShade);
        } else {
            canvas.ellipse(c, pad.rx * view.scale, pad.ry * view.scale, st.wire);
        }
    }

    // Palm creases: cubic Bezier curves, sampled and drawn as polylines.
    for (const std::vector<Vec2>& crease : hand.palmCreases()) {
        const std::vector<Vec2> ctrl = toPixels(crease, view);
        const int segments = curves::segmentsForLength(curves::polylineLength(ctrl), 3.0);
        canvas.polyline(curves::sampleBezier(ctrl, segments), s.filled ? st.crease : st.wire);
    }
}

void drawSkin(Canvas& canvas, const Hand& hand, const HandView& view, const RenderSettings& s, const Style& st) {
    const std::vector<Vec2> forearm = toPixels(hand.forearmOutline(), view);
    if (s.filled) {
        canvas.fillPolygon(forearm, st.skin);
        canvas.polygonOutline(forearm, st.outline);
    } else {
        canvas.polygonOutline(forearm, st.wire);
    }

    drawPalm(canvas, hand, view, s, st);

    // Painter's algorithm: draw back to front so nearer parts cover farther ones.
    // Fingers first (pinky to index), thumb last because it crosses in front.
    for (int f : {Pinky, Ring, Middle, Index, Thumb}) drawFingerSkin(canvas, hand, f, view, s, st);
}

void drawSkeleton(Canvas& canvas, const Hand& hand, const HandView& view, const Style& st) {
    const Vec2 wrist = view.toPixels(hand.wrist());
    const double jointR = std::max(1.0, std::round(view.scale * 0.028));

    // Bones: wrist -> base joint, then along the chain to the fingertip.
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerChain& chain = hand.chain(f);
        Vec2 prev = wrist;
        for (int i = 0; i < kPoints; ++i) {
            const Vec2 p = view.toPixels(chain.point[i]);
            canvas.line(prev, p, st.bone);
            prev = p;
        }
    }

    // Joints: a filled dot with a midpoint-circle ring around it.
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerChain& chain = hand.chain(f);
        for (int i = 0; i < kPoints; ++i) {
            const Vec2 p = view.toPixels(chain.point[i]);
            canvas.fillCircle(p, jointR - 1.0, st.bone);
            canvas.circle(p, jointR, st.joint);
        }
    }

    // The root of the hierarchy (wrist/palm) is drawn larger.
    canvas.fillCircle(wrist, jointR * 2.0, st.root);
    canvas.circle(wrist, jointR * 2.0 + 1.0, st.joint);
}

}  // namespace

void drawHand(Canvas& canvas, const Hand& hand, const HandView& view, const RenderSettings& settings) {
    const Style st = styleFor(settings);
    if (settings.showSkin) drawSkin(canvas, hand, view, settings, st);
    if (settings.showSkeleton) drawSkeleton(canvas, hand, view, st);
}

void drawHandScene(Framebuffer& fb, const Hand& hand, const RenderSettings& settings, const HandPose* ghostPose) {
    fb.clear(palette::background);
    Canvas canvas(fb, fb.bounds(), settings.lineAlgo);
    const HandView view = HandView::fit(fb.width(), fb.height());

    if (ghostPose) {
        Hand ghostHand;
        ghostHand.setPose(*ghostPose);
        RenderSettings g = settings;
        g.ghost = true;
        g.filled = false;
        g.showDetails = false;
        g.showSkeleton = false;
        drawHand(canvas, ghostHand, view, g);
    }
    drawHand(canvas, hand, view, settings);
}

// ===========================================================================
// Detail view
// ===========================================================================
// Window-to-viewport mapping: a point inside the window W is moved into the
// viewport V by
//      x_v = V.xmin + (x_w - W.xmin) * k,    k = V.width / W.width
// (the same for y). Applying this to our HandView gives a new view with
// scale * k and a shifted origin, so the hand is simply drawn again at a
// larger size - and clipped to V so it cannot spill onto the main view.
DetailView detailViewFor(const Framebuffer& fb, const Hand& hand) {
    const int w = fb.width();
    const int h = fb.height();
    const int size = std::max(8, static_cast<int>(std::min(w * 0.30, h * 0.46)));
    const int x0 = static_cast<int>(std::lround(w * 0.025));
    const int y0 = static_cast<int>(std::lround(h * 0.06));
    const ClipRect viewport{x0, y0, x0 + size - 1, y0 + size - 1};

    // Focus on the index and middle fingers (their three outer points).
    const HandView view = HandView::fit(w, h);
    Vec2 focus{0.0, 0.0};
    int count = 0;
    for (int f : {Index, Middle}) {
        for (int i = 1; i < kPoints; ++i) {
            focus = focus + view.toPixels(hand.chain(f).point[i]);
            ++count;
        }
    }
    focus = focus * (1.0 / count);

    const double magnification = 2.0;
    const int half = static_cast<int>(std::lround(size / (2.0 * magnification)));
    const IPoint c = toPixel(focus);
    const ClipRect window{c.x - half, c.y - half, c.x + half, c.y + half};
    return {window, viewport, static_cast<double>(viewport.width()) / window.width()};
}

void drawDetailView(Framebuffer& fb, const Hand& hand, const RenderSettings& settings) {
    const DetailView dv = detailViewFor(fb, hand);
    const HandView main = HandView::fit(fb.width(), fb.height());
    const double k = dv.magnification;

    // Mark the window on the main view with a dashed rectangle.
    Canvas screen(fb, fb.bounds(), settings.lineAlgo);
    screen.rectOutline(dv.window, palette::root, true);

    // Same hand, mapped window -> viewport, clipped to the viewport.
    const HandView zoomed{main.scale * k, dv.viewport.xmin + (main.ox - dv.window.xmin) * k,
                          dv.viewport.ymin + (main.oy - dv.window.ymin) * k};
    Canvas inset(fb, dv.viewport, settings.lineAlgo);
    inset.fillClipRect(palette::insetBackground);
    drawHand(inset, hand, zoomed, settings);
    inset.rectOutline(dv.viewport, palette::root);
}

// ===========================================================================
// Motion trails
// ===========================================================================
void drawMotionTrails(Framebuffer& fb, const HandPose& from, const HandPose& to, const RenderSettings& settings) {
    static const Color trailColors[kFingerCount] = {
        {255, 150, 60}, {90, 230, 120}, {80, 210, 255}, {230, 110, 240}, {255, 225, 80},
    };
    const HandView view = HandView::fit(fb.width(), fb.height());
    Canvas canvas(fb, fb.bounds(), settings.lineAlgo);

    // Keyframes: the pose at t = 0, 1/4, 1/2, 3/4, 1 of the transition.
    const int keyframes = 5;
    std::array<std::vector<Vec2>, kFingerCount> keys;
    Hand h;
    for (int k = 0; k < keyframes; ++k) {
        const double t = static_cast<double>(k) / (keyframes - 1);
        h.setPose(lerpPose(from, to, easeInOut(t)));
        for (int f = 0; f < kFingerCount; ++f) keys[f].push_back(view.toPixels(h.chain(f).point[kPoints - 1]));
    }

    const int width = std::max(1, static_cast<int>(std::lround(view.scale * 0.016)));
    const double dot = std::max(1.0, view.scale * 0.022);
    for (int f = 0; f < kFingerCount; ++f) {
        const std::vector<Vec2> path = curves::sampleCatmullRom(keys[f], 12, false);
        for (size_t i = 0; i + 1 < path.size(); ++i) canvas.thickLine(path[i], path[i + 1], width, trailColors[f]);
        for (const Vec2& p : keys[f]) {
            canvas.fillCircle(p, dot, trailColors[f]);
            canvas.circle(p, dot + 1.0, palette::text);
        }
    }
}
