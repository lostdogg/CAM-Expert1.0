#include "WireEDM.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

static constexpr double PI_EDM = 3.14159265358979323846;

// --------------------------------------------------------------------------
WireEDM::WireEDM(WireEDMCutParams params)
    : m_params(std::move(params)) {}

// --------------------------------------------------------------------------
// Internal: build a single axis pass (2-D profile at fixed Z)
// --------------------------------------------------------------------------
Toolpath WireEDM::buildSingleAxisPass(const std::vector<Geom::Vec2>& profile,
                                       double z,
                                       MotionType motion,
                                       double     feedRate,
                                       const std::string& name) {
    CuttingTool wireTool;
    wireTool.name     = "Wire EDM";
    wireTool.type     = ToolType::EndMill;   // no specific wire type; use as proxy
    wireTool.diameter = 0.25;

    CuttingParams cp;
    cp.feedRate = feedRate;

    Toolpath tp(StrategyType::WireEDM2Axis, wireTool, cp);
    tp.setName(name);

    if (profile.empty()) return tp;

    // Rapid to first point
    {
        ToolpathPoint pt;
        pt.position = {profile[0].x, profile[0].y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Rapid;
        tp.addPoint(pt);
    }

    // Follow the profile
    for (const auto& v : profile) {
        ToolpathPoint pt;
        pt.position = {v.x, v.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = motion;
        pt.feedOverride = 1.0;
        tp.addPoint(pt);
    }

    // Close the loop back to the first point
    {
        ToolpathPoint pt;
        pt.position = {profile[0].x, profile[0].y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = motion;
        tp.addPoint(pt);
    }

    return tp;
}

// --------------------------------------------------------------------------
// Internal: offset a 2-D profile (simple vertex-normal offset)
// --------------------------------------------------------------------------
std::vector<Geom::Vec2>
WireEDM::offsetProfile(const std::vector<Geom::Vec2>& profile, double amount) {
    if (profile.size() < 3) return profile;

    std::vector<Geom::Vec2> result;
    result.reserve(profile.size());

    const int n = static_cast<int>(profile.size());
    for (int i = 0; i < n; ++i) {
        const Geom::Vec2& prev = profile[(i + n - 1) % n];
        const Geom::Vec2& curr = profile[i];
        const Geom::Vec2& next = profile[(i + 1) % n];

        // Edge vectors
        double ax = curr.x - prev.x, ay = curr.y - prev.y;
        double bx = next.x - curr.x, by = next.y - curr.y;

        double la = std::sqrt(ax*ax + ay*ay);
        double lb = std::sqrt(bx*bx + by*by);
        if (la < 1e-12 || lb < 1e-12) { result.push_back(curr); continue; }

        // Inward normals of each edge
        double nx1 = -ay / la, ny1 =  ax / la;
        double nx2 = -by / lb, ny2 =  bx / lb;

        // Bisector
        double bsx = nx1 + nx2, bsy = ny1 + ny2;
        double bsl = std::sqrt(bsx*bsx + bsy*bsy);
        if (bsl < 1e-12) { result.push_back(curr); continue; }
        bsx /= bsl;  bsy /= bsl;

        // Scale so the offset is `amount` perpendicular to the edge
        double sinHalf = std::clamp(bsx * nx1 + bsy * ny1, -1.0, 1.0);
        if (std::abs(sinHalf) < 1e-6) { result.push_back(curr); continue; }
        double scale = amount / sinHalf;

        result.push_back({curr.x + bsx * scale, curr.y + bsy * scale});
    }
    return result;
}

// --------------------------------------------------------------------------
double WireEDM::signedPerimeter(const std::vector<Geom::Vec2>& profile) {
    double area = 0.0;
    const int n = static_cast<int>(profile.size());
    for (int i = 0; i < n; ++i) {
        const auto& a = profile[i];
        const auto& b = profile[(i + 1) % n];
        area += (a.x * b.y - b.x * a.y);
    }
    return area * 0.5;   // signed: + = CCW
}

// --------------------------------------------------------------------------
double WireEDM::estimateCutTime(double perimeterMm,
                                  double /*partHeightMm*/,
                                  double feedRateMmMin) {
    if (feedRateMmMin <= 0.0) return 0.0;
    return perimeterMm / feedRateMmMin;
}

// --------------------------------------------------------------------------
WireEDMCutResult WireEDM::cut2Axis(const std::vector<Geom::Vec2>& profile,
                                     double                         partHeight,
                                     const WireEDMThread&           thread) const {
    WireEDMCutResult result;
    if (profile.size() < 3) return result;

    const double z = thread.z;

    // 1. Approach from thread hole to start of profile
    CuttingTool wt; wt.name = "Wire"; wt.diameter = m_params.wireDiameter;
    CuttingParams cp; cp.feedRate = m_params.roughFeed;

    Toolpath rough(StrategyType::WireEDM2Axis, wt, cp);
    rough.setName("Rough 2-Axis");

    // Thread position rapid
    {
        ToolpathPoint pt;
        pt.position = {thread.position.x, thread.position.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Rapid;
        rough.addPoint(pt);
    }

    // Approach (cut to profile start)
    {
        ToolpathPoint pt;
        pt.position = {profile[0].x, profile[0].y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        rough.addPoint(pt);
    }

    // Follow profile
    for (const auto& v : profile) {
        ToolpathPoint pt;
        pt.position = {v.x, v.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        rough.addPoint(pt);
    }

    // Close and return to thread
    {
        ToolpathPoint pt;
        pt.position = {profile[0].x, profile[0].y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        rough.addPoint(pt);
    }

    result.roughPass = rough;

    // Estimate perimeter for time calculation
    double peri = 0.0;
    for (int i = 0; i < static_cast<int>(profile.size()); ++i)
    {
        const auto& a = profile[i];
        const auto& b = profile[(i + 1) % profile.size()];
        peri += std::sqrt((b.x-a.x)*(b.x-a.x) + (b.y-a.y)*(b.y-a.y));
    }
    result.estimatedTimeMin = estimateCutTime(peri, partHeight, m_params.roughFeed);

    // 2. Skim passes
    if (!m_params.skimOffsets.empty()) {
        addSkimPasses(result, profile, partHeight, m_params);
    }

    // 3. Tab relief if land was requested
    if (m_params.useLand) {
        auto segs = landProfile(profile, m_params.landWidth, m_params.landOffset);
        CuttingParams tabCp; tabCp.feedRate = m_params.tabFeed;
        Toolpath relief(StrategyType::WireEDM2Axis, wt, tabCp);
        relief.setName("Tab Relief");
        for (const auto& seg : segs) {
            for (const auto& v : seg) {
                ToolpathPoint pt;
                pt.position = {v.x, v.y, z};
                pt.toolAxis = {0, 0, 1};
                pt.motion   = MotionType::Linear;
                relief.addPoint(pt);
            }
        }
        result.tabRelief = relief;
    }

    // The core can fall if the profile is closed and no land was used
    result.coreWillFall = !m_params.useLand;

    return result;
}

// --------------------------------------------------------------------------
WireEDMCutResult WireEDM::cut4Axis(const std::vector<Geom::Vec2>& lowerProfile,
                                     const std::vector<Geom::Vec2>& upperProfile,
                                     double                         partHeight,
                                     const WireEDMThread&           thread) const {
    WireEDMCutResult result;
    if (lowerProfile.size() < 3) return result;

    const int n = static_cast<int>(lowerProfile.size());
    const int nUp = static_cast<int>(upperProfile.size());

    CuttingTool wt; wt.name = "Wire"; wt.diameter = m_params.wireDiameter;
    CuttingParams cp; cp.feedRate = m_params.roughFeed;

    Toolpath rough(StrategyType::WireEDM4Axis, wt, cp);
    rough.setName("4-Axis Taper");

    const double zBot = thread.z;
    const double zTop = zBot + partHeight;

    // Rapid to thread
    {
        ToolpathPoint pt;
        pt.position = {thread.position.x, thread.position.y, zBot};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Rapid;
        rough.addPoint(pt);
    }

    // Interpolate lower and upper profiles simultaneously
    for (int i = 0; i < n; ++i) {
        const Geom::Vec2& lo = lowerProfile[i];
        const Geom::Vec2 up = (i < nUp) ? upperProfile[i]
                                         : upperProfile[i % nUp];
        ToolpathPoint pt;
        pt.position = {lo.x, lo.y, zBot};
        // Encode upper guide UV offset in toolAxis X/Y
        pt.toolAxis = {up.x - lo.x, up.y - lo.y, partHeight};
        pt.motion   = MotionType::Linear;
        rough.addPoint(pt);
    }

    // Close
    {
        const Geom::Vec2& lo = lowerProfile[0];
        const Geom::Vec2& up = (nUp > 0) ? upperProfile[0] : lowerProfile[0];
        ToolpathPoint pt;
        pt.position = {lo.x, lo.y, zBot};
        pt.toolAxis = {up.x - lo.x, up.y - lo.y, partHeight};
        pt.motion   = MotionType::Linear;
        rough.addPoint(pt);
    }

    result.roughPass = rough;

    double peri = 0.0;
    for (int i = 0; i < n; ++i) {
        const auto& a = lowerProfile[i];
        const auto& b = lowerProfile[(i+1)%n];
        peri += std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
    }
    result.estimatedTimeMin = estimateCutTime(peri, partHeight, m_params.roughFeed);
    result.coreWillFall = false; // taper parts usually require manual ejection

    return result;
}

// --------------------------------------------------------------------------
WireEDMCutResult WireEDM::noCoreCut(const std::vector<Geom::Vec2>& profile,
                                      double                         partHeight,
                                      const WireEDMThread&           thread) const {
    WireEDMCutResult result;
    if (profile.size() < 3) return result;

    CuttingTool wt; wt.name = "Wire"; wt.diameter = m_params.wireDiameter;
    CuttingParams cp; cp.feedRate = m_params.roughFeed;

    Toolpath rough(StrategyType::WireEDMNoCore, wt, cp);
    rough.setName("No-Core Cut");

    const double z = thread.z;

    // Find the bounding box centre
    double xMin = profile[0].x, xMax = profile[0].x;
    double yMin = profile[0].y, yMax = profile[0].y;
    for (const auto& v : profile) {
        xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x);
        yMin = std::min(yMin, v.y); yMax = std::max(yMax, v.y);
    }
    const double cx = (xMin + xMax) * 0.5;
    const double cy = (yMin + yMax) * 0.5;
    const double maxR = std::max(xMax - cx, yMax - cy);

    // Rapid to thread / centre
    {
        ToolpathPoint pt;
        pt.position = {thread.position.x, thread.position.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Rapid;
        rough.addPoint(pt);
    }

    // Spiral outward from centre to boundary in concentric circles ("nibble")
    const double kerf = m_params.kerf;
    const double pitch = kerf * 0.9;
    int nibbleCount = static_cast<int>(maxR / pitch) + 1;

    for (int k = 1; k <= nibbleCount; ++k) {
        double r = k * pitch;
        if (r > maxR) r = maxR;
        // Generate a small circle at radius r
        const int segs = 32;
        for (int s = 0; s <= segs; ++s) {
            double angle = 2.0 * PI_EDM * s / segs;
            ToolpathPoint pt;
            pt.position = {cx + r * std::cos(angle), cy + r * std::sin(angle), z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::Linear;
            rough.addPoint(pt);
        }
    }

    result.roughPass = rough;
    result.coreWillFall = true; // no-core strategy removes slug entirely
    result.estimatedTimeMin = 0.0; // estimated separately via profile perimeter
    return result;
}

// --------------------------------------------------------------------------
void WireEDM::addSkimPasses(WireEDMCutResult&              result,
                              const std::vector<Geom::Vec2>& baseProfile,
                              double                         /*partHeight*/,
                              const WireEDMCutParams&        params) {
    if (baseProfile.size() < 3) return;

    CuttingTool wt; wt.name = "Wire"; wt.diameter = params.wireDiameter;

    const int numSkims = static_cast<int>(params.skimOffsets.size());
    result.skimPasses.reserve(numSkims);

    for (int i = 0; i < numSkims; ++i) {
        const double offset = params.skimOffsets[i];

        // Determine if this is the reverse skim pass
        const bool isLast = (i == numSkims - 1);
        const bool isReverse = isLast &&
            (params.mode == WireEDMCutParams::CutMode::ReverseSkimCut);

        std::vector<Geom::Vec2> skimProfile = offsetProfile(baseProfile, offset);

        if (isReverse && skimProfile.size() > 1) {
            std::reverse(skimProfile.begin(), skimProfile.end());
        }

        CuttingParams cp; cp.feedRate = params.skimFeed;
        Toolpath skim(StrategyType::WireEDMSkim, wt, cp);
        skim.setName(isReverse ? "Reverse Skim" : ("Skim " + std::to_string(i + 1)));

        const double z = 0.0; // use Z from thread or pass it in later
        for (const auto& v : skimProfile) {
            ToolpathPoint pt;
            pt.position = {v.x, v.y, z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::Linear;
            skim.addPoint(pt);
        }
        // Close
        if (!skimProfile.empty()) {
            ToolpathPoint pt;
            pt.position = {skimProfile[0].x, skimProfile[0].y, z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::Linear;
            skim.addPoint(pt);
        }
        result.skimPasses.push_back(std::move(skim));
    }
}

// --------------------------------------------------------------------------
std::vector<EdmStockFeature>
WireEDM::recogniseStock(const BRep& model, double /*zTolerance*/) {
    std::vector<EdmStockFeature> features;

    // Walk the B-Rep faces; look for faces whose surface normal is close to ±Z.
    // A sequence of such faces that forms a closed loop is a through-feature.
    // (Simplified: we use the face bounding-box and UV mid-point normal check.)
    for (const auto& face : model.faces()) {
        // Get the UV mid-point normal
        Geom::Vec3 n = face.normalAt(0.5, 0.5);
        // Check if the normal is approximately horizontal (nearly ±Z is NOT what we want;
        // we want faces whose normal is nearly horizontal, i.e. the face is a vertical wall)
        const double absNz = std::abs(n.z);
        if (absNz > 0.97) {
            // This is a horizontal face (top or bottom) — skip
            continue;
        }

        // Vertical or near-vertical face: candidate wall
        // Build a simplified 2-D projection of the face loops
        EdmStockFeature feat;
        const auto& verts = face.vertices();
        for (const auto& v : verts) {
            feat.profile.push_back({v.x, v.y});
        }

        if (feat.profile.size() < 3) continue;

        // Estimate Z extents from vertices
        double zMin =  1e9, zMax = -1e9;
        for (const auto& v : verts) {
            zMin = std::min(zMin, v.z);
            zMax = std::max(zMax, v.z);
        }
        feat.topZ    = zMax;
        feat.botZ    = zMin;
        feat.isPocket = false;
        features.push_back(std::move(feat));
    }

    return features;
}

// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
WireEDM::landProfile(const std::vector<Geom::Vec2>& profile,
                      double                         landWidth,
                      double                         landOffset) {
    if (profile.size() < 3) return {};

    // Compute cumulative arc length
    const int n = static_cast<int>(profile.size());
    std::vector<double> arcLen(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        const auto& a = profile[i];
        const auto& b = profile[(i + 1) % n];
        arcLen[i + 1] = arcLen[i] + std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
    }
    const double totalLen = arcLen[n];
    if (totalLen < 1e-9) return {};

    // Wrap offset to [0, totalLen)
    double landStart = std::fmod(landOffset, totalLen);
    if (landStart < 0) landStart += totalLen;
    double landEnd = landStart + landWidth;
    if (landEnd > totalLen) landEnd -= totalLen;

    // Split profile into two segments around the land
    auto interpolate = [&](double s) -> Geom::Vec2 {
        // Find segment
        for (int i = 0; i < n; ++i) {
            if (arcLen[i] <= s && s <= arcLen[i+1]) {
                double t = (arcLen[i+1] > arcLen[i]) ?
                           (s - arcLen[i]) / (arcLen[i+1] - arcLen[i]) : 0.0;
                const auto& a = profile[i];
                const auto& b = profile[(i+1)%n];
                return {a.x + t*(b.x-a.x), a.y + t*(b.y-a.y)};
            }
        }
        return profile[0];
    };

    const Geom::Vec2 ptStart = interpolate(landStart);
    const Geom::Vec2 ptEnd   = interpolate(landEnd);

    // Segment 1: from 0 to landStart, then jump to landEnd and go to totalLen
    std::vector<Geom::Vec2> seg1, seg2;
    seg1.push_back(ptEnd);
    for (int i = 0; i < n; ++i) {
        double s = arcLen[i];
        if (s > landEnd || s < landStart) {
            seg1.push_back(profile[i]);
        }
    }
    seg1.push_back(ptStart);

    // Segment 2: just the land itself (used for the tab relief pass)
    seg2.push_back(ptStart);
    for (int i = 0; i < n; ++i) {
        double s = arcLen[i];
        if (s >= landStart && s <= landEnd) {
            seg2.push_back(profile[i]);
        }
    }
    seg2.push_back(ptEnd);

    return {seg1, seg2};
}
