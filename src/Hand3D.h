#pragma once
// Hand3D: the same rig as Hand.h, solved in 3D (Checkpoint 3).
//
// The hand keeps the sizes and the joint angles of the 2D hand (Hand.h), so
// every sign written for Checkpoints 1 and 2 works unchanged. What changes is
// how the angles are used:
//
//   2D (Checkpoints 1-2): flex shortened the bone on screen (length * cos)
//   3D (Checkpoint 3)   : flex really rotates the bone towards the palm side
//
// Hand coordinates (wrist = origin), right hand, palm facing the viewer:
//   x towards the thumb, y along the fingers, z out of the palm.
//
// For every bone, the matrix stack does:
//   rotateZ(bend)    spread the finger inside the palm plane
//   rotateY(-flex)   curl the finger towards the palm (+z)
//   translate(len)   move along the bone to the next joint
// exactly the 2D chain with one more rotation, now with 4x4 matrices.
//
// The hierarchy above the fingers is
//   forearm = T(position) * S(scale) * [mirror] * Rz(turn) * Rx(tip) * Ry(roll)
//   hand    = forearm * Rz(wristBend) * Rx(wristFlex)
// so the whole arm can be placed anywhere in front of the body, the palm can
// face any direction, and the wrist still bends on its own.

#include <array>
#include <string>

#include "Hand.h"
#include "Math3D.h"

// Where the hand is and which way it faces, in body space (hand units).
struct Placement3D {
    Vec3 position{0.0, 0.0, 0.0};  // the wrist
    double turn = 0.0;       // degrees about z: tilts the whole arm sideways in the picture
    double tip = 0.0;        // degrees about x: + tips the fingers towards the viewer
    double roll = 0.0;       // degrees about the arm (y): 0 = palm to the viewer, 180 = palm to the signer
    double wristBend = 0.0;  // degrees about z at the wrist only (the 2D wristAngle)
    double wristFlex = 0.0;  // degrees about x at the wrist only: + bends towards the palm
    double scale = 1.0;
    bool left = false;       // true = mirror image: the signer's LEFT hand

    Mat4 forearmMatrix() const;
    Mat4 handMatrix() const;
};

// Blends two placements (angles and position) with weight t.
Placement3D lerpPlacement(const Placement3D& a, const Placement3D& b, double t);

struct FingerChain3D {
    std::array<Vec3, kPoints> point;      // joints and tip, hand coordinates
    std::array<Mat4, kBones> jointFrame;  // before the joint's own rotations
    std::array<Mat4, kBones> boneFrame;   // bone b runs along this frame's x-axis
};

// Named points on the hand, used to put a hand exactly on the chin, the chest,
// the other palm, ...
enum class HandSite { IndexTip, MiddleTip, ThumbTip, PinkyTip, PalmFront, PalmBack, FingersFront, Knuckles, Wrist };

class Hand3D {
public:
    Hand3D();

    void set(const HandPose& pose, const Placement3D& placement);
    const HandPose& pose() const { return pose_; }
    const Placement3D& placement() const { return placement_; }

    // The 2D hand supplies the fixed shape: bone lengths, widths, the palm outline.
    const Hand& shape() const { return shape_; }

    const FingerChain3D& chain(int finger) const { return chains_[finger]; }
    double radius(int finger, int point) const { return 0.5 * shape_.spec(finger).width[point]; }

    Mat4 forearmMatrix() const { return placement_.forearmMatrix(); }
    Mat4 handMatrix() const { return handMatrix_; }

    Vec3 site(HandSite s) const;  // hand coordinates
    Vec3 worldSite(HandSite s) const { return handMatrix_.apply(site(s)); }

    static constexpr double kPalmThickness = 0.26;  // hand units, front to back
    static constexpr double kThumbRoll = -35.0;     // tilts the thumb's curl towards the palm

private:
    void solve();

    Hand shape_;
    HandPose pose_;
    Placement3D placement_;
    Mat4 handMatrix_ = Mat4::identity();
    std::array<FingerChain3D, kFingerCount> chains_;
};

// Moves a placement so that a site of the hand lands exactly on a target
// point: position = target - (hand matrix without translation) * site.
Placement3D placeSiteAt(const HandPose& pose, Placement3D placement, HandSite site, Vec3 target);

// Finds thumb angles (and small changes to the other finger's curl) that
// bring the thumb tip against the tip of another finger (for O, F, D, 6-9,
// flat-O): a small numeric search that nudges one angle at a time and keeps
// the change if the tips get closer to just touching.
HandPose touchThumbTo(HandPose pose, int finger, double reach = 1.0);
