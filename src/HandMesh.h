#pragma once
// HandMesh: the 3D surfaces of the hand, built once from its fixed sizes.
//
// Every piece is built in its OWN local coordinates and placed each frame by
// the matrices the forward kinematics already computed (Hand3D):
//
//   piece            local coordinates           placed by
//   forearm          tube down the -y axis       forearm matrix
//   wrist            flattened ellipsoid         hand matrix
//   palm             slab in the x-y plane       hand matrix
//   bone (f, b)      tapered tube along +x       hand matrix * boneFrame[b]
//   joint            unit sphere                 hand matrix * T(point) * S(radius)
//   nail             unit sphere                 hand matrix * boneFrame[2] * T * S
//
// So the meshes never change while signing: only the matrices do. That is
// the matrix-stack idea from Checkpoint 2, now feeding OpenGL.

#include <array>

#include "Hand3D.h"
#include "Mesh.h"

struct HandMesh {
    Mesh palm;
    Mesh wrist;
    Mesh forearm;
    Mesh unitSphere;
    std::array<std::array<Mesh, kBones>, kFingerCount> bones;

    // Model matrices (hand units) for the pieces that depend on the pose.
    static Mat4 boneMatrix(const Hand3D& hand, int finger, int bone);
    static Mat4 jointMatrix(const Hand3D& hand, int finger, int point);  // sphere at a joint or tip
    static Mat4 nailMatrix(const Hand3D& hand, int finger);

    // The meshes for this hand's shape (built on first use).
    static const HandMesh& get();
};

// A matrix whose x-axis runs from 'from' to 'to' (used for the upper arm).
Mat4 alignX(Vec3 from, Vec3 to);

// The arm: shoulder (fixed on the body) -> elbow -> wrist (wherever the sign
// puts the hand). The elbow is found with two-bone inverse kinematics: it
// lies on the circle where a sphere of radius upperArm around the shoulder
// meets a sphere of radius forearm around the wrist (law of cosines), at the
// point of that circle nearest a "pole" below, outside and behind the arm.
constexpr double kUpperArmLength = 3.4;
Vec3 solveElbow(Vec3 shoulder, Vec3 wrist, bool leftArm);

// The forearm's model matrix: its y-axis runs from the elbow to the wrist and
// it twists with the hand (its x-axis follows the hand's thumb side).
Mat4 forearmMatrix(const Hand3D& hand, Vec3 elbow);
