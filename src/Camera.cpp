#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
}  // namespace

void OrbitCamera::setPreset(Preset p) {
    preset_ = p;
    freeEye_ = false;
    fov_ = 32.0;
    target_ = {0.0, 1.9, 0.4};
    distance_ = 14.0;
    switch (p) {
        case Front: yaw_ = 0.0; pitch_ = 6.0; break;
        case ThreeQuarter: yaw_ = -38.0; pitch_ = 10.0; break;
        case Side: yaw_ = -90.0; pitch_ = 4.0; break;
        case SignersView:
            // From just behind the signer, looking forward over the hands: the
            // learner sees the hands as their own, with the same left and
            // right. The head and torso are hidden so they do not block it.
            freeEye_ = true;
            eye_ = {-0.45, 3.6, -8.5};
            target_ = {-0.45, 1.5, 1.6};
            fov_ = 36.0;
            break;
        default: break;
    }
}

const char* OrbitCamera::presetName(Preset p) {
    switch (p) {
        case Front: return "front";
        case ThreeQuarter: return "three-quarter";
        case Side: return "side";
        case SignersView: return "signer's view";
        default: return "";
    }
}

void OrbitCamera::orbit(double dYaw, double dPitch) {
    if (freeEye_) {
        // Leaving the signer's view: start orbiting from the front.
        setPreset(Front);
    }
    yaw_ += dYaw;
    pitch_ = std::clamp(pitch_ + dPitch, -60.0, 75.0);
}

void OrbitCamera::focus(Vec3 target, double distance) {
    if (freeEye_) setPreset(Front);
    target_ = target;
    distance_ = distance;
}

void OrbitCamera::zoom(double factor) {
    if (freeEye_) {
        fov_ = std::clamp(fov_ * factor, 20.0, 80.0);
        return;
    }
    distance_ = std::clamp(distance_ * factor, 2.5, 40.0);
}

Vec3 OrbitCamera::eye() const {
    if (freeEye_) return eye_;
    return target_ + Vec3{std::sin(rad(yaw_)) * std::cos(rad(pitch_)), std::sin(rad(pitch_)),
                          std::cos(rad(yaw_)) * std::cos(rad(pitch_))} *
                         distance_;
}

Mat4 OrbitCamera::view() const { return xf3::lookAt(eye(), target_, {0.0, 1.0, 0.0}); }

Mat4 OrbitCamera::projection(double aspect) const { return xf3::perspective(fov_, aspect, 0.2, 120.0); }
