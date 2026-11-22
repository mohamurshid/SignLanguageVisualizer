#pragma once
// GLRenderer: draws the 3D scene with OpenGL (Checkpoint 3).
//
// Uses only OpenGL 1.1, which every Windows PC has (opengl32.lib), so the
// program needs nothing installed:
//   - vertex arrays + glDrawElements for the meshes
//   - glLoadMatrixd with OUR matrices (Math3D): the camera, the hierarchy and
//     the projection are all computed by our own code
//   - the fixed-function lighting: three lights (key, fill, rim) and
//     materials, evaluated with the Phong reflection model
//         colour = ambient + diffuse * max(N.L, 0) + specular * max(R.V, 0)^shininess
//     per vertex, then interpolated across each triangle (Gouraud shading)
//   - three shading modes to compare: smooth (averaged normals), flat (one
//     normal per triangle) and wireframe (edges only)
//   - depth buffer for hidden surfaces (replaces the painter's algorithm of
//     the 2D version)
//
// The overlay (title, sign name, subtitles, help) is drawn last in 2D with
// an orthographic projection and alpha blending.

#include <functional>
#include <string>
#include <vector>

#include "Camera.h"
#include "Hand3D.h"

enum class ShadeMode { Smooth = 0, Flat, Wireframe, Count };
const char* shadeModeName(ShadeMode m);

struct RenderOptions {
    ShadeMode shade = ShadeMode::Smooth;
    bool body = true;         // head and torso reference
    bool skeleton = false;    // joints and bones drawn on top (x-ray)
    bool frames = false;      // each joint's local axes (red x, green y, blue z)
    bool leftHanded = false;  // mirror the whole scene: a left-handed signer
    bool lighting = true;
};

// A joint to highlight (Build mode): hand 0 = right, 1 = left; point 0..3.
struct JointRef {
    int hand = -1;
    int finger = -1;
    int point = -1;
    bool valid() const { return hand >= 0 && finger >= 0 && point >= 0; }
};

struct OverlayRect {
    int x, y, w, h;  // pixels, origin top-left
    float r, g, b, a;
};
struct OverlayText {
    int x, y;  // baseline, pixels from the top-left
    std::string text;
    float r, g, b;
};
struct Overlay {
    std::vector<OverlayRect> rects;
    std::vector<OverlayText> texts;
};

class GLRenderer {
public:
    void resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }

    // The matrix that maps world points to clip space, including the mirror
    // for a left-handed signer (used for picking joints with the mouse).
    Mat4 projViewScene(const OrbitCamera& camera, const RenderOptions& options) const;

    // Draws the background, the body and both hands (right = hands[0]).
    void drawScene(const OrbitCamera& camera, const Hand3D& right, const Hand3D& left, const RenderOptions& options,
                   const JointRef& highlight);

    // Draws the 2D overlay. drawText is supplied by the window code (Win32 or
    // GLUT), because each has its own way of drawing text in OpenGL.
    void drawOverlay(const Overlay& overlay, const std::function<void(const OverlayText&)>& drawText);

private:
    void drawBackground();
    void drawArm(const Mat4& viewScene, const Hand3D& hand, bool leftArm, const RenderOptions& options);
    void drawHand(const Mat4& viewScene, const Hand3D& hand, int handIndex, const RenderOptions& options,
                  const JointRef& highlight);
    void drawSkeleton(const Mat4& viewScene, const Hand3D& hand);
    void drawFrames(const Mat4& viewScene, const Hand3D& hand);

    int width_ = 960;
    int height_ = 720;
};
