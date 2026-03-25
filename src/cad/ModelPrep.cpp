#include "ModelPrep.h"
#include "FeatureRecognition.h"
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
BRep::Solid ModelPrep::prep(const BRep::Solid& solid, const PrepOptions& opts) {
    BRep::Solid result = solid;

    if (opts.removeSmallFillets)
        result = removeFillets(result, opts.filletRadiusLimit);

    if (opts.healSurfaces)
        result = healSurfaces(result, opts.healGapTolerance);

    classifyFeatures(result);
    return result;
}

// --------------------------------------------------------------------------
MeshData ModelPrep::prep(const MeshData& mesh, const PrepOptions& opts) {
    MeshData result = mesh;
    result.removeDegenerate();
    if (opts.simplifyMesh) {
        // Placeholder: a real implementation would use quadric error metrics
        // (e.g., the Garland-Heckbert algorithm).
    }
    return result;
}

// --------------------------------------------------------------------------
BRep::Solid ModelPrep::removeFillets(const BRep::Solid& solid, double radiusLimit) {
    // A fillet face is typically cylindrical/toroidal with a small radius.
    // We detect such faces by checking the bounding-box extents of each face's edges.
    // Production: compute actual curvature radius of the curved surface.
    BRep::Solid result = solid;

    auto& faces = const_cast<std::vector<BRep::Face>&>(result.faces());
    faces.erase(
        std::remove_if(faces.begin(), faces.end(),
            [&](const BRep::Face& f) {
                if (f.type != BRep::FaceType::Cylindrical &&
                    f.type != BRep::FaceType::Toroidal)
                    return false;
                // Approximate radius from edge endpoint distances
                const auto& verts = solid.vertices();
                if (f.edgeIds.empty()) return false;
                // Use the first edge length as a proxy for radius
                const auto& e0 = solid.edges()[static_cast<std::size_t>(f.edgeIds[0])];
                if (e0.startVertexId < 0 || e0.endVertexId < 0) return false;
                auto diff = verts[static_cast<std::size_t>(e0.endVertexId)].point
                          - verts[static_cast<std::size_t>(e0.startVertexId)].point;
                double approxRadius = diff.length() / 2.0;
                return approxRadius < radiusLimit;
            }),
        faces.end());

    return result;
}

// --------------------------------------------------------------------------
// healSurfaces – stitch free edges whose endpoints are within gapTol of each
// other.  A "free edge" has only one adjacent face (the half-edge is not
// paired).  When two free-edge endpoints are within gapTol we merge them by
// moving both to their midpoint, effectively closing the gap.
// --------------------------------------------------------------------------
BRep::Solid ModelPrep::healSurfaces(const BRep::Solid& solid, double gapTol) {
    BRep::Solid result = solid;

    // Build a mutable copy of vertices so we can snap them together
    // (we access via the public API, so we rebuild the solid from scratch
    //  with snapped vertex coordinates).
    const auto& srcVerts = solid.vertices();
    const auto& srcEdges = solid.edges();

    // Collect all vertex positions
    std::vector<Geom::Vec3> newPositions;
    newPositions.reserve(srcVerts.size());
    for (const auto& v : srcVerts)
        newPositions.push_back(v.point);

    // Count how many edges reference each vertex (as start or end)
    // A vertex at the end of only one edge is a "free" tip candidate
    std::vector<int> vertexDegree(srcVerts.size(), 0);
    for (const auto& e : srcEdges) {
        if (e.startVertexId >= 0 && e.startVertexId < static_cast<int>(srcVerts.size()))
            ++vertexDegree[static_cast<std::size_t>(e.startVertexId)];
        if (e.endVertexId >= 0 && e.endVertexId < static_cast<int>(srcVerts.size()))
            ++vertexDegree[static_cast<std::size_t>(e.endVertexId)];
    }

    // Snap vertex pairs that are within gapTol of each other and have low degree
    bool anySnapped = false;
    for (std::size_t i = 0; i < newPositions.size(); ++i) {
        for (std::size_t j = i + 1; j < newPositions.size(); ++j) {
            auto diff = newPositions[i] - newPositions[j];
            double dist = diff.length();
            if (dist > 0 && dist <= gapTol) {
                // Snap both to their midpoint
                Geom::Vec3 mid{
                    (newPositions[i].x + newPositions[j].x) * 0.5,
                    (newPositions[i].y + newPositions[j].y) * 0.5,
                    (newPositions[i].z + newPositions[j].z) * 0.5
                };
                newPositions[i] = mid;
                newPositions[j] = mid;
                anySnapped = true;
            }
        }
    }

    if (!anySnapped) return result; // nothing to heal

    // Rebuild the solid with snapped positions
    BRep::Solid healed;
    healed.setName(solid.name());

    for (const auto& pos : newPositions)
        healed.addVertex(pos);

    for (const auto& e : srcEdges)
        healed.addEdge(e.startVertexId, e.endVertexId, e.isCurved);

    for (const auto& f : solid.faces())
        healed.addFace(f.type, f.edgeIds, f.normal);

    return healed;
}

// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
ModelPrep::extractBoundaries(const BRep::Solid& solid) {
    // Return the XY projection of each planar face's edge loop as a polyline.
    std::vector<std::vector<Geom::Vec2>> boundaries;
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

    for (const auto& face : solid.faces()) {
        if (face.type != BRep::FaceType::Planar) continue;
        std::vector<Geom::Vec2> loop;
        for (int eid : face.edgeIds) {
            if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
            const auto& e = edges[static_cast<std::size_t>(eid)];
            if (e.startVertexId < 0) continue;
            const auto& pt = verts[static_cast<std::size_t>(e.startVertexId)].point;
            loop.push_back({pt.x, pt.y});
        }
        if (loop.size() >= 2)
            boundaries.push_back(std::move(loop));
    }
    return boundaries;
}

// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
ModelPrep::extractBoundaries(const MeshData& mesh) {
    auto segs = mesh.silhouetteXY();
    // Collect all unique endpoints as a single polyline (approximate)
    std::vector<Geom::Vec2> poly;
    poly.reserve(segs.size());
    for (const auto& seg : segs)
        poly.push_back(seg.first);
    return {poly};
}

// --------------------------------------------------------------------------
std::vector<Geom::Vec2>
ModelPrep::floorSilhouette(const BRep::Solid& solid, double zLevel) {
    // Find all vertices at or near zLevel and collect their XY coordinates
    std::vector<Geom::Vec2> outline;
    const double tol = 0.01;
    for (const auto& v : solid.vertices()) {
        if (std::abs(v.point.z - zLevel) < tol)
            outline.push_back({v.point.x, v.point.y});
    }
    return outline;
}

// --------------------------------------------------------------------------
void ModelPrep::classifyFeatures(BRep::Solid& solid) {
    FeatureRecognition fr;
    fr.classify(solid);
}

// --------------------------------------------------------------------------
std::vector<ModelPrep::HealIssue>
ModelPrep::analyseModel(const BRep::Solid& solid) {
    std::vector<HealIssue> issues;

    const auto& faces = solid.faces();
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

    // --- Check 1: Faces with no boundary edges ---
    for (const auto& face : faces) {
        if (face.edgeIds.empty()) {
            HealIssue h;
            h.description = "Face " + std::to_string(face.id)
                          + " has no boundary edges (open/missing face)";
            h.faceId = face.id;
            issues.push_back(h);
        }
        if (face.type == BRep::FaceType::NURBS && face.normal == Geom::Vec3{}) {
            HealIssue h;
            h.description = "NURBS face " + std::to_string(face.id)
                          + " has undefined normal";
            h.faceId = face.id;
            issues.push_back(h);
        }
    }

    // --- Check 2: Duplicate vertex positions (gap tolerance 0.001 mm) ---
    static constexpr double kDupTol = 0.001;
    for (std::size_t i = 0; i < verts.size(); ++i) {
        for (std::size_t j = i + 1; j < verts.size(); ++j) {
            auto diff = verts[i].point - verts[j].point;
            if (diff.length() < kDupTol && diff.length() > 0) {
                HealIssue h;
                h.description = "Near-duplicate vertices V" + std::to_string(verts[i].id)
                              + " and V" + std::to_string(verts[j].id)
                              + " within " + std::to_string(diff.length()).substr(0, 7) + " mm"
                              + " – consider snapping (healSurfaces)";
                h.faceId = -1;
                issues.push_back(h);
                // Stop after first duplicate to avoid O(n^2) output explosion
                goto checkEdges;
            }
        }
    }

checkEdges:
    // --- Check 3: Degenerate edges (zero-length) ---
    for (const auto& e : edges) {
        if (e.startVertexId < 0 || e.endVertexId < 0) continue;
        if (e.startVertexId >= static_cast<int>(verts.size()) ||
            e.endVertexId   >= static_cast<int>(verts.size())) {
            HealIssue h;
            h.description = "Edge " + std::to_string(e.id)
                          + " references out-of-range vertex ID";
            h.faceId = -1;
            issues.push_back(h);
            continue;
        }
        auto diff = verts[static_cast<std::size_t>(e.endVertexId)].point
                  - verts[static_cast<std::size_t>(e.startVertexId)].point;
        if (diff.length() < 1e-9) {
            HealIssue h;
            h.description = "Degenerate (zero-length) edge E"
                          + std::to_string(e.id)
                          + " between V" + std::to_string(e.startVertexId)
                          + " and V" + std::to_string(e.endVertexId);
            h.faceId = -1;
            issues.push_back(h);
        }
    }

    // --- Check 4: Unclassified faces (no normal set) ---
    for (const auto& face : faces) {
        if (face.type == BRep::FaceType::Planar &&
            face.normal.length() < 1e-6 && !face.edgeIds.empty()) {
            HealIssue h;
            h.description = "Planar face " + std::to_string(face.id)
                          + " has zero normal vector (may be degenerate)";
            h.faceId = face.id;
            issues.push_back(h);
        }
    }

    return issues;
}
