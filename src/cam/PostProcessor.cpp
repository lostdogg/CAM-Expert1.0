#include "PostProcessor.h"
#include "../managers/ToolpathManager.h"
#include "../cam/MultiAxis.h"
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
        // Safety retract before tool change (prevents crashes during swap)
        oss << safetyRetractBlock();
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
// Safety retract: Z to machine home before any rotary axis movement.
// Prevents tool from swinging into the part during a head/table transition.
// --------------------------------------------------------------------------
std::string PostProcessor::safetyRetractBlock() const {
    return "G28 G91 Z0.\nG90\n";
}

// --------------------------------------------------------------------------
// Over-travel detection
// X_machine = X_part + workOffset_X
// If X_machine > X_limit  → Fatal Error
// --------------------------------------------------------------------------
bool PostProcessor::checkOverTravel(double x, double y, double z,
                                     int recordIdx, PostError& err) const {
    double xm = x + m_cfg.workOffsetX;
    double ym = y + m_cfg.workOffsetY;
    double zm = z + m_cfg.workOffsetZ;

    auto flag = [&](const std::string& axis, double val, double mn, double mx) -> bool {
        if (val < mn || val > mx) {
            err.type      = PostError::Type::OverTravel;
            err.recordIdx = recordIdx;
            err.message   = "Over-travel on " + axis + " axis: " +
                            std::to_string(val) + " (limits " +
                            std::to_string(mn) + " .. " + std::to_string(mx) + ")";
            return true;
        }
        return false;
    };

    return flag("X", xm, m_cfg.xMin, m_cfg.xMax) ||
           flag("Y", ym, m_cfg.yMin, m_cfg.yMax) ||
           flag("Z", zm, m_cfg.zMin, m_cfg.zMax);
}

// --------------------------------------------------------------------------
// Singularity detection
// Returns true when the tool axis is nearly aligned with Z (within threshold)
// --------------------------------------------------------------------------
bool PostProcessor::isSingularity(const Geom::Vec3& toolAxis) const {
    // Dot product with (0,0,1)
    double len = toolAxis.length();
    if (len < 1e-9) return false;
    double cosTheta = toolAxis.z / len;
    double angleDeg = std::acos(std::max(-1.0, std::min(1.0, cosTheta)))
                     * (180.0 / 3.14159265358979);
    return angleDeg < m_cfg.singularityAngleDeg;
}

// --------------------------------------------------------------------------
// Pre-Roll: perturb the tool axis slightly away from Z singularity
// --------------------------------------------------------------------------
Geom::Vec3 PostProcessor::applyPreRoll(const Geom::Vec3& toolAxis, double preRollDeg) {
    double rad = preRollDeg * (3.14159265358979 / 180.0);
    // Tilt in X direction
    Geom::Vec3 perturbed{toolAxis.x + std::sin(rad),
                          toolAxis.y,
                          toolAxis.z};
    double len = perturbed.length();
    if (len > 1e-9)
        return {perturbed.x / len, perturbed.y / len, perturbed.z / len};
    return toolAxis;
}

// --------------------------------------------------------------------------
// Linearization filter
//
// Merges sequences of near-collinear feed moves into a single longer segment.
// The deviation test uses the perpendicular distance from a middle point to
// the line connecting the first and last point of a candidate sequence.
// If that distance is < tol, the intermediate points are dropped.
// --------------------------------------------------------------------------
std::vector<NciRecord> PostProcessor::linearize(
    const std::vector<NciRecord>& records, double tol) {

    if (records.size() < 3) return records;

    std::vector<NciRecord> out;
    out.reserve(records.size());

    std::size_t start = 0;
    while (start < records.size()) {
        // Only linearize feed moves (G01, code==1)
        if (records[start].isRapid || records[start].code != 1) {
            out.push_back(records[start++]);
            continue;
        }

        // Find longest chain starting at 'start' that fits within tolerance
        std::size_t end = start + 1;
        while (end < records.size()) {
            if (records[end].isRapid || records[end].code != 1) break;

            // Check all intermediate points
            const NciRecord& r0 = records[start];
            const NciRecord& r1 = records[end];
            double lx = r1.x - r0.x, ly = r1.y - r0.y, lz = r1.z - r0.z;
            double lLen = std::sqrt(lx*lx + ly*ly + lz*lz);
            bool ok = true;
            if (lLen > 1e-9) {
                for (std::size_t m = start + 1; m < end; ++m) {
                    // Vector from r0 to rm
                    double mx = records[m].x - r0.x;
                    double my = records[m].y - r0.y;
                    double mz = records[m].z - r0.z;
                    // Cross product magnitude / lLen = perpendicular distance
                    double cx = my * lz - mz * ly;
                    double cy = mz * lx - mx * lz;
                    double cz = mx * ly - my * lx;
                    double dist = std::sqrt(cx*cx + cy*cy + cz*cz) / lLen;
                    if (dist > tol) { ok = false; break; }
                }
            }
            if (!ok) break;
            ++end;
        }

        // Emit only start (and end will be emitted in next iteration)
        out.push_back(records[start]);
        // If we merged any, skip the intermediates
        if (end > start + 1)
            start = end - 1; // end-1 will be start in next loop iteration
        else
            ++start;
    }

    // Make sure the last record is always included
    if (!out.empty() && !records.empty() &&
        out.back().x == records.back().x &&
        out.back().y == records.back().y &&
        out.back().z == records.back().z) {
        // already there
    } else if (!records.empty()) {
        out.push_back(records.back());
    }

    return out;
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
    m_lastError = PostError{};

    std::string nciText = NciFormat::serialize(*tp);
    auto records        = NciFormat::parse(nciText);

    // Apply linearization if requested
    if (m_cfg.enableLinearizationFilter)
        records = linearize(records, m_cfg.linearizationTol);

    std::ostringstream oss;
    oss << preamble(tp->tool());
    oss << spindleBlock(tp->params().spindleRPM);
    oss << coolantBlock(tp->params().coolant, true);

    NciRecord prev{};
    bool      modalG01 = false;

    for (int idx = 0; idx < static_cast<int>(records.size()); ++idx) {
        const auto& rec = records[static_cast<std::size_t>(idx)];

        // Over-travel check
        PostError err;
        if (checkOverTravel(rec.x, rec.y, rec.z, idx, err)) {
            m_lastError = err;
            oss << "( *** FATAL: " << err.message << " )\n";
            break; // abort posting
        }

        // Singularity check (for 5-axis: use i,j,k as tool axis)
        if (m_cfg.enableSingularityHandling) {
            Geom::Vec3 axis{rec.i, rec.j, rec.k};
            if (isSingularity(axis)) {
                // Insert pre-roll: safety retract + rotary pre-position
                oss << "( Singularity pre-roll )\n";
                oss << safetyRetractBlock();
            }
        }

        oss << formatRecord(rec, prev, modalG01);
    }

    oss << postamble();
    return oss.str();
}

// --------------------------------------------------------------------------
std::string PostProcessor::generate(const std::vector<Toolpath>& toolpaths) {
    if (toolpaths.empty()) return "";
    m_lastError = PostError{};

    auto nciText = NciFormat::serializeAll(toolpaths);
    std::ostringstream oss;

    // Preamble uses first tool
    oss << preamble(toolpaths[0].tool());

    NciRecord prev{};
    bool      modalG01 = false;
    int       lastToolId = -1;
    int       globalIdx  = 0;

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
        if (m_cfg.enableLinearizationFilter)
            recs = linearize(recs, m_cfg.linearizationTol);

        bool aborted = false;
        for (const auto& rec : recs) {
            PostError err;
            if (checkOverTravel(rec.x, rec.y, rec.z, globalIdx, err)) {
                m_lastError = err;
                oss << "( *** FATAL: " << err.message << " )\n";
                aborted = true;
                break;
            }

            if (m_cfg.enableSingularityHandling) {
                Geom::Vec3 axis{rec.i, rec.j, rec.k};
                if (isSingularity(axis)) {
                    oss << "( Singularity pre-roll )\n";
                    oss << safetyRetractBlock();
                }
            }

            oss << formatRecord(rec, prev, modalG01);
            ++globalIdx;
        }
        if (aborted) break;
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
