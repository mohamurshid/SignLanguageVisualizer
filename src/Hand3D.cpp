#include "Hand3D.h"

#include <cmath>
#include <utility>

Mat4 Placement3D::forearmMatrix() const {
    Mat4 m = xf3::translate(position) * xf3::scale(scale);
    if (left) m = m * xf3::reflectX();
    return m * xf3::rotateZ(turn) * xf3::rotateX(tip) * xf3::rotateY(roll);
}

Mat4 Placement3D::handMatrix() const {
    return forearmMatrix() * xf3::rotateZ(wristBend) * xf3::rotateX(wristFlex);
}

Placement3D lerpPlacement(const Placement3D& a, const Placement3D& b, double t) {
    Placement3D r = b;
    r.position = lerp(a.position, b.position, t);
    r.turn = a.turn + (b.turn - a.turn) * t;
    r.tip = a.tip + (b.tip - a.tip) * t;
    r.roll = a.roll + (b.roll - a.roll) * t;
    r.wristBend = a.wristBend + (b.wristBend - a.wristBend) * t;
    r.wristFlex = a.wristFlex + (b.wristFlex - a.wristFlex) * t;
    r.scale = a.scale + (b.scale - a.scale) * t;
    return r;
}

Hand3D::Hand3D() { set(HandPose{}, Placement3D{}); }

void Hand3D::set(const HandPose& pose, const Placement3D& placement) {
    pose_ = pose;
    placement_ = placement;
    handMatrix_ = placement_.handMatrix();
    solve();
}

// Forward kinematics with the 4x4 matrix stack. The structure is the same as
// Hand::solveForwardKinematics in 2D: push, go to the finger's base, turn to
// its rest direction, then rotate and translate bone by bone, pop.
void Hand3D::solve() {
    MatrixStack4 stack;  // hand coordinates
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerSpec& spec = shape_.spec(f);
        const FingerPose& fp = pose_.fingers[f];
        FingerChain3D& chain = chains_[f];

        stack.push();
        // The thumb sits slightly in front of the palm plane (on the fleshy pad).
        stack.translate(spec.base.x, spec.base.y, f == Thumb ? 0.06 : 0.0);
        stack.rotateZ(spec.restDirection);
        if (f == Thumb) stack.rotateX(kThumbRoll);
        chain.point[0] = stack.top().origin();

        for (int b = 0; b < kBones; ++b) {
            chain.jointFrame[b] = stack.top();
            stack.rotateZ(fp.bend[b]);
            stack.rotateY(-fp.flex[b]);
            chain.boneFrame[b] = stack.top();
            stack.translate(spec.length[b], 0.0, 0.0);
            chain.point[b + 1] = stack.top().origin();
        }
        stack.pop();
    }
}

Vec3 Hand3D::site(HandSite s) const {
    const double front = 0.5 * kPalmThickness;
    switch (s) {
        case HandSite::IndexTip: return chains_[Index].point[3];
        case HandSite::MiddleTip: return chains_[Middle].point[3];
        case HandSite::ThumbTip: return chains_[Thumb].point[3];
        case HandSite::PinkyTip: return chains_[Pinky].point[3];
        case HandSite::PalmFront: return {0.0, 0.55, front};
        case HandSite::PalmBack: return {0.0, 0.55, -front};
        case HandSite::FingersFront: {
            // Front surface of the middle finger, half way along it.
            const FingerChain3D& c = chains_[Middle];
            const Vec3 mid = lerp(c.point[1], c.point[2], 0.5);
            return mid + c.boneFrame[1].axis(2) * radius(Middle, 1);
        }
        case HandSite::Knuckles: return chains_[Middle].point[1];
        case HandSite::Wrist: return {0.0, 0.0, 0.0};
    }
    return {0.0, 0.0, 0.0};
}

Placement3D placeSiteAt(const HandPose& pose, Placement3D placement, HandSite site, Vec3 target) {
    placement.position = {0.0, 0.0, 0.0};
    Hand3D hand;
    hand.set(pose, placement);
    const Vec3 offset = hand.worldSite(site);  // where the site is with the wrist at the origin
    placement.position = target - offset;
    return placement;
}

HandPose touchThumbTo(HandPose pose, int finger, double reach) {
    Hand3D hand;
    const double contact = 0.5 * (hand.shape().spec(Thumb).width[3] + hand.shape().spec(finger).width[3]);
    // Error = how far the tips are from just touching, plus a very small pull
    // towards the starting angles so that, of all the thumbs that touch, the
    // most natural one (closest to what was asked for) wins.
    const FingerPose start = pose.fingers[Thumb];
    const FingerPose fingerStart = pose.fingers[finger];
    auto error = [&](const HandPose& p) {
        hand.set(p, Placement3D{});
        const Vec3 a = hand.chain(Thumb).point[3];
        const Vec3 b = hand.chain(finger).point[3];
        const double gap = length(a - b) - contact * reach;
        double reg = 0.0;
        for (int i = 0; i < kBones; ++i) {
            reg += std::pow(p.fingers[Thumb].bend[i] - start.bend[i], 2) +
                   std::pow(p.fingers[Thumb].flex[i] - start.flex[i], 2) +
                   4.0 * std::pow(p.fingers[finger].flex[i] - fingerStart.flex[i], 2);  // the finger moves less
        }
        return gap * gap + 2e-8 * reg;
    };
    // Coordinate descent: nudge one angle at a time, keep any nudge that
    // lowers the error, halve the step when nothing helps. Started from a
    // few different thumb positions, keeping the best result.
    auto descend = [&](HandPose p) {
        double best = error(p);
        for (double step = 24.0; step > 0.05; step *= 0.5) {
            bool improved = true;
            for (int pass = 0; pass < 80 && improved; ++pass) {
                improved = false;
                for (int k = 0; k < 3 * kBones; ++k) {
                    for (double dir : {1.0, -1.0}) {
                        HandPose trial = p;
                        // Variables: the thumb's 3 bends and 3 flexes, and the
                        // other finger's 3 flexes (the two tips meet halfway).
                        double& angle = k < kBones       ? trial.fingers[Thumb].bend[k]
                                        : k < 2 * kBones ? trial.fingers[Thumb].flex[k - kBones]
                                                         : trial.fingers[finger].flex[k - 2 * kBones];
                        angle += dir * step;
                        if (k >= kBones && (angle < -10.0 || angle > 110.0)) continue;  // keep the joints plausible
                        if (k < kBones && std::fabs(angle) > 90.0) continue;
                        const double e = error(trial);
                        if (e < best) {
                            best = e;
                            p = trial;
                            improved = true;
                        }
                    }
                }
            }
        }
        return std::make_pair(best, p);
    };
    std::pair<double, HandPose> best = descend(pose);
    const double starts[3][2] = {{20.0, 60.0}, {45.0, 45.0}, {0.0, 80.0}};  // (bend, flex) at the base
    for (const auto& s : starts) {
        HandPose p = pose;
        p.fingers[Thumb].bend[0] = s[0];
        p.fingers[Thumb].flex[0] = s[1];
        const auto r = descend(p);
        if (r.first < best.first) best = r;
    }
    return best.second;
}
