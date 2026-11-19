#include "HandMesh.h"

#include <algorithm>
#include <cmath>

#include "Body.h"
#include "Curves.h"

namespace {

HandMesh build() {
    HandMesh hm;
    const Hand shape;

    // Palm: the CP1 Catmull-Rom palm outline, sampled and extruded with a
    // rounded edge. The outline is concave at the web of the thumb, so the
    // front and back faces need ear clipping.
    const std::vector<Vec2> outline = curves::sampleCatmullRom(shape.palmOutline(), 6, true);
    std::vector<Vec2> ring(outline.begin(), outline.end());
    if (ring.size() > 1 && std::fabs(ring.front().x - ring.back().x) < 1e-9 &&
        std::fabs(ring.front().y - ring.back().y) < 1e-9)
        ring.pop_back();  // the sampled closed curve repeats its first point
    hm.palm = meshes::slab(ring, Hand3D::kPalmThickness, 0.075, 4);

    // Wrist: a flattened ball that hides the joint between palm and forearm.
    hm.wrist.append(meshes::sphere(28, 16), xf3::translate(0.0, 0.02, 0.0) * xf3::scale(0.31, 0.16, 0.13));

    // Forearm: elliptical rings from the wrist down to the elbow, slightly
    // wider and thicker towards the elbow.
    hm.forearm = meshes::profileTube({{0.04, 0.29, 0.125},
                                      {-0.35, 0.32, 0.145},
                                      {-1.3, 0.37, 0.19},
                                      {-2.1, 0.40, 0.22},
                                      {-kForearmLength, 0.38, 0.22}},
                                     28, false, true);

    hm.unitSphere = meshes::sphere(20, 12);

    // One tapered tube per bone: from the joint's radius to the next one's.
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerSpec& s = shape.spec(f);
        for (int b = 0; b < kBones; ++b) {
            hm.bones[f][b] = meshes::frustum(0.5 * s.width[b], 0.5 * s.width[b + 1], s.length[b], 18);
        }
    }
    return hm;
}

}  // namespace

const HandMesh& HandMesh::get() {
    static const HandMesh mesh = build();
    return mesh;
}

Mat4 HandMesh::boneMatrix(const Hand3D& hand, int finger, int bone) {
    return hand.handMatrix() * hand.chain(finger).boneFrame[bone];
}

Mat4 HandMesh::jointMatrix(const Hand3D& hand, int finger, int point) {
    const double r = hand.radius(finger, point);
    return hand.handMatrix() * xf3::translate(hand.chain(finger).point[point]) * xf3::scale(r);
}

// The nail sits on the back (-z side) of the last bone, near its end.
Mat4 HandMesh::nailMatrix(const Hand3D& hand, int finger) {
    const FingerSpec& s = hand.shape().spec(finger);
    const double len = s.length[2];
    const double r = 0.5 * s.width[3];
    return hand.handMatrix() * hand.chain(finger).boneFrame[2] * xf3::translate(0.66 * len, 0.0, -0.80 * r) *
           xf3::scale(0.36 * len, 0.78 * r, 0.28 * r);
}

Vec3 solveElbow(Vec3 shoulder, Vec3 wrist, bool leftArm) {
    const double l1 = kUpperArmLength, l2 = kForearmLength;
    Vec3 axis = wrist - shoulder;
    double d = length(axis);
    if (d < 1e-6) axis = {0.0, -1.0, 0.0}, d = 1e-6;
    const Vec3 u = axis * (1.0 / d);
    d = std::clamp(d, std::fabs(l1 - l2) + 1e-3, l1 + l2 - 1e-3);  // the arm cannot stretch further
    // Distance from the shoulder to the circle's centre along u, and its radius.
    const double a = (l1 * l1 - l2 * l2 + d * d) / (2.0 * d);
    const double h = std::sqrt(std::max(0.0, l1 * l1 - a * a));
    // Pole: down, out to the side, and a little behind. The higher the hand
    // (signs at the head), the further out the elbow swings, as a real arm does.
    const double raised = std::clamp((wrist.y - shoulder.y + 0.5) / 2.0, 0.0, 1.0);
    const double out = 0.4 + 0.8 * raised;
    const Vec3 pole{leftArm ? out : -out, -1.0, -0.4};
    Vec3 p = pole - u * dot(pole, u);  // the part of the pole perpendicular to the arm
    if (length(p) < 1e-6) p = {0.0, 0.0, -1.0};
    p = normalize(p);
    return shoulder + u * a + p * h;
}

Mat4 forearmMatrix(const Hand3D& hand, Vec3 elbow) {
    const Mat4 hm = hand.handMatrix();
    const Vec3 wrist = hm.origin();
    const Vec3 y = normalize(wrist - elbow);
    Vec3 x = hm.axis(0) - y * dot(hm.axis(0), y);
    if (length(x) < 1e-6) x = cross(y, Vec3{0.0, 0.0, 1.0});
    x = normalize(x);
    const Vec3 z = cross(x, y);
    const double s = hand.placement().scale;
    Mat4 m = Mat4::identity();
    m.m[0][0] = x.x * s; m.m[1][0] = x.y * s; m.m[2][0] = x.z * s;
    m.m[0][1] = y.x * s; m.m[1][1] = y.y * s; m.m[2][1] = y.z * s;
    m.m[0][2] = z.x * s; m.m[1][2] = z.y * s; m.m[2][2] = z.z * s;
    m.m[0][3] = wrist.x; m.m[1][3] = wrist.y; m.m[2][3] = wrist.z;
    return m;
}

Mat4 alignX(Vec3 from, Vec3 to) {
    const Vec3 x = normalize(to - from);
    Vec3 helper = std::fabs(x.y) < 0.9 ? Vec3{0.0, 1.0, 0.0} : Vec3{1.0, 0.0, 0.0};
    const Vec3 z = normalize(cross(x, helper));
    const Vec3 y = cross(z, x);
    Mat4 m = Mat4::identity();
    m.m[0][0] = x.x; m.m[1][0] = x.y; m.m[2][0] = x.z;
    m.m[0][1] = y.x; m.m[1][1] = y.y; m.m[2][1] = y.z;
    m.m[0][2] = z.x; m.m[1][2] = z.y; m.m[2][2] = z.z;
    m.m[0][3] = from.x; m.m[1][3] = from.y; m.m[2][3] = from.z;
    return m;
}
