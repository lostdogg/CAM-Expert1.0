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
};

struct CollisionResult {
    bool   hasCollision      = false;
    bool   hasOverTravel     = false;
    int    collisionMoveIdx  = -1;
    int    overTravelMoveIdx = -1;
    std::string description;
};

class MachineSimulation {
public:
    explicit MachineSimulation(MachineModel model = MachineModel::threeAxis());

    // Run the full simulation
    CollisionResult run(const ToolpathManager* mgr);

    // Check a single tool-axis vector against machine limits
    bool checkKinematics(const Geom::Vec3& toolAxis,
                          double& aAngle, double& bAngle) const;

    const MachineModel& machineModel() const { return m_model; }
    void setMachineModel(MachineModel m) { m_model = std::move(m); }

    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback cb) { m_progress = std::move(cb); }

private:
    bool checkOverTravel(const ToolpathPoint& pt, int& axisIdx) const;
    bool checkCollision(const ToolpathPoint& pt,
                         const CuttingTool& tool) const;

    MachineModel     m_model;
    ProgressCallback m_progress;
};

#endif // MACHINE_SIMULATION_H
