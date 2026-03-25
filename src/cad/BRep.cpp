#include "BRep.h"
#include <algorithm>
#include <cmath>

namespace BRep {

// --------------------------------------------------------------------------
int Solid::addVertex(const Geom::Vec3& pt) {
    Vertex v;
    v.point = pt;
    v.id    = static_cast<int>(m_vertices.size());
    m_vertices.push_back(v);
    return v.id;
}

int Solid::addEdge(int v0, int v1, bool curved) {
    Edge e;
    e.startVertexId = v0;
    e.endVertexId   = v1;
    e.isCurved      = curved;
    e.id            = static_cast<int>(m_edges.size());
    m_edges.push_back(e);
    return e.id;
}

int Solid::addFace(FaceType type, const std::vector<int>& edgeIds,
                   const Geom::Vec3& normal) {
    Face f;
    f.type    = type;
    f.edgeIds = edgeIds;
    f.normal  = normal;
    f.id      = static_cast<int>(m_faces.size());
    m_shell.faceIds.push_back(f.id);
    m_faces.push_back(f);
    return f.id;
}

// --------------------------------------------------------------------------
Geom::AABB Solid::boundingBox() const {
    Geom::AABB box;
    for (const auto& v : m_vertices)
        box.expand(v.point);
    return box;
}

// --------------------------------------------------------------------------
std::vector<int> Solid::holeFaces() const {
    std::vector<int> result;
    for (const auto& f : m_faces)
        if (f.isHole) result.push_back(f.id);
    return result;
}

std::vector<int> Solid::pocketFloors() const {
    std::vector<int> result;
    for (const auto& f : m_faces)
        if (f.isFloor) result.push_back(f.id);
    return result;
}

// --------------------------------------------------------------------------
// Factory: axis-aligned box
// --------------------------------------------------------------------------
Solid Solid::makeBox(double dx, double dy, double dz) {
    Solid s;
    s.setName("Box");
    // 8 vertices
    int v[8];
    v[0] = s.addVertex({0,  0,  0});
    v[1] = s.addVertex({dx, 0,  0});
    v[2] = s.addVertex({dx, dy, 0});
    v[3] = s.addVertex({0,  dy, 0});
    v[4] = s.addVertex({0,  0,  dz});
    v[5] = s.addVertex({dx, 0,  dz});
    v[6] = s.addVertex({dx, dy, dz});
    v[7] = s.addVertex({0,  dy, dz});

    // 12 edges
    int e[12];
    e[0]  = s.addEdge(v[0], v[1]);
    e[1]  = s.addEdge(v[1], v[2]);
    e[2]  = s.addEdge(v[2], v[3]);
    e[3]  = s.addEdge(v[3], v[0]);
    e[4]  = s.addEdge(v[4], v[5]);
    e[5]  = s.addEdge(v[5], v[6]);
    e[6]  = s.addEdge(v[6], v[7]);
    e[7]  = s.addEdge(v[7], v[4]);
    e[8]  = s.addEdge(v[0], v[4]);
    e[9]  = s.addEdge(v[1], v[5]);
    e[10] = s.addEdge(v[2], v[6]);
    e[11] = s.addEdge(v[3], v[7]);

    // 6 planar faces
    s.addFace(FaceType::Planar, {e[0],e[1],e[2],e[3]}, {0,0,-1}); // bottom
    s.addFace(FaceType::Planar, {e[4],e[5],e[6],e[7]}, {0,0,1});  // top
    s.addFace(FaceType::Planar, {e[0],e[9],e[4],e[8]}, {0,-1,0}); // front
    s.addFace(FaceType::Planar, {e[2],e[10],e[6],e[11]},{0,1,0}); // back
    s.addFace(FaceType::Planar, {e[3],e[8],e[7],e[11]}, {-1,0,0});// left
    s.addFace(FaceType::Planar, {e[1],e[9],e[5],e[10]}, {1,0,0}); // right

    return s;
}

// --------------------------------------------------------------------------
// Factory: upright cylinder (approximated as B-Rep for topology)
// --------------------------------------------------------------------------
Solid Solid::makeCylinder(double radius, double height) {
    Solid s;
    s.setName("Cylinder");

    // Two circular faces (top and bottom) and one cylindrical side face
    // Approximate with 16-gon for vertex/edge representation
    const int N = 16;
    std::vector<int> botVerts, topVerts;
    for (int i = 0; i < N; ++i) {
        double angle = 2.0 * 3.14159265358979 * i / N;
        botVerts.push_back(s.addVertex({radius*std::cos(angle),
                                        radius*std::sin(angle), 0.0}));
        topVerts.push_back(s.addVertex({radius*std::cos(angle),
                                        radius*std::sin(angle), height}));
    }

    // Bottom ring edges
    std::vector<int> botEdges, topEdges, sideEdges;
    for (int i = 0; i < N; ++i) {
        botEdges.push_back(s.addEdge(botVerts[i], botVerts[(i+1)%N], true));
        topEdges.push_back(s.addEdge(topVerts[i], topVerts[(i+1)%N], true));
        sideEdges.push_back(s.addEdge(botVerts[i], topVerts[i]));
    }

    // Bottom face (floor)
    auto& bf = s.m_faces.emplace_back();
    bf.type    = FaceType::Planar;
    bf.normal  = {0, 0, -1};
    bf.isFloor = true;
    bf.edgeIds = botEdges;
    bf.id      = static_cast<int>(s.m_faces.size()-1);
    s.m_shell.faceIds.push_back(bf.id);

    // Top face
    Face tf;
    tf.type    = FaceType::Planar;
    tf.normal  = {0, 0, 1};
    tf.isFloor = false;
    tf.edgeIds = topEdges;
    tf.id      = static_cast<int>(s.m_faces.size());
    s.m_faces.push_back(tf);
    s.m_shell.faceIds.push_back(tf.id);

    // Cylindrical side face (hole classification)
    Face sf;
    sf.type   = FaceType::Cylindrical;
    sf.isHole = true;
    for (int i = 0; i < N; ++i) {
        sf.edgeIds.push_back(botEdges[i]);
        sf.edgeIds.push_back(sideEdges[i]);
        sf.edgeIds.push_back(topEdges[i]);
        sf.edgeIds.push_back(sideEdges[(i+1)%N]);
    }
    sf.id = static_cast<int>(s.m_faces.size());
    s.m_faces.push_back(sf);
    s.m_shell.faceIds.push_back(sf.id);

    return s;
}

// --------------------------------------------------------------------------
// Factory: UV sphere (tessellated with flat-shaded triangles/quads)
// latBands  – number of horizontal latitude strips (≥ 2)
// lonSegs   – number of longitudinal segments per ring (≥ 3)
// --------------------------------------------------------------------------
Solid Solid::makeSphere(double radius, int latBands, int lonSegs) {
    Solid s;
    s.setName("Sphere");
    if (latBands < 2) latBands = 2;
    if (lonSegs  < 3) lonSegs  = 3;

    const double PI = 3.14159265358979323846;

    // Top pole vertex
    int topPole = s.addVertex({0.0, 0.0, radius});

    // Ring vertices: rings[lat][lon]  (lat = 1 .. latBands-1)
    std::vector<std::vector<int>> rings;
    rings.reserve(static_cast<std::size_t>(latBands - 1));
    for (int i = 1; i < latBands; ++i) {
        double phi = PI * i / latBands;        // 0 (top) → PI (bottom)
        double z   = radius * std::cos(phi);
        double r   = radius * std::sin(phi);
        std::vector<int> ring;
        ring.reserve(static_cast<std::size_t>(lonSegs));
        for (int j = 0; j < lonSegs; ++j) {
            double theta = 2.0 * PI * j / lonSegs;
            ring.push_back(s.addVertex({r * std::cos(theta), r * std::sin(theta), z}));
        }
        rings.push_back(ring);
    }

    // Bottom pole vertex
    int botPole = s.addVertex({0.0, 0.0, -radius});

    const auto& verts = s.m_vertices;  // alias for normal computation

    // Top cap: one triangle fan from topPole → first ring
    for (int j = 0; j < lonSegs; ++j) {
        int v0 = topPole;
        int v1 = rings[0][j];
        int v2 = rings[0][(j + 1) % lonSegs];
        int e0 = s.addEdge(v0, v1);
        int e1 = s.addEdge(v1, v2, true);
        int e2 = s.addEdge(v2, v0);
        // Face normal: centroid of the three vertices / radius
        Geom::Vec3 n{
            (verts[v0].point.x + verts[v1].point.x + verts[v2].point.x) / (3.0 * radius),
            (verts[v0].point.y + verts[v1].point.y + verts[v2].point.y) / (3.0 * radius),
            (verts[v0].point.z + verts[v1].point.z + verts[v2].point.z) / (3.0 * radius)
        };
        s.addFace(FaceType::Spherical, {e0, e1, e2}, n);
    }

    // Middle bands: quad strips between adjacent rings
    for (int i = 0; i < static_cast<int>(rings.size()) - 1; ++i) {
        for (int j = 0; j < lonSegs; ++j) {
            int v0 = rings[i][j];
            int v1 = rings[i][(j + 1) % lonSegs];
            int v2 = rings[i + 1][(j + 1) % lonSegs];
            int v3 = rings[i + 1][j];
            int e0 = s.addEdge(v0, v1, true);
            int e1 = s.addEdge(v1, v2);
            int e2 = s.addEdge(v2, v3, true);
            int e3 = s.addEdge(v3, v0);
            Geom::Vec3 n{
                (verts[v0].point.x + verts[v1].point.x +
                 verts[v2].point.x + verts[v3].point.x) / (4.0 * radius),
                (verts[v0].point.y + verts[v1].point.y +
                 verts[v2].point.y + verts[v3].point.y) / (4.0 * radius),
                (verts[v0].point.z + verts[v1].point.z +
                 verts[v2].point.z + verts[v3].point.z) / (4.0 * radius)
            };
            s.addFace(FaceType::Spherical, {e0, e1, e2, e3}, n);
        }
    }

    // Bottom cap: one triangle fan from last ring → botPole
    const auto& lastRing = rings.back();
    for (int j = 0; j < lonSegs; ++j) {
        int v0 = lastRing[j];
        int v1 = lastRing[(j + 1) % lonSegs];
        int v2 = botPole;
        int e0 = s.addEdge(v0, v1, true);
        int e1 = s.addEdge(v1, v2);
        int e2 = s.addEdge(v2, v0);
        Geom::Vec3 n{
            (verts[v0].point.x + verts[v1].point.x + verts[v2].point.x) / (3.0 * radius),
            (verts[v0].point.y + verts[v1].point.y + verts[v2].point.y) / (3.0 * radius),
            (verts[v0].point.z + verts[v1].point.z + verts[v2].point.z) / (3.0 * radius)
        };
        s.addFace(FaceType::Spherical, {e0, e1, e2}, n);
    }

    return s;
}

} // namespace BRep
