#pragma once
// Animation: keyframe animation of the hand (Watch mode).
//
// A sign that moves (HELLO, GOODBYE, ...) is a list of KEYFRAMES. Each keyframe
// says, at one moment in time, what the hand looks like:
//   - its pose      : every joint angle (the handshape)
//   - its placement : where the hand is, its wrist rotation and its scale
// Between two keyframes the computer fills in the frames ("in-betweening"):
//
//   joint angles, wrist angle, scale
//       linear interpolation  a + (b - a) * e
//       with eased time       e = ease(u),  u = (t - t0) / (t1 - t0)
//   wrist position
//       Catmull-Rom spline through all the keyframe positions, so the hand
//       travels along a smooth curve instead of a zig-zag (a motion path)
//
// Playback is time-based: the frame shown depends on the elapsed time
// multiplied by a speed factor, not on how fast the computer draws.

#include <string>
#include <vector>

#include "Hand.h"

struct Keyframe {
    double time;  // seconds from the start
    HandPose pose;
    Placement placement;
};

struct SignAnimation {
    std::string name;
    std::string description;
    std::vector<Keyframe> keys;  // sorted by time; at least one

    double duration() const { return keys.empty() ? 0.0 : keys.back().time; }
};

// The pose and placement at time t (clamped to the animation's length).
struct AnimationFrame {
    HandPose pose;
    Placement placement;
};
AnimationFrame sampleAnimation(const SignAnimation& anim, double t);

// The wrist's motion path: the Catmull-Rom curve through the keyframe
// positions (hand units), and the keyframe positions themselves.
std::vector<Vec2> motionPath(const SignAnimation& anim, int segmentsPerSpan = 16);
std::vector<Vec2> keyframePositions(const SignAnimation& anim);

// The animated signs: HELLO, GOODBYE, YES, NO, THANK YOU, and the
// fingerspelled word B-A-B-Y.
const std::vector<SignAnimation>& animationLibrary();

// Time-based playback with a speed control.
class AnimationPlayer {
public:
    void start(const SignAnimation* anim);
    void stop() { anim_ = nullptr; }
    bool playing() const { return anim_ != nullptr; }
    const SignAnimation* animation() const { return anim_; }

    // Moves time forward by 'seconds' of real time (times the speed).
    // At the end the animation starts again after a short pause.
    void advance(double seconds);

    void setSpeed(double speed);
    double speed() const { return speed_; }
    double time() const { return time_; }
    AnimationFrame frame() const;

private:
    const SignAnimation* anim_ = nullptr;
    double time_ = 0.0;
    double speed_ = 1.0;
};
