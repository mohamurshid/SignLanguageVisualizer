#include "Math3D.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
double toRadians(double degrees) { return degrees * kPi / 180.0; }
}  // namespace

Mat4 Mat4::identity() {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = i == j ? 1.0 : 0.0;
    return r;
}

Vec3 Mat4::apply(Vec3 p) const {
    return {m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]};
}

Vec3 Mat4::applyVector(Vec3 v) const {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
}

Vec4 Mat4::apply4(Vec4 p) const {
    const double in[4] = {p.x, p.y, p.z, p.w};
    double out[4];
    for (int r = 0; r < 4; ++r) {
        out[r] = 0.0;
        for (int c = 0; c < 4; ++c) out[r] += m[r][c] * in[c];
    }
    return {out[0], out[1], out[2], out[3]};
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) s += m[i][k] * o.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

namespace {
// The 3x3 determinant left after deleting row 'row' and column 'col'.
double minor3(const double a[4][4], int row, int col) {
    double s[3][3];
    for (int i = 0, si = 0; i < 4; ++i) {
        if (i == row) continue;
        for (int j = 0, sj = 0; j < 4; ++j) {
            if (j == col) continue;
            s[si][sj++] = a[i][j];
        }
        ++si;
    }
    return s[0][0] * (s[1][1] * s[2][2] - s[1][2] * s[2][1]) - s[0][1] * (s[1][0] * s[2][2] - s[1][2] * s[2][0]) +
           s[0][2] * (s[1][0] * s[2][1] - s[1][1] * s[2][0]);
}
}  // namespace

double Mat4::determinant() const {
    double det = 0.0;
    for (int j = 0; j < 4; ++j) det += ((j % 2 == 0) ? 1.0 : -1.0) * m[0][j] * minor3(m, 0, j);
    return det;
}

// inverse = adjugate / determinant, where the adjugate is the transposed
// matrix of cofactors C(i,j) = (-1)^(i+j) * minor(i,j).
Mat4 Mat4::inverse() const {
    const double det = determinant();
    Mat4 r{};
    if (std::fabs(det) < 1e-15) return identity();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const double cofactor = (((i + j) % 2 == 0) ? 1.0 : -1.0) * minor3(m, i, j);
            r.m[j][i] = cofactor / det;  // transposed
        }
    return r;
}

Mat4 Mat4::transposed() const {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = m[j][i];
    return r;
}

std::array<double, 16> Mat4::toColumnMajor() const {
    std::array<double, 16> a{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) a[c * 4 + r] = m[r][c];
    return a;
}

namespace xf3 {

Mat4 translate(double tx, double ty, double tz) {
    Mat4 r = Mat4::identity();
    r.m[0][3] = tx;
    r.m[1][3] = ty;
    r.m[2][3] = tz;
    return r;
}

Mat4 rotateX(double degrees) {
    const double c = std::cos(toRadians(degrees)), s = std::sin(toRadians(degrees));
    Mat4 r = Mat4::identity();
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    return r;
}

Mat4 rotateY(double degrees) {
    const double c = std::cos(toRadians(degrees)), s = std::sin(toRadians(degrees));
    Mat4 r = Mat4::identity();
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    return r;
}

Mat4 rotateZ(double degrees) {
    const double c = std::cos(toRadians(degrees)), s = std::sin(toRadians(degrees));
    Mat4 r = Mat4::identity();
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    return r;
}

// Rodrigues: R = cos(a) I + sin(a) [k]x + (1 - cos(a)) k k^T, k = unit axis.
Mat4 rotateAxis(Vec3 axis, double degrees) {
    const Vec3 k = normalize(axis);
    const double c = std::cos(toRadians(degrees)), s = std::sin(toRadians(degrees)), t = 1.0 - c;
    Mat4 r = Mat4::identity();
    r.m[0][0] = c + t * k.x * k.x;
    r.m[0][1] = t * k.x * k.y - s * k.z;
    r.m[0][2] = t * k.x * k.z + s * k.y;
    r.m[1][0] = t * k.x * k.y + s * k.z;
    r.m[1][1] = c + t * k.y * k.y;
    r.m[1][2] = t * k.y * k.z - s * k.x;
    r.m[2][0] = t * k.x * k.z - s * k.y;
    r.m[2][1] = t * k.y * k.z + s * k.x;
    r.m[2][2] = c + t * k.z * k.z;
    return r;
}

Mat4 scale(double sx, double sy, double sz) {
    Mat4 r = Mat4::identity();
    r.m[0][0] = sx;
    r.m[1][1] = sy;
    r.m[2][2] = sz;
    return r;
}

Mat4 reflectX() { return scale(-1.0, 1.0, 1.0); }

// The camera sits at 'eye' looking at 'target'. The view matrix moves the
// world so the camera is at the origin looking down -z, with 'up' as +y:
// rows are the camera's right (s), up (u) and backward (-f) axes.
Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x;
    r.m[0][1] = s.y;
    r.m[0][2] = s.z;
    r.m[1][0] = u.x;
    r.m[1][1] = u.y;
    r.m[1][2] = u.z;
    r.m[2][0] = -f.x;
    r.m[2][1] = -f.y;
    r.m[2][2] = -f.z;
    r.m[0][3] = -dot(s, eye);
    r.m[1][3] = -dot(u, eye);
    r.m[2][3] = dot(f, eye);
    return r;
}

// Perspective projection: points further away are divided by a larger w,
// so they appear smaller. Depth between zNear and zFar maps to -1..1.
Mat4 perspective(double fovYDegrees, double aspect, double zNear, double zFar) {
    const double f = 1.0 / std::tan(toRadians(fovYDegrees) / 2.0);
    Mat4 r{};
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zFar + zNear) / (zNear - zFar);
    r.m[2][3] = 2.0 * zFar * zNear / (zNear - zFar);
    r.m[3][2] = -1.0;
    return r;
}

Mat4 orthographic(double left, double right, double bottom, double top, double zNear, double zFar) {
    Mat4 r = Mat4::identity();
    r.m[0][0] = 2.0 / (right - left);
    r.m[1][1] = 2.0 / (top - bottom);
    r.m[2][2] = -2.0 / (zFar - zNear);
    r.m[0][3] = -(right + left) / (right - left);
    r.m[1][3] = -(top + bottom) / (top - bottom);
    r.m[2][3] = -(zFar + zNear) / (zFar - zNear);
    return r;
}

bool project(const Mat4& projView, Vec3 p, int viewportW, int viewportH, double& px, double& py, double& depth) {
    const Vec4 clip = projView.apply4({p.x, p.y, p.z, 1.0});
    if (clip.w <= 1e-9) return false;
    const double nx = clip.x / clip.w, ny = clip.y / clip.w;  // normalised device coordinates -1..1
    px = (nx + 1.0) * 0.5 * viewportW;
    py = (1.0 - ny) * 0.5 * viewportH;  // window y grows downwards
    depth = clip.z / clip.w;
    return true;
}

}  // namespace xf3

MatrixStack4::MatrixStack4() : stack_{Mat4::identity()} {}

void MatrixStack4::push() { stack_.push_back(stack_.back()); }
void MatrixStack4::pop() {
    if (stack_.size() > 1) stack_.pop_back();
}
void MatrixStack4::load(const Mat4& m) { stack_.back() = m; }
void MatrixStack4::multiply(const Mat4& m) { stack_.back() = stack_.back() * m; }
void MatrixStack4::translate(double tx, double ty, double tz) { multiply(xf3::translate(tx, ty, tz)); }
void MatrixStack4::rotateX(double degrees) { multiply(xf3::rotateX(degrees)); }
void MatrixStack4::rotateY(double degrees) { multiply(xf3::rotateY(degrees)); }
void MatrixStack4::rotateZ(double degrees) { multiply(xf3::rotateZ(degrees)); }
void MatrixStack4::scale(double sx, double sy, double sz) { multiply(xf3::scale(sx, sy, sz)); }
