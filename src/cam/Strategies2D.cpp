#include "Strategies2D.h"
#include <cmath>
#include <algorithm>

static constexpr double PI2D = 3.14159265358979323846;

// --------------------------------------------------------------------------
// Add one planar contour pass with optional lead-in and lead-out arcs.
//
// Lead-In:  A tangential quarter-circle arc that smoothly brings the tool
//           onto the profile, avoiding the "shock" of a straight plunge.
//
// Lead-Out: A matching tangential quarter-circle arc that lifts the tool
//           away from the profile at exit, preventing a dwell mark on the
//           finished surface.
// --------------------------------------------------------------------------
void Strategies2D::addContourPass(Toolpath& tp,
                                   const std::vector<Geom::Vec2>& profile,
                                   double z, double leadInR, double leadOutR) {
    if (profile.empty()) return;

    // ---- Lead-In arc (quarter circle, tangent to first edge) ----
    if (profile.size() >= 2 && leadInR > 0) {
        Geom::Vec2 dir = (profile[1] - profile[0]).normalized();
        Geom::Vec2 perp{dir.y, -dir.x};
        Geom::Vec2 arcStart{profile[0].x + perp.x * leadInR,
                            profile[0].y + perp.y * leadInR};
        int arcPts = 8;
        for (int i = 0; i <= arcPts; ++i) {
            double a = -PI2D / 2.0 + PI2D / 2.0 * i / arcPts;
            ToolpathPoint pt;
            pt.position = {arcStart.x + leadInR * std::cos(a),
                           arcStart.y + leadInR * std::sin(a), z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::ArcCCW;
            tp.addPoint(pt);
        }
    }

    // ---- Profile points (The Cut) ----
    for (const auto& p : profile) {
        ToolpathPoint pt;
        pt.position = {p.x, p.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    // Close the profile if open
    if (!(profile.front() == profile.back())) {
        ToolpathPoint close;
        close.position = {profile[0].x, profile[0].y, z};
        close.toolAxis = {0, 0, 1};
        close.motion   = MotionType::Linear;
        tp.addPoint(close);
    }

    // ---- Lead-Out arc (quarter circle, tangent to last edge) ----
    // Mirrors the lead-in geometry at the exit point so the tool departs
    // smoothly rather than retracting straight up through the finish surface.
    // The perpendicular direction is reversed relative to lead-in because the
    // tool is now exiting (departing away from the material), so the arc
    // curves outward on the opposite side of the exit edge.
    if (profile.size() >= 2 && leadOutR > 0) {
        std::size_t n = profile.size();
        Geom::Vec2 exitDir = (profile[n-1] - profile[n-2]).normalized();
        Geom::Vec2 exitPerp{-exitDir.y, exitDir.x};
        Geom::Vec2 arcEnd{profile[n-1].x + exitPerp.x * leadOutR,
                          profile[n-1].y + exitPerp.y * leadOutR};
        int arcPts = 8;
        for (int i = 0; i <= arcPts; ++i) {
            double a = PI2D / 2.0 * i / arcPts;  // 0 → π/2
            ToolpathPoint pt;
            pt.position = {arcEnd.x - leadOutR * std::cos(a),
                           arcEnd.y + leadOutR * std::sin(a), z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::ArcCW;
            tp.addPoint(pt);
        }
    }
}

// --------------------------------------------------------------------------
Toolpath Strategies2D::contour2D(const std::vector<Geom::Vec2>& profile,
                                   const Contour2DParams& p,
                                   const CuttingTool& tool,
                                   const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Contour2D, tool, cuts);
    tp.setName("2D Contour");

    if (profile.empty()) { tp.markClean(); return tp; }

    double safeZ    = 5.0;
    double depthInc = p.depth / p.passes;

    // Rapid to start
    ToolpathPoint rapid;
    rapid.position = {profile[0].x, profile[0].y, safeZ};
    rapid.toolAxis = {0, 0, 1};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    for (int pass = 0; pass < p.passes; ++pass) {
        double z = -(pass + 1) * depthInc;
        // Plunge
        ToolpathPoint plunge = rapid;
        plunge.position.z = z;
        plunge.motion     = MotionType::PlungeFeed;
        tp.addPoint(plunge);

        addContourPass(tp, profile, z, p.leadInRadius, p.leadOutRadius);

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

// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
Strategies2D::concentricOffsets(const std::vector<Geom::Vec2>& boundary,
                                  double stepOver) {
    std::vector<std::vector<Geom::Vec2>> offsets;
    offsets.push_back(boundary);

    // Simple inward shrink: move each vertex toward the centroid
    // Production uses Clipper-lib offset polygons
    Geom::Vec2 centroid{};
    for (const auto& p : boundary) { centroid.x += p.x; centroid.y += p.y; }
    centroid.x /= boundary.size();
    centroid.y /= boundary.size();

    std::vector<Geom::Vec2> current = boundary;
    for (int iter = 0; iter < 50; ++iter) {
        std::vector<Geom::Vec2> inner;
        inner.reserve(current.size());
        for (const auto& p : current) {
            Geom::Vec2 dir{centroid.x - p.x, centroid.y - p.y};
            double d = dir.length();
            if (d < stepOver * 0.5) goto done;
            inner.push_back({p.x + dir.x / d * stepOver,
                             p.y + dir.y / d * stepOver});
        }
        offsets.push_back(inner);
        current = inner;
    }
done:
    return offsets;
}

// --------------------------------------------------------------------------
Toolpath Strategies2D::pocket2D(const std::vector<Geom::Vec2>& boundary,
                                  const Pocket2DParams& p,
                                  const CuttingTool& tool,
                                  const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Pocket2D, tool, cuts);
    tp.setName("2D Pocket");

    if (boundary.empty()) { tp.markClean(); return tp; }

    double safeZ  = 5.0;
    double stepO  = tool.diameter * p.stepOver;
    auto   loops  = concentricOffsets(boundary, stepO);

    // Rapid to start
    ToolpathPoint rapid;
    rapid.position = {boundary[0].x, boundary[0].y, safeZ};
    rapid.toolAxis = {0, 0, 1};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    // Single depth pass
    double z = -p.depth;
    // Plunge
    ToolpathPoint plunge = rapid;
    plunge.position.z = z;
    plunge.motion     = MotionType::PlungeFeed;
    tp.addPoint(plunge);

    // Add loops from inside out (or outside in depending on spiralIn)
    auto& orderedLoops = loops;
    if (p.spiralIn) std::reverse(orderedLoops.begin(), orderedLoops.end());

    for (const auto& loop : orderedLoops)
        addContourPass(tp, loop, z, 0);

    // Retract
    ToolpathPoint ret;
    ret.position = tp.points().back().position;
    ret.position.z = safeZ;
    ret.toolAxis = {0, 0, 1};
    ret.motion   = MotionType::Retract;
    tp.addPoint(ret);

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
Toolpath Strategies2D::faceMill(const FaceMillParams& p,
                                  const CuttingTool& tool,
                                  const CuttingParams& cuts) {
    Toolpath tp(StrategyType::FaceMill, tool, cuts);
    tp.setName("Face Mill");

    double safeZ = 5.0;
    double stepO = tool.diameter * p.stepOver;
    double z     = -p.depth;

    // Rapid to start
    ToolpathPoint rapid;
    rapid.position = {p.xMin - tool.diameter * 0.5, p.yMin, safeZ};
    rapid.toolAxis = {0, 0, 1};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    // Plunge
    ToolpathPoint plunge = rapid;
    plunge.position.z = z;
    plunge.motion     = MotionType::PlungeFeed;
    tp.addPoint(plunge);

    bool leftToRight = true;
    for (double y = p.yMin; y <= p.yMax + stepO; y += stepO) {
        double xS = leftToRight ? p.xMin - tool.diameter * 0.5
                                : p.xMax + tool.diameter * 0.5;
        double xE = leftToRight ? p.xMax + tool.diameter * 0.5
                                : p.xMin - tool.diameter * 0.5;

        ToolpathPoint pt;
        pt.position = {xS, std::min(y, p.yMax), z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);

        pt.position.x = xE;
        tp.addPoint(pt);

        leftToRight = !leftToRight;
    }

    // Retract
    ToolpathPoint ret;
    ret.position = tp.points().back().position;
    ret.position.z = safeZ;
    ret.toolAxis = {0, 0, 1};
    ret.motion   = MotionType::Retract;
    tp.addPoint(ret);

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
Toolpath Strategies2D::drilling(const std::vector<Geom::Vec2>& holes,
                                  const DrillParams& p,
                                  const CuttingTool& tool,
                                  const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Drilling, tool, cuts);
    tp.setName("Drilling");

    double safeZ = 5.0;

    for (const auto& hole : holes) {
        // Rapid to hole XY at safe height
        ToolpathPoint rapid;
        rapid.position = {hole.x, hole.y, safeZ};
        rapid.toolAxis = {0, 0, 1};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);

        if (p.usePeck) {
            // Peck drill (G83)
            double z = 0;
            while (z > -p.totalDepth) {
                z -= p.peckDepth;
                if (z < -p.totalDepth) z = -p.totalDepth;

                ToolpathPoint drill;
                drill.position = {hole.x, hole.y, z};
                drill.toolAxis = {0, 0, 1};
                drill.motion   = MotionType::PlungeFeed;
                tp.addPoint(drill);

                // Retract to safe
                ToolpathPoint ret;
                ret.position = {hole.x, hole.y, safeZ};
                ret.toolAxis = {0, 0, 1};
                ret.motion   = MotionType::Rapid;
                tp.addPoint(ret);
            }
        } else {
            // Simple drill (G81)
            ToolpathPoint drill;
            drill.position = {hole.x, hole.y, -p.totalDepth};
            drill.toolAxis = {0, 0, 1};
            drill.motion   = MotionType::PlungeFeed;
            tp.addPoint(drill);

            if (p.dwellSec > 0) {
                ToolpathPoint dwell = drill;
                dwell.motion = MotionType::Dwell;
                tp.addPoint(dwell);
            }

            ToolpathPoint ret;
            ret.position = {hole.x, hole.y, safeZ};
            ret.toolAxis = {0, 0, 1};
            ret.motion   = MotionType::Rapid;
            tp.addPoint(ret);
        }
    }

    tp.markClean();
    return tp;
}
