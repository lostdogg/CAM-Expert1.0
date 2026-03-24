#include "DynamicMotion.h"
#include <cmath>
#include <algorithm>
#include <numeric>

static constexpr double PI = 3.14159265358979323846;

// --------------------------------------------------------------------------
DynamicMotion::DynamicMotion(DynamicParams params)
    : m_params(params) {}

// --------------------------------------------------------------------------
// Trochoidal loops along a core segment from 'from' to 'to'
// --------------------------------------------------------------------------
std::vector<ToolpathPoint>
DynamicMotion::trochoidalSegment(const Geom::Vec2& from,
                                  const Geom::Vec2& to,
                                  double toolDiameter,
                                  double trochRadius,
                                  double trochPitch,
                                  double z) {
    std::vector<ToolpathPoint> pts;

    Geom::Vec2 dir = (to - from).normalized();
    double     len = (to - from).length();
    if (len < 1e-9) return pts;

    double loopR     = toolDiameter * trochRadius;
    int    numLoops  = std::max(1, static_cast<int>(std::ceil(len / trochPitch)));
    double pitchStep = len / numLoops;
    int    arcSteps  = 36; // points per circular loop

    for (int loop = 0; loop < numLoops; ++loop) {
        Geom::Vec2 centre{
            from.x + dir.x * (loop + 0.5) * pitchStep,
            from.y + dir.y * (loop + 0.5) * pitchStep
        };
        // Circular loop
        for (int s = 0; s <= arcSteps; ++s) {
            double angle = -PI + 2.0 * PI * s / arcSteps;
            ToolpathPoint pt;
            pt.position = {
                centre.x + loopR * std::cos(angle),
                centre.y + loopR * std::sin(angle),
                z
            };
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::Linear;
            pts.push_back(pt);
        }
        // Advance along core
        ToolpathPoint adv;
        adv.position = {centre.x + dir.x * pitchStep * 0.5,
                        centre.y + dir.y * pitchStep * 0.5, z};
        adv.toolAxis = {0, 0, 1};
        adv.motion   = MotionType::MicroLift;
        pts.push_back(adv);
    }
    return pts;
}

// --------------------------------------------------------------------------
// Helical arc entry into material
// --------------------------------------------------------------------------
std::vector<ToolpathPoint>
DynamicMotion::helicalEntry(const Geom::Vec2& centre,
                             double toolDiameter,
                             double entryRadiusFrac,
                             double rampAngleDeg,
                             double startZ,
                             double endZ) {
    std::vector<ToolpathPoint> pts;
    double r      = toolDiameter * entryRadiusFrac;
    double dz     = endZ - startZ;
    double rampRad = rampAngleDeg * PI / 180.0;
    double circum  = 2.0 * PI * r;
    double zPerRev = circum * std::tan(rampRad);
    int    numRevs = std::max(1, static_cast<int>(std::ceil(std::abs(dz) / zPerRev)));
    int    steps   = numRevs * 36;

    for (int i = 0; i <= steps; ++i) {
        double t     = static_cast<double>(i) / steps;
        double angle = 2.0 * PI * numRevs * t;
        double z     = startZ + dz * t;
        ToolpathPoint pt;
        pt.position = { centre.x + r * std::cos(angle),
                        centre.y + r * std::sin(angle),
                        z };
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        pts.push_back(pt);
    }
    return pts;
}

// --------------------------------------------------------------------------
// Apply micro-lifts: insert a small Z+ retract between consecutive moves
// --------------------------------------------------------------------------
void DynamicMotion::applyMicroLifts(Toolpath& tp, double liftHeight) {
    const auto& src = tp.points();
    if (src.size() < 2) return;

    std::vector<ToolpathPoint> result;
    result.reserve(src.size() * 2);

    for (std::size_t i = 0; i + 1 < src.size(); ++i) {
        result.push_back(src[i]);
        // Insert lift at end of each cutting move
        if (src[i].motion == MotionType::Linear) {
            ToolpathPoint lift = src[i];
            lift.position.z += liftHeight;
            lift.motion = MotionType::MicroLift;
            result.push_back(lift);
        }
    }
    result.push_back(src.back());

    tp.clearPoints();
    for (const auto& pt : result)
        tp.addPoint(pt);
}

// --------------------------------------------------------------------------
// Inward offset of a 2-D polygon (simplified Minkowski offset)
// --------------------------------------------------------------------------
std::vector<Geom::Vec2>
DynamicMotion::offsetInward(const std::vector<Geom::Vec2>& poly, double dist) {
    std::vector<Geom::Vec2> result;
    int n = static_cast<int>(poly.size());
    if (n < 3) return result;

    for (int i = 0; i < n; ++i) {
        const auto& prev = poly[(i - 1 + n) % n];
        const auto& curr = poly[i];
        const auto& next = poly[(i + 1) % n];

        Geom::Vec2 d1 = (curr - prev).normalized();
        Geom::Vec2 d2 = (next - curr).normalized();

        // Inward normals (rotate 90° CW)
        Geom::Vec2 n1{d1.y, -d1.x};
        Geom::Vec2 n2{d2.y, -d2.x};

        // Bisector
        Geom::Vec2 bis{n1.x + n2.x, n1.y + n2.y};
        double bLen = bis.length();
        if (bLen < 1e-9) {
            result.push_back({curr.x + n1.x * dist, curr.y + n1.y * dist});
        } else {
            double scale = dist / bLen;
            result.push_back({curr.x + bis.x * scale, curr.y + bis.y * scale});
        }
    }
    return result;
}

// --------------------------------------------------------------------------
// Parallel scan-line grid within a bounding box of the boundary
// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
DynamicMotion::scanLines(const std::vector<Geom::Vec2>& boundary,
                          double stepOver) {
    if (boundary.empty()) return {};

    double minX = boundary[0].x, maxX = minX;
    double minY = boundary[0].y, maxY = minY;
    for (const auto& p : boundary) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    std::vector<std::vector<Geom::Vec2>> lines;
    for (double y = minY; y <= maxY; y += stepOver) {
        lines.push_back({{minX, y}, {maxX, y}});
    }
    return lines;
}

// --------------------------------------------------------------------------
Toolpath DynamicMotion::generatePocketRough(
    const std::vector<Geom::Vec2>& boundary,
    double depth,
    const CuttingTool& tool,
    const CuttingParams& cuttingParams) {

    Toolpath tp(StrategyType::DynamicMill, tool, cuttingParams);
    tp.setName("Dynamic Pocket Rough");

    double safeZ  = 5.0;   // safe retract height (mm)
    double stepD  = m_params.stepDown;
    double stepO  = tool.diameter * (1.0 - m_params.maxEngagement);
    int    passes = std::max(1, static_cast<int>(std::ceil(depth / stepD)));

    // Rapid to safe height, approach point
    if (!boundary.empty()) {
        ToolpathPoint safe;
        safe.position = {boundary[0].x, boundary[0].y, safeZ};
        safe.toolAxis = {0, 0, 1};
        safe.motion   = MotionType::Rapid;
        tp.addPoint(safe);

        for (int pass = 0; pass < passes; ++pass) {
            double z = -(pass + 1) * stepD;
            if (-z > depth) z = -depth;

            // Helical entry
            Geom::Vec2 centre{(boundary[0].x + boundary[1].x) / 2,
                              (boundary[0].y + boundary[1].y) / 2};
            auto entry = helicalEntry(centre, tool.diameter,
                                      m_params.entryArcRadius,
                                      m_params.entryRampAngle,
                                      0.0, z);
            for (const auto& p : entry)
                tp.addPoint(p);

            // Generate scan lines and apply trochoidal motion
            auto offset = offsetInward(boundary, tool.diameter * 0.5);
            auto lines  = scanLines(offset.empty() ? boundary : offset, stepO);

            for (std::size_t li = 0; li + 1 < lines.size(); li += 2) {
                const auto& from = lines[li][0];
                const auto& to   = li + 1 < lines.size()
                                       ? lines[li+1][0] : lines[li][1];
                auto seg = trochoidalSegment(from, to, tool.diameter,
                                             m_params.trochRadius,
                                             m_params.trochPitch, z);
                for (const auto& p : seg)
                    tp.addPoint(p);
            }

            // Micro-lift retract at end of pass
            ToolpathPoint lift;
            lift.position = tp.points().back().position;
            lift.position.z = safeZ;
            lift.toolAxis = {0, 0, 1};
            lift.motion   = MotionType::Retract;
            tp.addPoint(lift);
        }
    }

    applyMicroLifts(tp, m_params.liftHeight);
    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
Toolpath DynamicMotion::generateContour(
    const std::vector<Geom::Vec2>& profile,
    double depth,
    const CuttingTool& tool,
    const CuttingParams& cuttingParams) {

    Toolpath tp(StrategyType::DynamicMill, tool, cuttingParams);
    tp.setName("Dynamic Contour");

    if (profile.size() < 2) {
        tp.markClean();
        return tp;
    }

    double safeZ = 5.0;
    int    passes = std::max(1, static_cast<int>(std::ceil(depth / m_params.stepDown)));

    // Rapid to safe height
    ToolpathPoint safe;
    safe.position = {profile[0].x, profile[0].y, safeZ};
    safe.toolAxis = {0, 0, 1};
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    for (int pass = 0; pass < passes; ++pass) {
        double z = -(pass + 1) * m_params.stepDown;
        if (-z > depth) z = -depth;

        // Plunge entry
        ToolpathPoint plunge = safe;
        plunge.position.z = z;
        plunge.motion     = MotionType::PlungeFeed;
        tp.addPoint(plunge);

        // Follow the profile with trochoidal moves
        for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
            auto seg = trochoidalSegment(profile[i], profile[i+1],
                                          tool.diameter,
                                          m_params.trochRadius,
                                          m_params.trochPitch, z);
            for (const auto& p : seg)
                tp.addPoint(p);
        }

        // Retract
        ToolpathPoint ret;
        ret.position = tp.points().back().position;
        ret.position.z = safeZ;
        ret.toolAxis = {0, 0, 1};
        ret.motion   = MotionType::Retract;
        tp.addPoint(ret);
    }

    tp.markClean();
    return tp;
}
