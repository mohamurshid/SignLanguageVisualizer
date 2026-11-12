#include "ViewerCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Canvas.h"
#include "Poses.h"
#include "TestScene.h"
#include "Transform.h"
#include "TransformScene.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
const int kPixelSizes[] = {1, 2, 3, 4, 6, 8, 12};
constexpr int kPixelSizeCount = static_cast<int>(sizeof(kPixelSizes) / sizeof(kPixelSizes[0]));
constexpr double kTransitionSeconds = 0.5;
constexpr double kAutoPlaySeconds = 1.8;
constexpr Color kDim{150, 165, 200};

double degrees(double radians) { return radians * 180.0 / kPi; }
double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

std::string format(const char* fmt, double a, double b = 0.0, double c = 0.0) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), fmt, a, b, c);
    return buf;
}
}  // namespace

ViewerCore::ViewerCore() {
    toPose_ = signLibrary()[0];
    fromPose_ = toPose_;
    hand_.setPose(toPose_);
    resize(windowW_, windowH_);
}

int ViewerCore::pixelSize() const { return kPixelSizes[pixelSizeIndex_]; }
int ViewerCore::pageCount() const { return algorithmPageCount() + transformPageCount(); }

void ViewerCore::resize(int w, int h) {
    windowW_ = std::max(1, w);
    windowH_ = std::max(1, h);
}

// ---- Sign changes ------------------------------------------------------------------
void ViewerCore::startTransition(const HandPose& to) {
    fromPose_ = hand_.pose();
    toPose_ = to;
    transitionStart_ = now_;
}

void ViewerCore::selectSign(int index) {
    const int n = static_cast<int>(signLibrary().size());
    signIndex_ = ((index % n) + n) % n;
    if (mode_ == Mode::Watch || mode_ == Mode::Pages) mode_ = Mode::Hand;
    player_.stop();
    startTransition(signLibrary()[signIndex_]);
}

void ViewerCore::startWatching(int index) {
    const auto& lib = animationLibrary();
    animIndex_ = ((index % static_cast<int>(lib.size())) + static_cast<int>(lib.size())) % static_cast<int>(lib.size());
    mode_ = Mode::Watch;
    autoPlay_ = false;
    player_.start(&lib[animIndex_]);
}

void ViewerCore::applyViewMode() {
    settings_.showSkin = viewMode_ != 2;
    settings_.showSkeleton = viewMode_ != 1;
}

// ---- Keyboard ------------------------------------------------------------------------
void ViewerCore::onChar(char key) {
    if (key >= '1' && key <= '9') {
        autoPlay_ = false;
        selectSign(key - '1');
        return;
    }
    switch (key) {
        case 27: quit_ = true; break;  // Esc
        case ',': case '<': autoPlay_ = false; selectSign(signIndex_ - 1); break;
        case '.': case '>': autoPlay_ = false; selectSign(signIndex_ + 1); break;
        case 'p': case 'P':
            autoPlay_ = !autoPlay_;
            if (autoPlay_) {
                mode_ = Mode::Hand;
                player_.stop();
                lastAutoSign_ = now_ - kAutoPlaySeconds;
            }
            break;
        case 'w': case 'W': startWatching(mode_ == Mode::Watch ? animIndex_ + 1 : std::max(0, animIndex_)); break;
        case '[': player_.setSpeed(player_.speed() / 2.0); break;
        case ']': player_.setSpeed(player_.speed() * 2.0); break;
        case 'l': case 'L': placement_.mirror = !placement_.mirror; break;
        case 'b': case 'B':
            if (mode_ == Mode::Build) {
                mode_ = Mode::Hand;
            } else {
                mode_ = Mode::Build;
                autoPlay_ = false;
                player_.stop();
                hand_.setPose(toPose_);
                transitionStart_ = -1.0;
            }
            selFinger_ = selBone_ = -1;
            break;
        case 'r': case 'R':
            placement_ = Placement{};
            hand_.setPose(signLibrary()[signIndex_]);
            toPose_ = fromPose_ = hand_.pose();
            transitionStart_ = -1.0;
            player_.setSpeed(1.0);
            selFinger_ = selBone_ = -1;
            break;
        case 'd': case 'D':
            settings_.lineAlgo = settings_.lineAlgo == LineAlgo::DDA ? LineAlgo::Bresenham : LineAlgo::DDA;
            break;
        case 'f': case 'F': settings_.filled = !settings_.filled; break;
        case 's': case 'S': viewMode_ = (viewMode_ + 1) % 3; applyViewMode(); break;
        case 'x': case 'X': settings_.showDetails = !settings_.showDetails; break;
        case 'g': case 'G': settings_.showFrames = !settings_.showFrames; break;
        case 'c': case 'C': detailView_ = !detailView_; if (mode_ == Mode::Pages) mode_ = Mode::Hand; break;
        case 'm': case 'M': trails_ = !trails_; if (mode_ == Mode::Pages) mode_ = Mode::Hand; break;
        case '+': case '=': if (pixelSizeIndex_ < kPixelSizeCount - 1) ++pixelSizeIndex_; break;
        case '-': case '_': if (pixelSizeIndex_ > 0) --pixelSizeIndex_; break;
        case 't': case 'T':
            if (mode_ != Mode::Pages) {
                mode_ = Mode::Pages;
                page_ = 0;
                player_.stop();
                autoPlay_ = false;
            } else if (++page_ >= pageCount()) {
                mode_ = Mode::Hand;
            }
            break;
        default: break;
    }
}

void ViewerCore::onSpecialKey(SpecialKey key) {
    if (mode_ == Mode::Build && selFinger_ >= 0) {
        // Arrow keys change the selected joint: Up/Down curl it towards /
        // away from the viewer (flex), Left/Right bend it sideways.
        HandPose pose = hand_.pose();
        FingerPose& fp = pose.fingers[selFinger_];
        if (key == SpecialKey::Up) fp.flex[selBone_] = clampd(fp.flex[selBone_] + 10.0, -20.0, 120.0);
        if (key == SpecialKey::Down) fp.flex[selBone_] = clampd(fp.flex[selBone_] - 10.0, -20.0, 120.0);
        if (key == SpecialKey::Left) fp.bend[selBone_] = clampd(fp.bend[selBone_] + 5.0, -90.0, 90.0);
        if (key == SpecialKey::Right) fp.bend[selBone_] = clampd(fp.bend[selBone_] - 5.0, -90.0, 90.0);
        pose.name = "custom";
        pose.description = "posed by hand in Build mode";
        hand_.setPose(pose);
        toPose_ = pose;
        return;
    }
    // Otherwise the arrows transform the whole hand.
    if (key == SpecialKey::Left) placement_.wristAngle += 5.0;
    if (key == SpecialKey::Right) placement_.wristAngle -= 5.0;
    if (key == SpecialKey::Up) placement_.scale = clampd(placement_.scale + 0.05, 0.4, 1.6);
    if (key == SpecialKey::Down) placement_.scale = clampd(placement_.scale - 0.05, 0.4, 1.6);
}

// ---- Mouse (Build mode) ------------------------------------------------------------------
// The window shows the framebuffer enlarged by the pixel size, with its
// bottom row on the window's bottom edge.
Vec2 ViewerCore::toFramebuffer(int x, int yFromTop) const {
    const double ps = pixelSize();
    return {(x + 0.5) / ps, (windowH_ - 1 - yFromTop + 0.5) / ps};
}

// Finds the bone end (middle joint, end joint or fingertip) nearest to p.
bool ViewerCore::pick(Vec2 p) {
    const Mat3 toScreen = handToScreen(HandView::fit(fb_.width(), fb_.height()), hand_);
    double best = 1e30;
    selFinger_ = selBone_ = -1;
    for (int f = 0; f < kFingerCount; ++f) {
        for (int k = 1; k < kPoints; ++k) {
            const Vec2 q = toScreen.apply(hand_.chain(f).point[k]);
            const double d = (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y);
            if (d < best) {
                best = d;
                selFinger_ = f;
                selBone_ = k - 1;  // dragging the END of bone k-1 aims that bone
            }
        }
    }
    const double radius = std::max(6.0, fb_.height() * 0.035);
    if (best > radius * radius) selFinger_ = selBone_ = -1;
    return selFinger_ >= 0;
}

// Aims the selected bone at p, or turns the wrist.
//
// The key idea is the INVERSE matrix. The joint's frame maps local
// coordinates to the screen:  screen = M * local. So  local = M^-1 * screen,
// and in the joint's own frame the angle to the mouse is simply
// atan2(local.y, local.x) - which is exactly the joint's bend angle.
void ViewerCore::dragTo(Vec2 p) {
    const HandView view = HandView::fit(fb_.width(), fb_.height());
    if (draggingWrist_) {
        // Angle of the mouse around the wrist, in forearm coordinates
        // (so it also works for the mirrored left hand).
        const Vec2 local = forearmToScreen(view, hand_).inverse().apply(p);
        const double angle = degrees(std::atan2(local.y, local.x));
        placement_.wristAngle = clampd(dragStartWrist_ + angle - dragStartAngle_, -90.0, 90.0);
        return;
    }
    if (selFinger_ < 0) return;
    const FingerChain& chain = hand_.chain(selFinger_);
    const Mat3 joint = handToScreen(view, hand_) * chain.jointFrame[selBone_];
    Vec2 local = joint.inverse().apply(p);
    // A bone flexed past 90 degrees points backwards on screen (cos < 0).
    if (std::cos(chain.totalFlex[selBone_] * kPi / 180.0) < 0.0) local = local * -1.0;
    HandPose pose = hand_.pose();
    pose.fingers[selFinger_].bend[selBone_] = clampd(degrees(std::atan2(local.y, local.x)), -90.0, 90.0);
    pose.name = "custom";
    pose.description = "posed by hand in Build mode";
    hand_.setPose(pose);
    toPose_ = pose;
}

void ViewerCore::onMouseDown(int x, int yFromTop) {
    if (mode_ != Mode::Build) return;
    const Vec2 p = toFramebuffer(x, yFromTop);
    dragging_ = true;
    draggingWrist_ = !pick(p);
    if (draggingWrist_) {
        const Vec2 local = forearmToScreen(HandView::fit(fb_.width(), fb_.height()), hand_).inverse().apply(p);
        dragStartAngle_ = degrees(std::atan2(local.y, local.x));
        dragStartWrist_ = placement_.wristAngle;
    }
}

void ViewerCore::onMouseMove(int x, int yFromTop) {
    if (mode_ == Mode::Build && dragging_) dragTo(toFramebuffer(x, yFromTop));
}

void ViewerCore::onMouseUp() {
    dragging_ = false;
    draggingWrist_ = false;
}

// ---- Time -------------------------------------------------------------------------------
bool ViewerCore::update(double nowSeconds) {
    now_ = nowSeconds;
    const double dt = last_ < 0.0 ? 0.0 : std::min(0.1, now_ - last_);
    last_ = now_;

    if (autoPlay_ && mode_ == Mode::Hand && now_ - lastAutoSign_ >= kAutoPlaySeconds) {
        lastAutoSign_ = now_;
        selectSign(signIndex_ + 1);
    }
    if (mode_ == Mode::Watch && player_.playing()) {
        player_.advance(dt);
        const AnimationFrame f = player_.frame();
        hand_.setPose(f.pose);
        Placement p = f.placement;
        p.mirror = placement_.mirror;  // L still swaps hands while watching
        hand_.setPlacement(p);
        return true;
    }
    if (transitionStart_ >= 0.0) {
        const double t = (now_ - transitionStart_) / kTransitionSeconds;
        if (t >= 1.0) {
            hand_.setPose(toPose_);
            transitionStart_ = -1.0;
        } else {
            hand_.setPose(lerpPose(fromPose_, toPose_, easeInOut(t)));
        }
    }
    hand_.setPlacement(placement_);
    return true;  // redrawing is cheap, so the picture is simply redrawn every tick
}

// ---- Drawing ----------------------------------------------------------------------------
void ViewerCore::render() {
    const int ps = pixelSize();
    const int fbW = (windowW_ + ps - 1) / ps;
    const int fbH = (windowH_ + ps - 1) / ps;
    if (fb_.width() != fbW || fb_.height() != fbH) fb_.resize(fbW, fbH);

    if (mode_ == Mode::Pages) {
        if (page_ < algorithmPageCount()) {
            const double t = 0.5 + 0.45 * std::sin(now_ * 1.2);
            drawAlgorithmPage(fb_, page_, settings_, page_ == 4 ? t : 0.5);
        } else {
            drawTransformPage(fb_, page_ - algorithmPageCount(), settings_);
        }
        return;
    }

    const bool watching = mode_ == Mode::Watch && player_.playing();
    drawHandScene(fb_, hand_, settings_, (trails_ && !watching) ? &fromPose_ : nullptr);
    const HandView view = HandView::fit(fb_.width(), fb_.height());
    Canvas canvas(fb_, fb_.bounds(), settings_.lineAlgo);
    if (trails_) {
        if (watching) {
            drawWristPath(canvas, view, motionPath(*player_.animation()), keyframePositions(*player_.animation()));
        } else {
            drawMotionTrails(fb_, fromPose_, toPose_, settings_, placement_);
        }
    }
    if (mode_ == Mode::Build && selFinger_ >= 0) {
        // Highlight the bone being aimed: a ring at its end.
        const Vec2 q = handToScreen(view, hand_).apply(hand_.chain(selFinger_).point[selBone_ + 1]);
        canvas.circle(q, std::max(4.0, view.scale * 0.05), palette::root);
        canvas.circle(q, std::max(5.0, view.scale * 0.05 + 1.0), palette::root);
    }
    if (detailView_) drawDetailView(fb_, hand_, settings_);
}

std::vector<HudText> ViewerCore::hud() const {
    std::vector<HudText> lines;
    const int top = windowH_;
    lines.push_back({12, top - 22, "Sign Language Gesture Visualizer  |  Checkpoint 2: transformations and animation",
                     palette::text});

    std::string status;
    if (mode_ == Mode::Pages) {
        const int n = algorithmPageCount();
        status = format("Page %.0f/%.0f: ", page_ + 1.0, static_cast<double>(pageCount())) +
                 (page_ < n ? algorithmPageTitle(page_) : transformPageTitle(page_ - n));
    } else if (mode_ == Mode::Watch && player_.playing()) {
        const SignAnimation& a = *player_.animation();
        status = "Watch: " + a.name + " - " + a.description +
                 format("   t = %.2f / %.2f s   speed %.3gx", player_.time(), a.duration(), player_.speed());
    } else if (mode_ == Mode::Build) {
        status = selFinger_ >= 0
                     ? std::string("Build: ") + hand_.spec(selFinger_).name + " " +
                           hand_.spec(selFinger_).pointNames[selBone_] + " selected - drag to bend, Up/Down to curl"
                     : "Build: drag a joint to bend it, drag anywhere else to turn the wrist";
    } else {
        status = "Sign " + std::to_string(signIndex_ + 1) + "/" + std::to_string(signLibrary().size()) + ": " +
                 hand_.pose().name + " - " + hand_.pose().description;
    }
    lines.push_back({12, top - 42, status, palette::root});

    std::string flags;
    if (detailView_) flags += "   [detail view]";
    if (trails_) flags += "   [trails]";
    if (settings_.showFrames) flags += "   [frames]";
    if (autoPlay_) flags += "   [autoplay]";
    const Placement& p = hand_.placement();
    lines.push_back({12, top - 62,
                     std::string("Lines: ") + lineAlgoName(settings_.lineAlgo) +
                         "   Fill: " + (settings_.filled ? "scan-line" : "wireframe") +
                         format("   Wrist: %.0f deg   Scale: %.2f", p.wristAngle, p.scale) +
                         "   Hand: " + (p.mirror ? "left (mirrored)" : "right") +
                         format("   Pixels: %.0fx", pixelSize()) + flags,
                     kDim});

    if (mode_ == Mode::Pages) {
        const int n = algorithmPageCount();
        const std::vector<PageLabel> labels = page_ < n ? algorithmPageLabels(page_) : transformPageLabels(page_ - n);
        for (const PageLabel& l : labels) {
            lines.push_back({static_cast<int>(l.fx * windowW_), static_cast<int>(l.fy * windowH_), l.text,
                             palette::text, true});
        }
    } else if (detailView_) {
        const DetailView dv = detailViewFor(fb_, hand_);
        lines.push_back({dv.viewport.xmin * pixelSize(), (dv.viewport.ymax + 1) * pixelSize() + 6,
                         "Detail view (clipped)", palette::root});
    }
    lines.push_back({12, 28, "1-9 , . sign  P play  W watch  [ ] speed  L left hand  arrows turn/scale  B build  R reset",
                     kDim});
    lines.push_back({12, 10, "D lines  F fill  S view  X details  G frames  C detail  M trails  T pages  +/- pixels  Esc",
                     kDim});
    return lines;
}
