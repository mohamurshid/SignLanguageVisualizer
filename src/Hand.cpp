#include "Hand.h"

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
double toRadians(double degrees) { return degrees * kPi / 180.0; }
}  // namespace

HandPose lerpPose(const HandPose& a, const HandPose& b, double t) {
    HandPose out = b;  // keep the target's name and description
    for (int f = 0; f < kFingerCount; ++f) {
        for (int i = 0; i < kBones; ++i) {
            out.fingers[f].bend[i] = a.fingers[f].bend[i] + (b.fingers[f].bend[i] - a.fingers[f].bend[i]) * t;
            out.fingers[f].flex[i] = a.fingers[f].flex[i] + (b.fingers[f].flex[i] - a.fingers[f].flex[i]) * t;
        }
    }
    return out;
}

Hand::Hand() {
    // Right hand, palm facing the viewer, so the thumb is on the RIGHT of the
    // screen and the pinky on the left. The wrist is at (0, 0).
    //            name      point names                      base            dir   lengths              widths
    specs_[Thumb]  = {"Thumb",  {"CMC", "MCP", "IP", "tip"},  {0.30, 0.22}, 50.0, {0.35, 0.27, 0.22}, {0.26, 0.22, 0.20, 0.17}};
    specs_[Index]  = {"Index",  {"MCP", "PIP", "DIP", "tip"}, {0.33, 0.98}, 90.0, {0.46, 0.27, 0.21}, {0.20, 0.18, 0.17, 0.15}};
    specs_[Middle] = {"Middle", {"MCP", "PIP", "DIP", "tip"}, {0.11, 1.04}, 90.0, {0.50, 0.31, 0.23}, {0.21, 0.19, 0.18, 0.16}};
    specs_[Ring]   = {"Ring",   {"MCP", "PIP", "DIP", "tip"}, {-0.11, 1.00}, 90.0, {0.46, 0.29, 0.22}, {0.20, 0.18, 0.17, 0.15}};
    specs_[Pinky]  = {"Pinky",  {"MCP", "PIP", "DIP", "tip"}, {-0.32, 0.90}, 90.0, {0.36, 0.22, 0.19}, {0.17, 0.15, 0.14, 0.13}};

    // Palm outline, anticlockwise. The dip at (0.40, 0.52) is the web of the
    // thumb, which makes the polygon CONCAVE - a good test for scan-line fill.
    palm_ = {
        {-0.30, -0.02}, {0.30, -0.02}, {0.42, 0.18}, {0.46, 0.38}, {0.40, 0.52},
        {0.44, 0.78},   {0.43, 0.98},  {0.22, 1.08}, {0.00, 1.10}, {-0.22, 1.06},
        {-0.41, 0.95},  {-0.45, 0.70}, {-0.42, 0.35}, {-0.34, 0.10},
    };

    // A short forearm. It runs off the bottom of the screen on purpose: the
    // framebuffer clips the pixels that fall outside it.
    forearm_ = {{-0.31, 0.02}, {0.31, 0.02}, {0.37, -0.90}, {-0.37, -0.90}};

    // Palm creases as cubic Bezier curves: start point, two control points
    // that pull the curve, end point.
    creases_ = {
        {{-0.44, 0.80}, {-0.20, 0.77}, {0.02, 0.80}, {0.22, 0.93}},  // heart line
        {{0.40, 0.66}, {0.18, 0.62}, {-0.08, 0.60}, {-0.32, 0.51}},  // head line
        {{0.39, 0.64}, {0.10, 0.56}, {0.02, 0.28}, {0.12, 0.05}},    // life line
    };

    // Fleshy pads of the palm as ellipses: base of the thumb (thenar) and
    // base of the little finger (hypothenar).
    pads_ = {
        {{0.20, 0.30}, 0.15, 0.21},
        {{-0.28, 0.42}, 0.12, 0.27},
    };

    setPose(HandPose{});
}

void Hand::setPose(const HandPose& pose) {
    pose_ = pose;
    solveForwardKinematics();
}

// Forward kinematics with a matrix stack.
// For each finger we push a copy of the hand frame, move to the finger's base
// and turn to its rest direction. Then for every bone:
//   1. remember the joint frame (used by Build mode to pick angles),
//   2. rotate by the joint's bend      -> the bone now lies along local x,
//   3. translate along local x by the bone's on-screen length
//      (length * cos(total flex): the orthographic projection), which moves
//      the frame to the next joint.
// Because every step is multiplied onto the frames before it, bending the
// knuckle carries the middle joint, end joint and fingertip with it: that
// is the hierarchy. pop() returns to the hand frame for the next finger.
void Hand::solveForwardKinematics() {
    MatrixStack stack;  // starts as the identity: hand coordinates
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerSpec& spec = specs_[f];
        const FingerPose& fp = pose_.fingers[f];
        FingerChain& chain = chains_[f];

        stack.push();
        stack.translate(spec.base.x, spec.base.y);
        stack.rotate(spec.restDirection);
        chain.point[0] = stack.top().apply({0.0, 0.0});

        double flex = 0.0;  // towards-viewer angle so far
        for (int b = 0; b < kBones; ++b) {
            chain.jointFrame[b] = stack.top();
            stack.rotate(fp.bend[b]);
            chain.boneFrame[b] = stack.top();
            flex += fp.flex[b];
            const double projected = spec.length[b] * std::cos(toRadians(flex));
            stack.translate(projected, 0.0);
            chain.point[b + 1] = stack.top().apply({0.0, 0.0});
            chain.totalFlex[b] = flex;
        }
        stack.pop();
    }
}

Mat3 Placement::forearmMatrix() const {
    Mat3 m = xf::translate(position.x, position.y) * xf::scale(scale, scale);
    if (mirror) m = m * xf::reflectY();
    return m;
}

Mat3 Placement::handMatrix() const { return forearmMatrix() * xf::rotate(wristAngle); }
