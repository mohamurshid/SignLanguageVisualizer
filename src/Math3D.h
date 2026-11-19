#pragma once
// Math3D: 3D vectors and 4x4 matrices in homogeneous coordinates (Checkpoint 3).
//
// This is the 3D version of Transform.h. A point (x, y, z) is written as the
// column (x, y, z, 1) and a direction as (x, y, z, 0). Every transformation is
// a 4x4 matrix M and the transformed point is p' = M p. As in 2D:
//   - A * B means "apply B first, then A"
//   - translation needs the extra coordinate, so ANY chain of translations,
//     rotations and scalings collapses into ONE matrix
//
// Coordinate system (right-handed, the same as OpenGL):
//   x to the right, y up, z towards the viewer.
//
// The same matrices drive the whole OpenGL scene: the hand hierarchy is built
// with Mat4 and a MatrixStack4, and the final matrices are handed to OpenGL
// with glLoadMatrixd (toColumnMajor). The camera's view and projection
// matrices are built here too (lookAt, perspective), so no GLU is needed.

#include <array>
#include <cmath>
#include <vector>

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
inline Vec3 operator*(double k, Vec3 a) { return {a.x * k, a.y * k, a.z * k}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    const double len = length(a);
    return len > 1e-12 ? a * (1.0 / len) : Vec3{0.0, 0.0, 0.0};
}
inline Vec3 lerp(Vec3 a, Vec3 b, double t) { return a + (b - a) * t; }

struct Vec4 {
    double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
};

struct Mat4 {
    double m[4][4];  // m[row][column]

    static Mat4 identity();

    Vec3 apply(Vec3 p) const;        // a point (w = 1): translation applies
    Vec3 applyVector(Vec3 v) const;  // a direction (w = 0): translation ignored
    Vec4 apply4(Vec4 p) const;       // the full product, no divide by w
    Mat4 operator*(const Mat4& o) const;

    double determinant() const;  // < 0 means the matrix mirrors (reflection)
    Mat4 inverse() const;        // general 4x4 inverse (cofactors)
    Mat4 transposed() const;

    Vec3 origin() const { return {m[0][3], m[1][3], m[2][3]}; }  // where (0,0,0) goes
    Vec3 axis(int i) const { return {m[0][i], m[1][i], m[2][i]}; }  // local x, y or z axis

    // OpenGL stores matrices column by column (glLoadMatrixd / glMultMatrixd).
    std::array<double, 16> toColumnMajor() const;
};

namespace xf3 {

Mat4 translate(double tx, double ty, double tz);
inline Mat4 translate(Vec3 t) { return translate(t.x, t.y, t.z); }
Mat4 rotateX(double degrees);  // anticlockwise looking down the +x axis
Mat4 rotateY(double degrees);
Mat4 rotateZ(double degrees);
Mat4 rotateAxis(Vec3 axis, double degrees);  // Rodrigues' formula
Mat4 scale(double sx, double sy, double sz);
inline Mat4 scale(double s) { return scale(s, s, s); }
Mat4 reflectX();  // (x, y, z) -> (-x, y, z): turns a right hand into a left hand

// Camera matrices (the same formulas as gluLookAt and gluPerspective).
Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up);
Mat4 perspective(double fovYDegrees, double aspect, double zNear, double zFar);
Mat4 orthographic(double left, double right, double bottom, double top, double zNear, double zFar);

// Projects a world point to window pixels (origin top-left), like gluProject.
// Returns false when the point is behind the camera.
bool project(const Mat4& projView, Vec3 p, int viewportW, int viewportH, double& px, double& py,
             double& depth);

}  // namespace xf3

// The 3D matrix stack: identical idea to the 2D MatrixStack. Each call
// multiplies onto the RIGHT of top(), push() saves, pop() restores.
class MatrixStack4 {
public:
    MatrixStack4();

    void push();
    void pop();  // never removes the last matrix
    const Mat4& top() const { return stack_.back(); }
    size_t depth() const { return stack_.size(); }

    void load(const Mat4& m);
    void multiply(const Mat4& m);
    void translate(double tx, double ty, double tz);
    void rotateX(double degrees);
    void rotateY(double degrees);
    void rotateZ(double degrees);
    void scale(double sx, double sy, double sz);

private:
    std::vector<Mat4> stack_;
};
