#pragma once
#ifndef DYNAMIC_MOTION_H
#define DYNAMIC_MOTION_H

#include "Toolpath.h"
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
// --------------------------------------------------------------------------
struct DynamicParams {
    double maxEngagement   = 0.15;   // max radial engagement as fraction of Ø
    double stepDown        = 12.0;   // axial depth per pass (mm) – full flute
    double trochPitch      = 0.8;    // trochoidal loop pitch (mm)
    double trochRadius     = 0.3;    // trochoidal loop radius as fraction of Ø
    double liftHeight      = 0.25;   // micro-lift height (mm)
    double entryArcRadius  = 1.5;    // helical entry arc radius as fraction of Ø
    double entryRampAngle  = 3.0;    // helical entry ramp angle (degrees)
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

    // Apply micro-lift retracts between all linear moves in an existing toolpath
    static void applyMicroLifts(Toolpath& tp, double liftHeight = 0.25);

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

private:
    // Compute a closed-loop offset of 'boundary' inward by 'dist'
    static std::vector<Geom::Vec2>
        offsetInward(const std::vector<Geom::Vec2>& boundary, double dist);

    // Return a set of parallel scan-line centres within the boundary
    static std::vector<std::vector<Geom::Vec2>>
        scanLines(const std::vector<Geom::Vec2>& boundary,
                  double stepOver);

    DynamicParams m_params;
};

#endif // DYNAMIC_MOTION_H
