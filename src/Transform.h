#pragma once
// Transform: 2D transformations as 3x3 matrices in homogeneous coordinates.
//
// A point (x, y) is written as the column (x, y, 1). Every transformation is
// a 3x3 matrix M, and the transformed point is p' = M p:
//
//      | x' |   | a  b  c | | x |        x' = a x + b y + c
//      | y' | = | d  e  f | | y |        y' = d x + e y + f
//      | 1  |   | 0  0  1 | | 1 |
//
// The extra "1" is what lets translation (c, f) be a matrix too, so that ANY
// sequence of translations, rotations, scalings, reflections and shears
// collapses into ONE matrix by multiplication.
//
// Order: A * B means "apply B first, then A" (matrices act right to left).
//
// Used in the project:
//   - every joint of every finger is placed by a MatrixStack (Hand.cpp)
//   - forearm -> wrist -> palm -> fingers is a hierarchy of matrices
//   - the whole hand is moved, rotated, scaled and mirrored (Placement)
//   - the screen view and the detail view are matrices (window-to-viewport)
//   - Build mode uses the INVERSE matrix to turn a mouse position into a
//     joint angle

#include <vector>

#include "Geometry.h"

struct Mat3 {
    // m[row][column]; the bottom row of an affine matrix is always 0 0 1.
    double m[3][3];

    static Mat3 identity();

    Vec2 apply(Vec2 p) const;        // a point (w = 1): translation applies
    Vec2 applyVector(Vec2 v) const;  // a direction (w = 0): translation ignored
    Mat3 operator*(const Mat3& o) const;

    double determinant() const;  // < 0 means the matrix mirrors (reflection)
    Mat3 inverse() const;        // undoes the transformation
    bool isAxisAligned(double eps = 1e-9) const;  // no rotation or shear
    double uniformScale() const;                  // sqrt(|det|): how much lengths grow
};

namespace xf {

Mat3 translate(double tx, double ty);
Mat3 rotate(double degrees);                   // anticlockwise about the origin
Mat3 rotateAbout(Vec2 pivot, double degrees);  // T(pivot) R T(-pivot)
Mat3 scale(double sx, double sy);              // about the origin
Mat3 scaleAbout(Vec2 fixedPoint, double sx, double sy);
Mat3 reflectX();                     // mirror in the x-axis:  (x, y) -> (x, -y)
Mat3 reflectY();                     // mirror in the y-axis:  (x, y) -> (-x, y)
Mat3 shear(double shx, double shy);  // x' = x + shx y,  y' = y + shy x

std::vector<Vec2> apply(const Mat3& m, const std::vector<Vec2>& pts);

}  // namespace xf

// MatrixStack: the standard tool for hierarchical models.
// top() is the current transformation. Each call multiplies onto the RIGHT
// of top(), so it affects everything drawn after it, in that object's own
// local coordinates. push() saves the current matrix and pop() returns to it,
// so a child (a finger) can be transformed without affecting its siblings.
class MatrixStack {
public:
    MatrixStack();

    void push();
    void pop();
    const Mat3& top() const { return stack_.back(); }
    size_t depth() const { return stack_.size(); }

    void load(const Mat3& m);
    void multiply(const Mat3& m);  // top = top * m
    void translate(double tx, double ty);
    void rotate(double degrees);
    void scale(double sx, double sy);

private:
    std::vector<Mat3> stack_;
};
