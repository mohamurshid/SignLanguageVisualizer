#pragma once
// HandRenderer: draws the hand using ONLY our own algorithms.
//
//   Polygons  (scan-line fill + outline)   : forearm, finger bones (skin)
//   Spline    (closed Catmull-Rom)         : smooth palm outline, then filled
//   Circles   (midpoint, filled)           : rounded knuckles and fingertips
//   Ellipses  (midpoint, filled)           : palm pads, fingernails
//   Bezier    (cubic, de Casteljau)        : palm crease lines
//   Lines     (DDA or Bresenham)           : skeleton bones
//   Circles   (midpoint outline)           : skeleton joint markers
//   Clipping  (Cohen-Sutherland,
//              Sutherland-Hodgman)         : everything, through Canvas
//   Spline    (open Catmull-Rom)           : fingertip motion trails
//
// Checkpoint 2: every point goes through a chain of matrices on its way to
// the screen:
//      screen = View * Forearm * R(wrist) * (finger joint frames) * point
// View maps hand units to pixels, Forearm and R(wrist) come from the hand's
// Placement, and the joint frames come from the matrix stack in Hand.cpp.

#include "Canvas.h"
#include "Framebuffer.h"
#include "Hand.h"
#include "Raster.h"
#include "Transform.h"

struct RenderSettings {
    LineAlgo lineAlgo = LineAlgo::Bresenham;
    bool filled = true;        // false = wireframe: outlines only
    bool showSkin = true;
    bool showSkeleton = true;
    bool showDetails = true;   // palm creases (Bezier), pads and nails (ellipses)
    bool ghost = false;        // draw everything in one dim colour (used for "before" poses)
    bool showFrames = false;   // draw each joint's local coordinate frame (matrix stack)
};

namespace palette {
constexpr Color background{10, 16, 32};
constexpr Color insetBackground{18, 28, 52};
constexpr Color skin{226, 182, 146};
constexpr Color skinShade{216, 170, 134};   // palm pads
constexpr Color skinFolded{198, 152, 118};  // bones folded back over the palm
constexpr Color skinOutline{92, 56, 42};
constexpr Color crease{170, 118, 90};
constexpr Color nail{236, 192, 178};
constexpr Color wireframe{236, 196, 160};
constexpr Color ghost{70, 84, 120};
constexpr Color bone{40, 130, 255};
constexpr Color joint{140, 215, 255};
constexpr Color root{255, 196, 70};
constexpr Color controlPoint{255, 120, 160};
constexpr Color text{235, 240, 255};
constexpr Color dim{90, 100, 130};
}  // namespace palette

// The view (camera) matrix: hand units -> framebuffer pixels.
struct HandView {
    Mat3 m;        // the matrix itself
    double scale;  // pixels per hand unit (for radii and line widths)

    Vec2 toPixels(Vec2 p) const { return m.apply(p); }
    // This view followed by a local transformation (view * local).
    HandView then(const Mat3& local) const { return {m * local, scale * local.uniformScale()}; }
    // The standard view: T(wrist position on screen) * S(pixels per unit).
    static HandView fit(int width, int height);
};

// The matrices from forearm / hand coordinates to framebuffer pixels for this
// hand (View * Placement). Build mode uses their inverses for picking.
Mat3 forearmToScreen(const HandView& view, const Hand& hand);
Mat3 handToScreen(const HandView& view, const Hand& hand);

// Draws the hand onto a canvas (does not clear it).
void drawHand(Canvas& canvas, const Hand& hand, const HandView& view, const RenderSettings& settings);

// Clears the framebuffer and draws the whole scene. If 'ghostPose' is given,
// that pose is drawn first as a dim outline (to show where the hand came from).
void drawHandScene(Framebuffer& fb, const Hand& hand, const RenderSettings& settings,
                   const HandPose* ghostPose = nullptr);
// Same, with a complete ghost hand (its own pose and placement).
void drawHandSceneWithGhost(Framebuffer& fb, const Hand& hand, const RenderSettings& settings, const Hand& ghost);

// ---- Detail view: clipping + window-to-viewport mapping -------------------------
// A square "window" around the fingers is marked on the main view and drawn
// magnified into a small "viewport" in the corner. Everything drawn in the
// viewport is clipped to it.
struct DetailView {
    ClipRect window;    // area of the main view that is magnified
    ClipRect viewport;  // where it is shown
    double magnification;
};
DetailView detailViewFor(const Framebuffer& fb, const Hand& hand);
void drawDetailView(Framebuffer& fb, const Hand& hand, const RenderSettings& settings);

// ---- Motion trails: open Catmull-Rom splines -------------------------------------
// Samples the transition from 'from' to 'to' at 5 keyframes and draws a
// spline through each fingertip's keyframe positions.
void drawMotionTrails(Framebuffer& fb, const HandPose& from, const HandPose& to, const RenderSettings& settings,
                      const Placement& placement = Placement{});

// A path of the wrist in hand units (for example an animation's motion
// curve), drawn as a dashed polyline with a dot at every keyframe.
void drawWristPath(Canvas& canvas, const HandView& view, const std::vector<Vec2>& path,
                   const std::vector<Vec2>& keyframes);
