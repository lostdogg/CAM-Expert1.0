#pragma once
#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include "Toolpath.h"
#include "NciFormat.h"
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
};

class PostProcessor {
public:
    explicit PostProcessor(PostConfig cfg = {});

    // Generate G-code from a single toolpath (via NCI)
    std::string generate(const Toolpath* tp);
    std::string generate(const std::vector<Toolpath>& toolpaths);

    // Generate from the toolpath manager's operation list
    // (accepts a pointer so MainWindow can pass m_toolpathMgr.get())
    std::string generate(class ToolpathManager* mgr);

    // Write G-code to file
    bool writeNC(const std::string& filePath, const std::string& gcode);

    const PostConfig& config() const { return m_cfg; }
    void setConfig(const PostConfig& c) { m_cfg = c; }

private:
    // Format a single NCI record as G-code
    std::string formatRecord(const NciRecord& rec,
                              NciRecord& prev,
                              bool& modalG01);

    // Preamble / postamble code blocks
    std::string preamble(const CuttingTool& tool);
    std::string postamble();

    // Controller-specific code generators
    std::string toolChangeBlock(const CuttingTool& tool);
    std::string spindleBlock(double rpm, bool cw = true);
    std::string coolantBlock(CuttingParams::Coolant c, bool on);

    // Utility
    std::string coord(double val) const;
    std::string gAddr(const std::string& code, bool& modal,
                       const std::string& newCode) const;

    PostConfig m_cfg;
};

#endif // POST_PROCESSOR_H
