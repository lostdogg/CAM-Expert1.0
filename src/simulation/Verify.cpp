#include "Verify.h"
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// Verify
// --------------------------------------------------------------------------
Verify::Verify(Options opts) : m_opts(opts) {
    // Initialise stock Z-map to flat top surface
    m_stockMap.xMin = m_opts.stockXMin;
    m_stockMap.xMax = m_opts.stockXMax;
    m_stockMap.yMin = m_opts.stockYMin;
    m_stockMap.yMax = m_opts.stockYMax;
    m_stockMap.xRes = m_opts.zMapRes;
    m_stockMap.yRes = m_opts.zMapRes;
    m_stockMap.heights.assign(
        static_cast<std::size_t>(m_opts.zMapRes * m_opts.zMapRes),
        m_opts.stockZTop);
}

// --------------------------------------------------------------------------
VerifyResult Verify::run(const ToolpathManager* mgr) {
    if (!mgr) return {};

    // Reset stock
    std::fill(m_stockMap.heights.begin(), m_stockMap.heights.end(),
              m_opts.stockZTop);

    int totalMoves = 0;
    for (int i = 0; i < mgr->count(); ++i) {
        const auto& pts = mgr->at(i).points();
        totalMoves += static_cast<int>(pts.size());
    }

    // Helper: convert world XY to a clamped ZMap cell index pair.
    // Clamp the raw double value before casting to int to avoid undefined
    // behaviour when coordinates lie far outside the stock bounds.
    auto worldToCell = [&](double wx, double wy, int& xi, int& yi) {
        double rawXi = (wx - m_stockMap.xMin) / m_stockMap.cellW();
        double rawYi = (wy - m_stockMap.yMin) / m_stockMap.cellH();
        xi = static_cast<int>(std::max(0.0,
                 std::min(static_cast<double>(m_stockMap.xRes - 1), rawXi)));
        yi = static_cast<int>(std::max(0.0,
                 std::min(static_cast<double>(m_stockMap.yRes - 1), rawYi)));
    };

    VerifyResult res;
    res.hasGouge    = false;
    res.hasUndercut = false;

    int processed = 0;
    for (int opIdx = 0; opIdx < mgr->count(); ++opIdx) {
        const auto& tp  = mgr->at(opIdx);
        const auto& pts = tp.points();
        for (std::size_t i = 1; i < pts.size(); ++i) {
            // Detect a rapid-retract gouge (Z-jump dragging across the part)
            if ((pts[i].motion == MotionType::Rapid ||
                 pts[i].motion == MotionType::Retract)) {
                int xi, yi;
                worldToCell(pts[i].position.x, pts[i].position.y, xi, yi);
                double stockH = m_stockMap.at(xi, yi);
                if (pts[i].position.z < stockH) {
                    // Rapid move dips below current stock surface → flag gouge
                    res.hasGouge = true;
                    ++res.gougeCount;
                    double depth = stockH - pts[i].position.z;
                    res.maxGougeDepth = std::max(res.maxGougeDepth, depth);
                    if (res.gougeLocations.size() < 100)
                        res.gougeLocations.push_back(pts[i].position);
                }
            }

            subtractMove(pts[i-1], pts[i], tp.tool(), m_stockMap);
            ++processed;
            if (m_progress && (processed % 100 == 0))
                m_progress(processed * 100 / std::max(1, totalMoves));
        }
    }
    if (m_progress) m_progress(100);

    return res;
}

// --------------------------------------------------------------------------
// Step-through simulation: apply one move and check for a gouge.
// --------------------------------------------------------------------------
bool Verify::stepMove(const ToolpathPoint& from, const ToolpathPoint& to,
                      const CuttingTool& tool) {
    bool gouged = detectGouge(from, to, tool, m_stockMap);
    subtractMove(from, to, tool, m_stockMap);
    return gouged;
}

// --------------------------------------------------------------------------
// Detect gouge: the tool tip travels below stockZBot, meaning it cuts
// deeper than the allowed stock boundary.
// --------------------------------------------------------------------------
bool Verify::detectGouge(const ToolpathPoint& from, const ToolpathPoint& to,
                          const CuttingTool& tool, const ZMap& zmap) const {
    if (to.motion == MotionType::Rapid || to.motion == MotionType::Retract)
        return false;

    double toolR = tool.diameter * 0.5;
    Geom::Vec3 dir = to.position - from.position;
    double len = dir.length();
    if (len < 1e-9) return false;
    int samps = std::max(2, static_cast<int>(std::ceil(len / (toolR * 0.5))));

    for (int s = 0; s <= samps; ++s) {
        double t   = static_cast<double>(s) / samps;
        Geom::Vec3 pos = from.position + dir * t;
        if (pos.z < m_opts.stockZBot - m_opts.gougeTol)
            return true;

        // Also check if tip is already below the existing stock height
        int xi = static_cast<int>(std::max(0.0,
                     std::min(static_cast<double>(zmap.xRes - 1),
                              (pos.x - zmap.xMin) / zmap.cellW())));
        int yi = static_cast<int>(std::max(0.0,
                     std::min(static_cast<double>(zmap.yRes - 1),
                              (pos.y - zmap.yMin) / zmap.cellH())));
        if (pos.z < zmap.at(xi, yi) - m_opts.gougeTol)
            return true;
    }
    return false;
}
// Subtract the swept volume of one tool move from the Z-map.
// Approximation: use the tool's flat-end footprint at each sample along the move.
// --------------------------------------------------------------------------
void Verify::subtractMove(const ToolpathPoint& from, const ToolpathPoint& to,
                           const CuttingTool& tool, ZMap& zmap) {
    // Only process cutting moves (not rapids)
    if (to.motion == MotionType::Rapid || to.motion == MotionType::Retract)
        return;

    double toolR = tool.diameter * 0.5;
    // For bull-nose mills account for corner radius when sampling footprint
    double footprintR = toolR;
    if (tool.type == ToolType::BullNoseMill && tool.cornerRadius > 0.0)
        footprintR = toolR; // footprint projection is still full diameter

    // Sample the segment at intervals of (toolR / 2)
    Geom::Vec3 dir  = to.position - from.position;
    double     len  = dir.length();
    if (len < 1e-9) return;
    int samps = std::max(2, static_cast<int>(std::ceil(len / (footprintR * 0.5))));

    for (int s = 0; s <= samps; ++s) {
        double t   = static_cast<double>(s) / samps;
        Geom::Vec3 pos = from.position + dir * t;

        // Find Z-map cells within tool radius.
        // Clamp the raw double values before casting to avoid UB on out-of-bounds coords.
        int xiMin = static_cast<int>(std::max(0.0,
                        (pos.x - toolR - zmap.xMin) / zmap.cellW()));
        int xiMax = static_cast<int>(std::min(static_cast<double>(zmap.xRes - 1),
                        (pos.x + toolR - zmap.xMin) / zmap.cellW())) + 1;
        int yiMin = static_cast<int>(std::max(0.0,
                        (pos.y - toolR - zmap.yMin) / zmap.cellH()));
        int yiMax = static_cast<int>(std::min(static_cast<double>(zmap.yRes - 1),
                        (pos.y + toolR - zmap.yMin) / zmap.cellH())) + 1;

        xiMin = std::max(0, xiMin);
        xiMax = std::min(zmap.xRes - 1, xiMax);
        yiMin = std::max(0, yiMin);
        yiMax = std::min(zmap.yRes - 1, yiMax);

        for (int yi = yiMin; yi <= yiMax; ++yi) {
            for (int xi = xiMin; xi <= xiMax; ++xi) {
                double cx = zmap.xMin + xi * zmap.cellW();
                double cy = zmap.yMin + yi * zmap.cellH();
                double dx = cx - pos.x;
                double dy = cy - pos.y;
                if (dx*dx + dy*dy <= toolR*toolR) {
                    // For ball-end mills, adjust Z based on spherical tip geometry
                    double tipZ = pos.z;
                    if (tool.type == ToolType::BallEndMill) {
                        double r2 = dx*dx + dy*dy;
                        double ballR = toolR;
                        // The ball tip dips down: z_effective = z_centre - sqrt(R^2 - r^2)
                        tipZ = pos.z - (ballR - std::sqrt(std::max(0.0, ballR*ballR - r2)));
                    } else if (tool.type == ToolType::BullNoseMill &&
                               tool.cornerRadius > 0.0) {
                        // Bull-nose: blend between flat and ball at the corner
                        double flatR = toolR - tool.cornerRadius;
                        double r     = std::sqrt(dx*dx + dy*dy);
                        if (r > flatR) {
                            double dr  = r - flatR;
                            double cr  = tool.cornerRadius;
                            tipZ = pos.z - (cr - std::sqrt(std::max(0.0, cr*cr - dr*dr)));
                        }
                    }
                    // Lower the stock height to the tool tip Z if it cuts deeper
                    double& h = zmap.at(xi, yi);
                    if (tipZ < h)
                        h = tipZ;
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// Legacy compare: simplified check against target mesh minimum Z
// --------------------------------------------------------------------------
VerifyResult Verify::compare(const ZMap& stock, const MeshData& target) const {
    VerifyResult res;
    // Project each stock cell down and compare height to nearest target triangle
    // (simplified: just check if stock height is below the target model's min Z)
    double targetMinZ = 1e30;
    for (const auto& tri : target.triangles())
        for (int i = 0; i < 3; ++i)
            if (tri.v[i].z < targetMinZ)
                targetMinZ = tri.v[i].z;

    for (int yi = 0; yi < stock.yRes; ++yi) {
        for (int xi = 0; xi < stock.xRes; ++xi) {
            double h = stock.at(xi, yi);
            if (h < targetMinZ - m_opts.gougeTol) {
                res.hasGouge = true;
                ++res.gougeCount;
                res.maxGougeDepth = std::max(res.maxGougeDepth,
                                              targetMinZ - h);
                double cx = stock.xMin + xi * stock.cellW();
                double cy = stock.yMin + yi * stock.cellH();
                if (res.gougeLocations.size() < 100)
                    res.gougeLocations.push_back({cx, cy, h});
            }
        }
    }
    return res;
}

// --------------------------------------------------------------------------
MeshData Verify::zMapToMesh(const ZMap& zmap) {
    std::vector<Geom::Triangle> tris;
    tris.reserve(static_cast<std::size_t>(2 * (zmap.xRes-1) * (zmap.yRes-1)));

    for (int yi = 0; yi + 1 < zmap.yRes; ++yi) {
        for (int xi = 0; xi + 1 < zmap.xRes; ++xi) {
            double x0 = zmap.xMin +  xi    * zmap.cellW();
            double x1 = zmap.xMin + (xi+1) * zmap.cellW();
            double y0 = zmap.yMin +  yi    * zmap.cellH();
            double y1 = zmap.yMin + (yi+1) * zmap.cellH();
            double h00 = zmap.at(xi,   yi);
            double h10 = zmap.at(xi+1, yi);
            double h01 = zmap.at(xi,   yi+1);
            double h11 = zmap.at(xi+1, yi+1);

            tris.push_back({{ {x0,y0,h00}, {x1,y0,h10}, {x1,y1,h11} }});
            tris.push_back({{ {x0,y0,h00}, {x1,y1,h11}, {x0,y1,h01} }});
        }
    }
    return MeshData(std::move(tris));
}

// ==========================================================================
// StockCompare implementation
// ==========================================================================

StockCompare::StockCompare(double toleranceMm)
    : m_tolerance(toleranceMm) {}

// --------------------------------------------------------------------------
// compare() – iterate every ZMap cell, compute deviation from the target
// mesh, and assign a heat-map colour.
//
// Strategy: for each (xi, yi) cell, cast a vertical ray at the cell centre
// and find the highest triangle intersection with the target mesh that lies
// at or above the as-machined stock height.  The deviation is:
//   deviation = stock_height - target_Z
//   < -tolerance  → Red   (gouge)
//   > +tolerance  → Blue  (excess material)
//   otherwise     → Green (within tolerance)
// --------------------------------------------------------------------------
StockCompareResult StockCompare::compare(const ZMap& stock,
                                          const MeshData& target) const {
    StockCompareResult result;
    result.tolerance    = m_tolerance;
    result.minDeviation = 0.0;
    result.maxDeviation = 0.0;

    const std::size_t numCells =
        static_cast<std::size_t>(stock.xRes) * static_cast<std::size_t>(stock.yRes);
    result.cells.reserve(numCells);

    for (int yi = 0; yi < stock.yRes; ++yi) {
        for (int xi = 0; xi < stock.xRes; ++xi) {
            double cx = stock.xMin + (xi + 0.5) * stock.cellW();
            double cy = stock.yMin + (yi + 0.5) * stock.cellH();
            double stockH = stock.at(xi, yi);

            // Cast a ray upward from below and find the highest target surface Z
            Geom::Ray ray;
            ray.origin    = {cx, cy, -1e6};
            ray.direction = {0.0, 0.0,  1.0};

            double targetZ = -1e30;
            bool   hit     = false;
            for (const auto& tri : target.triangles()) {
                double tHit = 0.0;
                if (Geom::rayTriangleIntersect(ray, tri, tHit) && tHit > 0.0) {
                    double z = ray.at(tHit).z;
                    if (z > targetZ) {
                        targetZ = z;
                        hit     = true;
                    }
                }
            }

            StockCompareCell cell;
            cell.x = cx;
            cell.y = cy;

            if (!hit) {
                // No target surface here – stock should be untouched
                cell.deviation = 0.0;
                cell.color     = StockCompareColor::Green;
            } else {
                // deviation = stock_height – target_Z
                //   negative → tool went too deep (gouge, Red)
                //   positive → tool didn't cut enough (excess, Blue)
                cell.deviation = stockH - targetZ;

                if (cell.deviation < -m_tolerance) {
                    cell.color = StockCompareColor::Red;
                    ++result.gougeCount;
                } else if (cell.deviation > m_tolerance) {
                    cell.color = StockCompareColor::Blue;
                    ++result.excessCount;
                } else {
                    cell.color = StockCompareColor::Green;
                }

                result.minDeviation = std::min(result.minDeviation, cell.deviation);
                result.maxDeviation = std::max(result.maxDeviation, cell.deviation);
            }

            result.cells.push_back(cell);
        }
    }

    return result;
}

// --------------------------------------------------------------------------
double StockCompare::deviationAt(const StockCompareResult& result,
                                  const ZMap& stock,
                                  double worldX, double worldY) {
    // Find the nearest cell index; clamp before cast to avoid UB on large coords.
    int xi = static_cast<int>(std::max(0.0,
                 std::min(static_cast<double>(stock.xRes - 1),
                          (worldX - stock.xMin) / stock.cellW())));
    int yi = static_cast<int>(std::max(0.0,
                 std::min(static_cast<double>(stock.yRes - 1),
                          (worldY - stock.yMin) / stock.cellH())));

    std::size_t idx = static_cast<std::size_t>(yi * stock.xRes + xi);
    if (idx >= result.cells.size()) return 0.0;
    return result.cells[idx].deviation;
}

// ==========================================================================
// §4.6 ProbeSimulation
// ==========================================================================

ProbeSimulation::ProbeSimulation(ProbeSimOptions opts)
    : m_opts(std::move(opts)) {}

bool ProbeSimulation::isWithinTolerance(const ProbeContact& c, double toleranceMm) {
    return std::abs(c.deviation) <= toleranceMm;
}

bool ProbeSimulation::findContact(const Geom::Vec3& from,
                                    const Geom::Vec3& to,
                                    double stylusRadius,
                                    const ZMap& stock,
                                    Geom::Vec3& contactPt) const {
    // Walk the move in small steps and check probe ball against stock Z
    Geom::Vec3 dir = {to.x - from.x, to.y - from.y, to.z - from.z};
    double len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len < 1e-9) return false;
    dir.x /= len; dir.y /= len; dir.z /= len;

    const double stepMm = std::min(0.1, len / 100.0);
    const int steps = static_cast<int>(std::ceil(len / stepMm));

    const double xRange = stock.xMax - stock.xMin;
    const double yRange = stock.yMax - stock.yMin;
    if (xRange <= 0.0 || yRange <= 0.0) return false;

    for (int i = 0; i <= steps; ++i) {
        double t  = i * stepMm;
        Geom::Vec3 p = {from.x + dir.x * t,
                         from.y + dir.y * t,
                         from.z + dir.z * t};

        // Map (x,y) to zmap grid
        double fx = (p.x - stock.xMin) / xRange * (stock.xRes - 1);
        double fy = (p.y - stock.yMin) / yRange * (stock.yRes - 1);
        int xi = static_cast<int>(std::clamp(fx, 0.0, double(stock.xRes - 1)));
        int yi = static_cast<int>(std::clamp(fy, 0.0, double(stock.yRes - 1)));

        double stockZ = stock.at(xi, yi);

        // Probe tip is at (p.z - stylusRadius); contact when tip Z <= stockZ
        if ((p.z - stylusRadius) <= stockZ + 1e-6) {
            contactPt = {p.x, p.y, stockZ + stylusRadius};
            return true;
        }
    }
    return false;
}

ProbeSimResult
ProbeSimulation::simulate(const Toolpath& probePath, const ZMap& stock) const {
    ProbeSimResult result;

    const auto& pts = probePath.points();
    if (pts.empty()) return result;

    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const auto& from = pts[i];
        const auto& to   = pts[i + 1];

        if (to.motion != MotionType::Linear) continue;

        Geom::Vec3 contactPt;
        bool hit = findContact(from.position, to.position,
                                m_opts.stylusRadius, stock, contactPt);
        if (!hit) continue;

        ProbeContact c;
        c.actual    = contactPt;
        c.nominal   = to.position; // treat move endpoint as nominal
        c.deviation = contactPt.z - to.position.z;
        c.withinTol = std::abs(c.deviation) <= m_opts.toleranceMm;

        // Early contact: hit before the last step of the move
        double moveLen2 = 0.0;
        {
            double dx = to.position.x - from.position.x;
            double dy = to.position.y - from.position.y;
            double dz = to.position.z - from.position.z;
            moveLen2 = dx*dx + dy*dy + dz*dz;
        }
        double hitDist2 = 0.0;
        {
            double dx = contactPt.x - from.position.x;
            double dy = contactPt.y - from.position.y;
            double dz = contactPt.z - from.position.z;
            hitDist2 = dx*dx + dy*dy + dz*dz;
        }
        c.earlyContact = (hitDist2 < moveLen2 * 0.95);

        result.contacts.push_back(c);
        ++result.contactCount;
        if (!c.withinTol)    ++result.outOfTolCount;
        if (c.earlyContact)  ++result.earlyContactCount;
        if (c.deviation > result.maxDeviation) result.maxDeviation = c.deviation;
        if (c.deviation < result.minDeviation) result.minDeviation = c.deviation;
    }

    return result;
}
