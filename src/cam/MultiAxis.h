#pragma once
#ifndef MULTIAXIS_H
#define MULTIAXIS_H

#include "Toolpath.h"
#include "../cad/NurbsSurface.h"
#include "../cad/BRep.h"
#include <vector>

// --------------------------------------------------------------------------
// MultiAxis – 4-axis and 5-axis toolpath generation
//
// In multi-axis machining the tool axis vector changes continuously:
//
//  4-Axis (A-axis or B-axis rotation):
//    The part rotates about one rotary axis while the tool moves in X,Y,Z.
//    Typical application: cylindrical parts, cam lobes, turbine blades.
//
//  5-Axis simultaneous:
//    Full freedom in 3 translational + 2 rotational axes. Used for severe
//    undercuts, blade passages, port machining, and compound-curved moulds.
//
// The kinematic model is stored as a MachineKinematics struct and is used
// to resolve the inverse-kinematic solution for each tool-axis vector,
// ensuring the physical axes remain within travel limits and that no
// collision between the spindle head and the fixture occurs.
// --------------------------------------------------------------------------

struct MachineKinematics {
    enum class Type { Table_Table, Head_Table, Head_Head };

    // Tilt preference when two IK solutions exist for the same tool vector.
    //   MinTravel    – choose the solution requiring the smallest axis movement
    //                  from the previous position (default).
    //   PositiveA    – always prefer positive A angle.
    //   NegativeA    – always prefer negative A angle.
    //   KeepPrevious – keep the current A quadrant (minimizes A reversals).
    enum class TiltPreference { MinTravel, PositiveA, NegativeA, KeepPrevious };

    Type           type         = Type::Head_Table;
    TiltPreference tiltPref     = TiltPreference::MinTravel;

    // Rotary axis limits (degrees)
    double aAxisMin = -120, aAxisMax = 120;
    double bAxisMin = -90,  bAxisMax = 90;
    double cAxisMin = -360, cAxisMax = 360;

    // Tool length compensation offset (mm)
    double toolLengthOffset = 75.0;

    // Pivot point for TCPC: physical centre of rotation in part coordinates.
    // For table-table machines this is typically the table centre (0,0,0).
    Geom::Vec3 pivotPoint = {0, 0, 0};

    // Distance from pivot centre to tool tip along the tool axis (mm).
    // Used to compute the TCPC translation correction.
    double pivotLength = 75.0;
};

struct MultiAxisParams {
    enum class LeadLag { None, LeadFwd, LagBack };
    enum class TiltStrategy { Fixed, FromSurface, Automatic };

    LeadLag      leadLag      = LeadLag::LeadFwd;
    double       leadAngle    = 5.0;   // degrees
    double       lagAngle     = 0.0;   // degrees
    TiltStrategy tiltStrategy = TiltStrategy::FromSurface;
    double       fixedTiltAngle = 0.0;
    double       stockAllowance = 0.0;
    bool         gougeProtect   = true;
    double       gouge5AxisTol  = 0.01;  // mm

    MachineKinematics kinematics;
};

// --------------------------------------------------------------------------
// IKResult – result of an inverse-kinematics solve, including both candidate
// solutions.  For an AC machine the two solutions are:
//   primary:   A in [0, 180°],   C = atan2(J,I)
//   alternate: A in [-180°, 0°], C = atan2(J,I) + 180°  (flipped approach)
// --------------------------------------------------------------------------
struct IKResult {
    // Primary solution
    double aPrimary   = 0;    // A-axis angle (degrees)
    double cPrimary   = 0;    // C-axis angle (degrees)

    // Alternate solution (same tool orientation, different machine posture)
    double aAlternate = 0;
    double cAlternate = 0;

    bool   primaryValid   = false;
    bool   alternateValid = false;

    // Which solution was selected by the solver
    bool   usedAlternate = false;

    // Convenience: selected A and C
    double aSelected() const { return usedAlternate ? aAlternate : aPrimary; }
    double cSelected() const { return usedAlternate ? cAlternate : cPrimary; }
};

class MultiAxis {
public:
    explicit MultiAxis(MultiAxisParams params = {});

    // 5-axis swarf machining along a ruled surface
    Toolpath swarfMill(const NurbsSurface& surf,
                        const CuttingTool& tool,
                        const CuttingParams& cuts,
                        int uSteps = 50);

    // 4-axis rotary wrapping of a 2-D profile around a cylinder
    Toolpath rotaryWrap(const std::vector<Geom::Vec2>& profile2D,
                         double cylinderRadius,
                         const CuttingTool& tool,
                         const CuttingParams& cuts,
                         int steps = 100);

    // 5-axis surface normal machining (ball-end mill always perpendicular)
    Toolpath normalToSurface(const NurbsSurface& surf,
                              const CuttingTool& tool,
                              const CuttingParams& cuts,
                              int uSteps = 30,
                              int vSteps = 30);

    // Apply lead/lag tilt to all points of an existing toolpath
    static void applyLeadLag(Toolpath& tp, double leadDeg, double lagDeg);

    // -----------------------------------------------------------------------
    // Inverse kinematics: convert tool-axis vector to machine axis angles
    // (legacy interface – returns two angles stored as A and B).
    // Use solveIK_AC for the full IKResult with dual-solution support.
    // -----------------------------------------------------------------------
    static bool inverseKinematics(const Geom::Vec3& toolAxis,
                                   const MachineKinematics& kin,
                                   double& aOut, double& bOut);

    // -----------------------------------------------------------------------
    // AC-machine IK solver (Table-Table with C around Z and A around X).
    //
    // Implements the formulas from the problem specification:
    //   C = atan2(J, I)
    //   A = acos(K)
    //
    // Also computes the alternate posture solution and chooses between them
    // according to kin.tiltPref.  If prev != nullptr its aSelected/cSelected
    // values are used to break ties via the MinTravel / KeepPrevious rules.
    // -----------------------------------------------------------------------
    static IKResult solveIK_AC(const Geom::Vec3& toolVec,
                                const MachineKinematics& kin,
                                const IKResult* prev = nullptr);

    // -----------------------------------------------------------------------
    // Tool Centre Point Control (TCPC)
    //
    // Computes the corrected XYZ position that keeps the tool tip stationary
    // on the part when the rotary axes move.
    //
    //   P_new = R * (P_old - P_pivot) + P_pivot
    //
    // where R is the combined rotation matrix for the new axis angles.
    // -----------------------------------------------------------------------
    static Geom::Vec3 applyTCPC(const Geom::Vec3& pos,
                                  const Geom::Mat3& R,
                                  const Geom::Vec3& pivot);

    // -----------------------------------------------------------------------
    // Vector Smoothing / Singularity Engine
    //
    // Scans the toolpath with a look-ahead buffer and detects singularity
    // passages (where the tool vector approaches the Z-axis within
    // thresholdDeg degrees).  At each detected crossing, a micro-tilt of
    // microTiltDeg is introduced to force a continuous, predictable motion.
    // -----------------------------------------------------------------------
    static void smoothSingularities(Toolpath& tp,
                                     const MachineKinematics& kin,
                                     double thresholdDeg = 2.0,
                                     double microTiltDeg = 0.005);

    // -----------------------------------------------------------------------
    // Inverse-Time Feedrate (G93 / DPM)
    //
    // In 5-axis moves the linear distance can be tiny while the rotary travel
    // is large.  G93 specifies the reciprocal of the time for the move,
    // ensuring the tool tip maintains a constant surface speed.
    //
    //   T   = distance / feedrate          (time for the move, minutes)
    //   F93 = 1 / T                        (G93 F-word value)
    //
    // Returns 0 if distance or feedrate is effectively zero.
    // -----------------------------------------------------------------------
    static double inverseTimeFeed(double distance, double feedrateMmMin);

private:
    MultiAxisParams m_params;
};

#endif // MULTIAXIS_H
