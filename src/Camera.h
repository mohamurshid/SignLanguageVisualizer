#pragma once
// Camera: an orbit camera around the signer (Checkpoint 3).
//
// The camera sits on a sphere around a target point:
//   eye = target + distance * (sin(yaw) cos(pitch), sin(pitch), cos(yaw) cos(pitch))
// Dragging the mouse changes yaw and pitch; the wheel changes the distance.
// The view matrix is lookAt(eye, target, up) and the projection is a
// perspective projection, both built in Math3D (no GLU needed).
//
// Presets:
//   Front         facing the signer, as a learner sitting opposite would see
//   Three-quarter from the front, turned to the signer's right
//   Side          from the signer's right: shows depth (M, N, T, curled fingers)
//   Signer's view from behind the signer (head and torso hidden): the hands
//                 as you see your own, with the same left and right

#include <string>

#include "Math3D.h"

class OrbitCamera {
public:
    enum Preset { Front = 0, ThreeQuarter, Side, SignersView, PresetCount };

    OrbitCamera() { setPreset(Front); }

    void setPreset(Preset p);
    Preset preset() const { return preset_; }
    static const char* presetName(Preset p);

    void orbit(double dYaw, double dPitch);  // degrees
    // Close-up: aim at a point (the hand) from the current direction.
    void focus(Vec3 target, double distance);
    void setTarget(Vec3 target) { target_ = target; }
    double distance() const { return distance_; }
    void zoom(double factor);                // < 1 moves closer

    Vec3 eye() const;
    Vec3 target() const { return target_; }
    Mat4 view() const;
    Mat4 projection(double aspect) const;
    double fovY() const { return fov_; }

    bool hidesBody() const { return preset_ == SignersView; }  // the head and torso would block the view

private:
    Preset preset_ = Front;
    Vec3 target_{0.0, 1.9, 0.4};
    double yaw_ = 0.0, pitch_ = 6.0, distance_ = 14.0, fov_ = 32.0;
    bool freeEye_ = false;  // the signer's view uses a fixed eye point
    Vec3 eye_{0.0, 0.0, 0.0};
};
