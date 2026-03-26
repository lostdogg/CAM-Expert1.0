#pragma once
#ifndef FEATURE_RECOGNITION_H
#define FEATURE_RECOGNITION_H

#include "BRep.h"
#include "../cam/Toolpath.h"
#include "../cam/MaterialLibrary.h"
#include <vector>
#include <string>
#include <optional>

// --------------------------------------------------------------------------
// FeatureRecognition
//
// Analyses a B-Rep solid and automatically identifies machinable features:
//   • Holes (cylindrical faces with a floor)
//   • Blind pockets (planar floor + vertical walls)
//   • Through-pockets (no floor)
//   • Bosses / protrusions
//   • Slots
//
// Automatic Feature Recognition (AFR) enhancements:
//   • Knowledge-base strategy mapping: once a feature is identified the
//     system looks up the optimal strategy from the StrategyLibrary based
//     on feature geometry and the selected material.
//   • Multi-axis hole orientation: for holes whose axis is not aligned with
//     the machine Z-axis, the system computes the required A/B rotation
//     angles so the tool can be brought into alignment.
// --------------------------------------------------------------------------

struct RecognizedFeature {
    enum class Type { Hole, BlindPocket, ThroughPocket, Boss, Slot, Chamfer, Unknown };

    Type        type      = Type::Unknown;
    double      diameter  = 0;   // for holes and cylindrical pockets
    double      depth     = 0;   // axial depth
    double      width     = 0;   // slot width
    Geom::Vec3  axis      = {0, 0, 1}; // feature axis (default: Z)
    Geom::Vec3  centre    = {0, 0, 0}; // centroid of the feature in world space
    int         floorFaceId = -1;
    std::vector<int> wallFaceIds;
    std::string description;

    // ---- AFR additions ----
    // Suggested machining strategy (set by StrategyLibrary)
    std::optional<StrategyType> suggestedStrategy;
    // Suggested tool from the library (diameter in mm)
    double                      suggestedToolDiameter = 0;
    // Required machine rotations to align the tool with 'axis' (degrees)
    double                      requiredAAngle = 0;  // A-axis (tilt)
    double                      requiredBAngle = 0;  // B-axis (rotation)
    bool                        needsMultiAxis = false;
};

// --------------------------------------------------------------------------
// StrategyLibrary – knowledge base: feature geometry + material → strategy
// --------------------------------------------------------------------------
class StrategyLibrary {
public:
    StrategyLibrary() = default;

    // Given a recognized feature and the workpiece material, return the
    // optimal strategy type and suggested tool diameter.
    StrategyType recommendStrategy(const RecognizedFeature& feat,
                                   MaterialClass mat) const;

    // Recommend a tool diameter for the feature
    double recommendToolDiameter(const RecognizedFeature& feat) const;
};

// --------------------------------------------------------------------------
// FeatureRecognition
// --------------------------------------------------------------------------
class FeatureRecognition {
public:
    FeatureRecognition() = default;

    // Classify all faces in the solid (sets isFloor/isWall/isHole flags)
    void classify(BRep::Solid& solid);

    // Higher-level: return a list of recognised machinable features
    std::vector<RecognizedFeature> recognise(const BRep::Solid& solid);

    // AFR with strategy mapping: recognise features AND suggest strategies
    // based on the workpiece material.
    std::vector<RecognizedFeature> recogniseWithStrategy(
        const BRep::Solid& solid,
        MaterialClass mat = MaterialClass::Steel);

    // Compute the required A/B rotary angles to align the tool axis with
    // a feature's axis vector.  Uses simple inverse-kinematics (spherical).
    // Returns true if the angles are within typical 5-axis machine limits.
    static bool computeHoleOrientation(const Geom::Vec3& holeAxis,
                                        double& aAngleDeg,
                                        double& bAngleDeg);

private:
    std::vector<RecognizedFeature> findHoles(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findPockets(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findBosses(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findSlots(const BRep::Solid& solid);

    double estimateCylinderDiameter(const BRep::Face& face,
                                    const BRep::Solid& solid);

    StrategyLibrary m_stratLib;
};

#endif // FEATURE_RECOGNITION_H

