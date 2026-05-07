#include "MultiAxis.h"
#include "../cad/Geometry.h"
#include <cmath>
#include <algorithm>

static constexpr double PIMA = 3.14159265358979323846; // π (pi) – avoids M_PI portability concerns

// Helper: wrap angle to [-180, 180)
static double wrapDeg(double d) {
    while (d >  180.0) d -= 360.0;
    while (d <= -180.0) d += 360.0;
    return d;
}

// --------------------------------------------------------------------------
MultiAxis::MultiAxis(MultiAxisParams params)
    : m_params(std::move(params)) {}

// --------------------------------------------------------------------------
// Inverse kinematics – resolve (A, B) rotation from a tool-axis vector.
// The tool axis is expressed in the work-coordinate frame.
// --------------------------------------------------------------------------
bool MultiAxis::inverseKinematics(const Geom::Vec3& toolAxis,
                                   const MachineKinematics& kin,
                                   double& aOut, double& bOut) {
    Geom::Vec3 ax = toolAxis.normalized();

    double A = 0.0, B = 0.0;

    switch (kin.type) {
    case MachineKinematics::Type::Head_Table:
        // Head carries B (rotation about Y), table carries A (rotation about X).
        // Resolution: B = asin(ax.x),  A = atan2(-ax.y, ax.z)
        B = std::asin(std::max(-1.0, std::min(1.0, ax.x)));
        A = std::atan2(-ax.y, ax.z);
        break;

    case MachineKinematics::Type::Table_Table:
        // Both rotary axes are on the table:
        //   C-axis rotates about Z (selects the XY direction of the tool)
        //   A-axis rotates about X (tilts the tool from vertical)
        //
        // Convention: the tool points downward (-Z in machine frame).
        // To get tool axis = ax, first tilt by A around X, then rotate by C (=B here).
        //   A = acos(ax.z)                    ← tilt from vertical
        //   B = atan2(ax.y, ax.x)             ← rotation about Z (stored as B)
        //
        // This is a standard Euler ZX decomposition.
        A = std::acos(std::max(-1.0, std::min(1.0, ax.z)));
        // Disambiguate sign: A is always in [0, π]; store tilt as negative for upward cut
        if (A > PIMA / 2.0) A = PIMA - A;
        B = std::atan2(ax.y, ax.x);
        break;

    case MachineKinematics::Type::Head_Head:
        // Both rotary axes are on the head:
        //   First rotation A around X, then B around (tilted) Y
        //
        // This is equivalent to Head_Table but with the axes belonging to
        // the spindle head assembly instead of the table.  The kinematic
        // solution is identical – the physical difference is in how the
        // pivot points relate to the tool tip, handled by TLC.
        //   B = atan2(ax.x, ax.z)   ← tilt in XZ plane
        //   A = -atan2(ax.y, sqrt(ax.x²+ax.z²))  ← tilt in YZ plane after B
        {
            double proj = std::sqrt(ax.x * ax.x + ax.z * ax.z);
            B = std::atan2(ax.x, ax.z);
            A = -std::atan2(ax.y, proj);
        }
        break;
    }

    aOut = A * 180.0 / PIMA;
    bOut = B * 180.0 / PIMA;

    // Clamp to travel limits
    if (aOut < kin.aAxisMin || aOut > kin.aAxisMax) return false;
    if (bOut < kin.bAxisMin || bOut > kin.bAxisMax) return false;
    return true;
}

// --------------------------------------------------------------------------
// AC-machine IK solver
//
// Implements the specification formulas:
//   C = atan2(J, I)
//   A = acos(K)
//
// The two solutions arise because:
//   primary:   A in  [0, 180°],  C = atan2(J, I)
//   alternate: A in [-180°, 0°], C = atan2(J, I) + 180°  (flipped approach)
//
// Both solutions produce the same tool-axis direction.  The tiltPref setting
// (and optionally the previous IK result) selects which one is used.
// --------------------------------------------------------------------------
IKResult MultiAxis::solveIK_AC(const Geom::Vec3& toolVec,
                                 const MachineKinematics& kin,
                                 const IKResult* prev) {
    IKResult res;
    Geom::Vec3 ax = toolVec.normalized();

    // ---- Primary solution -------------------------------------------------
    // A = acos(K),  C = atan2(J, I)
    double aPrim = std::acos(std::max(-1.0, std::min(1.0, ax.z))) * 180.0 / PIMA;
    double cPrim = std::atan2(ax.y, ax.x) * 180.0 / PIMA;

    res.aPrimary = aPrim;
    res.cPrimary = cPrim;
    res.primaryValid = (aPrim >= kin.aAxisMin && aPrim <= kin.aAxisMax &&
                        cPrim >= kin.cAxisMin && cPrim <= kin.cAxisMax);

    // ---- Alternate solution -----------------------------------------------
    // Flip A to its negative equivalent and rotate C by 180°
    double aAlt = -aPrim;
    double cAlt = wrapDeg(cPrim + 180.0);

    res.aAlternate = aAlt;
    res.cAlternate = cAlt;
    res.alternateValid = (aAlt >= kin.aAxisMin && aAlt <= kin.aAxisMax &&
                          cAlt >= kin.cAxisMin && cAlt <= kin.cAxisMax);

    // ---- Solution selection -----------------------------------------------
    // Default: use primary
    res.usedAlternate = false;

    // If only one solution is valid, choose it
    if (!res.primaryValid && res.alternateValid) {
        res.usedAlternate = true;
        return res;
    }
    if (res.primaryValid && !res.alternateValid) {
        res.usedAlternate = false;
        return res;
    }
    if (!res.primaryValid && !res.alternateValid) {
        // Neither within limits – pick primary and let caller handle it
        return res;
    }

    // Both valid: apply preference
    switch (kin.tiltPref) {
    case MachineKinematics::TiltPreference::PositiveA:
        res.usedAlternate = (aAlt > aPrim);
        break;

    case MachineKinematics::TiltPreference::NegativeA:
        res.usedAlternate = (aAlt < aPrim);
        break;

    case MachineKinematics::TiltPreference::KeepPrevious:
        if (prev) {
            // Stay in the same A-sign quadrant as the previous move
            double prevA = prev->aSelected();
            double distPrim = std::abs(aPrim - prevA);
            double distAlt  = std::abs(aAlt  - prevA);
            res.usedAlternate = (distAlt < distPrim);
        }
        break;

    case MachineKinematics::TiltPreference::MinTravel:
    default:
        if (prev) {
            // Minimise total rotary travel (sum of |ΔA| + |ΔC|)
            double prevA = prev->aSelected();
            double prevC = prev->cSelected();
            double travelPrim = std::abs(aPrim - prevA) + std::abs(wrapDeg(cPrim - prevC));
            double travelAlt  = std::abs(aAlt  - prevA) + std::abs(wrapDeg(cAlt  - prevC));
            res.usedAlternate = (travelAlt < travelPrim);
        }
        break;
    }

    return res;
}

// --------------------------------------------------------------------------
// TCPC – Tool Centre Point Control
//
// Keeps the tool tip stationary on the part when the rotary axes tilt:
//   P_new = R * (P_old − P_pivot) + P_pivot
// --------------------------------------------------------------------------
Geom::Vec3 MultiAxis::applyTCPC(const Geom::Vec3& pos,
                                  const Geom::Mat3& R,
                                  const Geom::Vec3& pivot) {
    return R * (pos - pivot) + pivot;
}

// --------------------------------------------------------------------------
// Vector Smoothing / Singularity Engine
//
// A singularity occurs when the tool axis aligns with Z (K ≈ 1), making
// the C-axis indeterminate (gimbal lock).
//
// Algorithm (look-ahead):
//  1. Scan ahead up to LOOKAHEAD points from each candidate point.
//  2. If the angle between the tool axis and +Z is within thresholdDeg,
//     a singularity passage is detected.
//  3. A micro-tilt of microTiltDeg is introduced in the direction of the
//     previous non-singular tool axis (or +X if unavailable) to force
//     a predictable C-axis value through the crossing.
// --------------------------------------------------------------------------
void MultiAxis::smoothSingularities(Toolpath& tp,
                                     const MachineKinematics& /*kin*/,
                                     double thresholdDeg,
                                     double microTiltDeg) {
    auto& pts = tp.mutablePoints();
    if (pts.size() < 2) return;

    constexpr int LOOKAHEAD = 8;
    const double cosThresh = std::cos(thresholdDeg * PIMA / 180.0);
    const double tiltRad   = microTiltDeg * PIMA / 180.0;

    // Track the last non-singular tilt direction for continuity
    Geom::Vec3 tiltDir = {1.0, 0.0, 0.0};  // default X direction

    for (std::size_t i = 0; i < pts.size(); ++i) {
        Geom::Vec3 ax = pts[i].toolAxis.normalized();

        // Dot product with +Z  (singularity when ax.z ≈ 1)
        double cosAngle = ax.z;  // dot(ax, {0,0,1}) = ax.z for unit vectors
        bool nearSingular = (cosAngle > cosThresh);

        if (!nearSingular) {
            // Update tilt direction: project ax onto XY plane
            Geom::Vec3 xy = {ax.x, ax.y, 0.0};
            if (xy.length() > 1e-6)
                tiltDir = xy.normalized();
            continue;
        }

        // Look ahead to determine the exit direction of the singularity zone
        Geom::Vec3 exitDir = tiltDir;
        for (int k = 1; k <= LOOKAHEAD && (i + static_cast<std::size_t>(k)) < pts.size(); ++k) {
            Geom::Vec3 fwdAx = pts[i + static_cast<std::size_t>(k)].toolAxis.normalized();
            double fwdCos = fwdAx.z;
            if (fwdCos <= cosThresh) {
                // This future point is outside the singularity zone – use its
                // XY projection as the preferred exit direction
                Geom::Vec3 xy = {fwdAx.x, fwdAx.y, 0.0};
                if (xy.length() > 1e-6)
                    exitDir = xy.normalized();
                break;
            }
        }

        // Blend entry and exit tilt directions
        Geom::Vec3 blendDir = (tiltDir + exitDir).normalized();
        if (blendDir.length() < 1e-9) blendDir = tiltDir;

        // Apply micro-tilt: rotate tool axis slightly toward blendDir
        // New axis = (original + blendDir * tan(tiltRad)).normalized()
        Geom::Vec3 newAx = (ax + blendDir * std::tan(tiltRad)).normalized();
        pts[i].toolAxis = newAx;
    }
}

// --------------------------------------------------------------------------
// Inverse-Time Feedrate (G93 / DPM)
//
// Returns the G93 F-word value = 1/T where T = distance / feedrate.
// --------------------------------------------------------------------------
double MultiAxis::inverseTimeFeed(double distance, double feedrateMmMin) {
    if (distance < 1e-9 || feedrateMmMin < 1e-9) return 0.0;
    double T = distance / feedrateMmMin;   // time in minutes
    return 1.0 / T;                         // G93 F-word
}

// --------------------------------------------------------------------------
// Apply lead/lag tilt: rotate each tool axis by leadDeg forward and lagDeg back
// --------------------------------------------------------------------------
void MultiAxis::applyLeadLag(Toolpath& tp, double leadDeg, double /*lagDeg*/) {
    const auto& pts = tp.points();
    if (pts.size() < 2) return;

    std::vector<ToolpathPoint> result;
    result.reserve(pts.size());

    for (std::size_t i = 0; i < pts.size(); ++i) {
        ToolpathPoint pt = pts[i];
        if (i + 1 < pts.size()) {
            // Forward motion direction
            Geom::Vec3 fwd = pts[i+1].position - pts[i].position;
            if (fwd.length() > 1e-9) {
                fwd = fwd.normalized();
                // Lead tilt: rotate toolAxis toward fwd by leadDeg
                double rad = leadDeg * PIMA / 180.0;
                pt.toolAxis = (pt.toolAxis + fwd * std::tan(rad)).normalized();
            }
        }
        result.push_back(pt);
    }

    tp.clearPoints();
    for (const auto& pt : result)
        tp.addPoint(pt);
}

// --------------------------------------------------------------------------
// 5-axis swarf machining along a ruled surface
// --------------------------------------------------------------------------
Toolpath MultiAxis::swarfMill(const NurbsSurface& surf,
                               const CuttingTool& tool,
                               const CuttingParams& cuts,
                               int uSteps) {
    Toolpath tp(StrategyType::Swarf4Axis, tool, cuts);
    tp.setName("5-Axis Swarf");

    double safeZ = 10.0;
    ToolpathPoint safe;
    safe.position = surf.evaluate(surf.uMin(), surf.vMin()) + Geom::Vec3{0,0,safeZ};
    safe.toolAxis = {0, 0, 1};
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    // Walk along U parameter; at each U, place the tool tangent to the surface
    for (int i = 0; i <= uSteps; ++i) {
        double u = surf.uMin() + (surf.uMax() - surf.uMin()) * i / uSteps;
        double v = (surf.vMin() + surf.vMax()) * 0.5;

        Geom::Vec3 pt  = surf.evaluate(u, v);
        Geom::Vec3 dU  = surf.derivU(u, v).normalized();
        Geom::Vec3 n   = surf.normal(u, v);

        // Tool axis = surface tangent along V (swarf direction)
        Geom::Vec3 dV  = surf.derivV(u, v).normalized();
        Geom::Vec3 axis = dV;

        // Gouge-protect: tilt by lead angle if enabled
        double leadRad = m_params.leadAngle * PIMA / 180.0;
        axis = (axis + dU * std::sin(leadRad)).normalized();

        // Offset by tool diameter in normal direction
        Geom::Vec3 tipPos = pt + n * (tool.diameter * 0.5 + m_params.stockAllowance);

        // Inverse kinematics check
        double a, b;
        if (m_params.kinematics.type != MachineKinematics::Type::Head_Table ||
            inverseKinematics(axis, m_params.kinematics, a, b)) {
            ToolpathPoint tpt;
            tpt.position = tipPos;
            tpt.toolAxis = axis;
            tpt.motion   = (i == 0) ? MotionType::PlungeFeed : MotionType::Linear;
            tp.addPoint(tpt);
        }
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// 4-axis rotary wrap: project a 2-D profile around a cylinder
// --------------------------------------------------------------------------
Toolpath MultiAxis::rotaryWrap(const std::vector<Geom::Vec2>& profile2D,
                                double cylinderRadius,
                                const CuttingTool& tool,
                                const CuttingParams& cuts,
                                int /*steps*/) {
    Toolpath tp(StrategyType::Swarf4Axis, tool, cuts);
    tp.setName("4-Axis Rotary Wrap");

    // 2-D profile is (X=axial position, Y=surface distance)
    // Y is mapped to a rotation angle A around the X axis
    double circumference = 2.0 * PIMA * cylinderRadius;

    ToolpathPoint safe;
    safe.position = {0, 0, cylinderRadius + 5.0};
    safe.toolAxis = {0, 0, 1};
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    for (const auto& p2 : profile2D) {
        double axial   = p2.x;
        double angle   = (p2.y / circumference) * 2.0 * PIMA;
        double r       = cylinderRadius + m_params.stockAllowance;

        Geom::Vec3 toolAxis{0,
                            -std::sin(angle),
                             std::cos(angle)};  // radial outward

        ToolpathPoint pt;
        pt.position = {axial,
                       r * std::sin(angle),
                       r * std::cos(angle)};
        pt.toolAxis = toolAxis;
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// 5-axis normal-to-surface (ball end-mill always perpendicular to surface)
// --------------------------------------------------------------------------
Toolpath MultiAxis::normalToSurface(const NurbsSurface& surf,
                                     const CuttingTool& tool,
                                     const CuttingParams& cuts,
                                     int uSteps, int vSteps) {
    Toolpath tp(StrategyType::Multiaxis5, tool, cuts);
    tp.setName("5-Axis Normal to Surface");

    double ballR = tool.diameter * 0.5;
    double safeZ = 10.0;

    ToolpathPoint safe;
    safe.position = surf.evaluate(surf.uMin(), surf.vMin()) + Geom::Vec3{0,0,safeZ};
    safe.toolAxis = {0, 0, 1};
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    for (int i = 0; i <= uSteps; ++i) {
        double u = surf.uMin() + (surf.uMax() - surf.uMin()) * i / uSteps;

        for (int j = 0; j <= vSteps; ++j) {
            double v = surf.vMin() + (surf.vMax() - surf.vMin()) * j / vSteps;

            Geom::Vec3 pt   = surf.evaluate(u, v);
            Geom::Vec3 n    = surf.normal(u, v);
            Geom::Vec3 axis = n; // tool axis = surface normal

            // Offset tool centre by ball radius along normal + stock allowance
            Geom::Vec3 tip = pt + n * (ballR + m_params.stockAllowance);

            // IK check
            double a, b;
            bool ikOk = inverseKinematics(axis, m_params.kinematics, a, b);

            if (m_params.gougeProtect && !ikOk) continue;

            ToolpathPoint tpt;
            tpt.position = tip;
            tpt.toolAxis = axis;
            tpt.motion   = (i == 0 && j == 0) ? MotionType::PlungeFeed
                                               : MotionType::Linear;
            tp.addPoint(tpt);
        }

        // Retract and rapid to start of next pass
        if (i < uSteps && !tp.points().empty()) {
            ToolpathPoint ret = tp.points().back();
            ret.position.z += safeZ;
            ret.motion = MotionType::Retract;
            tp.addPoint(ret);
        }
    }

    tp.markClean();
    return tp;
}
