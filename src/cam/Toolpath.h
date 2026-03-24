#pragma once
#ifndef TOOLPATH_H
#define TOOLPATH_H

#include "../cad/Geometry.h"
#include <vector>
#include <string>
#include <memory>

// --------------------------------------------------------------------------
// CNC Tool definition
// --------------------------------------------------------------------------
enum class ToolType {
    EndMill,
    BallEndMill,
    BullNoseMill,
    DrillBit,
    ReamBit,
    BoringBar,
    FaceMill,
    TurningInsert,
    ThreadingTool
};

struct CuttingTool {
    int         id          = 1;
    std::string name;
    ToolType    type        = ToolType::EndMill;
    double      diameter    = 12.0;   // mm
    double      cornerRadius= 0.0;    // mm (for bull-nose)
    double      fluteLength = 25.0;   // mm
    double      overallLength = 75.0; // mm
    int         numFlutes   = 4;
    double      rakeAngle   = 10.0;   // degrees
    std::string material    = "Carbide";
};

// --------------------------------------------------------------------------
// Feed / speed / coolant parameters for one operation
// --------------------------------------------------------------------------
struct CuttingParams {
    double spindleRPM       = 5000;   // rev/min
    double surfaceSpeed     = 200;    // m/min (for turning)
    double feedPerTooth     = 0.05;   // mm/tooth
    double feedRate         = 1000;   // mm/min (derived = feedPerTooth * flutes * RPM)
    double plungeRate       = 300;    // mm/min (Z down)
    double axialDepth       = 5.0;    // mm (depth of cut)
    double radialDepth      = 6.0;    // mm (step-over)
    double stockAllowance   = 0.25;   // mm (finish stock left on walls)

    enum class Coolant { Off, Flood, Mist, Air, ThroughTool } coolant = Coolant::Flood;
};

// --------------------------------------------------------------------------
// A single point on a toolpath (NCI-compatible)
// --------------------------------------------------------------------------
enum class MotionType {
    Rapid,      // G00 – fast traverse (no cutting)
    Linear,     // G01 – linear feed move
    ArcCW,      // G02 – clockwise arc
    ArcCCW,     // G03 – counter-clockwise arc
    Dwell,      // G04 – dwell
    MicroLift,  // High-speed retract between passes (Dynamic Motion)
    PlungeFeed, // Linear downward entry at plunge rate
    Retract     // Return to safe height
};

struct ToolpathPoint {
    Geom::Vec3 position;        // Tool tip position (X, Y, Z) in mm
    Geom::Vec3 toolAxis;        // Normalized tool axis vector (for 5-axis)
    MotionType motion      = MotionType::Linear;
    double     feedOverride= 1.0;  // 0..2  multiplier on base feed rate
    double     arcRadius   = 0;    // for ArcCW/ArcCCW
    Geom::Vec3 arcCenter;          // arc centre point
};

// --------------------------------------------------------------------------
// A single machining operation (one entry in the ToolpathManager list)
// --------------------------------------------------------------------------
enum class StrategyType {
    // 2D / 2.5D
    Contour2D, Pocket2D, FaceMill, Drilling, Chamfer, Thread,
    // Dynamic Motion
    DynamicMill, OptiRough,
    // 3D
    WaterlineRough, Raster3D, Scallop3D, Spiral3D, ProjectSurface,
    // Multi-Axis
    Swarf4Axis, Multiaxis5, PortMachining,
    // Turning
    RoughTurning, FinishTurning, Grooving, ThreadTurning,
    // Misc
    Custom
};

class Toolpath {
public:
    Toolpath() = default;
    explicit Toolpath(StrategyType strategy, CuttingTool tool,
                      CuttingParams params);

    // Accessors
    StrategyType                    strategy()  const { return m_strategy; }
    const CuttingTool&              tool()      const { return m_tool; }
    const CuttingParams&            params()    const { return m_params; }
    const std::vector<ToolpathPoint>& points()  const { return m_points; }
    const std::string&              name()      const { return m_name; }
    bool                            isDirty()   const { return m_dirty; }

    void setName(const std::string& n)  { m_name = n; }
    void markDirty()                    { m_dirty = true; }
    void markClean()                    { m_dirty = false; }

    // Add a motion point
    void addPoint(const ToolpathPoint& pt) { m_points.push_back(pt); }
    void clearPoints()                     { m_points.clear(); }

    // Total path length (sum of all move distances)
    double totalLength() const;

    // Estimated machining time (seconds)
    double estimatedTime() const;

    // Bounding box of all tool tip positions
    Geom::AABB boundingBox() const;

    // Generate an NCI-style text representation
    std::string toNCI() const;

private:
    StrategyType              m_strategy = StrategyType::Contour2D;
    CuttingTool               m_tool;
    CuttingParams             m_params;
    std::vector<ToolpathPoint> m_points;
    std::string               m_name    = "Operation";
    bool                      m_dirty   = true;
};

#endif // TOOLPATH_H
