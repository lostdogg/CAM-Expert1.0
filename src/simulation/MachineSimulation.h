#pragma once
#ifndef MACHINE_SIMULATION_H
#define MACHINE_SIMULATION_H

#include "../cam/MultiAxis.h"
#include "../managers/ToolpathManager.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// MachineSimulation
//
// Renders the full kinematic machine tool in 3-D space to:
//   • Detect physical collisions between spindle, table, and fixture
//   • Check that all axis movements remain within travel limits
//   • Verify the tool holder / shank doesn't collide with the part
//   • Identify machine over-travel errors before the part is cut
//
// The machine is described by a kinematic chain of rigid bodies (linked
// by joints). Each body has a geometric mesh for collision detection.
// --------------------------------------------------------------------------

struct MachineComponent {
    std::string name;
    Geom::Vec3  origin;
    Geom::Vec3  axisDir;         // joint rotation/translation axis
    double      minTravel = -1000;
    double      maxTravel =  1000;
    // In production this would hold a mesh for the component body
};

struct MachineModel {
    std::string               name;
    MachineKinematics         kinematics;
    std::vector<MachineComponent> components;

    // Pre-built machine configurations
    static MachineModel threeAxis();
    static MachineModel fourAxisHorizontal();
    static MachineModel fiveAxisHeadTable();
    static MachineModel fiveAxisTableTable();   // e.g. Fanuc 30iA tilting rotary table
    static MachineModel fiveAxisHeadHead();     // e.g. Hermle C600 dual-head
};

struct CollisionResult {
    bool   hasCollision      = false;
    bool   hasOverTravel     = false;
    int    collisionMoveIdx  = -1;
    int    overTravelMoveIdx = -1;
    std::string description;
};

// --------------------------------------------------------------------------
// TiltAvoidanceResult – outcome of one automatic-tilt calculation.
//
// When the holder approaches within clearanceDistance of the part the
// simulation dynamically tilts the tool axis to maintain clearance.
// --------------------------------------------------------------------------
struct TiltAvoidanceResult {
    bool       tiltApplied    = false;  // true if the axis was modified
    Geom::Vec3 originalAxis;            // tool axis before avoidance
    Geom::Vec3 adjustedAxis;            // tool axis after avoidance tilt
    double     tiltAngleDeg   = 0.0;   // magnitude of applied tilt (degrees)
    // Side-tilt or lead/lag applied
    enum class TiltType { None, SideTilt, LeadLag } tiltType = TiltType::None;
};

// --------------------------------------------------------------------------
// CollisionControlParams – configure the automatic tilt-avoidance logic.
//
// Matches the "Tilt Page" in real CAM software (Mastercam/Hypermill style).
// --------------------------------------------------------------------------
struct CollisionControlParams {
    // Distance (mm) between the holder envelope and the part that triggers
    // the avoidance logic. e.g. 1.27 mm ≈ 0.050".
    double clearanceDistance  = 1.27;

    // Maximum tilt angle the software is allowed to apply (degrees).
    double maxTiltAngleDeg    = 45.0;

    // Smoothing radius (mm): the arc over which the tilt transition is spread
    // to prevent abrupt "jerking" motion.
    double smoothingRadius    = 5.0;

    // Preferred tilt direction when avoiding holder collisions.
    enum class TiltStrategy { SideTilt, LeadLag, Auto } strategy = TiltStrategy::Auto;
};

class MachineSimulation {
public:
    explicit MachineSimulation(MachineModel model = MachineModel::threeAxis());

    // Run the full simulation
    CollisionResult run(const ToolpathManager* mgr);

    // Check a single tool-axis vector against machine limits
    bool checkKinematics(const Geom::Vec3& toolAxis,
                          double& aAngle, double& bAngle) const;

    // 5-axis automatic tilt avoidance:
    // Checks whether the holder at 'pt' comes within clearanceDistance of any
    // machine component / fixture.  If so, computes a new tool axis that
    // leans the holder away from the obstruction.
    TiltAvoidanceResult computeTiltAvoidance(
        const ToolpathPoint& pt,
        const CuttingTool&   tool,
        const CollisionControlParams& params = CollisionControlParams{}) const;

    // Apply smoothed tilt avoidance to every point of a toolpath, spreading
    // the transition over params.smoothingRadius.
    void applyTiltAvoidance(
        Toolpath& tp,
        const CollisionControlParams& params = CollisionControlParams{}) const;

    const MachineModel& machineModel() const { return m_model; }
    void setMachineModel(MachineModel m) { m_model = std::move(m); }

    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback cb) { m_progress = std::move(cb); }

private:
    bool checkOverTravel(const ToolpathPoint& pt, int& axisIdx) const;
    bool checkCollision(const ToolpathPoint& pt,
                         const CuttingTool& tool) const;

    // Distance from the holder envelope to the nearest obstacle at 'pt'.
    // Returns a large number when there is no nearby obstacle.
    double holderClearance(const ToolpathPoint& pt,
                            const CuttingTool& tool) const;

    MachineModel     m_model;
    ProgressCallback m_progress;
};

#endif // MACHINE_SIMULATION_H
