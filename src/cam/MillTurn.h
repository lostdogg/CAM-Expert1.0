#pragma once
#ifndef MILL_TURN_H
#define MILL_TURN_H

#include "Toolpath.h"
#include "Turning.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// MillTurn – Multi-Turret / Mill-Turn Synchronization Logic
//
// In a high-end Mill-Turn environment multiple turrets (and optionally a
// sub-spindle) operate simultaneously.  Coordinating them requires:
//
//  1. SyncManager  – a "timeline" view of all turret operations.
//     Sync (wait) codes (e.g. M100) ensure that Turret A pauses until
//     Turret B has completed a prerequisite operation before proceeding.
//
//  2. Pinch Turning – two tools cut the same diameter at once.
//     One leads while the other follows, preventing part deflection under
//     heavy cutting pressure.
//
//  3. Balanced Turning – two tools cut on opposite sides of the part at
//     the same Z position, cancelling cutting forces entirely.
//
//  4. Pickoff / Cutoff – the sub-spindle advances, grabs the spinning part,
//     the cutoff tool severs it from bar stock, and the sub-spindle retracts.
//
//  5. Collision-aware simulation – distances between all moving turrets are
//     checked continuously.  If D_collision = min(||P_T1 - P_T2||) < safety
//     threshold, a "Critical Interference" error is flagged.
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// SyncPoint – a wait-code insertion between turret operations
// --------------------------------------------------------------------------
struct SyncPoint {
    int    syncID    = 100;   // M-code number (e.g. M100)
    int    turretIdx = 0;     // which turret waits
    int    waitForTurret = 1; // which turret must complete first
    std::string description;
};

// --------------------------------------------------------------------------
// TurretOperation – one machining step assigned to a specific turret
// --------------------------------------------------------------------------
struct TurretOperation {
    int       turretIndex = 0;    // 0 = upper, 1 = lower, 2 = sub-spindle
    Toolpath  toolpath;
    int       syncAfter   = -1;   // syncID to wait before this op (-1 = none)
    int       syncEmit    = -1;   // syncID to emit when this op finishes (-1 = none)

    std::string comment;
};

// --------------------------------------------------------------------------
// CollisionCheckResult – output of inter-turret distance check
// --------------------------------------------------------------------------
struct TurretCollisionResult {
    bool   criticalInterference = false;
    double minDistance          = 1e9;
    int    turret1Idx           = -1;
    int    turret2Idx           = -1;
    int    moveIdx              = -1;
    std::string description;
};

// --------------------------------------------------------------------------
// SyncManager – timeline / schedule for all turret operations
// --------------------------------------------------------------------------
class SyncManager {
public:
    // Add an operation to the schedule
    void addOperation(TurretOperation op);

    // Insert a synchronization point between two turrets
    void addSyncPoint(SyncPoint sp);

    // Total number of operations
    int operationCount() const { return static_cast<int>(m_ops.size()); }

    // Access operations
    const std::vector<TurretOperation>& operations() const { return m_ops; }
    std::vector<TurretOperation>&       operations()       { return m_ops; }

    const std::vector<SyncPoint>& syncPoints() const { return m_syncs; }

    // Generate G-code for all turrets, interleaving sync wait codes.
    // Each turret's code is returned separately so it can be loaded into
    // its respective channel on the controller.
    // The 'syncMCode' prefix is prepended to each syncID (e.g. "M" → "M100")
    std::vector<std::string> generateChannelCode(const std::string& syncMCode = "M") const;

    // Check for inter-turret collisions across the entire schedule.
    // safetyGap: minimum allowed distance between any two turret positions (mm).
    TurretCollisionResult checkCollisions(double safetyGap = 10.0) const;

    void clear() { m_ops.clear(); m_syncs.clear(); }

private:
    std::vector<TurretOperation> m_ops;
    std::vector<SyncPoint>       m_syncs;
};

// --------------------------------------------------------------------------
// MillTurn – high-level factory for multi-turret operations
// --------------------------------------------------------------------------
class MillTurn {
public:
    // ---- Pinch Turning ----
    // Two tools cut the same diameter simultaneously.
    // turret0Tool cuts at the given radius; turret1Tool follows at the same
    // radius but on the opposite side.  The offset prevents part deflection.
    //
    // Returns TWO toolpaths: index 0 = leader, index 1 = follower.
    static std::vector<Toolpath> pinchTurn(const TurningParams& p,
                                            const CuttingTool& leaderTool,
                                            const CuttingTool& followerTool,
                                            const CuttingParams& cuts);

    // ---- Balanced Turning ----
    // Two tools cut at exactly the same Z depth on OPPOSITE sides of the
    // part, cancelling radial cutting forces entirely.
    //
    // Returns TWO toolpaths: index 0 = +X side, index 1 = -X side.
    static std::vector<Toolpath> balancedTurn(const TurningParams& p,
                                               const CuttingTool& tool,
                                               const CuttingParams& cuts);

    // ---- Pickoff and Cutoff ----
    // 1. Sub-spindle accelerates to match main spindle speed.
    // 2. Sub-spindle advances to transferZ and grabs the part.
    // 3. Cutoff tool severs bar stock at cutoffZ.
    // 4. Sub-spindle retracts with the finished part.
    //
    // Returns THREE toolpaths:
    //   [0] = sub-spindle transfer / approach
    //   [1] = cutoff tool path
    //   [2] = sub-spindle retract
    static std::vector<Toolpath> pickoffAndCutoff(
        double transferZ,      // Z position where sub-spindle grabs part
        double cutoffZ,        // Z position of the cutoff groove
        double spindleRPM,
        const CuttingTool& cutoffTool,
        const CuttingParams& cuts);

    // ---- Build a complete SyncManager schedule ----
    // Automatically assigns sync wait codes between a list of operations,
    // ensuring operations on the same turret execute sequentially while
    // allowing concurrent execution across turrets where safe.
    static SyncManager buildSchedule(std::vector<TurretOperation> ops,
                                      int startSyncID = 100);
};

#endif // MILL_TURN_H
