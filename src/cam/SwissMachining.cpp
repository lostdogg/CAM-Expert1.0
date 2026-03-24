#include "SwissMachining.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

static constexpr double PI_SW = 3.14159265358979323846;

// --------------------------------------------------------------------------
SwissMachining::SwissMachining(SwissParams params)
    : m_params(std::move(params)) {}

// --------------------------------------------------------------------------
// buildPass – internal helper
//
// Converts the (Z_part, X_radius) profile into machine-coordinate
// ToolpathPoints using Swiss sliding-headstock semantics:
//
//   Machine Z = guideBushingZ − part_Z   (headstock slides in −Z direction
//                                          as stock is fed through the bushing)
//   Machine X = xSign × (radius + stockAllowance)
//
// xSign = +1.0 for the leader tool (+X side),
//         −1.0 for the follower tool (−X side, pinch mode).
// --------------------------------------------------------------------------
Toolpath SwissMachining::buildPass(const std::vector<Geom::Vec2>& profile,
                                    const CuttingTool& tool,
                                    const CuttingParams& cuts,
                                    double xSign,
                                    double zOffset,
                                    StrategyType strat,
                                    const std::string& name) const {
    Toolpath tp(strat, tool, cuts);
    tp.setName(name);

    if (profile.empty()) { tp.markClean(); return tp; }

    double safeR = m_params.barDiameter * 0.5 + m_params.safeRadialClearance;

    // Rapid to safe position above first profile point
    {
        ToolpathPoint rapid;
        // Machine Z = guideBushingZ − partZ
        double machZ = m_params.guideBushingZ - profile[0].x + zOffset;
        rapid.position = {xSign * safeR, 0.0, machZ};
        rapid.toolAxis = {0.0, 0.0, 1.0};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);
    }

    // Feed into the first cut point
    {
        ToolpathPoint plunge;
        double machZ = m_params.guideBushingZ - profile[0].x + zOffset;
        double machX = xSign * profile[0].y; // profile.y = radius
        plunge.position = {machX, 0.0, machZ};
        plunge.toolAxis = {0.0, 0.0, 1.0};
        plunge.motion   = MotionType::PlungeFeed;
        tp.addPoint(plunge);
    }

    // Follow the profile (The Cut)
    for (std::size_t i = 1; i < profile.size(); ++i) {
        ToolpathPoint pt;
        double machZ = m_params.guideBushingZ - profile[i].x + zOffset;
        double machX = xSign * profile[i].y;
        pt.position = {machX, 0.0, machZ};
        pt.toolAxis = {0.0, 0.0, 1.0};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    // Retract to safe radius
    {
        ToolpathPoint ret;
        ret.position   = tp.points().back().position;
        ret.position.x = xSign * safeR;
        ret.toolAxis   = {0.0, 0.0, 1.0};
        ret.motion     = MotionType::Retract;
        tp.addPoint(ret);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// slidingHeadstock()
//
// Generates a single-tool sliding-headstock turning pass.  The toolpath
// name is prefixed with "SWISS-SLIDE" so downstream post-processors can
// recognise and apply the inverted Z convention.
// --------------------------------------------------------------------------
Toolpath SwissMachining::slidingHeadstock(const std::vector<Geom::Vec2>& profile,
                                           const CuttingTool& tool,
                                           const CuttingParams& cuts) const {
    std::ostringstream nameStream;
    nameStream << "SWISS-SLIDE (bar Ø" << m_params.barDiameter
               << " → Ø" << m_params.finishedDiameter
               << ", L=" << m_params.partLength << "mm)";

    return buildPass(profile, tool, cuts,
                     /*xSign=*/+1.0,
                     /*zOffset=*/0.0,
                     StrategyType::SwissSliding,
                     nameStream.str());
}

// --------------------------------------------------------------------------
// pinchSync()
//
// Generates a synchronised two-tool (leader + follower) toolpath pair.
// The follower is offset by pinchLeadOffset in Z and mirrored in X so the
// radial cutting forces on the slender part cancel precisely.
//
// Force balance:
//   F_leader = −F_follower   (opposite X, same Z magnitude)
//   Net deflection ≈ 0
// --------------------------------------------------------------------------
SwissToolpathPair SwissMachining::pinchSync(
    const std::vector<Geom::Vec2>& profile,
    const CuttingTool& leaderTool,
    const CuttingTool& followerTool,
    const CuttingParams& cuts) const {

    // Leader: +X side, no Z offset
    std::ostringstream lName;
    lName << "SWISS-PINCH Leader T" << leaderTool.id
          << " (Ø" << m_params.finishedDiameter << ")";

    Toolpath leader = buildPass(profile, leaderTool, cuts,
                                 /*xSign=*/+1.0,
                                 /*zOffset=*/0.0,
                                 StrategyType::SwissPinchSync,
                                 lName.str());

    // Follower: −X side, small Z lead offset to avoid chip coincidence
    std::ostringstream fName;
    fName << "SWISS-PINCH Follower T" << followerTool.id
          << " (+Δz=" << m_params.pinchLeadOffset << "mm)";

    Toolpath follower = buildPass(profile, followerTool, cuts,
                                   /*xSign=*/-1.0,
                                   /*zOffset=*/m_params.pinchLeadOffset,
                                   StrategyType::SwissPinchSync,
                                   fName.str());

    return {std::move(leader), std::move(follower)};
}

// --------------------------------------------------------------------------
// computeWhipLimit()
//
// Uses Euler–Bernoulli beam theory to find the maximum unsupported bar
// length before deflection under the radial cutting force exceeds the
// allowable limit:
//
//   δ = F·L³ / (3·E·I)
//   L_max = ∛(3·E·I·δ_max / F)
//
// I = π/64 · d⁴   for a solid circular cross-section.
// --------------------------------------------------------------------------
double SwissMachining::computeWhipLimit(double finishedDiameterMm,
                                         double radialCuttingForceN,
                                         double youngsModulusGPa,
                                         double maxDeflectionMm) {
    if (radialCuttingForceN <= 0.0) return 1e9; // no force → infinite limit

    double d_m    = finishedDiameterMm * 1e-3;           // m
    double E_Pa   = youngsModulusGPa   * 1e9;             // Pa
    double delta_m= maxDeflectionMm   * 1e-3;             // m

    // Second moment of area: I = π/64 · d⁴
    double I = (PI_SW / 64.0) * std::pow(d_m, 4.0);

    // L_max = ∛(3·E·I·δ / F)
    double L_m = std::cbrt(3.0 * E_Pa * I * delta_m / radialCuttingForceN);

    return L_m * 1000.0; // convert back to mm
}
