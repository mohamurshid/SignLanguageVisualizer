#include "Transform.h"

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
double toRadians(double degrees) { return degrees * kPi / 180.0; }
}  // namespace

// ===========================================================================
// Mat3
// ===========================================================================
Mat3 Mat3::identity() { return {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}; }

Vec2 Mat3::apply(Vec2 p) const {
    return {m[0][0] * p.x + m[0][1] * p.y + m[0][2], m[1][0] * p.x + m[1][1] * p.y + m[1][2]};
}

Vec2 Mat3::applyVector(Vec2 v) const { return {m[0][0] * v.x + m[0][1] * v.y, m[1][0] * v.x + m[1][1] * v.y}; }

// (A * B)[r][c] = sum over k of A[r][k] * B[k][c]
Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) sum += m[row][k] * o.m[k][col];
            r.m[row][col] = sum;
        }
    }
    return r;
}

double Mat3::determinant() const {
    // For an affine matrix (bottom row 0 0 1) this is a e - b d.
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// Inverse of an affine matrix  | A t |   is   | A^-1   -A^-1 t |
//                              | 0 1 |        | 0       1      |
// where A is the 2x2 part:  A^-1 = 1/(a e - b d) * |  e -b |
//                                                  | -d  a |
Mat3 Mat3::inverse() const {
    const double a = m[0][0], b = m[0][1], c = m[0][2];
    const double d = m[1][0], e = m[1][1], f = m[1][2];
    const double det = a * e - b * d;
    if (std::fabs(det) < 1e-12) return identity();  // not invertible (squashed flat)
    const double ia = e / det, ib = -b / det, id = -d / det, ie = a / det;
    return {{{ia, ib, -(ia * c + ib * f)}, {id, ie, -(id * c + ie * f)}, {0, 0, 1}}};
}

bool Mat3::isAxisAligned(double eps) const { return std::fabs(m[0][1]) < eps && std::fabs(m[1][0]) < eps; }

double Mat3::uniformScale() const {
    const double det = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    return std::sqrt(std::fabs(det));
}

// ===========================================================================
// The basic transformations
// ===========================================================================
namespace xf {

//  | 1 0 tx |
//  | 0 1 ty |
//  | 0 0 1  |
Mat3 translate(double tx, double ty) { return {{{1, 0, tx}, {0, 1, ty}, {0, 0, 1}}}; }

//  | cos -sin 0 |
//  | sin  cos 0 |    anticlockwise by theta, about the origin
//  |  0    0  1 |
Mat3 rotate(double degrees) {
    const double c = std::cos(toRadians(degrees));
    const double s = std::sin(toRadians(degrees));
    return {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
}

// Rotation about any point: move the pivot to the origin, rotate, move back.
// (Read right to left: T(-pivot) is applied first.)
Mat3 rotateAbout(Vec2 pivot, double degrees) {
    return translate(pivot.x, pivot.y) * rotate(degrees) * translate(-pivot.x, -pivot.y);
}

//  | sx 0  0 |
//  | 0  sy 0 |
//  | 0  0  1 |
Mat3 scale(double sx, double sy) { return {{{sx, 0, 0}, {0, sy, 0}, {0, 0, 1}}}; }

Mat3 scaleAbout(Vec2 p, double sx, double sy) { return translate(p.x, p.y) * scale(sx, sy) * translate(-p.x, -p.y); }

Mat3 reflectX() { return scale(1, -1); }
Mat3 reflectY() { return scale(-1, 1); }

//  | 1   shx 0 |
//  | shy 1   0 |
//  | 0   0   1 |
Mat3 shear(double shx, double shy) { return {{{1, shx, 0}, {shy, 1, 0}, {0, 0, 1}}}; }

std::vector<Vec2> apply(const Mat3& m, const std::vector<Vec2>& pts) {
    std::vector<Vec2> out;
    out.reserve(pts.size());
    for (const Vec2& p : pts) out.push_back(m.apply(p));
    return out;
}

}  // namespace xf

// ===========================================================================
// MatrixStack
// ===========================================================================
MatrixStack::MatrixStack() : stack_{Mat3::identity()} {}

void MatrixStack::push() { stack_.push_back(stack_.back()); }

void MatrixStack::pop() {
    if (stack_.size() > 1) stack_.pop_back();  // never pop the last matrix
}

void MatrixStack::load(const Mat3& m) { stack_.back() = m; }
void MatrixStack::multiply(const Mat3& m) { stack_.back() = stack_.back() * m; }
void MatrixStack::translate(double tx, double ty) { multiply(xf::translate(tx, ty)); }
void MatrixStack::rotate(double degrees) { multiply(xf::rotate(degrees)); }
void MatrixStack::scale(double sx, double sy) { multiply(xf::scale(sx, sy)); }
