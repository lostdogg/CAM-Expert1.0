#include "DynamicMotion.h"
#include <cmath>
#include <algorithm>
#include <numeric>

static constexpr double PI = 3.14159265358979323846;

// --------------------------------------------------------------------------
// DynamicParams::buildFromMaterial
// --------------------------------------------------------------------------
DynamicParams DynamicParams::buildFromMaterial(const CuttingTool& tool,
                                                const FeedSpeedResult& fsr) {
    DynamicParams p;
    p.maxEngagement  = fsr.recommendedEngagement;
    p.forceHSM       = fsr.useHSM;
    p.forceTrochoidal= fsr.useTrochoidal;
    p.applySmoothing = fsr.applySmoothing;
    p.entryMethod    = fsr.entryMethod;
    p.repoStyle      = fsr.repositionMode;

    if (fsr.useHSM) {
        // Aluminum: deep axial cuts, full flute; wide step-over
        p.stepDown       = tool.fluteLength;
        p.trochPitch     = tool.diameter * 0.5;   // wider loops
        p.trochRadius    = 0.15;                  // smaller loop fraction
        p.liftHeight     = 1.0;                   // bigger lift – fast air move
        p.entryArcRadius = 1.5;
        p.entryRampAngle = 5.0;                   // steeper helical entry
    } else if (fsr.useTrochoidal) {
        // Titanium / superalloy: shallow axial, tight trochoidal loops
        p.stepDown       = tool.diameter * 1.5;
        p.trochPitch     = tool.diameter * 0.08;  // very tight pitch
        p.trochRadius    = fsr.recommendedEngagement * 0.5;
        p.liftHeight     = 0.1;                   // gentle micro-lift
        p.entryArcRadius = 2.0;                   // wider entry arc
        p.entryRampAngle = 1.5;                   // very shallow ramp
    }
    return p;
}

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
// Tangential arc entry (preferred for Titanium)
// The tool approaches the first cut point along a smooth tangent arc so
// there is no abrupt mechanical shock to the spindle.
// --------------------------------------------------------------------------
std::vector<ToolpathPoint>
DynamicMotion::tangentialArcEntry(const Geom::Vec2& firstCutPoint,
                                   const Geom::Vec2& approachDir,
                                   double arcRadius,
                                   double z) {
    std::vector<ToolpathPoint> pts;

    // Centre of the tangent arc: perpendicular to approachDir at firstCutPoint
    Geom::Vec2 perp{-approachDir.y, approachDir.x}; // rotate 90°
    Geom::Vec2 arcCentre{firstCutPoint.x + perp.x * arcRadius,
                          firstCutPoint.y + perp.y * arcRadius};

    // Start point: diametrically opposite on the arc
    Geom::Vec2 startPt{arcCentre.x - perp.x * arcRadius,
                        arcCentre.y - perp.y * arcRadius};

    int steps = 18; // 18 points for 180° sweep
    for (int i = 0; i <= steps; ++i) {
        double t     = static_cast<double>(i) / steps;
        double angle = PI * t;  // 0 → π (180°)
        // Rotate start point around arcCentre
        double dx = (startPt.x - arcCentre.x) * std::cos(angle)
                  - (startPt.y - arcCentre.y) * std::sin(angle);
        double dy = (startPt.x - arcCentre.x) * std::sin(angle)
                  + (startPt.y - arcCentre.y) * std::cos(angle);
        ToolpathPoint pt;
        pt.position = {arcCentre.x + dx, arcCentre.y + dy, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::ArcCCW;
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
// G-code smoothing filter
//
// Replaces sharp direction changes (large angle between consecutive linear
// segments) with small blending arcs.  This prevents the CNC machine from
// "stuttering" on Titanium/Inconel and eliminates chatter harmonics.
//
// The algorithm identifies consecutive Linear moves that change direction by
// more than the threshold angle, then inserts a tiny ArcCCW point between
// them whose radius equals smoothingRadius.
// --------------------------------------------------------------------------
void DynamicMotion::applyGCodeSmoothing(Toolpath& tp, double smoothingRadius) {
    const auto& src = tp.points();
    if (src.size() < 3) return;

    std::vector<ToolpathPoint> result;
    result.reserve(src.size() + src.size() / 4);

    const double angleThreshRad = 15.0 * PI / 180.0; // only smooth > 15°

    result.push_back(src[0]);
    for (std::size_t i = 1; i + 1 < src.size(); ++i) {
        const auto& prev = src[i - 1];
        const auto& curr = src[i];
        const auto& next = src[i + 1];

        if (curr.motion == MotionType::Linear && next.motion == MotionType::Linear) {
            // Direction vectors in XY plane
            double dx1 = curr.position.x - prev.position.x;
            double dy1 = curr.position.y - prev.position.y;
            double dx2 = next.position.x - curr.position.x;
            double dy2 = next.position.y - curr.position.y;
            double len1 = std::sqrt(dx1*dx1 + dy1*dy1);
            double len2 = std::sqrt(dx2*dx2 + dy2*dy2);

            if (len1 > 1e-9 && len2 > 1e-9) {
                double dot   = (dx1*dx2 + dy1*dy2) / (len1 * len2);
                dot = std::max(-1.0, std::min(1.0, dot));
                double angle = std::acos(dot);

                if (angle > angleThreshRad) {
                    // Insert smoothing arc point at curr + offset toward next
                    double arcOffX = (dx2 / len2) * smoothingRadius;
                    double arcOffY = (dy2 / len2) * smoothingRadius;
                    ToolpathPoint arcPt = curr;
                    arcPt.position.x += arcOffX;
                    arcPt.position.y += arcOffY;
                    arcPt.motion      = MotionType::ArcCCW;
                    arcPt.arcRadius   = smoothingRadius;
                    arcPt.arcCenter   = curr.position; // approximate
                    result.push_back(curr);
                    result.push_back(arcPt);
                    continue;
                }
            }
        }
        result.push_back(curr);
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

            // Entry move: helical or tangential arc depending on material
            if (m_params.entryMethod == MaterialProperties::EntryMethod::TangentialArc
                && boundary.size() >= 2) {
                Geom::Vec2 dir = (boundary[1] - boundary[0]).normalized();
                auto entry = tangentialArcEntry(boundary[0], dir,
                                                m_params.entryArcRadius * tool.diameter,
                                                z);
                for (const auto& p : entry)
                    tp.addPoint(p);
            } else {
                Geom::Vec2 centre{(boundary[0].x + boundary[1].x) / 2,
                                  (boundary[0].y + boundary[1].y) / 2};
                auto entry = helicalEntry(centre, tool.diameter,
                                          m_params.entryArcRadius,
                                          m_params.entryRampAngle,
                                          0.0, z);
                for (const auto& p : entry)
                    tp.addPoint(p);
            }

            // Generate scan lines and apply trochoidal motion
            auto offset = offsetInward(boundary, tool.diameter * 0.5);
            auto lines  = scanLines(offset.empty() ? boundary : offset, stepO);

            for (std::size_t li = 0; li + 1 < lines.size(); li += 2) {
                const auto& from = lines[li][0];
                const auto& to   = li + 1 < lines.size()
                                       ? lines[li+1][0] : lines[li][1];

                if (m_params.forceTrochoidal) {
                    auto seg = trochoidalSegment(from, to, tool.diameter,
                                                  m_params.trochRadius,
                                                  m_params.trochPitch, z);
                    for (const auto& p : seg)
                        tp.addPoint(p);
                } else {
                    // Standard linear scan
                    ToolpathPoint ptA;
                    ptA.position = {from.x, from.y, z};
                    ptA.toolAxis = {0, 0, 1};
                    ptA.motion   = MotionType::Linear;
                    tp.addPoint(ptA);
                    ToolpathPoint ptB;
                    ptB.position = {to.x, to.y, z};
                    ptB.toolAxis = {0, 0, 1};
                    ptB.motion   = MotionType::Linear;
                    tp.addPoint(ptB);
                }
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

    // Apply G-code smoothing for materials that require it (e.g. Titanium)
    if (m_params.applySmoothing)
        applyGCodeSmoothing(tp);

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

        // Entry
        if (m_params.entryMethod == MaterialProperties::EntryMethod::TangentialArc) {
            Geom::Vec2 dir = (profile[1] - profile[0]).normalized();
            auto entry = tangentialArcEntry(profile[0], dir,
                                            m_params.entryArcRadius * tool.diameter,
                                            z);
            for (const auto& p : entry)
                tp.addPoint(p);
        } else {
            // Plunge entry
            ToolpathPoint plunge = safe;
            plunge.position.z = z;
            plunge.motion     = MotionType::PlungeFeed;
            tp.addPoint(plunge);
        }

        // Follow the profile
        for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
            if (m_params.forceTrochoidal) {
                auto seg = trochoidalSegment(profile[i], profile[i+1],
                                              tool.diameter,
                                              m_params.trochRadius,
                                              m_params.trochPitch, z);
                for (const auto& p : seg)
                    tp.addPoint(p);
            } else {
                ToolpathPoint pt;
                pt.position = {profile[i+1].x, profile[i+1].y, z};
                pt.toolAxis = {0, 0, 1};
                pt.motion   = MotionType::Linear;
                tp.addPoint(pt);
            }
        }

        // Retract
        ToolpathPoint ret;
        ret.position = tp.points().back().position;
        ret.position.z = safeZ;
        ret.toolAxis = {0, 0, 1};
        ret.motion   = MotionType::Retract;
        tp.addPoint(ret);
    }

    if (m_params.applySmoothing)
        applyGCodeSmoothing(tp);

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Aluminum: High-Speed Machining strategy
// --------------------------------------------------------------------------
Toolpath DynamicMotion::generateAluminumStrategy(
    const std::vector<Geom::Vec2>& boundary,
    double depth,
    const CuttingTool& tool) {

    FeedSpeedResult fsr = m_matLib.calculate(tool, MaterialClass::Aluminum);
    DynamicParams   p   = DynamicParams::buildFromMaterial(tool, fsr);
    DynamicMotion   dm(p);

    Toolpath tp = dm.generatePocketRough(boundary, depth, tool,
                                          fsr.toCuttingParams());
    tp.setName("Aluminum HSM - Pocket Rough");
    return tp;
}

// --------------------------------------------------------------------------
// Titanium: Low-and-slow trochoidal strategy
// --------------------------------------------------------------------------
Toolpath DynamicMotion::generateTitaniumStrategy(
    const std::vector<Geom::Vec2>& boundary,
    double depth,
    const CuttingTool& tool) {

    FeedSpeedResult fsr = m_matLib.calculate(tool, MaterialClass::Titanium);
    DynamicParams   p   = DynamicParams::buildFromMaterial(tool, fsr);
    DynamicMotion   dm(p);

    Toolpath tp = dm.generatePocketRough(boundary, depth, tool,
                                          fsr.toCuttingParams());
    tp.setName("Titanium Trochoidal - Pocket Rough");
    return tp;
}

// --------------------------------------------------------------------------
// Generic material-aware generator
// --------------------------------------------------------------------------
Toolpath DynamicMotion::generateForMaterial(
    const std::vector<Geom::Vec2>& boundary,
    double depth,
    const CuttingTool& tool,
    MaterialClass matClass) {

    FeedSpeedResult fsr = m_matLib.calculate(tool, matClass);
    DynamicParams   p   = DynamicParams::buildFromMaterial(tool, fsr);
    DynamicMotion   dm(p);

    Toolpath tp = dm.generatePocketRough(boundary, depth, tool,
                                          fsr.toCuttingParams());
    return tp;
}

// --------------------------------------------------------------------------
// §4.1 applyImprovedArcFitting
// --------------------------------------------------------------------------
void DynamicMotion::applyImprovedArcFitting(Toolpath& tp,
                                               double arcTolerance,
                                               double minArcAngleDeg) {
    auto& pts = const_cast<std::vector<ToolpathPoint>&>(tp.points());
    if (pts.size() < 3) return;

    const double minArcRad = minArcAngleDeg * 3.14159265358979323846 / 180.0;
    std::vector<ToolpathPoint> result;
    result.reserve(pts.size());

    std::size_t i = 0;
    while (i < pts.size()) {
        if (pts[i].motion != MotionType::Linear || i + 2 >= pts.size()) {
            result.push_back(pts[i]);
            ++i;
            continue;
        }

        // Try to fit an arc through pts[i], pts[i+1], pts[i+2]
        const Geom::Vec3& p0 = pts[i    ].position;
        const Geom::Vec3& p1 = pts[i + 1].position;
        const Geom::Vec3& p2 = pts[i + 2].position;

        // Work in XY plane (ignore Z for arc fitting on 2.5D paths)
        double ax = p0.x, ay = p0.y;
        double bx = p1.x, by = p1.y;
        double cx = p2.x, cy = p2.y;

        // Circumcentre of three points
        double D = 2.0 * (ax*(by - cy) + bx*(cy - ay) + cx*(ay - by));
        if (std::abs(D) < 1e-9) {
            // Collinear – keep as linear
            result.push_back(pts[i]);
            ++i;
            continue;
        }
        double ux = ((ax*ax + ay*ay)*(by - cy) +
                     (bx*bx + by*by)*(cy - ay) +
                     (cx*cx + cy*cy)*(ay - by)) / D;
        double uy = ((ax*ax + ay*ay)*(cx - bx) +
                     (bx*bx + by*by)*(ax - cx) +
                     (cx*cx + cy*cy)*(bx - ax)) / D;
        double radius = std::sqrt((ax-ux)*(ax-ux) + (ay-uy)*(ay-uy));

        // Check if p1 deviates from arc within tolerance (chord-distance heuristic)
        double midX = (ax + cx) * 0.5, midY = (ay + cy) * 0.5;
        double chordDist = std::sqrt((bx-midX)*(bx-midX) + (by-midY)*(by-midY));

        // Sweep angle
        double a0 = std::atan2(ay - uy, ax - ux);
        double a2 = std::atan2(cy - uy, cx - ux);
        double sweep = std::abs(a2 - a0);
        if (sweep > 3.14159265358979323846) sweep = 2.0*3.14159265358979323846 - sweep;

        if (chordDist < arcTolerance && sweep >= minArcRad) {
            // Emit arc move replacing pts[i+1]
            ToolpathPoint arcPt = pts[i + 2];
            arcPt.motion = MotionType::ArcCW;
            arcPt.arcCenter = {ux, uy, (p0.z + p2.z) * 0.5};
            result.push_back(pts[i]);
            result.push_back(arcPt);
            i += 3;
        } else {
            result.push_back(pts[i]);
            ++i;
        }
    }

    pts = std::move(result);
}

// --------------------------------------------------------------------------
// §4.1 enhancedTrochoidalPeeling
// --------------------------------------------------------------------------
Toolpath DynamicMotion::enhancedTrochoidalPeeling(
    const std::vector<Geom::Vec2>& boundary,
    double depth,
    const CuttingTool& tool,
    const DynamicParams& p,
    double peelLayerDepth,
    double trochRadiusMm) {

    CuttingParams cuts;
    cuts.feedRate = 2000.0;
    cuts.spindleRPM = 8000;

    Toolpath tp(StrategyType::DynamicMill, tool, cuts);
    tp.setName("Enhanced Trochoidal Peel");

    if (boundary.empty() || peelLayerDepth <= 0.0) return tp;

    const double toolDia = tool.diameter;
    const double trochR  = (trochRadiusMm > 0.0) ? trochRadiusMm
                           : (p.trochRadius * toolDia);

    // Find bounding box centroid
    double cx = 0.0, cy = 0.0;
    for (const auto& v : boundary) { cx += v.x; cy += v.y; }
    cx /= boundary.size(); cy /= boundary.size();

    const int numLayers = static_cast<int>(std::ceil(depth / peelLayerDepth));

    for (int layer = 0; layer < numLayers; ++layer) {
        const double z = -(layer + 1) * peelLayerDepth;

        // Stage 1: inward spiral peel
        double maxR = 0.0;
        for (const auto& v : boundary) {
            double r = std::sqrt((v.x-cx)*(v.x-cx) + (v.y-cy)*(v.y-cy));
            maxR = std::max(maxR, r);
        }

        const int spiralSteps = static_cast<int>(maxR / (p.maxEngagement * toolDia)) + 1;
        for (int s = spiralSteps; s >= 0; --s) {
            const double r  = maxR * s / static_cast<double>(spiralSteps);
            const int    nPts = std::max(16, static_cast<int>(2.0 * 3.14159265358979323846 * r / (trochR * 2.0)));
            for (int j = 0; j <= nPts; ++j) {
                const double angle = 2.0 * 3.14159265358979323846 * j / nPts;
                ToolpathPoint pt;
                pt.position  = {cx + r * std::cos(angle), cy + r * std::sin(angle), z};
                pt.toolAxis  = {0, 0, 1};
                pt.motion    = (s == spiralSteps && j == 0) ? MotionType::Rapid : MotionType::Linear;
                tp.addPoint(pt);
            }
        }

        // Stage 2: trochoidal wall finish
        for (std::size_t bi = 0; bi < boundary.size(); ++bi) {
            const Geom::Vec2& from = boundary[bi];
            const Geom::Vec2& to   = boundary[(bi + 1) % boundary.size()];
            auto segPts = trochoidalSegment(from, to, toolDia, trochR, p.trochPitch, z);
            for (auto& pt : segPts) tp.addPoint(pt);
        }
    }

    return tp;
}
