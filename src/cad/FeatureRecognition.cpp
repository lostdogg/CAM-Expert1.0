#include "FeatureRecognition.h"
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
void FeatureRecognition::classify(BRep::Solid& solid) {
    // Access non-const faces via const_cast (owner holds data)
    auto& faces = const_cast<std::vector<BRep::Face>&>(solid.faces());

    for (auto& face : faces) {
        // Reset
        face.isFloor = false;
        face.isWall  = false;
        face.isHole  = false;

        if (face.type == BRep::FaceType::Planar) {
            // Floor: normal points upward (Z > 0.7)
            if (face.normal.z > 0.7)
                face.isFloor = true;
            // Wall: normal is mostly horizontal
            else if (std::abs(face.normal.z) < 0.3)
                face.isWall = true;
        } else if (face.type == BRep::FaceType::Cylindrical) {
            face.isHole = true;
        }
    }
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::recognise(const BRep::Solid& solid) {
    std::vector<RecognizedFeature> features;

    auto holes   = findHoles(solid);
    auto pockets = findPockets(solid);
    auto bosses  = findBosses(solid);
    auto slots   = findSlots(solid);

    features.insert(features.end(), holes.begin(),   holes.end());
    features.insert(features.end(), pockets.begin(), pockets.end());
    features.insert(features.end(), bosses.begin(),  bosses.end());
    features.insert(features.end(), slots.begin(),   slots.end());
    return features;
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findHoles(const BRep::Solid& solid) {
    std::vector<RecognizedFeature> result;
    for (const auto& face : solid.faces()) {
        if (face.type != BRep::FaceType::Cylindrical) continue;
        RecognizedFeature f;
        f.type     = RecognizedFeature::Type::Hole;
        f.diameter = estimateCylinderDiameter(face, solid);
        f.depth    = 10.0; // placeholder – would be computed from face height
        f.description = "Hole Ø" + std::to_string(f.diameter) + " mm";
        result.push_back(f);
    }
    return result;
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findPockets(const BRep::Solid& solid) {
    std::vector<RecognizedFeature> result;
    for (const auto& face : solid.faces()) {
        if (!face.isFloor) continue;
        RecognizedFeature f;
        f.type        = RecognizedFeature::Type::BlindPocket;
        f.floorFaceId = face.id;
        f.depth       = 5.0; // placeholder
        f.description = "Blind pocket, floor face " + std::to_string(face.id);
        result.push_back(f);
    }
    return result;
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findBosses(const BRep::Solid& /*solid*/) {
    // Placeholder – bosses are protrusions above the stock reference plane
    return {};
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findSlots(const BRep::Solid& /*solid*/) {
    // Placeholder – slots are detected by opposing parallel walls with a common floor
    return {};
}

// --------------------------------------------------------------------------
double FeatureRecognition::estimateCylinderDiameter(const BRep::Face& face,
                                                     const BRep::Solid& solid) {
    if (face.edgeIds.empty()) return 0.0;
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

    // Approximate by finding the max XY distance between edge endpoints
    double maxDist = 0;
    for (int eid : face.edgeIds) {
        if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
        const auto& e = edges[static_cast<std::size_t>(eid)];
        if (e.startVertexId < 0 || e.endVertexId < 0) continue;
        const auto& a = verts[static_cast<std::size_t>(e.startVertexId)].point;
        const auto& b = verts[static_cast<std::size_t>(e.endVertexId)].point;
        double dx = a.x - b.x, dy = a.y - b.y;
        double d  = std::sqrt(dx*dx + dy*dy);
        if (d > maxDist) maxDist = d;
    }
    return maxDist; // diameter ≈ chord length for a full circle
}
