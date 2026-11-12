#pragma once
// ViewerCore: everything the interactive viewers do, except the window itself.
//
// Both viewer_win32.cpp (Windows' own window functions) and viewer_glut.cpp
// (OpenGL + freeglut) only open a window, pass key presses / mouse events /
// time to this class, and copy framebuffer() to the screen. All drawing is
// still done by our own algorithms.
//
// Modes
//   Hand   : show a sign; move it with the arrow keys (rotate, scale)
//   Watch  : play an animated sign (keyframes + interpolation), with speed
//   Build  : drag a joint to bend it; drag elsewhere to turn the wrist
//   Pages  : the algorithm and transformation pages

#include <string>
#include <vector>

#include "Animation.h"
#include "Framebuffer.h"
#include "Hand.h"
#include "HandRenderer.h"

struct HudText {
    int x;            // window pixels from the left (centre of the text if centred)
    int yFromBottom;  // window pixels from the bottom
    std::string text;
    Color color;
    bool centred = false;
};

enum class SpecialKey { Left, Right, Up, Down };

class ViewerCore {
public:
    ViewerCore();

    void resize(int windowWidth, int windowHeight);
    void onChar(char key);
    void onSpecialKey(SpecialKey key);
    void onMouseDown(int x, int yFromTop);
    void onMouseMove(int x, int yFromTop);
    void onMouseUp();

    // Moves animations forward. Returns true if the picture must be redrawn.
    bool update(double nowSeconds);
    void render();

    const Framebuffer& framebuffer() const { return fb_; }
    int pixelSize() const;
    std::vector<HudText> hud() const;
    bool wantsQuit() const { return quit_; }
    static const char* windowTitle() { return "Sign Language Gesture Visualizer - Checkpoint 2"; }

private:
    enum class Mode { Hand, Watch, Build, Pages };

    void selectSign(int index);
    void startTransition(const HandPose& to);
    void startWatching(int animationIndex);
    void applyViewMode();
    Vec2 toFramebuffer(int x, int yFromTop) const;
    bool pick(Vec2 p);
    void dragTo(Vec2 p);
    int pageCount() const;

    // Window and framebuffer
    int windowW_ = 960;
    int windowH_ = 720;
    int pixelSizeIndex_ = 1;
    Framebuffer fb_;

    // Scene
    Mode mode_ = Mode::Hand;
    Hand hand_;
    RenderSettings settings_;
    Placement placement_;  // the user's placement (arrow keys, L)
    int viewMode_ = 0;
    int page_ = 0;
    bool detailView_ = false;
    bool trails_ = false;
    bool quit_ = false;

    // Sign transitions (Hand mode)
    int signIndex_ = 0;
    HandPose fromPose_;
    HandPose toPose_;
    double transitionStart_ = -1.0;
    bool autoPlay_ = false;
    double lastAutoSign_ = 0.0;

    // Watch mode
    int animIndex_ = -1;
    AnimationPlayer player_;

    // Build mode: the bone being aimed (finger, bone), or the wrist
    int selFinger_ = -1;
    int selBone_ = -1;
    bool dragging_ = false;
    bool draggingWrist_ = false;
    double dragStartAngle_ = 0.0;
    double dragStartWrist_ = 0.0;

    double now_ = 0.0;
    double last_ = -1.0;
};
