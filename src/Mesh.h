#pragma once
// Mesh: triangle meshes for the 3D hand and body (Checkpoint 3).
//
// A mesh is a list of vertices (position + normal) and a list of triangles
// (three vertex indices each, anticlockwise when seen from outside). OpenGL
// draws it in one call with vertex arrays (glDrawElements).
//
// Normals decide the lighting: a smooth surface shares one averaged normal
// per vertex (Gouraud shading looks round), a flat face uses the face normal.
//
// The shapes we build ourselves:
//   sphere       latitude/longitude grid          joints, fingertips, head
//   frustum      tapered tube along +x            finger bones
//   profileTube  elliptical rings along +y        forearm, neck, torso, arms
//   slab         a 2D outline extruded with a     the palm (its outline is the
//                rounded edge                     CP1 Catmull-Rom palm curve)
// Flat caps are triangulated with EAR CLIPPING, which handles concave
// outlines such as the palm (the web of the thumb dips inwards).

#include <vector>

#include "Geometry.h"
#include "Math3D.h"

struct Mesh {
    std::vector<float> positions;   // x, y, z per vertex
    std::vector<float> normals;     // x, y, z per vertex (unit length)
    std::vector<unsigned> indices;  // 3 per triangle

    size_t vertexCount() const { return positions.size() / 3; }
    size_t triangleCount() const { return indices.size() / 3; }

    unsigned addVertex(Vec3 p, Vec3 n);
    void addTriangle(unsigned a, unsigned b, unsigned c);
    void append(const Mesh& other, const Mat4& transform);  // copies another mesh, transformed

    Vec3 position(size_t i) const { return {positions[3 * i], positions[3 * i + 1], positions[3 * i + 2]}; }
    Vec3 normal(size_t i) const { return {normals[3 * i], normals[3 * i + 1], normals[3 * i + 2]}; }

    // Recomputes every normal as the area-weighted average of the face
    // normals around the vertex (smooth shading).
    void computeSmoothNormals();
};

// One ring of a profile tube: at height y, an ellipse with radii rx and rz,
// centred at (cx, cz).
struct ProfileRing {
    double y, rx, rz;
    double cx = 0.0, cz = 0.0;
};

namespace meshes {

Mesh sphere(int slices = 24, int stacks = 16);  // unit sphere at the origin
Mesh frustum(double r0, double r1, double length, int slices = 20);  // along +x, open ends
Mesh profileTube(const std::vector<ProfileRing>& rings, int slices = 28, bool capBottom = true, bool capTop = true);
Mesh slab(const std::vector<Vec2>& outline, double thickness, double bevel, int bevelSteps = 4);

// Ear clipping: splits a simple polygon (anticlockwise) into triangles.
// Returns the vertex indices, three per triangle.
std::vector<int> triangulate(const std::vector<Vec2>& polygon);

// Moves every vertex of an anticlockwise polygon inwards by 'distance'
// along the bisector of its two edges.
std::vector<Vec2> insetPolygon(const std::vector<Vec2>& polygon, double distance);

}  // namespace meshes
