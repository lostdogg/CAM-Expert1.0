#include "Router.h"
#include <cmath>
#include <algorithm>
#include <numeric>

static constexpr double PI_ROUTER = 3.14159265358979323846;

// --------------------------------------------------------------------------
Router::Router(RouterParams params)
    : m_params(std::move(params)) {}

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------
double Router::profilePerimeter(const std::vector<Geom::Vec2>& profile) {
    double peri = 0.0;
    const int n = static_cast<int>(profile.size());
    for (int i = 0; i < n; ++i) {
        const auto& a = profile[i];
        const auto& b = profile[(i + 1) % n];
        peri += std::sqrt((b.x-a.x)*(b.x-a.x) + (b.y-a.y)*(b.y-a.y));
    }
    return peri;
}

std::vector<Geom::Vec2>
Router::offsetInward(const std::vector<Geom::Vec2>& boundary, double amount) {
    if (boundary.size() < 3) return boundary;
    const int n = static_cast<int>(boundary.size());
    std::vector<Geom::Vec2> result;
    result.reserve(n);

    for (int i = 0; i < n; ++i) {
        const Geom::Vec2& prev = boundary[(i + n - 1) % n];
        const Geom::Vec2& curr = boundary[i];
        const Geom::Vec2& next = boundary[(i + 1) % n];

        double ax = curr.x - prev.x, ay = curr.y - prev.y;
        double bx = next.x - curr.x, by = next.y - curr.y;
        double la = std::sqrt(ax*ax + ay*ay);
        double lb = std::sqrt(bx*bx + by*by);
        if (la < 1e-12 || lb < 1e-12) { result.push_back(curr); continue; }

        double nx1 = -ay / la, ny1 = ax / la;
        double nx2 = -by / lb, ny2 = bx / lb;
        double bsx = nx1 + nx2, bsy = ny1 + ny2;
        double bsl = std::sqrt(bsx*bsx + bsy*bsy);
        if (bsl < 1e-12) { result.push_back(curr); continue; }
        bsx /= bsl; bsy /= bsl;

        double sinH = std::clamp(bsx * nx1 + bsy * ny1, -1.0, 1.0);
        if (std::abs(sinH) < 1e-6) { result.push_back(curr); continue; }
        double scale = amount / sinH;

        result.push_back({curr.x + bsx * scale, curr.y + bsy * scale});
    }
    return result;
}

Toolpath Router::buildContourPass(const std::vector<Geom::Vec2>& profile,
                                    double z,
                                    double feedRate,
                                    double leadInR) const {
    CuttingTool dummy; dummy.name = "Router Bit";
    CuttingParams cp; cp.feedRate = feedRate;
    Toolpath tp(StrategyType::RouterNested, dummy, cp);

    if (profile.empty()) return tp;

    // Tangential lead-in arc before first cut point
    if (leadInR > 1e-6 && profile.size() >= 2) {
        // Compute direction from last profile point to first
        const Geom::Vec2& p0 = profile[0];
        const Geom::Vec2& p1 = profile[1];
        double dx = p1.x - p0.x, dy = p1.y - p0.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len > 1e-9) {
            dx /= len; dy /= len;
            // Approach from perpendicular to the lead-in
            double sx = p0.x - dy * leadInR;
            double sy = p0.y + dx * leadInR;
            {
                ToolpathPoint rapid;
                rapid.position = {sx, sy, z};
                rapid.toolAxis = {0, 0, 1};
                rapid.motion   = MotionType::Rapid;
                tp.addPoint(rapid);
            }
            {
                ToolpathPoint arc;
                arc.position   = {p0.x, p0.y, z};
                arc.toolAxis   = {0, 0, 1};
                arc.motion     = MotionType::ArcCCW;
                arc.arcRadius  = leadInR;
                arc.arcCenter  = {p0.x - dx * leadInR, p0.y - dy * leadInR, z};
                tp.addPoint(arc);
            }
        }
    } else {
        // Simple rapid to start
        ToolpathPoint rapid;
        rapid.position = {profile[0].x, profile[0].y, z};
        rapid.toolAxis = {0, 0, 1};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);
    }

    // Follow the profile
    for (const auto& v : profile) {
        ToolpathPoint pt;
        pt.position = {v.x, v.y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    // Close
    {
        ToolpathPoint pt;
        pt.position = {profile[0].x, profile[0].y, z};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    return tp;
}

// --------------------------------------------------------------------------
// contour()
// --------------------------------------------------------------------------
Toolpath Router::contour(const std::vector<Geom::Vec2>& profile,
                           const RouterParams&             p,
                           const CuttingTool&              bit,
                           const CuttingParams&            cuts,
                           const std::vector<RouterTab>&   tabs) const {
    Toolpath combined(StrategyType::RouterNested, bit, cuts);
    combined.setName("Router Contour");

    if (profile.size() < 3) return combined;

    const int passes = (p.stepDown > 0.0)
        ? static_cast<int>(std::ceil(p.depth / p.stepDown)) : 1;

    for (int pass = 0; pass < passes; ++pass) {
        double z = -std::min(p.depth, (pass + 1) * p.stepDown);

        if (!tabs.empty()) {
            // Insert tab lift moves
            auto segs = insertTabs(profile, tabs);
            for (int si = 0; si < static_cast<int>(segs.size()); ++si) {
                const auto& seg = segs[si];
                if (seg.empty()) continue;

                // Plunge to cut depth at start of segment
                {
                    ToolpathPoint plunge;
                    plunge.position = {seg[0].x, seg[0].y, z};
                    plunge.toolAxis = {0, 0, 1};
                    plunge.motion   = MotionType::PlungeFeed;
                    combined.addPoint(plunge);
                }

                for (const auto& v : seg) {
                    ToolpathPoint pt;
                    pt.position = {v.x, v.y, z};
                    pt.toolAxis = {0, 0, 1};
                    pt.motion   = MotionType::Linear;
                    combined.addPoint(pt);
                }

                // Lift over tab (if not the last segment)
                if (si < static_cast<int>(segs.size()) - 1 && pass == passes - 1) {
                    // Find the matching tab height
                    double tabZ = z + (tabs.empty() ? 0.0 : tabs[0].height);
                    ToolpathPoint lift;
                    lift.position = {combined.points().back().position.x,
                                     combined.points().back().position.y, tabZ};
                    lift.toolAxis = {0, 0, 1};
                    lift.motion   = MotionType::Linear;
                    combined.addPoint(lift);
                }
            }
        } else {
            Toolpath pass_tp = buildContourPass(profile, z, p.feedRate, p.leadInRadius);
            for (const auto& pt : pass_tp.points()) {
                combined.addPoint(pt);
            }
        }
    }

    return combined;
}

// --------------------------------------------------------------------------
// pocket()
// --------------------------------------------------------------------------
Toolpath Router::pocket(const std::vector<Geom::Vec2>& boundary,
                          const RouterParams&             p,
                          const CuttingTool&              bit,
                          const CuttingParams&            cuts) const {
    Toolpath combined(StrategyType::RouterNested, bit, cuts);
    combined.setName("Router Pocket");

    if (boundary.size() < 3) return combined;

    const int zPasses = (p.stepDown > 0.0)
        ? static_cast<int>(std::ceil(p.depth / p.stepDown)) : 1;

    for (int zp = 0; zp < zPasses; ++zp) {
        const double z = -std::min(p.depth, (zp + 1) * p.stepDown);
        const double stepOverMm = p.stepOver * bit.diameter;

        // Build concentric offset loops from boundary inward
        std::vector<Geom::Vec2> current = boundary;
        while (current.size() >= 3) {
            Toolpath pass_tp = buildContourPass(current, z, p.feedRate, 0.0);
            for (const auto& pt : pass_tp.points()) {
                combined.addPoint(pt);
            }
            // Shrink
            auto shrunk = offsetInward(current, stepOverMm);
            if (shrunk.size() < 3) break;
            // Check if the area collapsed
            double area = 0.0;
            for (int i = 0; i < static_cast<int>(shrunk.size()); ++i) {
                const auto& a = shrunk[i];
                const auto& b = shrunk[(i+1)%shrunk.size()];
                area += (a.x * b.y - b.x * a.y);
            }
            if (std::abs(area) < stepOverMm * stepOverMm) break;
            current = shrunk;
        }
    }

    return combined;
}

// --------------------------------------------------------------------------
// scoreAndSnap()
// --------------------------------------------------------------------------
Toolpath Router::scoreAndSnap(const std::vector<Geom::Vec2>& line,
                                double                         sheetThickness,
                                const CuttingTool&             vBit,
                                const CuttingParams&           cuts) const {
    Toolpath tp(StrategyType::RouterNested, vBit, cuts);
    tp.setName("Score and Snap");

    const double scoreZ = -sheetThickness / 3.0;  // score to 1/3 thickness

    if (line.empty()) return tp;

    {
        ToolpathPoint rapid;
        rapid.position = {line[0].x, line[0].y, 5.0};
        rapid.toolAxis = {0, 0, 1};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);
    }

    for (const auto& v : line) {
        ToolpathPoint pt;
        pt.position = {v.x, v.y, scoreZ};
        pt.toolAxis = {0, 0, 1};
        pt.motion   = MotionType::Linear;
        tp.addPoint(pt);
    }

    return tp;
}

// --------------------------------------------------------------------------
// aggregateOp()
// --------------------------------------------------------------------------
std::vector<Toolpath>
Router::aggregateOp(const std::vector<Geom::Vec2>& positions,
                     const AggregateHead&            head,
                     double                         sheetDepth,
                     const CuttingParams&            cuts) const {
    std::vector<Toolpath> result;

    CuttingTool agTool;
    agTool.name     = head.description.empty() ? "Aggregate" : head.description;
    agTool.type     = ToolType::DrillBit;
    agTool.diameter = head.diameter;

    // One toolpath per spindle in the aggregate bank
    for (int s = 0; s < head.spindleCount; ++s) {
        const double spindleOffsetY = s * head.spindleSpacing;

        CuttingParams cp = cuts;
        cp.spindleRPM = head.spindleRPM;

        Toolpath tp(StrategyType::RouterAggregate, agTool, cp);
        tp.setName("Aggregate Spindle " + std::to_string(s + 1));

        for (const auto& pos : positions) {
            // Rapid above
            {
                ToolpathPoint rapid;
                rapid.position = {pos.x, pos.y + spindleOffsetY, 5.0};
                rapid.toolAxis = {0, 0, 1};
                rapid.motion   = MotionType::Rapid;
                tp.addPoint(rapid);
            }
            // Plunge to depth
            {
                ToolpathPoint plunge;
                plunge.position = {pos.x, pos.y + spindleOffsetY, -head.depth};
                plunge.toolAxis = {0, 0, 1};
                plunge.motion   = MotionType::PlungeFeed;
                tp.addPoint(plunge);
            }
            // Retract
            {
                ToolpathPoint retract;
                retract.position = {pos.x, pos.y + spindleOffsetY, 5.0};
                retract.toolAxis = {0, 0, 1};
                retract.motion   = MotionType::Retract;
                tp.addPoint(retract);
            }
        }

        result.push_back(std::move(tp));
    }

    return result;
}

// --------------------------------------------------------------------------
// route5Axis()
// --------------------------------------------------------------------------
Toolpath Router::route5Axis(const NurbsSurface&    surf,
                               const Router5AxisParams& p,
                               const CuttingTool&     bit,
                               const CuttingParams&   cuts) const {
    Toolpath tp(StrategyType::Router5Axis, bit, cuts);
    tp.setName("5-Axis Route");

    // Build parallel passes along U at fixed V intervals
    const int vPasses = static_cast<int>(
        std::ceil(surf.vMax() / std::max(p.stepOver, 0.1)));

    for (int vp = 0; vp <= vPasses; ++vp) {
        const double v = surf.vMin() + (surf.vMax() - surf.vMin())
                         * vp / static_cast<double>(vPasses);

        const bool reverse = p.reverseAlternate && (vp % 2 == 1);
        const int uSteps = 40;

        for (int ui = 0; ui <= uSteps; ++ui) {
            const int uIdx = reverse ? (uSteps - ui) : ui;
            const double u = surf.uMin() + (surf.uMax() - surf.uMin())
                             * uIdx / static_cast<double>(uSteps);

            Geom::Vec3 pos = surf.evaluate(u, v);
            Geom::Vec3 n   = surf.normal(u, v);

            // Apply normal offset
            pos.x += n.x * p.normalOffset;
            pos.y += n.y * p.normalOffset;
            pos.z += n.z * p.normalOffset;

            // Apply lead angle: tilt tool axis in feed direction
            Geom::Vec3 axis = n;
            if (std::abs(p.leadAngle) > 0.001) {
                double leadRad = p.leadAngle * PI_ROUTER / 180.0;
                // Feed direction approximation: along U tangent
                Geom::Vec3 tu = surf.tangentU(u, v);
                double sign = reverse ? -1.0 : 1.0;
                axis.x += sign * tu.x * std::sin(leadRad);
                axis.y += sign * tu.y * std::sin(leadRad);
                axis.z += sign * tu.z * std::sin(leadRad);
                double len = std::sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
                if (len > 1e-9) { axis.x /= len; axis.y /= len; axis.z /= len; }
            }

            ToolpathPoint pt;
            pt.position = pos;
            pt.toolAxis = axis;
            pt.motion   = (ui == 0) ? MotionType::Rapid : MotionType::Linear;
            pt.feedOverride = 1.0;
            tp.addPoint(pt);
        }
    }

    return tp;
}

// --------------------------------------------------------------------------
// nestParts() – bottom-left guillotine nesting
// --------------------------------------------------------------------------
NestingResult Router::nestParts(const std::vector<PartOutline>& parts,
                                  const RouterSheet&               sheet,
                                  double                           gapMm) {
    NestingResult result;
    if (parts.empty()) return result;

    // Bounding-box helper
    auto bbox = [](const std::vector<Geom::Vec2>& poly,
                   double& w, double& h) {
        double xMin =  1e9, xMax = -1e9, yMin =  1e9, yMax = -1e9;
        for (const auto& v : poly) {
            xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x);
            yMin = std::min(yMin, v.y); yMax = std::max(yMax, v.y);
        }
        w = xMax - xMin; h = yMax - yMin;
    };

    // Expand part list for quantities
    struct PlacePart {
        const PartOutline* proto;
        int copyIndex;
        double bw, bh; // bounding box dimensions
    };
    std::vector<PlacePart> queue;
    for (const auto& po : parts) {
        double w, h;
        bbox(po.boundary, w, h);
        for (int q = 0; q < po.quantity; ++q) {
            queue.push_back({&po, q, w, h});
        }
    }

    // Sort by area descending
    std::sort(queue.begin(), queue.end(), [](const PlacePart& a, const PlacePart& b) {
        return (a.bw * a.bh) > (b.bw * b.bh);
    });

    // Simple bottom-left packing: skyline of current heights per column
    // For simplicity, use a 1-D shelf packing on Y (rows)
    double cursorX = 0.0;
    double cursorY = 0.0;
    double rowHeight = 0.0;

    double totalPartArea = 0.0;

    for (const auto& pp : queue) {
        double pw = pp.bw + gapMm;
        double ph = pp.bh + gapMm;

        // Try to rotate if it fits better and rotation is allowed
        if (pp.proto->canRotate && (cursorX + ph <= sheet.width)
            && ph < pw)
        {
            std::swap(pw, ph);
        }

        // Move to next row if needed
        if (cursorX + pw > sheet.width + 1e-6) {
            cursorX = 0.0;
            cursorY += rowHeight;
            rowHeight = 0.0;
        }

        if (cursorY + ph > sheet.height + 1e-6) {
            // Doesn't fit – mark as unplaced
            result.unplacedCount++;
            continue;
        }

        NestedPlacement pl;
        pl.partId    = pp.proto->partId;
        pl.copyIndex = pp.copyIndex;
        pl.origin    = {cursorX, cursorY};
        pl.rotation  = (pw == pp.bh + gapMm) ? 90.0 : 0.0;
        result.placements.push_back(pl);

        totalPartArea += pp.bw * pp.bh;
        cursorX  += pw;
        rowHeight = std::max(rowHeight, ph);
    }

    const double sheetArea = sheet.width * sheet.height;
    result.sheetUtilisation = (sheetArea > 0.0) ? totalPartArea / sheetArea : 0.0;
    result.totalScrapMm2    = sheetArea - totalPartArea;
    return result;
}

// --------------------------------------------------------------------------
// insertTabs()
// --------------------------------------------------------------------------
std::vector<std::vector<Geom::Vec2>>
Router::insertTabs(const std::vector<Geom::Vec2>& profile,
                    const std::vector<RouterTab>&  tabs) {
    if (tabs.empty()) return {profile};
    if (profile.size() < 2) return {profile};

    // Compute cumulative arc lengths
    const int n = static_cast<int>(profile.size());
    std::vector<double> arcLen(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        const auto& a = profile[i];
        const auto& b = profile[(i+1)%n];
        arcLen[i+1] = arcLen[i] + std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
    }
    const double total = arcLen[n];

    // Collect tab intervals [start, end) along arc length, sorted
    struct Interval { double start, end; };
    std::vector<Interval> intervals;
    for (const auto& tab : tabs) {
        if (!tab.isActive) continue;
        double s = std::fmod(tab.profileDistance, total);
        if (s < 0) s += total;
        double e = s + tab.width;
        if (e > total) e = total; // clip at end
        intervals.push_back({s, e});
    }
    std::sort(intervals.begin(), intervals.end(),
              [](const Interval& a, const Interval& b){ return a.start < b.start; });

    // Split profile at tab boundaries into sub-segments
    std::vector<std::vector<Geom::Vec2>> segments;
    std::vector<Geom::Vec2> current;
    int tabIdx = 0;
    double walked = 0.0;
    current.push_back(profile[0]);

    for (int i = 0; i < n; ++i) {
        const auto& a = profile[i];
        const auto& b = profile[(i+1)%n];
        double segLen = std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
        double segEnd = walked + segLen;

        // Check if any tab boundary falls within this segment
        while (tabIdx < static_cast<int>(intervals.size()) &&
               intervals[tabIdx].start < segEnd) {
            double tStart = intervals[tabIdx].start;
            double tEnd   = intervals[tabIdx].end;

            if (tStart > walked) {
                // Add point at tab start
                double t = (tStart - walked) / segLen;
                Geom::Vec2 pt = {a.x + t*(b.x-a.x), a.y + t*(b.y-a.y)};
                current.push_back(pt);
                segments.push_back(current);
                current.clear();
                current.push_back(pt);
            }

            if (tEnd < segEnd) {
                // Add point at tab end
                double t2 = (tEnd - walked) / segLen;
                Geom::Vec2 pt2 = {a.x + t2*(b.x-a.x), a.y + t2*(b.y-a.y)};
                current.push_back(pt2);
                tabIdx++;
            } else {
                break;
            }
        }

        current.push_back(b);
        walked = segEnd;
    }
    if (!current.empty()) segments.push_back(current);

    return segments.empty() ? std::vector<std::vector<Geom::Vec2>>{profile} : segments;
}

// --------------------------------------------------------------------------
// autoGenerateTabs()
// --------------------------------------------------------------------------
std::vector<RouterTab>
Router::autoGenerateTabs(const std::vector<Geom::Vec2>& profile,
                           int    tabCount,
                           double tabWidthMm,
                           double tabHeightMm) {
    std::vector<RouterTab> tabs;
    if (profile.size() < 3 || tabCount <= 0) return tabs;

    double peri = profilePerimeter(profile);
    if (peri < 1e-9) return tabs;

    const double spacing = peri / tabCount;

    for (int i = 0; i < tabCount; ++i) {
        RouterTab tab;
        tab.profileDistance = i * spacing + spacing * 0.5;
        tab.width           = tabWidthMm;
        tab.height          = tabHeightMm;
        tab.isActive        = true;
        tabs.push_back(tab);
    }

    return tabs;
}
