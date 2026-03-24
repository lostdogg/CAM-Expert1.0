#include "MillTurn.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <map>

// --------------------------------------------------------------------------
// SyncManager
// --------------------------------------------------------------------------
void SyncManager::addOperation(TurretOperation op) {
    m_ops.push_back(std::move(op));
}

void SyncManager::addSyncPoint(SyncPoint sp) {
    m_syncs.push_back(std::move(sp));
}

// --------------------------------------------------------------------------
// Generate per-channel G-code
//
// Each turret (channel) gets its own code block.  Sync wait codes are
// inserted where a turret must pause for another:
//
//   Controller side A (upper turret):
//     ... operations ...
//     M100        ← wait for turret B to emit M100
//     ... more operations ...
//
//   Controller side B (lower turret):
//     ... operations ...
//     M100        ← emit "I'm done" to unblock A
// --------------------------------------------------------------------------
std::vector<std::string> SyncManager::generateChannelCode(
    const std::string& syncMCode) const {

    // Find max turret index
    int maxTurret = 0;
    for (const auto& op : m_ops)
        if (op.turretIndex > maxTurret) maxTurret = op.turretIndex;

    std::vector<std::ostringstream> channels(static_cast<std::size_t>(maxTurret + 1));

    for (const auto& op : m_ops) {
        auto& ch = channels[static_cast<std::size_t>(op.turretIndex)];

        // Insert wait code before this operation if required
        if (op.syncAfter >= 0) {
            ch << "( Wait for sync " << op.syncAfter << " )\n";
            ch << syncMCode << op.syncAfter << "\n";
        }

        if (!op.comment.empty())
            ch << "( " << op.comment << " )\n";

        // Embed a compact summary of the toolpath points
        for (const auto& pt : op.toolpath.points()) {
            ch << std::fixed << std::setprecision(3);
            if (pt.motion == MotionType::Rapid)
                ch << "G00 ";
            else if (pt.motion == MotionType::Linear)
                ch << "G01 ";
            else if (pt.motion == MotionType::PlungeFeed)
                ch << "G01 ";
            else if (pt.motion == MotionType::Retract ||
                     pt.motion == MotionType::MicroLift)
                ch << "G00 ";
            else
                ch << "G01 ";

            ch << "X" << pt.position.x
               << " Y" << pt.position.y
               << " Z" << pt.position.z << "\n";
        }

        // Emit sync code after this operation if required
        if (op.syncEmit >= 0) {
            ch << "( Emit sync " << op.syncEmit << " )\n";
            ch << syncMCode << op.syncEmit << "\n";
        }
    }

    std::vector<std::string> result;
    for (auto& ch : channels)
        result.push_back(ch.str());
    return result;
}

// --------------------------------------------------------------------------
// Inter-turret collision check
//
// For every pair of operations (a, b) that could run concurrently (different
// turrets, no sync dependency between them), check the minimum distance
// between any point on turret a and any point on turret b.
//
// D_collision = min(||P_T1 - P_T2||)
// If D_collision < safetyGap → Critical Interference
// --------------------------------------------------------------------------
TurretCollisionResult SyncManager::checkCollisions(double safetyGap) const {
    TurretCollisionResult result;
    result.minDistance = 1e9;

    for (std::size_t a = 0; a < m_ops.size(); ++a) {
        for (std::size_t b = a + 1; b < m_ops.size(); ++b) {
            if (m_ops[a].turretIndex == m_ops[b].turretIndex) continue;

            const auto& ptsA = m_ops[a].toolpath.points();
            const auto& ptsB = m_ops[b].toolpath.points();

            for (std::size_t i = 0; i < ptsA.size(); ++i) {
                for (std::size_t j = 0; j < ptsB.size(); ++j) {
                    double dx = ptsA[i].position.x - ptsB[j].position.x;
                    double dy = ptsA[i].position.y - ptsB[j].position.y;
                    double dz = ptsA[i].position.z - ptsB[j].position.z;
                    double d  = std::sqrt(dx*dx + dy*dy + dz*dz);

                    if (d < result.minDistance) {
                        result.minDistance = d;
                        result.turret1Idx  = m_ops[a].turretIndex;
                        result.turret2Idx  = m_ops[b].turretIndex;
                        result.moveIdx     = static_cast<int>(i);
                    }
                    if (d < safetyGap) {
                        result.criticalInterference = true;
                        result.description =
                            "Critical Interference: turret " +
                            std::to_string(m_ops[a].turretIndex) +
                            " and turret " +
                            std::to_string(m_ops[b].turretIndex) +
                            " are " + std::to_string(d) +
                            " mm apart (limit " +
                            std::to_string(safetyGap) + " mm)";
                        return result; // stop at first critical hit
                    }
                }
            }
        }
    }
    return result;
}

// --------------------------------------------------------------------------
// MillTurn::pinchTurn
//
// Leader tool cuts from +X side at the given radius.
// Follower tool cuts from -X side (offset) at the same radius.
// Both move in Z together to prevent part deflection.
// --------------------------------------------------------------------------
std::vector<Toolpath> MillTurn::pinchTurn(
    const TurningParams& p,
    const CuttingTool& leaderTool,
    const CuttingTool& followerTool,
    const CuttingParams& cuts) {

    // Leader: standard rough turning
    Toolpath leader = Turning::roughTurn(p, leaderTool, cuts);
    leader.setName("PinchTurn Leader (T" + std::to_string(leaderTool.id) + ")");

    // Follower: same geometry but mirrored in X (opposite side of part)
    // The follower keeps a small axial lead offset so both tools don't
    // occupy the exact same XYZ simultaneously.
    TurningParams followerP = p;
    followerP.safeRadius *= -1.0; // approach from opposite side (conceptually)
    Toolpath follower = Turning::roughTurn(followerP, followerTool, cuts);
    follower.setName("PinchTurn Follower (T" + std::to_string(followerTool.id) + ")");

    // Mirror follower toolpath: negate Y to put tool on opposite side
    follower.clearPoints();
    for (const auto& src : leader.points()) {
        ToolpathPoint pt = src;  // explicit copy – we need to modify Y
        pt.position.y = -pt.position.y;
        pt.toolAxis.y = -pt.toolAxis.y;
        follower.addPoint(pt);
    }
    follower.markClean();

    return {leader, follower};
}

// --------------------------------------------------------------------------
// MillTurn::balancedTurn
//
// Tool A cuts from +X; Tool B (same spec) cuts from -X at the same Z.
// The radial forces cancel each other, allowing heavy cuts without deflection.
// --------------------------------------------------------------------------
std::vector<Toolpath> MillTurn::balancedTurn(
    const TurningParams& p,
    const CuttingTool& tool,
    const CuttingParams& cuts) {

    Toolpath side1 = Turning::roughTurn(p, tool, cuts);
    side1.setName("BalancedTurn +X Side");

    Toolpath side2(StrategyType::RoughTurning, tool, cuts);
    side2.setName("BalancedTurn -X Side");

    // Mirror all points through the Z axis (negate Y)
    for (const auto& src : side1.points()) {
        ToolpathPoint pt = src;  // explicit copy – we need to modify Y
        pt.position.y = -pt.position.y;
        pt.toolAxis.y = -pt.toolAxis.y;
        side2.addPoint(pt);
    }
    side2.markClean();

    return {side1, side2};
}

// --------------------------------------------------------------------------
// MillTurn::pickoffAndCutoff
//
// Phase 1: sub-spindle transfer (approach + grab)
// Phase 2: cutoff groove
// Phase 3: sub-spindle retract with finished part
// --------------------------------------------------------------------------
std::vector<Toolpath> MillTurn::pickoffAndCutoff(
    double transferZ,
    double cutoffZ,
    double spindleRPM,
    const CuttingTool& cutoffTool,
    const CuttingParams& cuts) {

    // ---- Phase 1: sub-spindle approach and transfer ----
    Toolpath subApproach = Turning::subSpindleTransfer(transferZ, spindleRPM, cutoffTool);
    subApproach.setName("Sub-Spindle Approach / Grab");

    // ---- Phase 2: cutoff groove ----
    GrooveParams gp;
    gp.centreDiameter = cuts.radialDepth * 2.0 + 10.0; // approximate OD
    gp.width          = cutoffTool.diameter;
    gp.depth          = gp.centreDiameter / 2.0;
    gp.feedPerRev     = cuts.feedPerTooth;
    gp.peckDepth      = 0.5;

    Toolpath cutoff = Turning::groove(gp, cutoffTool, cuts);
    cutoff.setName("Cutoff from Bar Stock");

    // Override Z position to cutoffZ
    cutoff.clearPoints();
    GrooveParams co = gp;
    double targetR = co.centreDiameter * 0.5 - co.depth;
    double startR  = co.centreDiameter * 0.5;

    ToolpathPoint rapid;
    rapid.position = {cutoffZ, startR + 5.0, 0};
    rapid.toolAxis = {1, 0, 0};
    rapid.motion   = MotionType::Rapid;
    cutoff.addPoint(rapid);

    double currentR = startR;
    while (currentR > targetR) {
        currentR -= gp.peckDepth;
        if (currentR < targetR) currentR = targetR;
        ToolpathPoint peck;
        peck.position = {cutoffZ, currentR, 0};
        peck.toolAxis = {1, 0, 0};
        peck.motion   = MotionType::PlungeFeed;
        cutoff.addPoint(peck);

        ToolpathPoint ret;
        ret.position = {cutoffZ, startR, 0};
        ret.toolAxis = {1, 0, 0};
        ret.motion   = MotionType::Rapid;
        cutoff.addPoint(ret);
    }
    cutoff.markClean();

    // ---- Phase 3: sub-spindle retract with part ----
    Toolpath subRetract(StrategyType::Custom, cutoffTool, cuts);
    subRetract.setName("Sub-Spindle Retract with Part");

    ToolpathPoint retractStart;
    retractStart.position = {transferZ, 0, 0};
    retractStart.toolAxis = {1, 0, 0};
    retractStart.motion   = MotionType::Rapid;
    subRetract.addPoint(retractStart);

    ToolpathPoint retractEnd;
    retractEnd.position = {transferZ - 200.0, 0, 0}; // retract 200 mm
    retractEnd.toolAxis = {1, 0, 0};
    retractEnd.motion   = MotionType::Linear;
    subRetract.addPoint(retractEnd);
    subRetract.markClean();

    return {subApproach, cutoff, subRetract};
}

// --------------------------------------------------------------------------
// MillTurn::buildSchedule
//
// Automatically inserts sync wait codes so that:
//  • Operations on the same turret run sequentially (safe).
//  • Operations on different turrets run concurrently where possible.
//  • A sync is emitted when one turret finishes and another needs to wait.
// --------------------------------------------------------------------------
SyncManager MillTurn::buildSchedule(std::vector<TurretOperation> ops,
                                     int startSyncID) {
    SyncManager mgr;

    // Track the "last sync emitted" per turret so dependent turrets can wait
    std::map<int, int> lastSyncPerTurret;
    int nextSync = startSyncID;

    for (std::size_t i = 0; i < ops.size(); ++i) {
        auto& op = ops[i];

        // Check if any previous operation on a *different* turret must
        // complete before this one can start.  For simplicity we sync
        // every inter-turret boundary.
        for (std::size_t j = 0; j < i; ++j) {
            if (ops[j].turretIndex != op.turretIndex) {
                // ops[j] must complete before ops[i] if ops[j] was the last
                // op on its turret
                bool isLastOnTurret = true;
                for (std::size_t k = j + 1; k < i; ++k) {
                    if (ops[k].turretIndex == ops[j].turretIndex) {
                        isLastOnTurret = false;
                        break;
                    }
                }
                if (isLastOnTurret && ops[j].syncEmit < 0) {
                    ops[j].syncEmit = nextSync;
                    op.syncAfter    = nextSync;
                    SyncPoint sp;
                    sp.syncID       = nextSync;
                    sp.turretIdx    = op.turretIndex;
                    sp.waitForTurret= ops[j].turretIndex;
                    sp.description  = "Turret " + std::to_string(op.turretIndex) +
                                      " waits for turret " +
                                      std::to_string(ops[j].turretIndex);
                    mgr.addSyncPoint(sp);
                    ++nextSync;
                }
            }
        }

        mgr.addOperation(op);
    }

    return mgr;
}
