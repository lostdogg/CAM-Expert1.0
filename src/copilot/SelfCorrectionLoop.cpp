#include "SelfCorrectionLoop.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// buildRequest
// --------------------------------------------------------------------------

bool SelfCorrectionLoop::buildRequest(const VerifyResult&  result,
                                       const CuttingParams& params,
                                       const CuttingTool&   tool,
                                       CorrectionRequest&   requestOut) const
{
    if (!result.hasGouge && !result.hasUndercut)
        return false;

    CorrectionRequest req;
    req.params = params;
    req.tool   = tool;

    if (result.hasGouge && !result.gougeLocations.empty()) {
        req.type           = CorrectionType::GougeDetected;
        req.collisionPoint = result.gougeLocations[0];
        req.requiredClearanceMm = 2.0;

        std::ostringstream ctx;
        ctx << std::fixed << std::setprecision(3)
            << "Gouge detected at ("
            << req.collisionPoint.x << ", "
            << req.collisionPoint.y << ", "
            << req.collisionPoint.z << ") mm. "
            << "Max gouge depth: " << result.maxGougeDepth << " mm. "
            << "Total gouge locations: " << result.gougeCount << ".";
        req.contextDescription = ctx.str();
    } else if (result.hasUndercut && !result.undercutLocations.empty()) {
        req.type           = CorrectionType::Undercut;
        req.collisionPoint = result.undercutLocations[0];
        req.requiredClearanceMm = 0.5;

        std::ostringstream ctx;
        ctx << std::fixed << std::setprecision(3)
            << "Undercut detected at ("
            << req.collisionPoint.x << ", "
            << req.collisionPoint.y << ", "
            << req.collisionPoint.z << ") mm. "
            << "Max undercut depth: " << result.maxUndercutDepth << " mm.";
        req.contextDescription = ctx.str();
    } else {
        return false;
    }

    requestOut = std::move(req);
    return true;
}

// --------------------------------------------------------------------------
// solve – dispatch to specialised solver
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::solve(const CorrectionRequest& request) const {
    switch (request.type) {
        case CorrectionType::HolderCollision:
            return solveHolderCollision(request);
        case CorrectionType::GougeDetected:
            return solveGouge(request);
        case CorrectionType::RapidGouge:
            return solveRapidGouge(request);
        case CorrectionType::Undercut:
            return solveUndercut(request);
        default:
            break;
    }
    CorrectionResult fail;
    fail.success = false;
    fail.description = "Unknown correction type.";
    return fail;
}

// --------------------------------------------------------------------------
// analyseAndCorrect
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::analyseAndCorrect(
    const VerifyResult&  result,
    const CuttingParams& params,
    const CuttingTool&   tool) const
{
    CorrectionRequest req;
    if (!buildRequest(result, params, tool, req)) {
        CorrectionResult none;
        none.success     = true;
        none.description = "No gouges or undercuts detected – no correction needed.";
        return none;
    }
    return solve(req);
}

// --------------------------------------------------------------------------
// computeTiltAngle
//
// Trigonometric approximation:
//   The holder protrudes beyond the tool shank by (holderDia - toolDia)/2.
//   At axial reach R from the tip, the holder edge is at R * tan(tilt).
//   We need R * tan(tilt) >= clearance + (holderDia - toolDia)/2.
//   → tilt = atan((clearance + holderOverhang) / reach)
// --------------------------------------------------------------------------

double SelfCorrectionLoop::computeTiltAngle(double holderDiameterMm,
                                             double reachMm,
                                             double clearanceMm)
{
    if (reachMm < 0.001)
        return 5.0; // safety default

    double overhang = std::max(0.0, (holderDiameterMm - 12.0) / 2.0);
    double tiltRad  = std::atan2(clearanceMm + overhang, reachMm);
    double tiltDeg  = tiltRad * 180.0 / 3.14159265358979323846;
    return std::min(tiltDeg, 30.0); // clamp to 30° machine limit
}

// --------------------------------------------------------------------------
// solveHolderCollision
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::solveHolderCollision(
    const CorrectionRequest& req) const
{
    CorrectionResult res;
    res.success           = true;
    res.recommendedAction = CorrectionResult::Action::AdjustTilt;

    // Estimate reach from collision point Z (assume part surface is at Z=0)
    double reachMm = std::fabs(req.collisionPoint.z);
    if (reachMm < 1.0) reachMm = 20.0; // fallback

    // Holder diameter: approximate as 2× tool diameter if unknown
    double holderDia = req.tool.diameter * 2.0;

    double newTilt = computeTiltAngle(holderDia, reachMm, req.requiredClearanceMm);

    // Tilt at least 1 degree more than current
    if (newTilt <= req.currentTiltDeg)
        newTilt = req.currentTiltDeg + 3.0;

    res.suggestedTiltDeg = newTilt;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "Holder collision at Z=" << req.collisionPoint.z << " mm. "
        << "Increasing tilt from " << req.currentTiltDeg << "° to "
        << newTilt << "° provides " << req.requiredClearanceMm << " mm clearance. "
        << "Re-verify after applying.";
    res.description = oss.str();

    // Advisory: if reach is very long, suggest extended-reach holder
    if (reachMm > 3.0 * req.tool.diameter) {
        res.warningMessage = "Reach exceeds 3×D. Consider an extended-reach "
                             "or stub holder to reduce tilt angle.";
    }

    return res;
}

// --------------------------------------------------------------------------
// solveGouge
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::solveGouge(const CorrectionRequest& req) const {
    CorrectionResult res;
    res.success = true;

    // Primary fix: increase stock allowance so the tool leaves material
    // that will be removed by a final finishing pass.
    double stockFix = req.params.stockAllowance + 0.127; // add 0.127 mm (0.005")
    res.suggestedStockMm   = stockFix;
    res.suggestedDepthMm   = req.params.axialDepth * 0.90; // reduce depth 10%
    res.recommendedAction  = CorrectionResult::Action::AddStockAllowance;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3)
        << "Gouge detected. "
        << "Suggested fixes: "
        << "(1) Increase stock allowance from " << req.params.stockAllowance
        << " mm to " << stockFix << " mm for the roughing pass; "
        << "(2) Reduce axial depth from " << req.params.axialDepth
        << " mm to " << res.suggestedDepthMm << " mm. "
        << "Add a dedicated finishing operation at 0 mm stock to achieve final geometry.";
    res.description = oss.str();

    return res;
}

// --------------------------------------------------------------------------
// solveRapidGouge
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::solveRapidGouge(
    const CorrectionRequest& req) const
{
    CorrectionResult res;
    res.success           = true;
    res.recommendedAction = CorrectionResult::Action::InsertRetract;
    res.needsRetract      = true;

    // Safe retract point: above the collision point by requiredClearanceMm
    res.safeRetractPoint = {
        req.collisionPoint.x,
        req.collisionPoint.y,
        req.collisionPoint.z + req.requiredClearanceMm + 5.0
    };

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "Rapid traverse crossed the part at ("
        << req.collisionPoint.x << ", "
        << req.collisionPoint.y << ", "
        << req.collisionPoint.z << ") mm. "
        << "Insert a G00 retract to Z=" << res.safeRetractPoint.z
        << " before this move.";
    res.description = oss.str();
    return res;
}

// --------------------------------------------------------------------------
// solveUndercut
// --------------------------------------------------------------------------

CorrectionResult SelfCorrectionLoop::solveUndercut(
    const CorrectionRequest& req) const
{
    CorrectionResult res;
    res.success           = true;
    res.recommendedAction = CorrectionResult::Action::AdjustTilt;

    // Undercutting requires a positive tilt to access the overhang
    double suggestedTilt = req.currentTiltDeg + 5.0; // add 5 degrees
    suggestedTilt = std::min(suggestedTilt, 45.0);   // cap at 45°

    res.suggestedTiltDeg = suggestedTilt;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "Undercut detected. Material remains under overhang at Z="
        << req.collisionPoint.z << " mm. "
        << "Increase tool tilt to " << suggestedTilt
        << "° to access the undercut region. "
        << "A 5-axis simultaneous move may be required.";
    res.description = oss.str();

    res.warningMessage = "Ensure the 5-axis machine envelope supports this tilt.";
    return res;
}
