#include "PostProcessor.h"
#include "../managers/ToolpathManager.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>

// --------------------------------------------------------------------------
PostProcessor::PostProcessor(PostConfig cfg)
    : m_cfg(std::move(cfg)) {}

// --------------------------------------------------------------------------
std::string PostProcessor::coord(double val) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(m_cfg.decimalPlaces) << val;
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::gAddr(const std::string& /*current*/,
                                  bool& modal,
                                  const std::string& newCode) const {
    if (m_cfg.modalCodes && modal) return "";
    modal = true;
    return newCode + " ";
}

// --------------------------------------------------------------------------
std::string PostProcessor::preamble(const CuttingTool& tool) {
    std::ostringstream oss;

    // Program number / header
    switch (m_cfg.controller) {
    case ControllerType::Fanuc:
    case ControllerType::Haas:
        oss << m_cfg.programNumber << " (" << m_cfg.programComment << ")\n";
        break;
    case ControllerType::Heidenhain:
        oss << "BEGIN PGM " << m_cfg.programComment << " MM\n";
        break;
    default:
        oss << "% " << m_cfg.programComment << "\n";
    }

    if (m_cfg.outputSafeStart) {
        oss << "G28 G91 Z0.\n";
        oss << "G90 G" << (m_cfg.metricMode ? "21" : "20") << "\n";
    }
    if (m_cfg.useAbsolute)
        oss << "G90\n";

    oss << toolChangeBlock(tool);
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::postamble() {
    std::ostringstream oss;
    oss << "G28 G91 Z0.\n";     // retract to machine home
    oss << "M05\n";              // spindle stop
    oss << "M09\n";              // coolant off
    if (m_cfg.outputM30)
        oss << "M30\n";          // program end
    if (m_cfg.controller == ControllerType::Heidenhain)
        oss << "END PGM\n";
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::toolChangeBlock(const CuttingTool& tool) {
    std::ostringstream oss;
    if (m_cfg.outputToolChange) {
        oss << "T" << tool.id << " M06 (" << tool.name << ")\n";
    }
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::spindleBlock(double rpm, bool cw) {
    std::ostringstream oss;
    oss << "S" << static_cast<int>(rpm)
        << (cw ? " M03" : " M04") << "\n";
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::coolantBlock(CuttingParams::Coolant c, bool on) {
    if (!on) return "M09\n";
    switch (c) {
    case CuttingParams::Coolant::Flood:       return "M08\n";
    case CuttingParams::Coolant::Mist:        return "M07\n";
    case CuttingParams::Coolant::ThroughTool: return "M88\n"; // Haas convention
    default:                                  return "";
    }
}

// --------------------------------------------------------------------------
std::string PostProcessor::formatRecord(const NciRecord& rec,
                                         NciRecord& prev,
                                         bool& modalG01) {
    std::ostringstream oss;

    // Motion code → G-code address
    std::string gCode;
    if (rec.isRapid)      gCode = "G00";
    else if (rec.code==1) gCode = "G01";
    else if (rec.code==2) gCode = "G02";
    else if (rec.code==3) gCode = "G03";
    else                  gCode = "G01"; // default

    bool suppress = m_cfg.modalCodes && (gCode == "G01") && modalG01;
    if (!suppress) {
        oss << gCode << " ";
        if (gCode == "G01") modalG01 = true;
        else if (gCode == "G00") modalG01 = false;
    }

    // Coordinates – only output changed axes
    if (std::abs(rec.x - prev.x) > 1e-6)
        oss << "X" << coord(rec.x) << " ";
    if (std::abs(rec.y - prev.y) > 1e-6)
        oss << "Y" << coord(rec.y) << " ";
    if (std::abs(rec.z - prev.z) > 1e-6)
        oss << "Z" << coord(rec.z) << " ";

    // Feed rate (only output when changed)
    if (std::abs(rec.feedRate - prev.feedRate) > 1e-3 && !rec.isRapid)
        oss << "F" << coord(rec.feedRate) << " ";

    prev = rec;
    oss << "\n";
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::generate(const Toolpath* tp) {
    if (!tp) return "";

    std::string nciText = NciFormat::serialize(*tp);
    auto records        = NciFormat::parse(nciText);

    std::ostringstream oss;
    oss << preamble(tp->tool());
    oss << spindleBlock(tp->params().spindleRPM);
    oss << coolantBlock(tp->params().coolant, true);

    NciRecord prev{};
    bool      modalG01 = false;

    for (const auto& rec : records)
        oss << formatRecord(rec, prev, modalG01);

    oss << postamble();
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::generate(const std::vector<Toolpath>& toolpaths) {
    if (toolpaths.empty()) return "";

    auto nciText = NciFormat::serializeAll(toolpaths);
    std::ostringstream oss;

    // Preamble uses first tool
    oss << preamble(toolpaths[0].tool());

    NciRecord prev{};
    bool      modalG01 = false;
    int       lastToolId = -1;

    for (const auto& tp : toolpaths) {
        // Tool change if needed
        if (tp.tool().id != lastToolId) {
            oss << "\n( " << tp.name() << " )\n";
            oss << toolChangeBlock(tp.tool());
            oss << spindleBlock(tp.params().spindleRPM);
            oss << coolantBlock(tp.params().coolant, true);
            lastToolId = tp.tool().id;
        }

        auto recs = NciFormat::parse(NciFormat::serialize(tp));
        for (const auto& rec : recs)
            oss << formatRecord(rec, prev, modalG01);
    }

    oss << postamble();
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::generate(ToolpathManager* mgr) {
    if (!mgr) return "";
    return generate(mgr->toolpaths());
}

// --------------------------------------------------------------------------
bool PostProcessor::writeNC(const std::string& filePath,
                              const std::string& gcode) {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << gcode;
    return f.good();
}
