#pragma once
#ifndef SWISS_MACHINING_H
#define SWISS_MACHINING_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// SwissMachining – Sliding-Headstock (Swiss-type) Toolpath Generation
//
// Swiss-type CNC lathes differ from conventional turning centres in one
// fundamental way: the PART moves axially through a fixed guide bushing
// while the cutting tools remain (largely) stationary in Z.
//
// Consequences for toolpath coordinates:
//  • What would be a "Z tool move" in conventional turning becomes a "Z
//    part move" (headstock slide) in Swiss coordinates.
//  • The post-processor must invert the Z relationship accordingly.
//  • Tool X/Y motion still describes radial and cross-axis moves.
//
// Pinch / Synchronised Turning:
//  Swiss parts are often long and slender (medical bone screws, watch shafts,
//  hydraulic fittings).  Without support, the cutting force would bow the
//  part ("whipping").  The solution is to cut with TWO tools on opposite
//  sides of the part simultaneously.  The radial forces cancel, keeping the
//  part rigidly on the Z-axis.
//
//  The SwissMachining class provides:
//   1. slidingHeadstock() – generates a standard turning profile with
//      Z-inverted coordinate logic flagged for the post-processor.
//   2. pinchSync()        – generates a matched pair of toolpaths (leader +
//      follower) that cut simultaneously from ±X, with synchronised Z slides.
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// SwissParams – configuration for a sliding-headstock operation
// --------------------------------------------------------------------------
struct SwissParams {
    // Part geometry
    double barDiameter      = 12.0;   // mm – raw bar stock diameter
    double finishedDiameter = 8.0;    // mm – target finished diameter
    double partLength       = 80.0;   // mm – usable part length (from guide bushing face)

    // Cutting parameters
    double depthOfCut       = 0.5;    // mm – radial depth per pass
    double feedPerRev       = 0.05;   // mm/rev
    double surfaceSpeed     = 150.0;  // m/min

    // Guide bushing position (reference Z = 0)
    double guideBushingZ    = 0.0;    // mm – guide bushing face Z in machine coords

    // Sliding headstock: part slides in -Z to expose more stock for cutting.
    // Each increment pulls the stock forward by slideIncrement mm.
    double slideIncrement   = 5.0;    // mm – part slide per pass segment

    // Safety / clearance
    double safeRadialClearance = 2.0; // mm above bar surface for rapids

    // Pinch turning
    bool   usePinchCutting  = false;  // enable simultaneous two-tool cutting
    double pinchLeadOffset  = 0.1;    // mm – axial lead of the follower tool
                                      //      (prevents exact XZ coincidence)
};

// --------------------------------------------------------------------------
// SwissToolpathPair – result of pinchSync(): leader + follower toolpaths
// --------------------------------------------------------------------------
struct SwissToolpathPair {
    Toolpath leader;    // primary tool (+X side)
    Toolpath follower;  // secondary tool (−X side, synchronised)
};

// --------------------------------------------------------------------------
// SwissMachining
// --------------------------------------------------------------------------
class SwissMachining {
public:
    explicit SwissMachining(SwissParams params = {});

    // -----------------------------------------------------------------------
    // slidingHeadstock()
    //
    // Generates a conventional-profile turning toolpath with Z-inverted
    // coordinate logic: the part slides in −Z (headstock advances toward the
    // tool) while the tool moves radially (X) and cross-axially (Y).
    //
    // The resulting ToolpathPoints are in MACHINE coordinates:
    //   • position.x  = tool radial position (unchanged from lathe convention)
    //   • position.z  = headstock Z slide position (negative = stock advanced)
    //
    // A flag in the toolpath name ("SWISS-SLIDE") allows the post-processor
    // to recognise and handle the inverted Z convention.
    //
    // 'profile' is a sequence of (Z_part, X_radius) pairs describing the
    // finished part profile measured from the guide bushing face.
    // -----------------------------------------------------------------------
    Toolpath slidingHeadstock(const std::vector<Geom::Vec2>& profile,
                               const CuttingTool& tool,
                               const CuttingParams& cuts) const;

    // -----------------------------------------------------------------------
    // pinchSync()
    //
    // Generates a synchronised two-tool toolpath for slender parts.
    //
    // The leader tool approaches from +X; the follower mirrors it from −X.
    // Both tools are fed along Z (headstock slides) at the same rate so the
    // net radial force on the part is zero.
    //
    // The follower is offset by SwissParams::pinchLeadOffset in Z to prevent
    // the two inserts from riding exactly the same chip groove.
    //
    // Returns a SwissToolpathPair containing both toolpaths.  The caller is
    // responsible for assigning them to separate turrets / channels.
    // -----------------------------------------------------------------------
    SwissToolpathPair pinchSync(const std::vector<Geom::Vec2>& profile,
                                 const CuttingTool& leaderTool,
                                 const CuttingTool& followerTool,
                                 const CuttingParams& cuts) const;

    // -----------------------------------------------------------------------
    // computeWhipLimit()
    //
    // Returns the maximum unsupported length (mm) beyond the guide bushing at
    // which the cutting force would cause the part to deflect ("whip") by
    // more than 'maxDeflectionMm'.
    //
    // Uses Euler–Bernoulli beam bending:
    //   δ = F·L³ / (3·E·I)  →  L_max = ∛(3·E·I·δ_max / F)
    //
    // where:
    //   F  = radial cutting force (N)   – estimated from material + DOC
    //   E  = Young's modulus (GPa)      – from material class
    //   I  = π/64 · d⁴                 – second moment of area for round bar
    //   d  = finished diameter (mm)
    // -----------------------------------------------------------------------
    static double computeWhipLimit(double finishedDiameterMm,
                                    double radialCuttingForceN,
                                    double youngsModulusGPa,
                                    double maxDeflectionMm = 0.01);

private:
    SwissParams m_params;

    // Build a single-tool pass along the profile at the given radial offset
    Toolpath buildPass(const std::vector<Geom::Vec2>& profile,
                        const CuttingTool& tool,
                        const CuttingParams& cuts,
                        double xSign,          // +1.0 or -1.0
                        double zOffset,        // additional Z offset (pinch lead)
                        StrategyType strat,
                        const std::string& name) const;
};

#endif // SWISS_MACHINING_H
