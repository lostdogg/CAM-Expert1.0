#include "FeatureRecognition.h"
#include <cmath>
#include <algorithm>

static constexpr double DEG = 180.0 / 3.14159265358979323846;
static constexpr double RAD = 3.14159265358979323846 / 180.0;

// --------------------------------------------------------------------------
// StrategyLibrary
// --------------------------------------------------------------------------
StrategyType StrategyLibrary::recommendStrategy(const RecognizedFeature& feat,
                                                 MaterialClass mat) const {
    switch (feat.type) {
    case RecognizedFeature::Type::Hole:
        // Deep narrow holes → peck drill; short wide holes → simple drill
        if (feat.diameter > 0 && feat.depth / feat.diameter > 3.0)
            return StrategyType::Drilling; // deep: peck cycle (G83)
        return StrategyType::Drilling;

    case RecognizedFeature::Type::BlindPocket:
    case RecognizedFeature::Type::ThroughPocket:
        // Hard materials (Titanium, Inconel): dynamic trochoidal
        if (mat == MaterialClass::Titanium || mat == MaterialClass::Inconel)
            return StrategyType::DynamicMill;
        // Soft materials (Aluminum): standard pocket or dynamic
        if (mat == MaterialClass::Aluminum)
            return StrategyType::DynamicMill; // HSM pocket
        return StrategyType::Pocket2D;

    case RecognizedFeature::Type::Boss:
        return StrategyType::Contour2D;

    case RecognizedFeature::Type::Slot:
        if (mat == MaterialClass::Titanium || mat == MaterialClass::Inconel)
            return StrategyType::DynamicMill;
        return StrategyType::Pocket2D;

    case RecognizedFeature::Type::Chamfer:
        return StrategyType::Chamfer;

    default:
        return StrategyType::Custom;
    }
}

double StrategyLibrary::recommendToolDiameter(const RecognizedFeature& feat) const {
    switch (feat.type) {
    case RecognizedFeature::Type::Hole:
        return feat.diameter;  // drill matches hole diameter exactly

    case RecognizedFeature::Type::BlindPocket:
    case RecognizedFeature::Type::ThroughPocket:
        // Tool diameter ≤ pocket width (use ~50% of narrowest dimension)
        if (feat.width > 0)
            return std::min(feat.width * 0.5, 25.0); // cap at 25 mm
        return 12.0; // default

    case RecognizedFeature::Type::Slot:
        return std::min(feat.width * 0.8, 20.0);

    default:
        return 12.0;
    }
}

// --------------------------------------------------------------------------
// FeatureRecognition::classify
// --------------------------------------------------------------------------
void FeatureRecognition::classify(BRep::Solid& solid) {
    auto& faces = const_cast<std::vector<BRep::Face>&>(solid.faces());

    for (auto& face : faces) {
        face.isFloor = false;
        face.isWall  = false;
        face.isHole  = false;

        if (face.type == BRep::FaceType::Planar) {
            if (face.normal.z > 0.7)
                face.isFloor = true;
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
// recogniseWithStrategy – AFR + strategy library lookup
// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::recogniseWithStrategy(const BRep::Solid& solid,
                                           MaterialClass mat) {
    auto features = recognise(solid);

    for (auto& feat : features) {
        // Strategy library mapping
        feat.suggestedStrategy    = m_stratLib.recommendStrategy(feat, mat);
        feat.suggestedToolDiameter= m_stratLib.recommendToolDiameter(feat);

        // Multi-axis orientation for non-Z-aligned holes
        if (feat.type == RecognizedFeature::Type::Hole) {
            double a = 0, b = 0;
            feat.needsMultiAxis = !computeHoleOrientation(feat.axis, a, b)
                                  || std::abs(a) > 0.1 || std::abs(b) > 0.1;
            feat.requiredAAngle = a;
            feat.requiredBAngle = b;
        }
    }
    return features;
}

// --------------------------------------------------------------------------
// computeHoleOrientation
//
// Given the axis of a hole (normalised direction vector), compute the
// required A (tilt) and B (rotation) angles to align the machine spindle.
//
// Convention (Head-Table machine):
//   B = atan2(holeAxis.x, holeAxis.z)   ← rotation about Y
//   A = -atan2(holeAxis.y, sqrt(x²+z²)) ← tilt about X
//
// Returns false if the resulting angles exceed typical ±120° / ±90° limits.
// --------------------------------------------------------------------------
bool FeatureRecognition::computeHoleOrientation(const Geom::Vec3& holeAxis,
                                                 double& aAngleDeg,
                                                 double& bAngleDeg) {
    double len = holeAxis.length();
    if (len < 1e-9) { aAngleDeg = bAngleDeg = 0; return true; }

    double nx = holeAxis.x / len;
    double ny = holeAxis.y / len;
    double nz = holeAxis.z / len;

    // B-axis (rotation about Y): bring axis into YZ plane
    bAngleDeg = std::atan2(nx, nz) * DEG;

    // A-axis (tilt about X): bring axis to Z
    double proj = std::sqrt(nx*nx + nz*nz);
    aAngleDeg   = -std::atan2(ny, proj) * DEG;

    // Check limits
    return (std::abs(aAngleDeg) <= 120.0 && std::abs(bAngleDeg) <= 90.0);
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
        f.axis     = face.normal.length() > 0.5
                    ? face.normal      // use face normal as approximate axis
                    : Geom::Vec3{0, 0, 1};
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
    return {};
}

// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findSlots(const BRep::Solid& /*solid*/) {
    return {};
}

// --------------------------------------------------------------------------
double FeatureRecognition::estimateCylinderDiameter(const BRep::Face& face,
                                                     const BRep::Solid& solid) {
    if (face.edgeIds.empty()) return 0.0;
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

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
    return maxDist;
}
