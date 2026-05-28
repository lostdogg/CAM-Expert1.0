#pragma once
#ifndef STRATEGIES3D_H
#define STRATEGIES3D_H

#include "Toolpath.h"
#include "../cad/NurbsSurface.h"
#include "../cad/MeshData.h"
#include <vector>

// --------------------------------------------------------------------------
// 3D Milling strategies for complex, contoured surfaces.
//
//  • Waterline (Z-level) roughing/finishing
//  • Raster (parallel passes projected onto surface)
//  • Scallop (constant step-over along surface normals)
//  • Spiral (spiral out from a point on the surface)
//  • ProjectSurface (drape toolpath onto a driver surface)
//
// All strategies include gouge-protection: the computed tool position is
// checked against the full B-Rep / mesh to ensure no intersection.
// --------------------------------------------------------------------------

class Strategies3D {
public:
    struct WaterlineParams {
        double topZ     = 0.0;     // start Z level (mm)
        double bottomZ  = -50.0;   // end Z level (mm)
        double zStep    = 1.0;     // Z increment between passes (mm)
        double stepOver = 0.5;     // fraction of tool Ø between offset passes at same Z
        double stockAllowance = 0.25;
        // §4.2 – Adaptive Stepdown
        bool   adaptiveStepdown    = false;  // vary zStep with local surface slope
        double adaptiveMinStep     = 0.1;    // minimum Z step when slope is shallow (mm)
        double adaptiveMaxStep     = 3.0;    // maximum Z step when slope is steep (mm)
        double adaptiveSlopeBreak  = 45.0;   // degrees – transition slope angle
    };

    struct RasterParams {
        double stepOver     = 0.5;   // fraction of tool Ø
        double angle        = 0.0;   // raster angle in degrees (0 = X-aligned)
        double stockAllowance = 0.0;
        // §4.2 – Mixed cusp height: adjust stepover per zone to keep scallop uniform
        bool   mixedCuspControl = false;
        double targetCuspHeight = 0.005; // mm – desired scallop height for all zones
    };

    struct ScallopParams {
        double stepOver     = 0.5;   // lateral distance between passes (mm)
        double stockAllowance = 0.0;
    };

    struct SpiralParams {
        Geom::Vec2 centre;
        double maxRadius    = 50.0;   // mm
        double pitchPerRev  = 0.5;    // step-over per revolution (mm)
        double stockAllowance = 0.0;
    };

    // -----------------------------------------------------------------------
    // §4.2 – Auto Boundary Selection
    //
    // Analyses a mesh to automatically derive a machining boundary that
    // excludes steep walls (handled by waterline) and concentrates the 3D
    // HST passes on shallow-to-moderate curvature regions.
    //
    //   shallowAngleDeg – faces shallower than this angle (from horizontal)
    //                     are included in the auto-boundary (default 75°).
    //
    // Returns a closed 2-D polygon in XY that encloses all shallow faces.
    // -----------------------------------------------------------------------
    static std::vector<Geom::Vec2>
        autoBoundarySelect(const MeshData& mesh,
                            double shallowAngleDeg = 75.0);

    // -----------------------------------------------------------------------
    // §4.2 – Mixed Cusp Height Raster
    //
    // Generates a raster toolpath where the lateral stepover is locally
    // adapted so that the scallop height is as close to targetCuspMm as
    // possible across the entire surface.  Steep zones get tighter stepovers;
    // flat zones can use wider stepovers.
    // -----------------------------------------------------------------------
    static Toolpath mixedCuspRaster(const MeshData& mesh,
                                     double targetCuspMm,
                                     const CuttingTool& tool,
                                     const CuttingParams& cuts);

    // Generate waterline (Z-level) toolpath on a NURBS surface
    static Toolpath waterline(const NurbsSurface& surf,
                               const WaterlineParams& p,
                               const CuttingTool& tool,
                               const CuttingParams& cuts);

    // Raster on a mesh model
    static Toolpath raster(const MeshData& mesh,
                            const RasterParams& p,
                            const CuttingTool& tool,
                            const CuttingParams& cuts);

    // Scallop on a NURBS surface
    static Toolpath scallop(const NurbsSurface& surf,
                             const ScallopParams& p,
                             const CuttingTool& tool,
                             const CuttingParams& cuts);

    // Spiral on a NURBS surface
    static Toolpath spiral(const NurbsSurface& surf,
                            const SpiralParams& p,
                            const CuttingTool& tool,
                            const CuttingParams& cuts);

    // -----------------------------------------------------------------------
    // Scallop height formula
    //
    // When a ball-end (or bull-nose) tool steps laterally by Sₒ mm across a
    // surface it leaves a small cusp of uncut material known as the "scallop".
    // Its height h controls the surface finish Ra:
    //
    //   h = R - √( R² - (Sₒ/2)² )
    //
    // where R is the tool radius and Sₒ is the stepover distance.
    // A smaller Sₒ → smaller h → smoother finish (higher Ra number means rougher).
    //
    // Conversely, given a target scallop height h, the maximum allowed stepover is:
    //   Sₒ = 2·√( R·(2h - h²/R) )   ≈   2·√(2·R·h)   for h << R
    // -----------------------------------------------------------------------
    static double scallopHeight(double toolRadius, double stepOver);
    static double stepOverFromScallop(double toolRadius, double targetScallopHeight);

private:
    // Project a 2-D point (x,y) down onto the mesh surface (ray-cast)
    // Returns true and sets z if hit; false if no intersection.
    static bool projectOntoMesh(const MeshData& mesh,
                                  double x, double y,
                                  double& z);

    // Ray–triangle intersection (Möller–Trumbore)
    static bool rayTriIntersect(const Geom::Ray& ray,
                                  const Geom::Triangle& tri,
                                  double& t);

    // Gouge-check: ensure tool tip at 'pos' with axis 'axis' doesn't intersect mesh
    static bool isGouge(const Geom::Vec3& tipPos,
                         const Geom::Vec3& toolAxis,
                         double toolRadius,
                         const MeshData& mesh);
};

#endif // STRATEGIES3D_H
