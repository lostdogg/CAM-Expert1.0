#pragma once
#ifndef BREP_H
#define BREP_H

#include "Geometry.h"
#include <vector>
#include <memory>
#include <string>

// --------------------------------------------------------------------------
// B-Rep (Boundary Representation) model
//
// A solid is described by a set of connected Faces. Each face is bounded by
// Edges and has an outward-pointing surface normal. Edges connect Vertices.
// This topology allows the CAM engine to calculate precise tool offsets from
// surface normals and identify features such as holes, pockets, and bosses.
// --------------------------------------------------------------------------

namespace BRep {

struct Vertex {
    Geom::Vec3 point;
    int        id = -1;
};

struct Edge {
    int  startVertexId = -1;
    int  endVertexId   = -1;
    bool isCurved      = false;  // true for arc/spline edges
    int  id            = -1;
};

// Face types
enum class FaceType { Planar, Cylindrical, Conical, Spherical, Toroidal, NURBS };

struct Face {
    FaceType          type    = FaceType::Planar;
    Geom::Vec3        normal;       // surface normal (for planar faces)
    std::vector<int>  edgeIds;      // boundary edges
    double            area    = 0;
    int               id      = -1;

    // Feature classification (set by FeatureRecognition)
    bool              isFloor = false;  // horizontal floor of a pocket/step
    bool              isWall  = false;  // vertical wall
    bool              isHole  = false;  // cylindrical hole face
};

struct Shell {
    std::vector<int> faceIds;
    bool             isClosed = true;  // solid shell vs. open surface
};

// --------------------------------------------------------------------------
// Solid – the top-level B-Rep entity
// --------------------------------------------------------------------------
class Solid {
public:
    Solid() = default;

    // Factory helpers
    static Solid makeBox(double dx, double dy, double dz);
    static Solid makeCylinder(double radius, double height);

    // Accessors
    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<Edge>&   edges()    const { return m_edges; }
    const std::vector<Face>&   faces()    const { return m_faces; }
    const Shell&               shell()    const { return m_shell; }

    Geom::AABB boundingBox() const;

    // Topology queries
    std::vector<int> holeFaces()   const;  // faces classified as holes
    std::vector<int> pocketFloors() const; // floor faces of pockets

    int addVertex(const Geom::Vec3& pt);
    int addEdge(int v0, int v1, bool curved = false);
    int addFace(FaceType type, const std::vector<int>& edgeIds,
                const Geom::Vec3& normal = {});

    const std::string& name() const     { return m_name; }
    void setName(const std::string& n)  { m_name = n; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<Edge>   m_edges;
    std::vector<Face>   m_faces;
    Shell               m_shell;
    std::string         m_name;
};

} // namespace BRep

#endif // BREP_H
