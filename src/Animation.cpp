#include "Animation.h"

#include <algorithm>
#include <cmath>

#include "Curves.h"
#include "Poses.h"

// ===========================================================================
// Sampling: in-betweening between keyframes
// ===========================================================================
AnimationFrame sampleAnimation(const SignAnimation& anim, double t) {
    const std::vector<Keyframe>& keys = anim.keys;
    if (keys.empty()) return {};
    if (keys.size() == 1 || t <= keys.front().time) return {keys.front().pose, keys.front().placement};
    if (t >= keys.back().time) return {keys.back().pose, keys.back().placement};

    // Find the pair of keyframes around t: keys[i].time <= t < keys[i+1].time.
    size_t i = 0;
    while (i + 2 < keys.size() && t >= keys[i + 1].time) ++i;
    const Keyframe& a = keys[i];
    const Keyframe& b = keys[i + 1];

    // u goes 0 -> 1 between the two keyframes; easing makes the motion start
    // and stop gently instead of jerking (constant speed looks robotic).
    const double u = (t - a.time) / (b.time - a.time);
    const double e = easeInOut(u);

    AnimationFrame f;
    f.pose = lerpPose(a.pose, b.pose, e);
    f.placement.wristAngle = a.placement.wristAngle + (b.placement.wristAngle - a.placement.wristAngle) * e;
    f.placement.scale = a.placement.scale + (b.placement.scale - a.placement.scale) * e;
    f.placement.mirror = a.placement.mirror;

    // Position: a Catmull-Rom span through the neighbouring keyframes
    // (the end keyframes are repeated so the curve starts and ends on them).
    const size_t n = keys.size();
    const Vec2 p0 = keys[i == 0 ? 0 : i - 1].placement.position;
    const Vec2 p1 = a.placement.position;
    const Vec2 p2 = b.placement.position;
    const Vec2 p3 = keys[std::min(n - 1, i + 2)].placement.position;
    if (std::fabs(p1.x - p2.x) < 1e-12 && std::fabs(p1.y - p2.y) < 1e-12) {
        f.placement.position = p1;  // a hold: the hand stays exactly where it is
    } else {
        f.placement.position = curves::catmullRom(p0, p1, p2, p3, e);
    }
    return f;
}

std::vector<Vec2> keyframePositions(const SignAnimation& anim) {
    std::vector<Vec2> pts;
    for (const Keyframe& k : anim.keys) {
        // Skip holds: a keyframe at the same spot as the one before.
        if (!pts.empty() && std::fabs(pts.back().x - k.placement.position.x) < 1e-9 &&
            std::fabs(pts.back().y - k.placement.position.y) < 1e-9)
            continue;
        pts.push_back(k.placement.position);
    }
    return pts;
}

std::vector<Vec2> motionPath(const SignAnimation& anim, int segmentsPerSpan) {
    return curves::sampleCatmullRom(keyframePositions(anim), segmentsPerSpan, false);
}

// ===========================================================================
// Playback
// ===========================================================================
namespace {
constexpr double kLoopPause = 0.6;  // seconds of stillness before repeating
}

void AnimationPlayer::start(const SignAnimation* anim) {
    anim_ = anim;
    time_ = 0.0;
}

void AnimationPlayer::advance(double seconds) {
    if (!anim_) return;
    time_ += seconds * speed_;
    const double loopLength = anim_->duration() + kLoopPause;
    if (time_ > loopLength) time_ = std::fmod(time_, loopLength);
}

void AnimationPlayer::setSpeed(double speed) { speed_ = std::min(4.0, std::max(0.125, speed)); }

AnimationFrame AnimationPlayer::frame() const {
    if (!anim_) return {};
    return sampleAnimation(*anim_, time_);
}

// ===========================================================================
// The animated signs
// ===========================================================================
// Positions are in hand units relative to the usual spot (+x = right, +y =
// up). Our hand is a RIGHT hand with the palm towards the viewer. Moving
// towards the viewer is shown by scaling up (it gets closer, so it looks
// bigger).
namespace {

Keyframe key(double time, const HandPose& pose, double x, double y, double wrist = 0.0, double scale = 1.0) {
    Keyframe k;
    k.time = time;
    k.pose = pose;
    k.placement.position = {x, y};
    k.placement.wristAngle = wrist;
    k.placement.scale = scale;
    return k;
}

// "NO": index and middle fingers held out, snapping shut onto the thumb.
HandPose noOpen() {
    HandPose p = signByName("U");
    p.name = "NO";
    p.fingers[Thumb].bend = {-10, 5, 0};
    p.fingers[Thumb].flex = {0, 0, 0};
    return p;
}
HandPose noClosed() {
    HandPose p = noOpen();
    p.fingers[Index].flex = {50, 55, 20};
    p.fingers[Middle].flex = {50, 55, 20};
    p.fingers[Thumb].bend = {30, 10, 5};
    p.fingers[Thumb].flex = {25, 15, 10};
    return p;
}

std::vector<SignAnimation> buildLibrary() {
    const HandPose five = signByName("5");
    const HandPose flat = signByName("B");
    const HandPose fist = signByName("S");
    std::vector<SignAnimation> lib;

    // HELLO: a salute that starts at the forehead and moves out to the side.
    lib.push_back({"HELLO", "flat hand moves out from the forehead, like a salute",
                   {key(0.0, flat, -0.4, 0.04, 10), key(0.35, flat, -0.4, 0.04, 10), key(0.85, flat, 0.0, 0.02, 0),
                    key(1.35, flat, 0.4, -0.15, -18), key(1.8, flat, 0.4, -0.15, -18)}});

    // GOODBYE: the open hand waves by rotating about the wrist.
    lib.push_back({"GOODBYE", "open hand waves side to side, turning at the wrist",
                   {key(0.0, five, 0, 0.0, 0), key(0.3, five, 0, 0.0, 22), key(0.7, five, 0, 0.0, -22),
                    key(1.1, five, 0, 0.0, 22), key(1.5, five, 0, 0.0, -22), key(1.8, five, 0, 0.0, 0)}});

    // YES: the fist nods: down and towards the viewer (bigger), then back.
    lib.push_back({"YES", "fist nods down and forwards twice",
                   {key(0.0, fist, 0, 0.12, 0, 1.0), key(0.3, fist, 0, -0.1, -6, 1.1), key(0.6, fist, 0, 0.12, 0, 1.0),
                    key(0.9, fist, 0, -0.1, -6, 1.1), key(1.2, fist, 0, 0.12, 0, 1.0)}});

    // NO: index and middle fingers snap onto the thumb twice (finger animation).
    lib.push_back({"NO", "index and middle fingers snap shut onto the thumb twice",
                   {key(0.0, noOpen(), 0, 0.1), key(0.3, noClosed(), 0, 0.1), key(0.55, noOpen(), 0, 0.1),
                    key(0.85, noClosed(), 0, 0.1), key(1.1, noOpen(), 0, 0.1)}});

    // THANK YOU: the flat hand starts at the chin and moves forwards and
    // down, towards the person being thanked (so it also grows).
    lib.push_back({"THANK YOU", "flat hand moves from the chin forwards and down",
                   {key(0.0, flat, -0.1, 0.06, 8, 0.92), key(0.35, flat, -0.1, 0.06, 8, 0.92),
                    key(1.0, flat, 0.12, -0.1, -8, 1.04), key(1.5, flat, 0.25, -0.25, -16, 1.12),
                    key(1.9, flat, 0.25, -0.25, -16, 1.12)}});

    // Fingerspelling: B-A-B-Y, holding each letter and blending between them.
    // The hand drifts slightly to the right, as it does when spelling.
    const char* word[] = {"B", "A", "B", "Y"};
    SignAnimation spell{"B-A-B-Y", "fingerspelling the word BABY, letter by letter", {}};
    double t = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double x = -0.15 + 0.1 * i;
        spell.keys.push_back(key(t, signByName(word[i]), x, 0.05));
        spell.keys.push_back(key(t + 0.55, signByName(word[i]), x, 0.05));
        t += 0.85;  // 0.55 s hold + 0.3 s blend to the next letter
    }
    lib.push_back(spell);
    return lib;
}

}  // namespace

const std::vector<SignAnimation>& animationLibrary() {
    static const std::vector<SignAnimation> lib = buildLibrary();
    return lib;
}
