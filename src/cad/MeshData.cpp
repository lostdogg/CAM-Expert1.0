#include "MeshData.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <set>

// --------------------------------------------------------------------------
MeshData::MeshData(std::vector<Geom::Triangle> triangles)
    : m_triangles(std::move(triangles)) {}

// --------------------------------------------------------------------------
Geom::AABB MeshData::boundingBox() const {
    Geom::AABB box;
    for (const auto& tri : m_triangles)
        for (int i = 0; i < 3; ++i)
            box.expand(tri.v[i]);
    return box;
}

// --------------------------------------------------------------------------
Geom::Vec3 MeshData::centroid() const {
    if (m_triangles.empty()) return {};
    Geom::Vec3 sum{};
    for (const auto& tri : m_triangles)
        for (int i = 0; i < 3; ++i)
            sum = sum + tri.v[i];
    double n = static_cast<double>(m_triangles.size() * 3);
    return sum * (1.0 / n);
}

// --------------------------------------------------------------------------
double MeshData::surfaceArea() const {
    double area = 0;
    for (const auto& tri : m_triangles) {
        auto e1 = tri.v[1] - tri.v[0];
        auto e2 = tri.v[2] - tri.v[0];
        area += 0.5 * e1.cross(e2).length();
    }
    return area;
}

// --------------------------------------------------------------------------
void MeshData::merge(const MeshData& other) {
    m_triangles.insert(m_triangles.end(),
                       other.m_triangles.begin(),
                       other.m_triangles.end());
}

// --------------------------------------------------------------------------
void MeshData::removeDegenerate(double areaTol) {
    m_triangles.erase(
        std::remove_if(m_triangles.begin(), m_triangles.end(),
            [areaTol](const Geom::Triangle& t) {
                auto e1 = t.v[1] - t.v[0];
                auto e2 = t.v[2] - t.v[0];
                return 0.5 * e1.cross(e2).length() < areaTol;
            }),
        m_triangles.end());
}

// --------------------------------------------------------------------------
void MeshData::offset(double amount) {
    // Compute per-vertex average normals first
    // (simplified: offset each triangle vertex along triangle normal)
    for (auto& tri : m_triangles) {
        auto n = tri.normal();
        auto d = n * amount;
        tri.v[0] = tri.v[0] + d;
        tri.v[1] = tri.v[1] + d;
        tri.v[2] = tri.v[2] + d;
    }
}

// --------------------------------------------------------------------------
// Silhouette: project each edge onto XY plane. An edge whose two adjacent
// triangles face opposite directions (one toward +Z, one toward -Z) forms
// part of the silhouette boundary.
// --------------------------------------------------------------------------
std::vector<std::pair<Geom::Vec2, Geom::Vec2>>
MeshData::silhouetteXY() const {
    // Use a simple approach: collect all edges where the triangle's Z-normal
    // sign changes between neighbouring faces. For a quick approximation,
    // return all boundary edges of upward-facing triangles.
    std::vector<std::pair<Geom::Vec2, Geom::Vec2>> segments;

    for (const auto& tri : m_triangles) {
        auto n = tri.normal();
        if (n.z <= 0) continue; // skip downward-facing

        // Project edges onto XY and add as silhouette candidates
        for (int i = 0; i < 3; ++i) {
            auto& a = tri.v[i];
            auto& b = tri.v[(i+1)%3];
            segments.emplace_back(
                Geom::Vec2{a.x, a.y},
                Geom::Vec2{b.x, b.y});
        }
    }
    return segments;
}

// --------------------------------------------------------------------------
std::vector<Geom::Vec3> MeshData::computeNormals() const {
    std::vector<Geom::Vec3> normals;
    normals.reserve(m_triangles.size());
    for (const auto& tri : m_triangles)
        normals.push_back(tri.normal());
    return normals;
}
