#pragma once
#ifndef TURNING_H
#define TURNING_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include <vector>

// --------------------------------------------------------------------------
// Turning & Mill-Turn Machining
//
// Turning operations are fundamentally different from milling: the part
// rotates at the spindle while a stationary insert cuts the profile.
//
// Supported operations:
//   • Rough turning        – stacked passes removing bulk material
//   • Finish turning       – single pass to final dimension
//   • Groove turning       – plunge-and-widen into a groove
//   • Thread turning       – synchronized Z + spindle for threading
//   • Parting              – cut-off operation
//   • Sub-spindle transfer – synchronize main to sub spindle (Mill-Turn)
// --------------------------------------------------------------------------

struct TurningParams {
    double stockDiameter    = 100.0;  // mm (raw stock outer Ø)
    double finishedDiameter = 70.0;   // mm (target finished Ø)
    double partLength       = 150.0;  // mm (along Z axis)
    double depthOfCut       = 2.0;    // mm (radial depth per pass)
    double feedPerRev       = 0.25;   // mm/rev
    double surfaceSpeed     = 200.0;  // m/min
    double stockAllowance   = 0.25;   // mm (finish stock)
    double safeRadius       = 5.0;    // mm clearance above stock surface
};

struct GrooveParams {
    double centreDiameter   = 80.0;   // mm groove centre Ø
    double width            = 5.0;    // mm groove width
    double depth            = 3.0;    // mm groove depth
    double feedPerRev       = 0.05;
    double peckDepth        = 0.5;    // mm – depth per peck
};

struct ThreadParams {
    double majorDiameter    = 20.0;   // mm
    double pitch            = 1.5;    // mm/thread
    double threadDepth      = 0.92;   // mm (0.613 × pitch for 60° thread)
    double length           = 30.0;   // mm thread length
    int    springPasses     = 2;      // finish spring passes
};

class Turning {
public:
    // Rough turning: remove material in successive radial passes
    static Toolpath roughTurn(const TurningParams& p,
                               const CuttingTool& tool,
                               const CuttingParams& cuts);

    // Finish turning: single-pass to exact diameter following a profile
    static Toolpath finishTurn(const std::vector<Geom::Vec2>& profile, // (Z, X=radius) pairs
                                const TurningParams& p,
                                const CuttingTool& tool,
                                const CuttingParams& cuts);

    // Groove turning (G75 canned peck-groove cycle)
    static Toolpath groove(const GrooveParams& p,
                            const CuttingTool& tool,
                            const CuttingParams& cuts);

    // Thread turning (G76 / G92 cycle)
    static Toolpath thread(const ThreadParams& p,
                            const CuttingTool& tool,
                            const CuttingParams& cuts);

    // Sub-spindle transfer: build a synchronization point
    static Toolpath subSpindleTransfer(double transferZ,
                                        double spindleRPM,
                                        const CuttingTool& tool);
};

#endif // TURNING_H
