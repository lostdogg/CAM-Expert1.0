#pragma once
#ifndef MESHDATA_H
#define MESHDATA_H

#include "Geometry.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// Polygon mesh (triangle-based)
//
// Used for STL, OBJ, and any tessellated surface. The CAM engine processes
// mesh geometry using high-density facet algorithms to produce smooth
// toolpaths free of faceting artefacts.
// --------------------------------------------------------------------------
class MeshData {
public:
    MeshData() = default;

    // Build from raw triangle list
    explicit MeshData(std::vector<Geom::Triangle> triangles);

    // Accessors
    const std::vector<Geom::Triangle>& triangles() const { return m_triangles; }
    std::size_t triCount() const { return m_triangles.size(); }

    // Geometry queries
    Geom::AABB  boundingBox() const;
    Geom::Vec3  centroid()    const;
    double      surfaceArea() const;

    // Merge another mesh into this one
    void merge(const MeshData& other);

    // Repair helpers
    // Remove degenerate (zero-area) triangles
    void removeDegenerate(double areaTol = 1e-12);

    // Offset every vertex outward by 'amount' along its average normal
    void offset(double amount);

    // Generate a 2-D silhouette / shadow boundary projected onto the XY plane
    // Returns polyline segments (pairs of 2-D points)
    std::vector<std::pair<Geom::Vec2, Geom::Vec2>> silhouetteXY() const;

    // Normals (per-triangle)
    std::vector<Geom::Vec3> computeNormals() const;

    const std::string& name() const        { return m_name; }
    void setName(const std::string& n)     { m_name = n; }

private:
    std::vector<Geom::Triangle> m_triangles;
    std::string                 m_name;
};

#endif // MESHDATA_H
