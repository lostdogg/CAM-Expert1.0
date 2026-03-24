#pragma once
#ifndef MODEL_PREP_H
#define MODEL_PREP_H

#include "BRep.h"
#include "MeshData.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// ModelPrep options – defined outside the class to avoid GCC default-argument
// restrictions with nested struct in-class initializers.
// --------------------------------------------------------------------------
struct ModelPrepOptions {
    bool   removeSmallFillets = true;
    double filletRadiusLimit  = 0.5;   // mm – fillets smaller than this removed
    bool   healSurfaces       = true;
    double healGapTolerance   = 0.01;  // mm – gap tolerance for surface healing
    bool   extractBoundaries  = true;
    bool   simplifyMesh       = false;
    double meshSimplifyRatio  = 0.5;   // fraction of triangles to keep
};

// --------------------------------------------------------------------------
// ModelPrep
//
// Tools for preparing imported geometry for machining. Common tasks include:
//   • Removing features that don't need to be machined (fillets, chamfers)
//   • Healing broken/mismatched surfaces (edge gaps, missing faces)
//   • Extracting boundary geometry (2-D silhouettes, hole outlines)
//   • Offsetting / extending surfaces for tool holder clearance checks
// --------------------------------------------------------------------------
class ModelPrep {
public:
    // Keep PrepOptions as an alias for backward compatibility
    using PrepOptions = ModelPrepOptions;

    // Main entry point: apply all enabled prep operations to a B-Rep solid
    static BRep::Solid  prep(const BRep::Solid&  solid,  const PrepOptions& opts = PrepOptions{});
    static MeshData     prep(const MeshData&     mesh,   const PrepOptions& opts = PrepOptions{});

    // Individual operations
    // Remove faces whose bounding radius is below opts.filletRadiusLimit
    static BRep::Solid removeFillets(const BRep::Solid& solid, double radiusLimit);

    // Close gaps between faces within healGapTolerance
    static BRep::Solid healSurfaces(const BRep::Solid& solid, double gapTol);

    // Extract all outer boundary loops as 2-D polylines (for 2-D toolpaths)
    static std::vector<std::vector<Geom::Vec2>>
        extractBoundaries(const BRep::Solid& solid);

    static std::vector<std::vector<Geom::Vec2>>
        extractBoundaries(const MeshData& mesh);

    // Build a "floor silhouette" – 2-D projection suitable as pocket boundary
    static std::vector<Geom::Vec2>
        floorSilhouette(const BRep::Solid& solid, double zLevel);

    // Analyse faces and flag them as holes / floors / walls
    static void classifyFeatures(BRep::Solid& solid);

    // Diagnostic: list all detected heal issues
    struct HealIssue {
        std::string description;
        int         faceId = -1;
    };
    static std::vector<HealIssue> analyseModel(const BRep::Solid& solid);
};

#endif // MODEL_PREP_H
