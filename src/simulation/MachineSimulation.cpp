#include "MachineSimulation.h"
#include "../cad/Geometry.h"
#include <cmath>
#include <algorithm>

// M_PI is not guaranteed by the C++ standard; define our own constant.
static constexpr double kPI = 3.14159265358979323846;

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

MachineModel MachineModel::fiveAxisTableTable() {
    MachineModel m;
    m.name = "5-Axis Table/Table (tilting rotary table)";
    m.kinematics.type     = MachineKinematics::Type::Table_Table;
    m.kinematics.aAxisMin =  -30; m.kinematics.aAxisMax = 120;  // tilt axis (A)
    m.kinematics.bAxisMin = -360; m.kinematics.bAxisMax = 360;  // rotation axis (C→stored as B)

    MachineComponent x{"X Axis", {0,0,0}, {1,0,0}, -600, 600};
    MachineComponent y{"Y Axis", {0,0,0}, {0,1,0}, -500, 500};
    MachineComponent z{"Z Axis", {0,0,0}, {0,0,1}, -400,   0};
    MachineComponent a{"A Axis", {0,0,0}, {1,0,0},  -30, 120};
    MachineComponent c{"C Axis", {0,0,0}, {0,0,1}, -360, 360};
    m.components = {x, y, z, a, c};
    return m;
}

MachineModel MachineModel::fiveAxisHeadHead() {
    MachineModel m;
    m.name = "5-Axis Head/Head (dual-spindle-head)";
    m.kinematics.type     = MachineKinematics::Type::Head_Head;
    m.kinematics.aAxisMin = -110; m.kinematics.aAxisMax = 110;
    m.kinematics.bAxisMin = -180; m.kinematics.bAxisMax = 180;

    MachineComponent x{"X Axis", {0,0,0}, {1,0,0}, -800, 800};
    MachineComponent y{"Y Axis", {0,0,0}, {0,1,0}, -700, 700};
    MachineComponent z{"Z Axis", {0,0,0}, {0,0,1}, -600,   0};
    MachineComponent a{"A Axis", {0,0,0}, {1,0,0}, -110, 110};
    MachineComponent b{"B Axis", {0,0,0}, {0,1,0}, -180, 180};
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
// holderClearance – compute the minimum distance (mm) between the tool holder
// envelope and the nearest obstacle.  Returns a large sentinel when clear.
// --------------------------------------------------------------------------
double MachineSimulation::holderClearance(const ToolpathPoint& pt,
                                           const CuttingTool& tool) const {
    double shankRadius = tool.diameter * kShankDiameterMultiplier;
    double fluteLen    = tool.fluteLength;
    double overallLen  = tool.overallLength;

    const Geom::Vec3& tip = pt.position;

    Geom::Vec3 axis = pt.toolAxis;
    if (axis.length() < 0.5) axis = {0, 0, 1};
    axis = axis.normalized();

    Geom::Vec3 holderBase = tip + axis * fluteLen;
    Geom::Vec3 holderTop  = tip + axis * overallLen;

    Geom::AABB holder;
    holder.expand(holderBase);
    holder.expand(holderTop);
    holder.min.x -= shankRadius; holder.max.x += shankRadius;
    holder.min.y -= shankRadius; holder.max.y += shankRadius;

    // Distance from holder AABB to the fixture block
    Geom::AABB fixture;
    fixture.min = {-kFixtureHalfExtentXY, -kFixtureHalfExtentXY, -kFixtureDepth};
    fixture.max = { kFixtureHalfExtentXY,  kFixtureHalfExtentXY,   0};

    // Gap in each axis (negative when overlapping)
    double gapX = std::max(fixture.min.x - holder.max.x,
                            holder.min.x - fixture.max.x);
    double gapY = std::max(fixture.min.y - holder.max.y,
                            holder.min.y - fixture.max.y);
    double gapZ = std::max(fixture.min.z - holder.max.z,
                            holder.min.z - fixture.max.z);

    // Minimum clearance is the distance between the closest faces
    if (gapX > 0 || gapY > 0 || gapZ > 0) {
        // AABBs don't overlap – distance is the gap along the separating axis
        double d2 = (gapX > 0 ? gapX*gapX : 0.0)
                  + (gapY > 0 ? gapY*gapY : 0.0)
                  + (gapZ > 0 ? gapZ*gapZ : 0.0);
        return std::sqrt(d2);
    }
    // Overlapping: clearance is negative (penetration depth)
    return std::min({gapX, gapY, gapZ});
}

// --------------------------------------------------------------------------
// computeTiltAvoidance – 5-axis automatic tilt away from holder collision.
//
// Algorithm:
//  1. Compute holder clearance distance D.
//  2. If D >= params.clearanceDistance → no tilt needed.
//  3. Otherwise, build an avoidance direction perpendicular to the tool axis
//     in the plane of the closest obstacle face.
//  4. Rotate the tool axis by (clearanceDistance − D) × scale, clamped to
//     params.maxTiltAngleDeg.
//  5. Verify the new axis satisfies IK limits; if not, try the opposite side.
// --------------------------------------------------------------------------
TiltAvoidanceResult MachineSimulation::computeTiltAvoidance(
        const ToolpathPoint& pt,
        const CuttingTool&   tool,
        const CollisionControlParams& params) const {

    TiltAvoidanceResult result;
    result.originalAxis = pt.toolAxis;
    result.adjustedAxis = pt.toolAxis;

    double clearance = holderClearance(pt, tool);
    if (clearance >= params.clearanceDistance)
        return result;  // no tilt needed

    // Compute how much tilt we need (proportional to penetration)
    double deficit    = params.clearanceDistance - clearance;
    double tiltDeg    = std::min(deficit * 10.0, params.maxTiltAngleDeg);
    double tiltRad    = tiltDeg * kPI / 180.0;

    Geom::Vec3 axis = pt.toolAxis;
    if (axis.length() < 0.5) axis = {0, 0, 1};
    axis = axis.normalized();

    // Direction to tilt: perpendicular to tool axis and pointing away from
    // the fixture block (away from the XY origin, which is the fixture centre)
    Geom::Vec3 toObstacle{-pt.position.x, -pt.position.y, 0.0};
    if (toObstacle.length() < 1e-6) toObstacle = {1.0, 0.0, 0.0};
    toObstacle = toObstacle.normalized();

    // Remove the component along the tool axis so we get a pure side direction
    Geom::Vec3 sideDir = toObstacle - axis * toObstacle.dot(axis);
    if (sideDir.length() < 1e-6) {
        // Axis is aligned with obstacle direction – use X as fallback
        sideDir = Geom::Vec3{1,0,0} - axis * axis.x;
    }
    sideDir = sideDir.normalized();

    // Determine tilt strategy
    bool useSide = true;
    if (params.strategy == CollisionControlParams::TiltStrategy::LeadLag)
        useSide = false;
    else if (params.strategy == CollisionControlParams::TiltStrategy::Auto) {
        // Side-tilt is generally preferred for holder avoidance
        useSide = true;
    }

    // Rotate tool axis by tiltRad around sideDir (cross product gives rotation axis)
    Geom::Vec3 rotAxis = useSide ? sideDir.cross(axis).normalized()
                                 : sideDir;
    if (rotAxis.length() < 1e-6) rotAxis = {0, 1, 0};
    rotAxis = rotAxis.normalized();

    // Rodrigues' rotation formula: rotate 'axis' by tiltRad around 'rotAxis'
    Geom::Vec3 newAxis = axis * std::cos(tiltRad)
                       + rotAxis.cross(axis) * std::sin(tiltRad)
                       + rotAxis * (rotAxis.dot(axis) * (1.0 - std::cos(tiltRad)));
    newAxis = newAxis.normalized();

    // Verify the new axis satisfies IK limits
    double a, b;
    bool ikOk = MultiAxis::inverseKinematics(newAxis, m_model.kinematics, a, b);
    if (!ikOk) {
        // Try tilting in the opposite direction
        newAxis = axis * std::cos(-tiltRad)
                + rotAxis.cross(axis) * std::sin(-tiltRad)
                + rotAxis * (rotAxis.dot(axis) * (1.0 - std::cos(-tiltRad)));
        newAxis = newAxis.normalized();
        ikOk = MultiAxis::inverseKinematics(newAxis, m_model.kinematics, a, b);
        if (!ikOk)
            return result;  // cannot fix within machine limits
    }

    result.tiltApplied  = true;
    result.adjustedAxis = newAxis;
    result.tiltAngleDeg = tiltDeg;
    result.tiltType     = useSide ? TiltAvoidanceResult::TiltType::SideTilt
                                  : TiltAvoidanceResult::TiltType::LeadLag;
    return result;
}

// --------------------------------------------------------------------------
// applyTiltAvoidance – apply computeTiltAvoidance to every point of a
// toolpath and smooth the tilt transitions over params.smoothingRadius.
// --------------------------------------------------------------------------
void MachineSimulation::applyTiltAvoidance(
        Toolpath& tp,
        const CollisionControlParams& params) const {

    if (tp.points().empty()) return;

    // Copy the points so we still have them after clearPoints()
    std::vector<ToolpathPoint> pts = tp.points();
    std::size_t n                  = pts.size();

    // First pass: compute avoidance for each point
    std::vector<Geom::Vec3> adjustedAxes(n);
    std::vector<bool>       tiltFlags(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        auto res = computeTiltAvoidance(pts[i], tp.tool(), params);
        adjustedAxes[i] = res.adjustedAxis;
        tiltFlags[i]    = res.tiltApplied;
    }

    // Second pass: smooth tilt transitions using the smoothing radius.
    // For each point where tilt changes, blend the tool axis over a window
    // proportional to params.smoothingRadius.
    std::vector<Geom::Vec3> smoothed = adjustedAxes;
    if (params.smoothingRadius > 0.0 && n > 1) {
        // Build cumulative arc-length
        std::vector<double> arcLen(n, 0.0);
        for (std::size_t i = 1; i < n; ++i) {
            arcLen[i] = arcLen[i-1] + (pts[i].position - pts[i-1].position).length();
        }

        for (std::size_t i = 0; i < n; ++i) {
            // Gather all points within the smoothing window
            double lo = arcLen[i] - params.smoothingRadius;
            double hi = arcLen[i] + params.smoothingRadius;

            Geom::Vec3 sumAxis{0,0,0};
            int        count = 0;
            for (std::size_t j = 0; j < n; ++j) {
                if (arcLen[j] >= lo && arcLen[j] <= hi) {
                    sumAxis = sumAxis + adjustedAxes[j];
                    ++count;
                }
            }
            if (count > 0) {
                Geom::Vec3 avg{sumAxis.x / count,
                               sumAxis.y / count,
                               sumAxis.z / count};
                if (avg.length() > 1e-6)
                    smoothed[i] = avg.normalized();
            }
        }
    }

    // Apply smoothed axes back to the toolpath points
    tp.clearPoints();
    for (std::size_t i = 0; i < n; ++i) {
        ToolpathPoint pt = pts[i];
        pt.toolAxis = smoothed[i];
        tp.addPoint(pt);
    }
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
