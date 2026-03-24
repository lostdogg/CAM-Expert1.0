#pragma once
#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include "Toolpath.h"
#include "NciFormat.h"
#include "../managers/PlanesManager.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declaration
class ToolpathManager;

// --------------------------------------------------------------------------
// PostProcessor
//
// Translates NCI intermediate data into machine-specific G-code.
//
// The post-processor is customizable through a PostConfig struct that
// captures all controller-specific settings:
//   • Controller dialect  (Fanuc, Haas, Heidenhain, Siemens, Mazak…)
//   • Axis naming         (A, B, or C for the 4th/5th rotary axis)
//   • Output format       (decimal places, modal vs. non-modal codes)
//   • Program numbering   (O-number / program name)
//   • Safety codes        (cancel cycles, reset modal state)
//   • Sub-program support
//
// Work Offset (G54/G55/G56…) emission:
//   The post uses the integer-based wcsOffset field on CoordPlane to decide
//   which G-code register to output.  The mapping is:
//     wcsOffset  -1  →  G54  (default, no explicit emit unless forced)
//     wcsOffset   0  →  G54
//     wcsOffset   1  →  G55
//     wcsOffset   2  →  G56
//     wcsOffset   n  →  G(54+n)   (n >= 0)
//   In 4/5-axis positional ("3+2") work all planes share wcsOffset=0 (G54)
//   so the machine stays on the same datum while the rotary axes index.
//
// Advanced features:
//   • Axis over-travel detection – checks every coordinate against the
//     machine's physical travel limits before writing any G-code.
//     Formula: X_machine = X_part + WorkOffset_X
//     If X_machine > X_limit  → Fatal Error, posting aborted.
//   • Rotary singularity handling – detects when the tool vector V(0,0,1)
//     approaches the Z-axis singularity and applies "Pre-Roll" logic to
//     choose a preferred rotary direction, avoiding mechanical stall.
//   • Look-ahead / linearization tolerance – merges near-collinear tiny
//     segments into one longer line, keeping the controller buffer full.
//   • Conditional logic – safety retracts, tool-change sequences, and
//     custom variable / M-code injection.
// --------------------------------------------------------------------------

enum class ControllerType {
    Fanuc,
    Haas,
    Heidenhain,
    SiemensSinumerik,
    Mazak,
    Okuma,
    MitsubishiM70,
    Generic
};

struct PostConfig {
    ControllerType controller     = ControllerType::Fanuc;
    std::string    programNumber  = "O1000";
    std::string    programComment = "CAM-Expert Output";

    // Axis naming for rotary axes
    std::string    fourthAxisName = "A";
    std::string    fifthAxisName  = "B";

    // Output format
    int    decimalPlaces   = 3;   // coordinate decimal places
    bool   modalCodes      = true; // suppress repeated G-codes
    bool   useAbsolute     = true; // G90 absolute (vs G91 incremental)
    bool   metricMode      = true; // G21 metric (vs G20 imperial)

    // Safety / setup
    bool   outputSafeStart = true;  // emit G28/G91 Z0 at program start
    bool   outputToolChange= true;  // emit M06 Txx on tool change
    bool   outputM30       = true;  // emit M30 program end

    // Feed / speed format
    bool   feedInMMMin     = true;  // true=mm/min (G94), false=mm/rev (G95)
    bool   constantSurfaceSpeed = false;  // G96 (turning)

    // ---- Machine limits (for over-travel detection) ----
    double xMin = -500, xMax = 500;
    double yMin = -400, yMax = 400;
    double zMin = -300, zMax =   0;

    // Work offset applied before limit check: X_machine = X_part + workOffset
    double workOffsetX = 0, workOffsetY = 0, workOffsetZ = 0;

    // ---- Look-ahead / linearization ----
    bool   enableLinearizationFilter = false; // merge near-collinear segments
    double linearizationTol          = 0.0025; // mm chord-height tolerance

    // ---- Singularity handling ----
    bool   enableSingularityHandling = true;
    double singularityAngleDeg       = 2.0;   // threshold: tool ≈ vertical
    double preRollOffsetDeg          = 5.0;   // tilt applied before singularity
};

// --------------------------------------------------------------------------
// PostError – issued when a fatal condition is detected
// --------------------------------------------------------------------------
struct PostError {
    enum class Type { None, OverTravel, Singularity, KinematicLimit };
    Type        type      = Type::None;
    int         recordIdx = -1;       // which NCI record triggered the error
    std::string message;
    bool isFatal() const { return type != Type::None; }
};

// --------------------------------------------------------------------------
// PostProcessor
// --------------------------------------------------------------------------
class PostProcessor {
public:
    explicit PostProcessor(PostConfig cfg = {});

    // Generate G-code from a single toolpath (via NCI)
    std::string generate(const Toolpath* tp,
                         const CoordPlane* wcsPlane = nullptr);
    std::string generate(const std::vector<Toolpath>& toolpaths,
                         const CoordPlane* wcsPlane = nullptr);

    // Generate from the toolpath manager's operation list
    // (accepts a pointer so MainWindow can pass m_toolpathMgr.get())
    std::string generate(class ToolpathManager* mgr,
                         const CoordPlane* wcsPlane = nullptr);

    // Write G-code to file
    bool writeNC(const std::string& filePath, const std::string& gcode);

    const PostConfig& config() const { return m_cfg; }
    void setConfig(const PostConfig& c) { m_cfg = c; }

    // Last error from generate()
    const PostError& lastError() const { return m_lastError; }
    bool             hasError()  const { return m_lastError.isFatal(); }

    // ---- Safety checks ----

    // Check whether a coordinate exceeds machine travel limits.
    // Returns true if an over-travel condition is found; fills 'err'.
    bool checkOverTravel(double x, double y, double z,
                         int recordIdx, PostError& err) const;

    // Detect rotary singularity: tool axis vector nearly aligned with Z.
    // Returns true if the axis is within singularityAngleDeg of (0,0,1).
    bool isSingularity(const Geom::Vec3& toolAxis) const;

    // Apply pre-roll to avoid singularity: slightly perturb the tool axis
    // away from Z by preRollOffsetDeg in the X direction.
    static Geom::Vec3 applyPreRoll(const Geom::Vec3& toolAxis,
                                    double preRollDeg);

    // ---- Linearization filter ----

    // Merge near-collinear NCI records into longer single segments.
    // Segments are merged if their deviation from a straight line is
    // within linearizationTol.
    static std::vector<NciRecord>
        linearize(const std::vector<NciRecord>& records, double tol);

private:
    // Format a single NCI record as G-code
    std::string formatRecord(const NciRecord& rec,
                              NciRecord& prev,
                              bool& modalG01);

    // Preamble / postamble code blocks
    std::string preamble(const CuttingTool& tool,
                         const CoordPlane* wcsPlane = nullptr);
    std::string postamble();

    // Controller-specific code generators
    std::string toolChangeBlock(const CuttingTool& tool);
    std::string spindleBlock(double rpm, bool cw = true);
    std::string coolantBlock(CuttingParams::Coolant c, bool on);

    // Safety-retract block: always retract Z to home before any rotary move
    std::string safetyRetractBlock() const;

    // Work offset block: emit G54/G55/… based on CoordPlane::wcsOffset integer.
    //   wcsOffset -1 or 0  → G54
    //   wcsOffset 1        → G55
    //   wcsOffset n        → G(54+n)
    // When prevOffset == newOffset the block is suppressed (modal behaviour).
    std::string workOffsetBlock(const CoordPlane* wcsPlane,
                                int& prevGOffset) const;

    // Utility
    std::string coord(double val) const;
    std::string gAddr(const std::string& code, bool& modal,
                       const std::string& newCode) const;

    PostConfig m_cfg;
    PostError  m_lastError;
};

#endif // POST_PROCESSOR_H

