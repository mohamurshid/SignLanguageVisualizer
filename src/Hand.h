#pragma once
// Hand: the rig from our Phase 1 design, in 2D.
//
// Hierarchy (the palm is the root):
//   wrist/palm -> finger base joint -> middle joint -> end joint -> fingertip
// Rotating a joint moves everything after it, because each bone starts where
// the previous one ended and adds its angle to the angles before it.
//
// Joint names (anatomically correct):
//   Fingers: MCP (knuckle), PIP (middle joint), DIP (end joint), then the tip
//   Thumb  : CMC (base, near the wrist), MCP, IP, then the tip
// The fingertip is an END POINT, not a joint, so each finger has 3 joints,
// 3 bones and 4 points.
//
// We view the hand from the front with the palm facing the viewer. Each
// joint has two angles:
//   bend : rotation inside the picture (spreading fingers apart)
//   flex : rotation towards the viewer (curling the finger)
// A bone flexed by angle theta towards the viewer appears shorter on the
// screen: projected length = length * cos(theta). This is an orthographic
// projection. Past 90 degrees cos is negative and the bone folds back over
// the palm, which is exactly what a curled finger looks like from the front.
// In Checkpoint 2 this hand-written trigonometry becomes a proper matrix
// stack of transformations.

#include <array>
#include <string>
#include <vector>

#include "Geometry.h"

constexpr int kFingerCount = 5;
constexpr int kBones = 3;   // bones per finger
constexpr int kPoints = 4;  // base joint, 2 more joints, fingertip
enum Finger { Thumb = 0, Index, Middle, Ring, Pinky };

// The angles of one finger, in degrees.
struct FingerPose {
    std::array<double, kBones> bend{};  // +ve = anticlockwise (towards the pinky side)
    std::array<double, kBones> flex{};  // 0 = straight, 90 = pointing at the viewer
};

struct HandPose {
    std::string name;
    std::string description;
    std::array<FingerPose, kFingerCount> fingers{};
};

// Blends two poses: t = 0 gives a, t = 1 gives b (linear interpolation).
HandPose lerpPose(const HandPose& a, const HandPose& b, double t);

// Smoothstep easing t^2 (3 - 2t): starts slowly, speeds up, slows down.
inline double easeInOut(double t) { return t * t * (3.0 - 2.0 * t); }

// The fixed shape of a finger: where it attaches and how long/wide it is.
// All sizes are in "hand units": the palm is roughly 1 unit tall.
struct FingerSpec {
    const char* name;
    std::array<const char*, kPoints> pointNames;
    Vec2 base;                         // base joint position relative to the wrist
    double restDirection;              // degrees, 90 = straight up
    std::array<double, kBones> length;
    std::array<double, kPoints> width; // skin width at each point
};

// The solved result for one finger: where each point ended up.
struct FingerChain {
    std::array<Vec2, kPoints> point;
    std::array<double, kBones> totalFlex;  // accumulated flex of each bone
};

class Hand {
public:
    Hand();

    void setPose(const HandPose& pose);
    const HandPose& pose() const { return pose_; }

    const FingerSpec& spec(int finger) const { return specs_[finger]; }
    const FingerChain& chain(int finger) const { return chains_[finger]; }

    // Control points of the palm outline. The outline drawn on screen is a
    // closed Catmull-Rom spline through these points (see HandRenderer).
    const std::vector<Vec2>& palmOutline() const { return palm_; }
    const std::vector<Vec2>& forearmOutline() const { return forearm_; }
    Vec2 wrist() const { return {0.0, 0.0}; }  // the root of the rig

    // Palm details, in hand units:
    //   creases : cubic Bezier curves (4 control points each)
    //   pads    : the fleshy pads at the base of the thumb and the little
    //             finger, drawn as ellipses
    struct EllipseSpec {
        Vec2 centre;
        double rx, ry;
    };
    const std::vector<std::vector<Vec2>>& palmCreases() const { return creases_; }
    const std::vector<EllipseSpec>& palmPads() const { return pads_; }

private:
    void solveForwardKinematics();

    HandPose pose_;
    std::array<FingerSpec, kFingerCount> specs_;
    std::array<FingerChain, kFingerCount> chains_;
    std::vector<Vec2> palm_;
    std::vector<Vec2> forearm_;
    std::vector<std::vector<Vec2>> creases_;
    std::vector<EllipseSpec> pads_;
};
