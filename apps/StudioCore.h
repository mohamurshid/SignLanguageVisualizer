#pragma once
// StudioCore: everything the 3D application (sign_studio) does, except the
// window itself. studio_win32.cpp (Windows + OpenGL, nothing to install) and
// studio_glut.cpp (OpenGL + freeglut) only open a window, pass keys / mouse /
// time to this class and give it a way to draw text.
//
// Modes
//   Learn : browse the dictionary - letters, numbers, words - each sign plays
//           in a loop with its description and where it is made
//   Type  : type a word or a sentence; known words are signed, other words
//           are fingerspelled, with subtitles showing what is being signed
//   Build : pose the hand yourself - drag any joint with the mouse; the
//           closest letter or number is shown as a hint
//
// The camera orbits with the mouse; presets show the front, three-quarter,
// side and the signer's own view. A close-up follows the hand.

#include <functional>
#include <string>
#include <vector>

#include "Camera.h"
#include "GLRenderer.h"
#include "Speller.h"
#include "Timeline.h"

enum class StudioKey { Left, Right, Up, Down, Enter, Backspace, Tab, Escape };
enum class StudioMode { Learn, Type, Build };
enum class Category { Letters = 0, Numbers, Words, Count };

class StudioCore {
public:
    StudioCore();

    static const char* windowTitle() { return "Sign Language Gesture Visualizer - 3D"; }

    // The window code supplies text drawing and measuring (size 0 = normal,
    // 1 = large), because each window system draws text differently.
    void setText(std::function<void(const OverlayText&, int size)> draw, std::function<int(const std::string&, int size)> measure);

    void resize(int width, int height);
    bool update(double nowSeconds);  // true = draw again
    void render();                   // the scene + overlay (needs a current OpenGL context)
    void renderScene();              // the scene only (no overlay)
    void drawLabel(int x, int y, const std::string& text, int size);  // text over the scene

    void onChar(char c);  // printable characters, also ' ', and Esc/Enter/Backspace as their codes
    void onKey(StudioKey k);
    void onMouseDown(int x, int y, bool rightButton);
    void onMouseMove(int x, int y);
    void onMouseUp();
    void onWheel(int steps);  // + = zoom in
    bool wantsQuit() const { return quit_; }

    // Used by the capture tool (screenshots and GIFs for the documentation).
    void showSign(Category c, int index);
    void performText(const std::string& text);
    void setPlaybackTime(double t);
    double playbackDuration() const;
    void enterBuildMode();
    void setBuildPose(const HandPose& pose);
    void selectJoint(JointRef j) { selected_ = j; }
    void setHelpVisible(bool v) { help_ = v; }
    void setFollowHand(bool v) { follow_ = v; }
    OrbitCamera& camera() { return camera_; }
    RenderOptions& options() { return options_; }
    StudioMode mode() const { return mode_; }
    const Hand3D& rightHand() const { return right_; }

private:
    const std::vector<Sign3D>& categoryList(Category c) const;
    void playCurrent();
    void applyFrame(const Frame3D& f);
    JointRef pick(int x, int y) const;
    void buildOverlay(Overlay& o);
    void startTyping();

    GLRenderer renderer_;
    OrbitCamera camera_;
    RenderOptions options_;
    TimelinePlayer player_;
    std::vector<Key3D> keys_;  // what the player is playing
    Performance performance_;
    bool performing_ = false;

    StudioMode mode_ = StudioMode::Learn;
    Category category_ = Category::Words;
    int index_[static_cast<int>(Category::Count)] = {0, 0, 0};

    Hand3D right_, left_;
    HandPose buildPose_;
    Placement3D buildPlace_;
    JointRef selected_;
    JointRef hover_;

    bool typing_ = false;
    std::string typed_;
    std::string lastTyped_;
    bool help_ = false;
    bool follow_ = false;
    bool quit_ = false;

    bool dragging_ = false;
    bool draggingJoint_ = false;
    int lastX_ = 0, lastY_ = 0;
    double lastTime_ = -1.0;
    double toastUntil_ = 0.0;
    double now_ = 0.0;
    std::string toast_;

    std::function<void(const OverlayText&, int)> drawText_;
    std::function<int(const std::string&, int)> measureText_;
    std::vector<int> textSizes_;  // size of each overlay text (parallel to Overlay::texts)
};
