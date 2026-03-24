#include "Verify.h"
#include <cmath>
#include <algorithm>

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

    int processed = 0;
    for (int opIdx = 0; opIdx < mgr->count(); ++opIdx) {
        const auto& tp  = mgr->at(opIdx);
        const auto& pts = tp.points();
        for (std::size_t i = 1; i < pts.size(); ++i) {
            subtractMove(pts[i-1], pts[i], tp.tool(), m_stockMap);
            ++processed;
            if (m_progress && (processed % 100 == 0))
                m_progress(processed * 100 / std::max(1, totalMoves));
        }
    }
    if (m_progress) m_progress(100);

    // Return a clean result (stock-compare requires target model)
    VerifyResult res;
    res.hasGouge    = false;
    res.hasUndercut = false;
    return res;
}

// --------------------------------------------------------------------------
// Subtract the swept volume of one tool move from the Z-map.
// Approximation: use the tool's flat-end footprint at each sample along the move.
// --------------------------------------------------------------------------
void Verify::subtractMove(const ToolpathPoint& from, const ToolpathPoint& to,
                           const CuttingTool& tool, ZMap& zmap) {
    // Only process cutting moves (not rapids)
    if (to.motion == MotionType::Rapid || to.motion == MotionType::Retract)
        return;

    double toolR = tool.diameter * 0.5;
    // Sample the segment at intervals of (toolR / 2)
    Geom::Vec3 dir  = to.position - from.position;
    double     len  = dir.length();
    if (len < 1e-9) return;
    int samps = std::max(2, static_cast<int>(std::ceil(len / (toolR * 0.5))));

    for (int s = 0; s <= samps; ++s) {
        double t   = static_cast<double>(s) / samps;
        Geom::Vec3 pos = from.position + dir * t;

        // Find Z-map cells within tool radius
        int xiMin = static_cast<int>((pos.x - toolR - zmap.xMin) / zmap.cellW());
        int xiMax = static_cast<int>((pos.x + toolR - zmap.xMin) / zmap.cellW()) + 1;
        int yiMin = static_cast<int>((pos.y - toolR - zmap.yMin) / zmap.cellH());
        int yiMax = static_cast<int>((pos.y + toolR - zmap.yMin) / zmap.cellH()) + 1;

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
                    // Lower the stock height to the tool tip Z if it cuts deeper
                    double& h = zmap.at(xi, yi);
                    if (pos.z < h)
                        h = pos.z;
                }
            }
        }
    }
}

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
