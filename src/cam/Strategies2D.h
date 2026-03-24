#pragma once
#ifndef STRATEGIES2D_H
#define STRATEGIES2D_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include <vector>

// --------------------------------------------------------------------------
// 2D / 2.5D Milling strategies
//
//  • Contour2D  – profile following around a 2-D boundary
//  • Pocket2D   – concentric offset pocketing (traditional)
//  • FaceMill   – zigzag face milling over a rectangular area
//  • Drilling   – standard canned drill cycle (G81/G83)
//  • Chamfer    – chamfer milling along a profile at a specified angle
//  • ThreadMill – helical thread milling
// --------------------------------------------------------------------------

class Strategies2D {
public:
    struct Contour2DParams {
        double depth            = 5.0;    // axial depth (mm)
        double stockAllowance   = 0.0;    // finish stock on walls
        int    passes           = 1;      // number of axial passes
        bool   climb            = true;   // climb (true) or conventional (false)
        double leadInRadius     = 2.0;    // tangential lead-in arc radius
    };

    struct Pocket2DParams {
        double depth            = 10.0;
        double stepOver         = 0.5;    // fraction of tool diameter
        double stockAllowance   = 0.25;
        bool   spiralIn         = true;   // true = spiral, false = zigzag
    };

    struct FaceMillParams {
        double xMin, yMin, xMax, yMax;   // bounding rectangle
        double depth        = 0.5;       // face mill depth
        double stepOver     = 0.75;      // fraction of tool diameter
        double stockAllowance = 0.0;
    };

    struct DrillParams {
        double totalDepth   = 20.0;      // total hole depth
        double peckDepth    = 5.0;       // peck increment (G83)
        bool   usePeck      = true;
        double dwellSec     = 0.0;       // dwell at bottom
    };

    // Factory methods – generate and return a complete Toolpath
    static Toolpath contour2D(const std::vector<Geom::Vec2>& profile,
                               const Contour2DParams& p,
                               const CuttingTool& tool,
                               const CuttingParams& cuts);

    static Toolpath pocket2D(const std::vector<Geom::Vec2>& boundary,
                              const Pocket2DParams& p,
                              const CuttingTool& tool,
                              const CuttingParams& cuts);

    static Toolpath faceMill(const FaceMillParams& p,
                              const CuttingTool& tool,
                              const CuttingParams& cuts);

    static Toolpath drilling(const std::vector<Geom::Vec2>& holePositions,
                              const DrillParams& p,
                              const CuttingTool& tool,
                              const CuttingParams& cuts);

private:
    // Generate a single planar pass at a given Z
    static void addContourPass(Toolpath& tp,
                                const std::vector<Geom::Vec2>& profile,
                                double z, double leadInR);

    // Generate concentric offset loops inward from a boundary
    static std::vector<std::vector<Geom::Vec2>>
        concentricOffsets(const std::vector<Geom::Vec2>& boundary,
                          double stepOver);
};

#endif // STRATEGIES2D_H
