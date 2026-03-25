#include "NciFormat.h"
#include <fstream>
#include <sstream>
#include <iomanip>

// --------------------------------------------------------------------------
int NciFormat::motionToCode(MotionType m) {
    switch (m) {
    case MotionType::Rapid:      return 0;
    case MotionType::Linear:     return 1;
    case MotionType::ArcCW:      return 2;
    case MotionType::ArcCCW:     return 3;
    case MotionType::Dwell:      return 4;
    case MotionType::MicroLift:  return 0;  // rapid
    case MotionType::PlungeFeed: return 1;
    case MotionType::Retract:    return 0;  // rapid
    default:                     return 1;
    }
}

// --------------------------------------------------------------------------
std::string NciFormat::serialize(const Toolpath& tp) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    // Header block
    oss << "0\n";                                       // start
    oss << "1001, " << tp.params().spindleRPM << "\n"; // spindle
    oss << "1000, " << tp.params().feedRate   << "\n"; // feed

    bool is5Axis = false;
    for (const auto& pt : tp.points()) {
        Geom::Vec3 ax = pt.toolAxis;
        if (!(std::abs(ax.x) < 1e-6 && std::abs(ax.y) < 1e-6))
            is5Axis = true;
    }

    for (const auto& pt : tp.points()) {
        int code = motionToCode(pt.motion);
        if (is5Axis) {
            // 14 = 5-axis move record with tool-axis vector
            oss << "14\n";
            oss << pt.toolAxis.x << ", "
                << pt.toolAxis.y << ", "
                << pt.toolAxis.z << "\n";
        }
        oss << code << "\n";
        oss << pt.position.x << ", "
            << pt.position.y << ", "
            << pt.position.z << "\n";

        if (pt.motion == MotionType::ArcCW || pt.motion == MotionType::ArcCCW) {
            oss << "ARC, "
                << pt.arcCenter.x << ", "
                << pt.arcCenter.y << ", "
                << pt.arcCenter.z << ", "
                << pt.arcRadius   << "\n";
        }
    }

    oss << "99\n"; // end
    return oss.str();
}

// --------------------------------------------------------------------------
std::string NciFormat::serializeAll(const std::vector<Toolpath>& toolpaths) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < toolpaths.size(); ++i) {
        oss << "# Operation " << (i+1) << ": "
            << toolpaths[i].name() << "\n";
        oss << serialize(toolpaths[i]);
    }
    return oss.str();
}

// --------------------------------------------------------------------------
std::vector<NciRecord> NciFormat::parse(const std::string& nciText) {
    std::vector<NciRecord> records;
    std::istringstream ss(nciText);
    std::string line;

    NciRecord current;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Detect keyword lines
        if (line.find("1001,") != std::string::npos) {
            // spindle
            std::istringstream ls(line.substr(line.find(',') + 1));
            ls >> current.spindleRPM;
            continue;
        }
        if (line.find("1000,") != std::string::npos) {
            // feed
            std::istringstream ls(line.substr(line.find(',') + 1));
            ls >> current.feedRate;
            continue;
        }

        // Arc annotation line: "ARC, cx, cy, cz, r"
        if (line.find("ARC,") != std::string::npos) {
            std::istringstream ls(line.substr(line.find(',') + 1));
            char comma;
            double cx, cy, cz, r;
            if (ls >> cx >> comma >> cy >> comma >> cz >> comma >> r) {
                // Store arc centre in i/j/k of the most recent record
                if (!records.empty()) {
                    records.back().i = cx;
                    records.back().j = cy;
                    records.back().k = cz;
                }
            }
            continue;
        }

        // 5-axis tool axis line: "14" followed by coordinate line
        if (line == "14") {
            std::string axLine;
            if (std::getline(ss, axLine)) {
                std::istringstream ls(axLine);
                char comma;
                ls >> current.i >> comma >> current.j >> comma >> current.k;
            }
            continue;
        }

        std::istringstream ls(line);
        int code;
        if (!(ls >> code)) continue;

        current.code    = code;
        current.isRapid = (code == 0);

        if (code == 99) { break; }
        if (code == 0 || code == 1 || code == 2 || code == 3) {
            if (!std::getline(ss, line)) break;
            std::istringstream cs(line);
            char comma;
            cs >> current.x >> comma >> current.y >> comma >> current.z;
            records.push_back(current);
        }
    }
    return records;
}

// --------------------------------------------------------------------------
bool NciFormat::writeFile(const std::string& filePath, const Toolpath& tp) {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << serialize(tp);
    return f.good();
}

bool NciFormat::writeFile(const std::string& filePath,
                           const std::vector<Toolpath>& tps) {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << serializeAll(tps);
    return f.good();
}
