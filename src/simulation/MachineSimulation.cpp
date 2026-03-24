#include "MachineSimulation.h"
#include "../cad/Geometry.h"
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// Pre-built machine configurations
// --------------------------------------------------------------------------
MachineModel MachineModel::threeAxis() {
    MachineModel m;
    m.name = "3-Axis VMC";
    m.kinematics.type    = MachineKinematics::Type::Head_Table;
    m.kinematics.aAxisMin =  0; m.kinematics.aAxisMax = 0;
    m.kinematics.bAxisMin =  0; m.kinematics.bAxisMax = 0;

    MachineComponent x{"X Axis", {0,0,0}, {1,0,0}, -500, 500};
    MachineComponent y{"Y Axis", {0,0,0}, {0,1,0}, -400, 400};
    MachineComponent z{"Z Axis", {0,0,0}, {0,0,1}, -300,   0};
    m.components = {x, y, z};
    return m;
}

MachineModel MachineModel::fourAxisHorizontal() {
    MachineModel m;
    m.name = "4-Axis HMC";
    m.kinematics.type    = MachineKinematics::Type::Head_Table;
    m.kinematics.aAxisMin = -360; m.kinematics.aAxisMax = 360;

    MachineComponent x{"X Axis", {0,0,0}, {1,0,0}, -600, 600};
    MachineComponent y{"Y Axis", {0,0,0}, {0,1,0}, -500, 500};
    MachineComponent z{"Z Axis", {0,0,0}, {0,0,1}, -400,   0};
    MachineComponent a{"A Axis", {0,0,0}, {1,0,0}, -360, 360};
    m.components = {x, y, z, a};
    return m;
}

MachineModel MachineModel::fiveAxisHeadTable() {
    MachineModel m;
    m.name = "5-Axis Head/Table";
    m.kinematics.type     = MachineKinematics::Type::Head_Table;
    m.kinematics.aAxisMin = -120; m.kinematics.aAxisMax = 120;
    m.kinematics.bAxisMin =  -90; m.kinematics.bAxisMax =  90;

    MachineComponent x{"X Axis", {0,0,0}, {1,0,0}, -700, 700};
    MachineComponent y{"Y Axis", {0,0,0}, {0,1,0}, -600, 600};
    MachineComponent z{"Z Axis", {0,0,0}, {0,0,1}, -500,   0};
    MachineComponent a{"A Axis", {0,0,0}, {1,0,0}, -120, 120};
    MachineComponent b{"B Axis", {0,0,0}, {0,1,0},  -90,  90};
    m.components = {x, y, z, a, b};
    return m;
}

// --------------------------------------------------------------------------
MachineSimulation::MachineSimulation(MachineModel model)
    : m_model(std::move(model)) {}

// --------------------------------------------------------------------------
bool MachineSimulation::checkKinematics(const Geom::Vec3& toolAxis,
                                          double& aAngle,
                                          double& bAngle) const {
    return MultiAxis::inverseKinematics(toolAxis, m_model.kinematics,
                                         aAngle, bAngle);
}

// --------------------------------------------------------------------------
bool MachineSimulation::checkOverTravel(const ToolpathPoint& pt,
                                          int& /*axisIdx*/) const {
    // Check linear axes
    for (const auto& comp : m_model.components) {
        if (comp.name == "X Axis" &&
            (pt.position.x < comp.minTravel || pt.position.x > comp.maxTravel))
            return true;
        if (comp.name == "Y Axis" &&
            (pt.position.y < comp.minTravel || pt.position.y > comp.maxTravel))
            return true;
        if (comp.name == "Z Axis" &&
            (pt.position.z < comp.minTravel || pt.position.z > comp.maxTravel))
            return true;
    }
    // Check rotary if tool axis is not trivial
    Geom::Vec3 ax = pt.toolAxis;
    if (ax.length() > 0.5) {
        double a, b;
        if (!MultiAxis::inverseKinematics(ax, m_model.kinematics, a, b))
            return true; // out of kinematic range
    }
    return false;
}

// --------------------------------------------------------------------------
// Collision-detection tunables
static constexpr double kShankDiameterMultiplier = 0.75; // shank half-diameter = cutter_dia * this
static constexpr double kFixtureHalfExtentXY     = 25.0; // mm – assumed fixture footprint half-size
static constexpr double kFixtureDepth            = 100.0; // mm – assumed fixture block height

// --------------------------------------------------------------------------
bool MachineSimulation::checkCollision(const ToolpathPoint& pt,
                                        const CuttingTool& tool) const {
    // Approximate the tool holder as an AABB above the tool tip.
    // Holder is modelled as a cylinder from tool tip + flute length up to
    // tool tip + overall length, with a shank diameter = 1.5 × cutter diameter.
    double shankRadius  = tool.diameter * kShankDiameterMultiplier;
    double fluteLen     = tool.fluteLength;
    double overallLen   = tool.overallLength;

    // Tool tip position
    const Geom::Vec3& tip = pt.position;

    // Tool axis unit vector (default to +Z for 3-axis)
    Geom::Vec3 axis = pt.toolAxis;
    if (axis.length() < 0.5) axis = {0, 0, 1};
    axis = axis.normalized();

    // Holder AABB: from (tip + flute*axis) to (tip + overall*axis) +/- shankRadius
    Geom::Vec3 holderBase = tip + axis * fluteLen;
    Geom::Vec3 holderTop  = tip + axis * overallLen;

    Geom::AABB holder;
    holder.expand(holderBase);
    holder.expand(holderTop);
    // Inflate by shank radius in X and Y directions
    holder.min.x -= shankRadius; holder.max.x += shankRadius;
    holder.min.y -= shankRadius; holder.max.y += shankRadius;

    // Check holder against machine travel limits (simplified fixture region).
    // A simple rule: the holder must not penetrate below Z = 0 (table surface)
    // nor extend above the Z-axis max travel.
    for (const auto& comp : m_model.components) {
        if (comp.name == "Z Axis") {
            // Holder top must not exceed the spindle head clearance
            if (holderTop.z > comp.maxTravel) return true;
        }
    }

    // Check that the holder AABB does not overlap the assumed fixture block.
    // Fixture occupies ±kFixtureHalfExtentXY in X/Y, depth kFixtureDepth below Z=0.
    Geom::AABB fixture;
    fixture.min = {-kFixtureHalfExtentXY, -kFixtureHalfExtentXY, -kFixtureDepth};
    fixture.max = { kFixtureHalfExtentXY,  kFixtureHalfExtentXY,   0};

    // Only check if the tool is cutting (not during rapid moves)
    if (pt.motion == MotionType::Rapid || pt.motion == MotionType::Retract)
        return false;

    // Holder collides with fixture if their AABBs overlap and the holder
    // extends below the tool tip level
    if (Geom::aabbsOverlap(holder, fixture) && holder.min.z < tip.z)
        return true;

    return false;
}

// --------------------------------------------------------------------------
CollisionResult MachineSimulation::run(const ToolpathManager* mgr) {
    CollisionResult res;
    if (!mgr) return res;

    int totalMoves = 0;
    for (int i = 0; i < mgr->count(); ++i)
        totalMoves += static_cast<int>(mgr->at(i).points().size());

    int processed = 0;
    for (int opIdx = 0; opIdx < mgr->count() && !res.hasCollision; ++opIdx) {
        const auto& tp  = mgr->at(opIdx);
        const auto& pts = tp.points();

        for (std::size_t i = 0; i < pts.size(); ++i) {
            int axisIdx = -1;
            if (checkOverTravel(pts[i], axisIdx)) {
                res.hasOverTravel     = true;
                res.overTravelMoveIdx = static_cast<int>(i);
                res.description       = "Over-travel detected at operation "
                                        + std::to_string(opIdx) + " move "
                                        + std::to_string(i);
            }
            if (checkCollision(pts[i], tp.tool())) {
                res.hasCollision     = true;
                res.collisionMoveIdx = static_cast<int>(i);
                res.description      = "Collision at operation "
                                        + std::to_string(opIdx) + " move "
                                        + std::to_string(i);
                break;
            }
            ++processed;
            if (m_progress && (processed % 50 == 0))
                m_progress(processed * 100 / std::max(1, totalMoves));
        }
    }
    if (m_progress) m_progress(100);
    return res;
}
