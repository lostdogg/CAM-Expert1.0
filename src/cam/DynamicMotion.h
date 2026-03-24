#pragma once
#ifndef DYNAMIC_MOTION_H
#define DYNAMIC_MOTION_H

#include "Toolpath.h"
#include "MaterialLibrary.h"
#include "../cad/MeshData.h"
#include "../cad/BRep.h"
#include <vector>

// --------------------------------------------------------------------------
// DynamicMotion
//
// Implements CAM's "Dynamic" (high-efficiency) machining algorithms:
//
//  1. Trochoidal milling:   The tool follows tiny circular loops along a
//                           core path so it never becomes "buried" in material.
//
//  2. Constant chip-load:   Radial engagement is maintained ≤ maxEngagement
//                           fraction of the tool diameter at all times.
//
//  3. Micro-lift:           Between non-cutting repositions the tool lifts by
//                           a tiny amount (liftHeight) and traverses at high
//                           speed to avoid dragging across the machined surface.
//
//  4. Optimal arc entry:    The tool ramps into material on a helix or arc
//                           rather than plunging vertically.
//
//  5. Material-aware strategy selection:
//       Aluminum – High-Speed Machining (HSM): maximum RPM, large step-over,
//                  deep axial cuts, fast helical ramp, high-speed air moves.
//       Titanium – Low-and-slow: thin radial engagement (5-10 % of Ø),
//                  trochoidal loops, tangential arc entry, controlled
//                  micro-lifts, and G-code smoothing filter output.
// --------------------------------------------------------------------------
struct DynamicParams {
    double maxEngagement   = 0.15;   // max radial engagement as fraction of Ø
    double stepDown        = 12.0;   // axial depth per pass (mm) – full flute
    double trochPitch      = 0.8;    // trochoidal loop pitch (mm)
    double trochRadius     = 0.3;    // trochoidal loop radius as fraction of Ø
    double liftHeight      = 0.25;   // micro-lift height (mm)
    double entryArcRadius  = 1.5;    // helical entry arc radius as fraction of Ø
    double entryRampAngle  = 3.0;    // helical entry ramp angle (degrees)

    // Material-strategy overrides (populated by buildFromMaterial())
    bool   forceHSM         = false;  // force High-Speed Machining mode
    bool   forceTrochoidal  = false;  // force trochoidal loops on all segments
    bool   applySmoothing   = false;  // apply G-code smoothing filter
    MaterialProperties::EntryMethod     entryMethod = MaterialProperties::EntryMethod::HelicalRamp;
    MaterialProperties::RepositionStyle repoStyle   = MaterialProperties::RepositionStyle::HighSpeedAir;

    // Build params from a MaterialLibrary result for a given tool
    static DynamicParams buildFromMaterial(const CuttingTool& tool,
                                            const FeedSpeedResult& fsr);
};

class DynamicMotion {
public:
    explicit DynamicMotion(DynamicParams params = {});

    // Generate a dynamic (high-efficiency) roughing toolpath inside a pocket
    // defined by its 2-D boundary and depth.
    Toolpath generatePocketRough(const std::vector<Geom::Vec2>& boundary,
                                 double depth,
                                 const CuttingTool& tool,
                                 const CuttingParams& cuttingParams);

    // Generate a dynamic contour-following toolpath around a profile
    Toolpath generateContour(const std::vector<Geom::Vec2>& profile,
                             double depth,
                             const CuttingTool& tool,
                             const CuttingParams& cuttingParams);

    // ---- Material-aware strategy generators ----

    // Aluminum: High-Speed Machining strategy
    //   • Maximum RPM and feed (derived from MaterialLibrary)
    //   • Large radial step-over (50–70 % of tool Ø)
    //   • Full flute axial depth
    //   • Wide sweeping moves for chip evacuation
    //   • Fast helical ramp entry
    Toolpath generateAluminumStrategy(const std::vector<Geom::Vec2>& boundary,
                                      double depth,
                                      const CuttingTool& tool);

    // Titanium: Low-and-slow trochoidal strategy
    //   • Reduced RPM, maintained feed-per-tooth
    //   • Thin radial engagement (5–10 % of Ø) for short arc-of-contact
    //   • Trochoidal loops on every segment
    //   • Gradual tangential arc lead-in / lead-out
    //   • Controlled micro-lifts between passes
    //   • Optional G-code smoothing filter applied to output
    Toolpath generateTitaniumStrategy(const std::vector<Geom::Vec2>& boundary,
                                      double depth,
                                      const CuttingTool& tool);

    // Generic material-aware generator: picks the right strategy from the library
    Toolpath generateForMaterial(const std::vector<Geom::Vec2>& boundary,
                                 double depth,
                                 const CuttingTool& tool,
                                 MaterialClass matClass);

    // Apply micro-lift retracts between all linear moves in an existing toolpath
    static void applyMicroLifts(Toolpath& tp, double liftHeight = 0.25);

    // Apply G-code smoothing: replace sharp direction changes with tiny arcs.
    // This is mandatory for Titanium/Inconel to prevent chatter and tool breakage.
    static void applyGCodeSmoothing(Toolpath& tp, double smoothingRadius = 0.05);

    // Generate trochoidal loop moves along a straight core path segment
    static std::vector<ToolpathPoint>
        trochoidalSegment(const Geom::Vec2& from,
                          const Geom::Vec2& to,
                          double toolDiameter,
                          double trochRadius,
                          double trochPitch,
                          double z);

    // Compute a helical arc entry point sequence
    static std::vector<ToolpathPoint>
        helicalEntry(const Geom::Vec2& centre,
                     double toolDiameter,
                     double entryRadiusFrac,
                     double rampAngleDeg,
                     double startZ,
                     double endZ);

    // Tangential arc entry (for Titanium): tool approaches on a smooth tangent
    static std::vector<ToolpathPoint>
        tangentialArcEntry(const Geom::Vec2& firstCutPoint,
                           const Geom::Vec2& approachDir,
                           double arcRadius,
                           double z);

private:
    // Compute a closed-loop offset of 'boundary' inward by 'dist'
    static std::vector<Geom::Vec2>
        offsetInward(const std::vector<Geom::Vec2>& boundary, double dist);

    // Return a set of parallel scan-line centres within the boundary
    static std::vector<std::vector<Geom::Vec2>>
        scanLines(const std::vector<Geom::Vec2>& boundary,
                  double stepOver);

    DynamicParams  m_params;
    MaterialLibrary m_matLib;
};

#endif // DYNAMIC_MOTION_H
