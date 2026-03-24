#pragma once
#ifndef FEATURE_RECOGNITION_H
#define FEATURE_RECOGNITION_H

#include "BRep.h"
#include <vector>
#include <string>

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
// The recognition results are used to suggest machining strategies and
// initial tool selection in the ToolpathManager.
// --------------------------------------------------------------------------

struct RecognizedFeature {
    enum class Type { Hole, BlindPocket, ThroughPocket, Boss, Slot, Unknown };

    Type        type      = Type::Unknown;
    double      diameter  = 0;   // for holes
    double      depth     = 0;   // axial depth
    double      width     = 0;   // slot width
    int         floorFaceId = -1;
    std::vector<int> wallFaceIds;
    std::string description;
};

class FeatureRecognition {
public:
    FeatureRecognition() = default;

    // Classify all faces in the solid (sets isFloor/isWall/isHole flags)
    void classify(BRep::Solid& solid);

    // Higher-level: return a list of recognised machinable features
    std::vector<RecognizedFeature> recognise(const BRep::Solid& solid);

private:
    std::vector<RecognizedFeature> findHoles(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findPockets(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findBosses(const BRep::Solid& solid);
    std::vector<RecognizedFeature> findSlots(const BRep::Solid& solid);

    double estimateCylinderDiameter(const BRep::Face& face,
                                    const BRep::Solid& solid);
};

#endif // FEATURE_RECOGNITION_H
