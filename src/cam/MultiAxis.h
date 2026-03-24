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

    Type   type         = Type::Head_Table;
    // Rotary axis limits (degrees)
    double aAxisMin = -120, aAxisMax = 120;
    double bAxisMin = -90,  bAxisMax = 90;
    double cAxisMin = -360, cAxisMax = 360;
    // Tool length compensation offset (mm)
    double toolLengthOffset = 75.0;
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

    // Inverse kinematics: convert tool-axis vector to machine axis angles
    static bool inverseKinematics(const Geom::Vec3& toolAxis,
                                   const MachineKinematics& kin,
                                   double& aOut, double& bOut);

private:
    MultiAxisParams m_params;
};

#endif // MULTIAXIS_H
