#pragma once
#ifndef SELF_CORRECTION_LOOP_H
#define SELF_CORRECTION_LOOP_H

#include "../simulation/Verify.h"
#include "../cam/Toolpath.h"
#include "../cam/MaterialLibrary.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// SelfCorrectionLoop – Local Re-Inference on Gouge Detection
//
// Implements the "Local Self-Correction Logic" from the FRD.  When the
// simulation engine flags a collision or gouge, the loop:
//
//   1. Extracts the toolpath parameters used for the offending move.
//   2. Assembles a "correction context" describing the collision event.
//   3. Passes the context to the local inference engine (or the built-in
//      rule-based solver) to generate a corrected vector.
//   4. Returns a CorrectionResult that the CopilotEngine can apply and
//      re-verify — all without leaving the local machine.
//
// Correction types supported
// ──────────────────────────
//   HolderCollision – tool holder hits workpiece; fix: increase tilt angle
//                     or suggest a shorter, extended-reach holder.
//   GougeDetected   – tool tip removes too much material; fix: reduce depth
//                     or add a finishing-stock allowance.
//   RapidGouge      – rapid traverse crosses the part; fix: insert a safe
//                     retract + repositioning move.
// --------------------------------------------------------------------------

enum class CorrectionType {
    HolderCollision,    // tool-holder body hits part
    GougeDetected,      // tool cuts below nominal surface
    RapidGouge,         // rapid move crosses part geometry
    Undercut,           // material left under an overhang (not removed)
};

// ---- Input to the correction solver ---------------------------------------
struct CorrectionRequest {
    CorrectionType  type;

    // Location where the collision / gouge was detected (world mm)
    Geom::Vec3      collisionPoint;

    // Toolpath parameters that caused the event
    CuttingParams   params;
    CuttingTool     tool;

    // Clearance goal: how much additional space is needed (mm)
    double          requiredClearanceMm = 2.0;

    // Natural-language description for the inference context block
    std::string     contextDescription;

    // Current tool tilt angles (for multi-axis holder-clearance fixes)
    double          currentTiltDeg  = 0.0;
    double          currentLagDeg   = 0.0;
};

// ---- Output from the correction solver ------------------------------------
struct CorrectionResult {
    bool        success = false;
    std::string description;       // human-readable explanation

    // Proposed geometry correction
    double      suggestedTiltDeg   = 0.0;  // new A-axis tilt angle (deg)
    double      suggestedLagDeg    = 0.0;  // new lead/lag angle (deg)
    double      suggestedDepthMm   = 0.0;  // revised axial depth (mm)
    double      suggestedStockMm   = 0.0;  // revised stock allowance (mm)

    // Proposed safe retract point
    Geom::Vec3  safeRetractPoint;
    bool        needsRetract = false;

    // Recommended post-correction action
    enum class Action {
        AdjustTilt,        // change tool tilt (holder clearance)
        ReduceDepth,       // decrease axial depth of cut
        AddStockAllowance, // increase finishing stock to prevent gouge
        InsertRetract,     // add safe retract before the offending move
        SuggestLongerTool, // current reach is insufficient
    };
    Action      recommendedAction = Action::ReduceDepth;

    std::string warningMessage;  // non-fatal advisory
};

// --------------------------------------------------------------------------
class SelfCorrectionLoop {
public:
    SelfCorrectionLoop() = default;

    // Build a CorrectionRequest from a live VerifyResult.
    // Uses the first detected gouge or holder collision.
    // Returns false if the verify result has no issues.
    bool buildRequest(const VerifyResult& result,
                      const CuttingParams& params,
                      const CuttingTool&   tool,
                      CorrectionRequest&   requestOut) const;

    // Solve a CorrectionRequest and return a suggested fix.
    // Operates entirely locally (no network calls).
    CorrectionResult solve(const CorrectionRequest& request) const;

    // Convenience: build + solve in one call.
    // Returns an unsuccessful result if result has no issues.
    CorrectionResult analyseAndCorrect(const VerifyResult& result,
                                        const CuttingParams& params,
                                        const CuttingTool&   tool) const;

private:
    // Correction strategies
    CorrectionResult solveHolderCollision(const CorrectionRequest& req) const;
    CorrectionResult solveGouge          (const CorrectionRequest& req) const;
    CorrectionResult solveRapidGouge     (const CorrectionRequest& req) const;
    CorrectionResult solveUndercut       (const CorrectionRequest& req) const;

    // Geometry helpers
    // Compute tilt angle needed to clear a holder of outerDiameter by clearance mm
    // at a given axial reach.
    static double computeTiltAngle(double holderDiameterMm,
                                   double reachMm,
                                   double clearanceMm);
};

#endif // SELF_CORRECTION_LOOP_H
