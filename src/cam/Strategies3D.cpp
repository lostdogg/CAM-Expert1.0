#include "Strategies3D.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>
#if defined(CAMEXPERT_USE_OPENMP)
#include <omp.h>
#endif

static constexpr double PI3D = 3.14159265358979323846;

// --------------------------------------------------------------------------
// Scallop height: h = R - √(R² - (Sₒ/2)²)
//
// Predicts the height of the residual cusp left between adjacent tool passes.
// Used by the scallop strategy to verify surface finish before cutting.
// --------------------------------------------------------------------------
double Strategies3D::scallopHeight(double toolRadius, double stepOver) {
    double half = stepOver * 0.5;
    if (half >= toolRadius) return toolRadius; // degenerate: full step
    return toolRadius - std::sqrt(toolRadius * toolRadius - half * half);
}

// --------------------------------------------------------------------------
// Inverse: compute the stepover that achieves a target scallop height.
//   Sₒ = 2·√( R² - (R - h)² )
// --------------------------------------------------------------------------
double Strategies3D::stepOverFromScallop(double toolRadius, double targetScallopHeight) {
    double h = std::max(0.0, std::min(targetScallopHeight, toolRadius));
    return 2.0 * std::sqrt(toolRadius * toolRadius - (toolRadius - h) * (toolRadius - h));
}


bool Strategies3D::rayTriIntersect(const Geom::Ray& ray,
                                    const Geom::Triangle& tri,
                                    double& t) {
    const double EPS = 1e-9;
    Geom::Vec3 e1 = tri.v[1] - tri.v[0];
    Geom::Vec3 e2 = tri.v[2] - tri.v[0];
    Geom::Vec3 h  = ray.direction.cross(e2);
    double     a  = e1.dot(h);
    if (std::abs(a) < EPS) return false; // parallel

    double f = 1.0 / a;
    Geom::Vec3 s = ray.origin - tri.v[0];
    double     u = f * s.dot(h);
    if (u < 0 || u > 1) return false;

    Geom::Vec3 q = s.cross(e1);
    double     v = f * ray.direction.dot(q);
    if (v < 0 || u + v > 1) return false;

    t = f * e2.dot(q);
    return t > EPS;
}

// --------------------------------------------------------------------------
bool Strategies3D::projectOntoMesh(const MeshData& mesh,
                                    double x, double y, double& z) {
    Geom::Ray ray{{x, y, 1e6}, {0, 0, -1}};
    double tMin = std::numeric_limits<double>::max();
    bool   hit  = false;

    const auto& tris = mesh.triangles();
#if defined(CAMEXPERT_USE_OPENMP)
    double localMin = std::numeric_limits<double>::max();
    int hitInt = 0;
#pragma omp parallel for reduction(min:localMin) reduction(|:hitInt) if(tris.size() > 1000)
    for (int i = 0; i < static_cast<int>(tris.size()); ++i) {
        double t = 0.0;
        if (rayTriIntersect(ray, tris[static_cast<std::size_t>(i)], t)) {
            hitInt |= 1;
            if (t < localMin) localMin = t;
        }
    }
    hit = (hitInt != 0);
    tMin = localMin;
#else
    for (const auto& tri : tris) {
        double t;
        if (rayTriIntersect(ray, tri, t) && t < tMin) {
            tMin = t;
            hit  = true;
        }
    }
#endif
    if (hit) z = 1e6 - tMin;
    return hit;
}

// --------------------------------------------------------------------------
bool Strategies3D::isGouge(const Geom::Vec3& tipPos,
                             const Geom::Vec3& /*toolAxis*/,
                             double toolRadius,
                             const MeshData& mesh) {
    // Simple sphere-triangle gouge check
    for (const auto& tri : mesh.triangles()) {
        Geom::Vec3 n = tri.normal();
        double d     = (tipPos - tri.v[0]).dot(n);
        if (d < -toolRadius)
            return true; // tool centre penetrates below surface
    }
    return false;
}

// --------------------------------------------------------------------------
// Waterline (Z-level) toolpath on a NURBS surface
// --------------------------------------------------------------------------
Toolpath Strategies3D::waterline(const NurbsSurface& surf,
                                   const WaterlineParams& p,
                                   const CuttingTool& tool,
                                   const CuttingParams& cuts) {
    Toolpath tp(StrategyType::WaterlineRough, tool, cuts);
    tp.setName("Waterline 3D");

    double safeZ = p.topZ + 5.0;
    int    uRes  = 60, vRes = 60;

    for (double z = p.topZ; z >= p.bottomZ; z -= p.zStep) {
        // Collect points on the surface near this Z level
        std::vector<Geom::Vec3> contour;
        for (int i = 0; i <= uRes; ++i) {
            double u = surf.uMin() + (surf.uMax() - surf.uMin()) * i / uRes;
            for (int j = 0; j <= vRes; ++j) {
                double v  = surf.vMin() + (surf.vMax() - surf.vMin()) * j / vRes;
                Geom::Vec3 pt = surf.evaluate(u, v);
                if (std::abs(pt.z - z) < p.zStep * 0.5)
                    contour.push_back(pt);
            }
        }

        if (contour.empty()) continue;

        // Sort by nearest-neighbour chain rather than pure angular sort.
        // Angular sort fails for U-shaped or concave contours; nearest-
        // neighbour chains produce a connected path along the level set.
        {
            std::vector<Geom::Vec3> sorted;
            sorted.reserve(contour.size());
            std::vector<bool> used(contour.size(), false);

            // Start from the point with the smallest X (deterministic start)
            std::size_t startIdx = 0;
            for (std::size_t k = 1; k < contour.size(); ++k)
                if (contour[k].x < contour[startIdx].x) startIdx = k;

            sorted.push_back(contour[startIdx]);
            used[startIdx] = true;

            while (sorted.size() < contour.size()) {
                const Geom::Vec3& last = sorted.back();
                double bestDist = 1e30;
                std::size_t bestIdx = 0;
                for (std::size_t k = 0; k < contour.size(); ++k) {
                    if (used[k]) continue;
                    auto d = contour[k] - last;
                    double dist = d.x*d.x + d.y*d.y + d.z*d.z;
                    if (dist < bestDist) { bestDist = dist; bestIdx = k; }
                }
                sorted.push_back(contour[bestIdx]);
                used[bestIdx] = true;
            }
            contour = sorted;
        }

        // Rapid to start of level
        if (tp.points().empty()) {
            ToolpathPoint safe;
            safe.position = {contour[0].x, contour[0].y, safeZ};
            safe.toolAxis = {0, 0, 1};
            safe.motion   = MotionType::Rapid;
            tp.addPoint(safe);
        }

        // Offset tool from surface by tool radius
        for (const auto& pt : contour) {
            Geom::Vec3 n = surf.normal(
                surf.uMin() + (surf.uMax() - surf.uMin()) * 0.5,
                surf.vMin() + (surf.vMax() - surf.vMin()) * 0.5);
            ToolpathPoint tpt;
            tpt.position = pt + n * (tool.diameter * 0.5 + p.stockAllowance);
            tpt.toolAxis = {0, 0, 1};
            tpt.motion   = MotionType::Linear;
            tp.addPoint(tpt);
        }
        // Close loop
        if (!contour.empty()) {
            ToolpathPoint close;
            close.position = tp.points().back().position;
            close.position.z = safeZ;
            close.toolAxis = {0, 0, 1};
            close.motion   = MotionType::Retract;
            tp.addPoint(close);
        }
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Raster on a mesh (parallel X lines, projected onto surface)
// --------------------------------------------------------------------------
Toolpath Strategies3D::raster(const MeshData& mesh,
                                const RasterParams& p,
                                const CuttingTool& tool,
                                const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Raster3D, tool, cuts);
    tp.setName("Raster 3D");

    auto bbox = mesh.boundingBox();
    if (!bbox.isValid()) { tp.markClean(); return tp; }

    double safeZ  = bbox.max.z + 5.0;
    double stepO  = tool.diameter * p.stepOver;
    double cosA   = std::cos(p.angle * PI3D / 180.0);
    double sinA   = std::sin(p.angle * PI3D / 180.0);

    bool leftRight = true;
    for (double v = bbox.min.y; v <= bbox.max.y + stepO; v += stepO) {
        // Raster line endpoints (rotated by angle)
        double x0 = bbox.min.x * cosA - v * sinA;
        double x1 = bbox.max.x * cosA - v * sinA;
        double y0 = bbox.min.x * sinA + v * cosA;
        double y1 = bbox.max.x * sinA + v * cosA;
        int    n  = std::max(2, static_cast<int>((bbox.max.x - bbox.min.x) / stepO));

        std::vector<Geom::Vec3> line;
        for (int i = 0; i <= n; ++i) {
            double t  = static_cast<double>(i) / n;
            double x  = x0 + (x1 - x0) * t;
            double y  = y0 + (y1 - y0) * t;
            double z  = 0;
            if (projectOntoMesh(mesh, x, y, z)) {
                line.push_back({x, y, z + p.stockAllowance});
            }
        }

        if (line.empty()) { leftRight = !leftRight; continue; }
        if (!leftRight) std::reverse(line.begin(), line.end());

        // Rapid to start of line
        ToolpathPoint rapid;
        rapid.position = {line[0].x, line[0].y, safeZ};
        rapid.toolAxis = {0, 0, 1};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);

        ToolpathPoint plunge;
        plunge.position = line[0];
        plunge.toolAxis = {0, 0, 1};
        plunge.motion   = MotionType::PlungeFeed;
        tp.addPoint(plunge);

        for (std::size_t i = 1; i < line.size(); ++i) {
            ToolpathPoint pt;
            pt.position = line[i];
            pt.toolAxis = {0, 0, 1};
            pt.motion   = MotionType::Linear;
            tp.addPoint(pt);
        }

        leftRight = !leftRight;
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Scallop – constant step-over along surface (simplified: waterline variant)
//
// The stepover (Sₒ) stored in ScallopParams is the lateral distance between
// passes.  The resulting cusp height is calculated via:
//   h = R - √(R² - (Sₒ/2)²)
// This value is recorded in the toolpath name so the operator can verify the
// predicted finish before the first chip is cut.
// --------------------------------------------------------------------------
Toolpath Strategies3D::scallop(const NurbsSurface& surf,
                                 const ScallopParams& p,
                                 const CuttingTool& tool,
                                 const CuttingParams& cuts) {
    double toolRadius = tool.diameter * 0.5;
    double h = scallopHeight(toolRadius, p.stepOver);

    WaterlineParams wp;
    wp.topZ           = surf.evaluate(surf.uMin() + (surf.uMax()-surf.uMin())*0.5,
                                      surf.vMin()).z + 2.0;
    wp.bottomZ        = surf.evaluate(surf.uMin(), surf.vMin()).z - 2.0;
    wp.zStep          = p.stepOver;
    wp.stockAllowance = p.stockAllowance;

    Toolpath tp = waterline(surf, wp, tool, cuts);

    // Embed predicted scallop height in the operation name
    std::ostringstream nameStream;
    nameStream << std::fixed;
    nameStream.precision(3);
    nameStream << "Scallop 3D (So=" << p.stepOver << "mm, h=" << h << "mm)";
    tp.setName(nameStream.str());
    return tp;
}

// --------------------------------------------------------------------------
// Spiral – expand from centre outward in a planar spiral (Z follows surface)
// --------------------------------------------------------------------------
Toolpath Strategies3D::spiral(const NurbsSurface& surf,
                                const SpiralParams& p,
                                const CuttingTool& tool,
                                const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Spiral3D, tool, cuts);
    tp.setName("Spiral 3D");

    int    turns    = static_cast<int>(p.maxRadius / p.pitchPerRev);
    int    steps    = turns * 72;
    double safeZ    = 5.0;

    ToolpathPoint safe;
    safe.position = {p.centre.x, p.centre.y, safeZ};
    safe.toolAxis = {0, 0, 1};
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    for (int i = 0; i <= steps; ++i) {
        double t     = static_cast<double>(i) / steps;
        double angle = 2.0 * PI3D * turns * t;
        double r     = p.maxRadius * t;
        double x     = p.centre.x + r * std::cos(angle);
        double y     = p.centre.y + r * std::sin(angle);

        // Project (x,y) onto the NURBS surface by finding closest (u,v)
        // Simplified: use the centre-relative parameter
        double u = surf.uMin() + (surf.uMax()-surf.uMin()) * (0.5 + r * std::cos(angle) / (p.maxRadius*2));
        double v = surf.vMin() + (surf.vMax()-surf.vMin()) * (0.5 + r * std::sin(angle) / (p.maxRadius*2));
        u = std::max(surf.uMin(), std::min(u, surf.uMax()));
        v = std::max(surf.vMin(), std::min(v, surf.vMax()));
        Geom::Vec3 pt = surf.evaluate(u, v);

        ToolpathPoint tpt;
        tpt.position = {x, y, pt.z + p.stockAllowance};
        tpt.toolAxis = {0, 0, 1};
        tpt.motion   = (i == 0) ? MotionType::PlungeFeed : MotionType::Linear;
        tp.addPoint(tpt);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// §4.2 autoBoundarySelect
// --------------------------------------------------------------------------
std::vector<Geom::Vec2>
Strategies3D::autoBoundarySelect(const MeshData& mesh,
                                   double shallowAngleDeg) {
    const auto& tris = mesh.triangles();
    if (tris.empty()) return {};

    const double cosThreshold = std::cos(shallowAngleDeg * 3.14159265358979323846 / 180.0);

    double xMin =  1e9, xMax = -1e9;
    double yMin =  1e9, yMax = -1e9;
    bool   anyShallow = false;

    for (const auto& tri : tris) {
        // Triangle struct stores direct Vec3 vertices in v[0..2]
        const Geom::Vec3& v0 = tri.v[0];
        const Geom::Vec3& v1 = tri.v[1];
        const Geom::Vec3& v2 = tri.v[2];

        Geom::Vec3 e1 = {v1.x-v0.x, v1.y-v0.y, v1.z-v0.z};
        Geom::Vec3 e2 = {v2.x-v0.x, v2.y-v0.y, v2.z-v0.z};
        Geom::Vec3 n  = {e1.y*e2.z - e1.z*e2.y,
                         e1.z*e2.x - e1.x*e2.z,
                         e1.x*e2.y - e1.y*e2.x};
        double len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
        if (len < 1e-12) continue;
        n.x /= len; n.y /= len; n.z /= len;

        if (std::abs(n.z) >= cosThreshold) {
            for (const auto& vk : {v0, v1, v2}) {
                xMin = std::min(xMin, vk.x); xMax = std::max(xMax, vk.x);
                yMin = std::min(yMin, vk.y); yMax = std::max(yMax, vk.y);
                anyShallow = true;
            }
        }
    }

    if (!anyShallow || xMin > xMax) return {};

    // Return a rectangular boundary enclosing all shallow-facing vertices
    return {
        {xMin, yMin}, {xMax, yMin},
        {xMax, yMax}, {xMin, yMax},
        {xMin, yMin}  // closed
    };
}

// --------------------------------------------------------------------------
// §4.2 mixedCuspRaster
// --------------------------------------------------------------------------
Toolpath Strategies3D::mixedCuspRaster(const MeshData& mesh,
                                         double targetCuspMm,
                                         const CuttingTool& tool,
                                         const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Raster3D, tool, cuts);
    tp.setName("Mixed Cusp Raster");

    const auto& tris = mesh.triangles();
    if (tris.empty() || targetCuspMm <= 0.0) return tp;

    const double toolRadius = tool.diameter * 0.5;
    if (toolRadius <= 0.0) return tp;

    // Bounding box from triangle vertices
    double xMin =  1e9, xMax = -1e9;
    double yMin =  1e9, yMax = -1e9;
    for (const auto& tri : tris) {
        for (const auto& v : tri.v) {
            xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x);
            yMin = std::min(yMin, v.y); yMax = std::max(yMax, v.y);
        }
    }

    // Default stepover from target cusp (flat surface approximation)
    // stepover = 2 * sqrt(2 * R * h) for ball end mill on flat surface
    const double defaultSO = 2.0 * std::sqrt(2.0 * toolRadius * targetCuspMm);

    bool forward = true;
    double y = yMin;
    while (y <= yMax + 1e-9) {
        // Estimate local slope at this Y row (sample 5 points along X)
        double maxSlope = 0.0;
        const int samples = 5;
        for (int s = 0; s <= samples; ++s) {
            double x = xMin + (xMax - xMin) * s / samples;
            double z0 = 0.0, z1 = 0.0;
            double dy = defaultSO * 0.1;
            (void)projectOntoMesh(mesh, x, y,      z0);
            (void)projectOntoMesh(mesh, x, y + dy, z1);
            double slopeHere = std::abs(z1 - z0) / (dy + 1e-12);
            maxSlope = std::max(maxSlope, slopeHere);
        }

        // Adapt stepover: shallower slope → wider step, steeper → tighter
        double localSO = stepOverFromScallop(toolRadius,
                            targetCuspMm / std::max(1.0, 1.0 + maxSlope));
        localSO = std::max(defaultSO * 0.1, std::min(localSO, defaultSO * 2.0));

        double startX = forward ? xMin : xMax;
        double endX   = forward ? xMax : xMin;
        double stepX  = forward ? localSO : -localSO;

        for (double x = startX;
             forward ? (x <= endX + 1e-9) : (x >= endX - 1e-9);
             x += stepX) {
            double z = 0.0;
            (void)projectOntoMesh(mesh, x, y, z);

            ToolpathPoint pt;
            pt.position = {x, y, z};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = tp.points().empty() ? MotionType::Rapid : MotionType::Linear;
            tp.addPoint(pt);
        }

        y += localSO;
        forward = !forward;
    }

    tp.markClean();
    return tp;
}
