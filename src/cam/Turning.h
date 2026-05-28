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

// --------------------------------------------------------------------------
// §4.4 – SemiFinishParams
//
// Controls a semi-finishing pass between rough and finish turning.
// Leaves a controlled stock allowance for the final finish pass while
// improving surface quality and stock consistency for the finisher.
// --------------------------------------------------------------------------
struct SemiFinishParams {
    double targetDiameter   = 72.0;   // mm – semi-finish target Ø (between stock and finish)
    double semiAllowance    = 0.15;   // mm – radial stock to leave for finish pass
    double feedPerRev       = 0.15;   // mm/rev – lighter than rough, heavier than finish
    double surfaceSpeed     = 220.0;  // m/min
    double depthOfCut       = 0.5;    // mm – light radial pass
    int    numPasses        = 2;      // number of semi-finish passes
};

// --------------------------------------------------------------------------
// §4.4 – ThreadProfileParams
//
// Allows the programmer to override the standard thread profile with a
// custom insert geometry.  The profile is defined as a list of (Z, X/2)
// pairs in insert-local coordinates (origin at insert tip).
// --------------------------------------------------------------------------
struct ThreadProfileParams {
    double majorDiameter  = 20.0;
    double pitch          = 1.5;
    double length         = 30.0;
    int    springPasses   = 2;
    // Custom profile: list of (axial-offset, radial-offset) from insert tip
    std::vector<Geom::Vec2> profilePoints;  // empty = use standard 60° V-form
    double profileAngleDeg = 60.0;          // used when profilePoints is empty
};

// --------------------------------------------------------------------------
// §4.4 – CustomInsert
//
// Describes a non-standard turning insert.  The geometry is used to
// compute the true gouge envelope for finish/semi-finish passes.
// --------------------------------------------------------------------------
struct CustomInsert {
    enum class Shape { CNMG, DNMG, TNMG, VNMG, Custom };
    Shape   shape         = Shape::CNMG;
    double  inscribedCircle = 12.7;    // mm (IC) – standard designation
    double  thickness       = 4.76;    // mm
    double  noseRadius      = 0.8;     // mm
    double  reliefAngleDeg  = 7.0;     // °
    double  rakeAngleDeg    = 0.0;     // ° (positive = positive rake)
    // Custom outline (for Shape::Custom): a list of 2D points around the insert
    std::vector<Geom::Vec2> customOutline;
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

    // -----------------------------------------------------------------------
    // §4.4 – Semi-finish turning
    //
    // Generates a light semi-finishing pass between rough and finish turning.
    // The pass removes the bulk of the rough stock allowance while leaving
    // a small, consistent stock for the finish pass.
    // -----------------------------------------------------------------------
    static Toolpath semiFinish(const SemiFinishParams& p,
                                const CuttingTool&      tool,
                                const CuttingParams&    cuts);

    // -----------------------------------------------------------------------
    // §4.4 – Custom thread profile turning
    //
    // Generates a threading pass using either the standard 60° V-form or a
    // user-defined custom insert profile.  The profile is described as a
    // list of (Z, X/2) deviations from the nominal thread form.
    // -----------------------------------------------------------------------
    static Toolpath customThreadProfile(const ThreadProfileParams& p,
                                          const CuttingTool&         tool,
                                          const CuttingParams&       cuts);
};

#endif // TURNING_H
