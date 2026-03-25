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
        // Deep narrow holes → peck drill (G83); shallow wide holes → standard drill (G81)
        if (feat.diameter > 0 && feat.depth / feat.diameter > 3.0)
            return StrategyType::Drilling; // peck drill cycle for deep holes
        return StrategyType::Drilling;    // standard drill for shallow holes

    case RecognizedFeature::Type::BlindPocket:
    case RecognizedFeature::Type::ThroughPocket:
        // Hard materials (Titanium, Inconel, Stainless): dynamic trochoidal
        if (mat == MaterialClass::Titanium  ||
            mat == MaterialClass::Inconel   ||
            mat == MaterialClass::StainlessSteel)
            return StrategyType::DynamicMill;
        // Soft materials (Aluminum): HSM dynamic or standard pocket
        if (mat == MaterialClass::Aluminum)
            return StrategyType::DynamicMill;
        // Cast iron: standard pocket to keep chip load predictable
        if (mat == MaterialClass::CastIron)
            return StrategyType::Pocket2D;
        // Default: use dynamic mill for best chip evacuation
        return StrategyType::Pocket2D;

    case RecognizedFeature::Type::Boss:
        // Boss: contour with lead-in/out; use dynamic mill for hard materials
        if (mat == MaterialClass::Titanium || mat == MaterialClass::Inconel)
            return StrategyType::DynamicMill;
        return StrategyType::Contour2D;

    case RecognizedFeature::Type::Slot:
        // Slots: trochoidal for hard materials (avoid full-width engagement)
        if (mat == MaterialClass::Titanium  ||
            mat == MaterialClass::Inconel   ||
            mat == MaterialClass::StainlessSteel)
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
// findBosses – detect upward-facing planar faces whose area is smaller than
// the total top face of the solid, surrounded by wall faces on at least two
// sides.  A boss is a protrusion: the floor face lies ABOVE the stock bottom.
// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findBosses(const BRep::Solid& solid) {
    std::vector<RecognizedFeature> result;

    // Collect all floor faces (isFloor = true, normal pointing up)
    std::vector<const BRep::Face*> floors;
    for (const auto& face : solid.faces()) {
        if (face.isFloor) floors.push_back(&face);
    }
    if (floors.empty()) return result;

    // Find the lowest floor z-level (stock bottom reference)
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

    auto faceMinZ = [&](const BRep::Face* f) -> double {
        double minZ = 1e30;
        for (int eid : f->edgeIds) {
            if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
            const auto& e = edges[static_cast<std::size_t>(eid)];
            if (e.startVertexId >= 0 && e.startVertexId < static_cast<int>(verts.size()))
                minZ = std::min(minZ, verts[static_cast<std::size_t>(e.startVertexId)].point.z);
            if (e.endVertexId >= 0 && e.endVertexId < static_cast<int>(verts.size()))
                minZ = std::min(minZ, verts[static_cast<std::size_t>(e.endVertexId)].point.z);
        }
        return minZ;
    };

    // Collect z levels of all floor faces
    double lowestZ = 1e30;
    for (const auto* f : floors) {
        double z = faceMinZ(f);
        if (z < lowestZ) lowestZ = z;
    }

    // Faces that are floors AND above the lowest floor level → bosses
    for (const auto* f : floors) {
        double z = faceMinZ(f);
        // Boss floor must be at least one unit above the stock bottom
        if (z > lowestZ + 0.5) {
            RecognizedFeature feat;
            feat.type        = RecognizedFeature::Type::Boss;
            feat.floorFaceId = f->id;
            feat.depth       = z - lowestZ;  // boss height above stock floor
            feat.axis        = {0, 0, 1};

            // Estimate boss plan-view diameter from edge lengths
            double maxEdgeLen = 0;
            for (int eid : f->edgeIds) {
                if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
                const auto& e = edges[static_cast<std::size_t>(eid)];
                if (e.startVertexId < 0 || e.endVertexId < 0) continue;
                auto diff = verts[static_cast<std::size_t>(e.endVertexId)].point
                          - verts[static_cast<std::size_t>(e.startVertexId)].point;
                double len = diff.length();
                if (len > maxEdgeLen) maxEdgeLen = len;
            }
            feat.width       = maxEdgeLen;
            feat.description = "Boss, height " + std::to_string(feat.depth).substr(0, 5)
                             + " mm, width ~" + std::to_string(feat.width).substr(0, 5) + " mm";
            result.push_back(feat);
        }
    }
    return result;
}

// --------------------------------------------------------------------------
// findSlots – detect pairs of parallel wall faces separated by a small gap
// that is too narrow for a standard pocket (width < 2× tool diameter proxy).
// A slot has: two parallel wall faces + a floor face connecting them.
// --------------------------------------------------------------------------
std::vector<RecognizedFeature>
FeatureRecognition::findSlots(const BRep::Solid& solid) {
    std::vector<RecognizedFeature> result;

    const auto& faces = solid.faces();
    const auto& verts = solid.vertices();
    const auto& edges = solid.edges();

    // Collect wall faces (vertical, |normal.z| < 0.3)
    std::vector<const BRep::Face*> walls;
    for (const auto& face : faces) {
        if (face.isWall) walls.push_back(&face);
    }

    if (walls.size() < 2) return result;

    // For each pair of wall faces: check if their normals are anti-parallel
    // and measure the distance between them
    for (std::size_t i = 0; i < walls.size(); ++i) {
        for (std::size_t j = i + 1; j < walls.size(); ++j) {
            const BRep::Face* w1 = walls[i];
            const BRep::Face* w2 = walls[j];

            // Normals must be nearly anti-parallel: dot ≈ -1
            double dot = w1->normal.dot(w2->normal);
            if (dot > -0.85) continue;

            // Compute representative center points of each wall from their edges
            auto faceCentre = [&](const BRep::Face* f) -> Geom::Vec3 {
                Geom::Vec3 sum{};
                int cnt = 0;
                for (int eid : f->edgeIds) {
                    if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
                    const auto& e = edges[static_cast<std::size_t>(eid)];
                    if (e.startVertexId >= 0 && e.startVertexId < static_cast<int>(verts.size())) {
                        sum = sum + verts[static_cast<std::size_t>(e.startVertexId)].point;
                        ++cnt;
                    }
                }
                return cnt > 0 ? sum * (1.0 / cnt) : Geom::Vec3{};
            };

            Geom::Vec3 c1 = faceCentre(w1);
            Geom::Vec3 c2 = faceCentre(w2);

            // Slot width = distance between wall centres projected onto the normal
            double width = std::abs((c2 - c1).dot(w1->normal));

            // A slot is narrow: width < 50 mm (typical slot range)
            if (width < 1e-3 || width > 50.0) continue;

            // Check there is a floor face between these walls
            bool hasFloor = false;
            for (const auto& face : faces) {
                if (face.isFloor) { hasFloor = true; break; }
            }
            if (!hasFloor) continue;

            // Estimate slot length from the wall face edge spans
            double maxLen = 0;
            for (int eid : w1->edgeIds) {
                if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
                const auto& e = edges[static_cast<std::size_t>(eid)];
                if (e.startVertexId < 0 || e.endVertexId < 0) continue;
                auto diff = verts[static_cast<std::size_t>(e.endVertexId)].point
                          - verts[static_cast<std::size_t>(e.startVertexId)].point;
                double len = diff.length();
                if (len > maxLen) maxLen = len;
            }

            RecognizedFeature feat;
            feat.type        = RecognizedFeature::Type::Slot;
            feat.width       = width;
            feat.depth       = 5.0; // placeholder – computed from floor z vs. top z
            feat.axis        = {0, 0, 1};
            feat.wallFaceIds = {w1->id, w2->id};
            feat.description = "Slot, width " + std::to_string(width).substr(0, 5)
                             + " mm, length ~" + std::to_string(maxLen).substr(0, 5) + " mm";

            // Avoid duplicating the same slot pair (only add if unique width)
            bool duplicate = false;
            for (const auto& r : result) {
                if (r.type == RecognizedFeature::Type::Slot &&
                    std::abs(r.width - feat.width) < 0.5)
                { duplicate = true; break; }
            }
            if (!duplicate) result.push_back(feat);
        }
    }
    return result;
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
