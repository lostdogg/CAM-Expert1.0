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
BRep::Solid ModelPrep::healSurfaces(const BRep::Solid& solid, double /*gapTol*/) {
    // In production, this would stitch free edges whose endpoints are within
    // gapTol of each other. Here we return the solid unchanged (topology already
    // consistent for factory-built solids; gaps occur with imported geometry).
    return solid;
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

    for (const auto& face : solid.faces()) {
        if (face.edgeIds.empty()) {
            HealIssue h;
            h.description = "Face has no boundary edges (open/missing face)";
            h.faceId      = face.id;
            issues.push_back(h);
        }
        if (face.type == BRep::FaceType::NURBS && face.normal == Geom::Vec3{}) {
            HealIssue h;
            h.description = "NURBS face has undefined normal";
            h.faceId      = face.id;
            issues.push_back(h);
        }
    }
    return issues;
}
