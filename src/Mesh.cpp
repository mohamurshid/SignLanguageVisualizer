#include "Mesh.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

double cross2(Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

bool insideTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    const double d1 = cross2(a, b, p), d2 = cross2(b, c, p), d3 = cross2(c, a, p);
    return d1 >= -1e-12 && d2 >= -1e-12 && d3 >= -1e-12;
}
}  // namespace

unsigned Mesh::addVertex(Vec3 p, Vec3 n) {
    positions.insert(positions.end(), {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)});
    normals.insert(normals.end(), {static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)});
    return static_cast<unsigned>(vertexCount() - 1);
}

void Mesh::addTriangle(unsigned a, unsigned b, unsigned c) { indices.insert(indices.end(), {a, b, c}); }

void Mesh::append(const Mesh& other, const Mat4& transform) {
    // Normals use the inverse transpose, so they stay perpendicular to the
    // surface even under non-uniform scaling.
    const Mat4 normalMatrix = transform.inverse().transposed();
    const unsigned base = static_cast<unsigned>(vertexCount());
    for (size_t i = 0; i < other.vertexCount(); ++i) {
        addVertex(transform.apply(other.position(i)), normalize(normalMatrix.applyVector(other.normal(i))));
    }
    const bool flip = transform.determinant() < 0.0;  // a mirror reverses the winding
    for (size_t t = 0; t < other.triangleCount(); ++t) {
        const unsigned a = base + other.indices[3 * t], b = base + other.indices[3 * t + 1],
                       c = base + other.indices[3 * t + 2];
        if (flip) addTriangle(a, c, b);
        else addTriangle(a, b, c);
    }
}

void Mesh::computeSmoothNormals() {
    std::vector<Vec3> acc(vertexCount());
    for (size_t t = 0; t < triangleCount(); ++t) {
        const unsigned a = indices[3 * t], b = indices[3 * t + 1], c = indices[3 * t + 2];
        const Vec3 n = cross(position(b) - position(a), position(c) - position(a));  // length = 2 * area
        acc[a] = acc[a] + n;
        acc[b] = acc[b] + n;
        acc[c] = acc[c] + n;
    }
    for (size_t i = 0; i < acc.size(); ++i) {
        const Vec3 n = normalize(acc[i]);
        normals[3 * i] = static_cast<float>(n.x);
        normals[3 * i + 1] = static_cast<float>(n.y);
        normals[3 * i + 2] = static_cast<float>(n.z);
    }
}

namespace meshes {

// A grid of latitude (stacks) and longitude (slices) lines. On a unit sphere
// the normal at a point is the point itself.
Mesh sphere(int slices, int stacks) {
    Mesh m;
    for (int i = 0; i <= stacks; ++i) {
        const double phi = kPi * i / stacks;  // 0 at the north pole
        for (int j = 0; j <= slices; ++j) {
            const double theta = 2.0 * kPi * j / slices;
            const Vec3 p{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            m.addVertex(p, p);
        }
    }
    const int row = slices + 1;
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            const unsigned a = i * row + j, b = a + 1, c = a + row, d = c + 1;
            if (i > 0) m.addTriangle(a, b, c);
            if (i < stacks - 1) m.addTriangle(b, d, c);
        }
    return m;
}

// A cone with its top cut off, from x = 0 (radius r0) to x = length (radius
// r1). The normal leans towards the narrow end by the slope of the side.
Mesh frustum(double r0, double r1, double length, int slices) {
    Mesh m;
    const double slope = (r0 - r1) / length;
    for (int i = 0; i <= 1; ++i) {
        const double x = i * length, r = i == 0 ? r0 : r1;
        for (int j = 0; j <= slices; ++j) {
            const double a = 2.0 * kPi * j / slices;
            const Vec3 dir{0.0, std::cos(a), std::sin(a)};
            m.addVertex({x, r * dir.y, r * dir.z}, normalize({slope, dir.y, dir.z}));
        }
    }
    const int row = slices + 1;
    for (int j = 0; j < slices; ++j) {
        const unsigned a = j, b = j + 1, c = j + row, d = c + 1;
        m.addTriangle(a, b, c);
        m.addTriangle(b, d, c);
    }
    return m;
}

Mesh profileTube(const std::vector<ProfileRing>& rings, int slices, bool capBottom, bool capTop) {
    Mesh m;
    const int row = slices + 1;
    for (const ProfileRing& r : rings) {
        for (int j = 0; j <= slices; ++j) {
            const double a = 2.0 * kPi * j / slices;
            m.addVertex({r.cx + r.rx * std::cos(a), r.y, r.cz + r.rz * std::sin(a)}, {0, 0, 0});
        }
    }
    for (size_t i = 0; i + 1 < rings.size(); ++i)
        for (int j = 0; j < slices; ++j) {
            const unsigned a = static_cast<unsigned>(i * row + j), b = a + 1, c = a + row, d = c + 1;
            if (rings[i + 1].y > rings[i].y) {
                m.addTriangle(a, c, b);
                m.addTriangle(b, c, d);
            } else {
                m.addTriangle(a, b, c);
                m.addTriangle(b, d, c);
            }
        }
    m.computeSmoothNormals();
    // Flat caps with their own vertices, so their normals stay flat.
    auto cap = [&](const ProfileRing& r, bool up) {
        const unsigned centre = m.addVertex({r.cx, r.y, r.cz}, {0.0, up ? 1.0 : -1.0, 0.0});
        const unsigned first = static_cast<unsigned>(m.vertexCount());
        for (int j = 0; j <= slices; ++j) {
            const double a = 2.0 * kPi * j / slices;
            m.addVertex({r.cx + r.rx * std::cos(a), r.y, r.cz + r.rz * std::sin(a)}, {0.0, up ? 1.0 : -1.0, 0.0});
        }
        for (int j = 0; j < slices; ++j) {
            if (up) m.addTriangle(centre, first + j + 1, first + j);
            else m.addTriangle(centre, first + j, first + j + 1);
        }
    };
    const bool increasing = rings.back().y > rings.front().y;
    if (capBottom) cap(rings.front(), !increasing);
    if (capTop) cap(rings.back(), increasing);
    return m;
}

std::vector<Vec2> insetPolygon(const std::vector<Vec2>& poly, double distance) {
    const size_t n = poly.size();
    std::vector<Vec2> out(n);
    for (size_t i = 0; i < n; ++i) {
        const Vec2 prev = poly[(i + n - 1) % n], cur = poly[i], next = poly[(i + 1) % n];
        auto leftNormal = [](Vec2 a, Vec2 b) {
            const double dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
            return Vec2{-dy / len, dx / len};  // inside of an anticlockwise polygon
        };
        const Vec2 n1 = leftNormal(prev, cur), n2 = leftNormal(cur, next);
        Vec2 bis{n1.x + n2.x, n1.y + n2.y};
        const double bl = std::sqrt(bis.x * bis.x + bis.y * bis.y);
        if (bl < 1e-9) {
            out[i] = cur + n1 * distance;
            continue;
        }
        bis = bis * (1.0 / bl);
        const double cosHalf = bis.x * n1.x + bis.y * n1.y;
        out[i] = cur + bis * (distance / std::max(cosHalf, 0.35));  // mitre, limited at sharp corners
    }
    return out;
}

// Ear clipping: a vertex is an "ear" if the triangle it makes with its two
// neighbours turns the right way (convex) and contains no other vertex.
// Cut the ear off, repeat until one triangle is left. O(n^2), fine for a palm.
std::vector<int> triangulate(const std::vector<Vec2>& poly) {
    std::vector<int> idx(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) idx[i] = static_cast<int>(i);
    std::vector<int> tris;
    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool cut = false;
        const size_t n = idx.size();
        for (size_t i = 0; i < n; ++i) {
            const int ia = idx[(i + n - 1) % n], ib = idx[i], ic = idx[(i + 1) % n];
            const Vec2 a = poly[ia], b = poly[ib], c = poly[ic];
            if (cross2(a, b, c) <= 1e-12) continue;  // reflex corner: not an ear
            bool empty = true;
            for (int k : idx) {
                if (k == ia || k == ib || k == ic) continue;
                if (insideTriangle(poly[k], a, b, c)) {
                    empty = false;
                    break;
                }
            }
            if (!empty) continue;
            tris.insert(tris.end(), {ia, ib, ic});
            idx.erase(idx.begin() + static_cast<long>(i));
            cut = true;
            break;
        }
        if (!cut) break;  // degenerate input: stop rather than loop forever
    }
    if (idx.size() == 3) tris.insert(tris.end(), {idx[0], idx[1], idx[2]});
    return tris;
}

// The outline is extruded along z, centred on z = 0. The edge is rounded:
// rings at angles from -90 to +90 degrees, each ring inset by
// bevel * (1 - cos(angle)) and lifted to z = (thickness / 2) * sin(angle).
Mesh slab(const std::vector<Vec2>& outline, double thickness, double bevel, int bevelSteps) {
    Mesh m;
    const int n = static_cast<int>(outline.size());
    const int ringCount = 2 * bevelSteps + 1;
    std::vector<std::vector<Vec2>> rings;
    std::vector<double> zs;
    for (int r = 0; r < ringCount; ++r) {
        const double a = -kPi / 2.0 + kPi * r / (ringCount - 1);
        rings.push_back(insetPolygon(outline, bevel * (1.0 - std::cos(a))));
        zs.push_back(0.5 * thickness * std::sin(a));
    }
    for (int r = 0; r < ringCount; ++r)
        for (int i = 0; i < n; ++i) m.addVertex({rings[r][i].x, rings[r][i].y, zs[r]}, {0, 0, 0});
    for (int r = 0; r + 1 < ringCount; ++r)
        for (int i = 0; i < n; ++i) {
            const unsigned a = r * n + i, b = r * n + (i + 1) % n, c = a + n, d = b + n;
            m.addTriangle(a, b, c);
            m.addTriangle(b, d, c);
        }
    m.computeSmoothNormals();
    // Front and back faces: the innermost rings, triangulated by ear clipping.
    const std::vector<int> tris = triangulate(rings.back());
    for (int side = 0; side < 2; ++side) {
        const bool front = side == 1;
        const std::vector<Vec2>& ring = front ? rings.back() : rings.front();
        const double z = front ? zs.back() : zs.front();
        const unsigned base = static_cast<unsigned>(m.vertexCount());
        for (const Vec2& p : ring) m.addVertex({p.x, p.y, z}, {0.0, 0.0, front ? 1.0 : -1.0});
        for (size_t t = 0; t + 2 < tris.size(); t += 3) {
            if (front) m.addTriangle(base + tris[t], base + tris[t + 1], base + tris[t + 2]);
            else m.addTriangle(base + tris[t], base + tris[t + 2], base + tris[t + 1]);
        }
    }
    return m;
}

}  // namespace meshes
