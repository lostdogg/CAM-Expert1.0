#include "Toolpath.h"
#include <sstream>
#include <cmath>
#include <iomanip>

// --------------------------------------------------------------------------
Toolpath::Toolpath(StrategyType strategy, CuttingTool tool, CuttingParams params)
    : m_strategy(strategy), m_tool(std::move(tool)), m_params(std::move(params)) {}

// --------------------------------------------------------------------------
double Toolpath::totalLength() const {
    double len = 0;
    for (std::size_t i = 1; i < m_points.size(); ++i) {
        auto d = m_points[i].position - m_points[i-1].position;
        len += d.length();
    }
    return len;
}

// --------------------------------------------------------------------------
double Toolpath::estimatedTime() const {
    // Approximate: sum of (segment length / effective feed rate)
    double time = 0;
    for (std::size_t i = 1; i < m_points.size(); ++i) {
        auto d = m_points[i].position - m_points[i-1].position;
        double dist = d.length();
        double feed = m_params.feedRate; // mm/min
        switch (m_points[i].motion) {
        case MotionType::Rapid:
        case MotionType::MicroLift:
        case MotionType::Retract:
            feed = 10000; // rapid traverse
            break;
        case MotionType::PlungeFeed:
            feed = m_params.plungeRate;
            break;
        default:
            feed = m_params.feedRate * m_points[i].feedOverride;
        }
        if (feed > 0)
            time += (dist / feed) * 60.0; // convert min → sec
    }
    return time;
}

// --------------------------------------------------------------------------
Geom::AABB Toolpath::boundingBox() const {
    Geom::AABB box;
    for (const auto& pt : m_points)
        box.expand(pt.position);
    return box;
}

// --------------------------------------------------------------------------
// Produce a simplified NCI-style text record.
// Production NCI format: each line is a numeric code + coordinates.
// --------------------------------------------------------------------------
std::string Toolpath::toNCI() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    // NCI header
    oss << "0\n";                           // start-of-block
    oss << "1, " << m_tool.id << "\n";      // tool number
    oss << "2, " << m_params.spindleRPM << "\n"; // spindle RPM
    oss << "3, " << m_params.feedRate << "\n";    // feed rate

    for (const auto& pt : m_points) {
        // Motion code
        int code = 0;
        switch (pt.motion) {
        case MotionType::Rapid:      code = 0; break;
        case MotionType::Linear:     code = 1; break;
        case MotionType::ArcCW:      code = 2; break;
        case MotionType::ArcCCW:     code = 3; break;
        case MotionType::MicroLift:  code = 0; break; // rapid
        case MotionType::Retract:    code = 0; break; // rapid
        case MotionType::PlungeFeed: code = 1; break;
        default:                     code = 1; break;
        }
        oss << code << ", "
            << pt.position.x << ", "
            << pt.position.y << ", "
            << pt.position.z << ", "
            << pt.toolAxis.x << ", "
            << pt.toolAxis.y << ", "
            << pt.toolAxis.z << "\n";
    }

    oss << "99\n"; // end-of-block
    return oss.str();
}
