#pragma once
#ifndef ART_H
#define ART_H

#include "../cad/MeshData.h"
#include "../cad/NurbsSurface.h"
#include "../cad/Geometry.h"
#include "Toolpath.h"
#include <vector>
#include <string>
#include <cstdint>

// --------------------------------------------------------------------------
// Art – Artistic / Relief Modeling and Machining
//
// This module covers the creative side of CAM: organic surfaces, decorative
// reliefs, and art-to-machining pipelines.
//
// Three main workflows are provided:
//
//  1. Image-to-Relief
//       A grayscale image (8-bit, row-major) is converted to a height-field
//       (Z-map) and then to a triangulated mesh relief.  Pixel intensity
//       maps linearly to height: black (0) = base level, white (255) = full
//       relief height.  The mesh can then be machined with the standard 3D
//       strategies or the specialised reliefToolpath() method.
//
//  2. Vector-to-Texture
//       Closed 2-D vector chains (e.g. imported from SVG or DXF) are draped
//       onto a host surface to produce an embossed or engraved texture.
//       Each chain generates a raised or recessed band of configurable width
//       and profile (flat, round, V-groove).
//
//  3. Organic Surface Modeling
//       A set of 3-D control points is smoothed using a Laplacian
//       relaxation to produce a flowing organic surface mesh.  This
//       mesh can be further blended with an existing B-Rep surface.
//
// Relief toolpath:
//       A specialised raster-based toolpath that follows the relief mesh
//       with a ball-end mill.  Stepover, angle, and cusp height are
//       controlled as in Strategies3D, but the relief-specific version
//       adds:
//         • Depth limiting (so only the positive relief is machined, not
//           the flat background).
//         • Feature isolation (optional boundary to confine the relief).
//         • Multiple depth bands (rough + finish in one operation).
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// GrayscaleImage – lightweight image wrapper for relief generation
// --------------------------------------------------------------------------
struct GrayscaleImage {
    int                    width  = 0;    // pixels
    int                    height = 0;    // pixels
    std::vector<uint8_t>   pixels;        // row-major, [0,255]

    // Bilinear interpolation at fractional pixel coordinates
    float sample(float px, float py) const;

    bool valid() const { return width > 0 && height > 0
                             && static_cast<int>(pixels.size()) == width * height; }
};

// --------------------------------------------------------------------------
// ImageToReliefParams – controls the image → Z-map conversion
// --------------------------------------------------------------------------
struct ImageToReliefParams {
    double  physicalWidth   = 200.0;    // mm – physical width of the relief
    double  physicalHeight  = 100.0;    // mm – physical height of the relief
    double  maxReliefHeight = 10.0;     // mm – Z at 100 % white
    double  baseZ           = 0.0;      // mm – Z at 100 % black
    bool    invertImage     = false;    // invert: white = low, black = high
    bool    smoothEdges     = true;     // apply a 3×3 Gaussian blur to reduce aliasing
    double  smoothSigma     = 1.0;      // Gaussian sigma (pixels)
    int     meshResX        = 256;      // mesh grid resolution in X
    int     meshResY        = 128;      // mesh grid resolution in Y
};

// --------------------------------------------------------------------------
// VectorTextureParams – controls vector → texture draped on a surface
// --------------------------------------------------------------------------
struct VectorTextureParams {
    double  bandWidth       = 2.0;      // mm – width of the raised/recessed band
    double  bandHeight      = 1.0;      // mm – height (positive = emboss, negative = engrave)
    enum class BandProfile { Flat, Round, VGroove } profile = BandProfile::Round;
    double  vGrooveAngle    = 60.0;     // degrees – only for VGroove profile
    bool    drapeToSurface  = true;     // if false, keep Z from input chains
    double  drapeOffset     = 0.5;      // mm – offset above the host surface
};

// --------------------------------------------------------------------------
// OrganicSmoothingParams – controls Laplacian surface relaxation
// --------------------------------------------------------------------------
struct OrganicSmoothingParams {
    int    iterations       = 20;       // Laplacian relaxation iterations
    double lambda           = 0.5;      // relaxation factor (0 = none, 1 = full)
    bool   preserveBoundary = true;     // do not move boundary vertices
    bool   featurePreserve  = true;     // attenuate smoothing at high curvature
    double featureAngle     = 45.0;     // degrees – sharp edges above this are preserved
};

// --------------------------------------------------------------------------
// ReliefToolpathParams – controls the relief-specific machining strategy
// --------------------------------------------------------------------------
struct ReliefToolpathParams {
    double  stepOver        = 0.3;      // mm – lateral stepover
    double  angle           = 0.0;      // degrees – raster direction (0 = X-aligned)
    double  stockAllowance  = 0.0;      // mm – finish stock
    double  maxDepth        = -1.0;     // mm – limit cut depth; -1 = no limit
    bool    roughAndFinish  = true;     // generate rough + finish band in one call
    double  roughStepOver   = 1.5;      // mm – stepover for the roughing band
    double  roughAllowance  = 0.3;      // mm – stock left for finish pass
    // Optional 2-D boundary to confine the toolpath
    std::vector<Geom::Vec2> confineBoundary;
};

// --------------------------------------------------------------------------
// Art
// --------------------------------------------------------------------------
class Art {
public:
    // -----------------------------------------------------------------------
    // imageToRelief()
    //
    // Converts a grayscale image to a triangulated relief mesh.
    // Each pixel maps to one (or more) mesh vertices depending on meshResX/Y.
    // The returned MeshData is ready for use with Strategies3D or
    // reliefToolpath().
    // -----------------------------------------------------------------------
    static MeshData imageToRelief(const GrayscaleImage&      image,
                                   const ImageToReliefParams& p = {});

    // -----------------------------------------------------------------------
    // vectorToTexture()
    //
    // Drapes a set of closed 2-D chains onto a NURBS host surface to produce
    // an embossed or engraved texture mesh.  Each chain is offset inward and
    // outward by bandWidth/2 and the resulting strip is raised or lowered by
    // bandHeight.  The returned mesh can be blended with the host surface.
    //
    // `chains` – list of closed 2-D profiles (CCW = emboss, CW = engrave
    //            for positive bandHeight).
    // -----------------------------------------------------------------------
    static MeshData vectorToTexture(
        const std::vector<std::vector<Geom::Vec2>>& chains,
        const NurbsSurface&                          hostSurf,
        const VectorTextureParams&                   p = {});

    // -----------------------------------------------------------------------
    // organicSmooth()
    //
    // Applies Laplacian relaxation to a mesh to produce flowing organic forms.
    // Returns a new MeshData with smoothed vertex positions.
    // -----------------------------------------------------------------------
    static MeshData organicSmooth(const MeshData&              mesh,
                                   const OrganicSmoothingParams& p = {});

    // -----------------------------------------------------------------------
    // reliefToolpath()
    //
    // Generates a relief-specific raster toolpath for a relief mesh.
    // The strategy is similar to Strategies3D::raster() but adds:
    //   • Depth limiting: only cuts cells above p.maxDepth (if set).
    //   • Feature boundary: restricts cutting to p.confineBoundary (if set).
    //   • Automatic rough + finish pass if p.roughAndFinish is true.
    //
    // Returns a pair of toolpaths: [0] = rough (if requested), [1] = finish.
    // If roughAndFinish is false, only one toolpath is returned.
    // -----------------------------------------------------------------------
    static std::vector<Toolpath>
        reliefToolpath(const MeshData&              relief,
                        const ReliefToolpathParams&  p,
                        const CuttingTool&           tool,
                        const CuttingParams&         cuts);

    // -----------------------------------------------------------------------
    // blendMeshes()
    //
    // Blends an "overlay" mesh (e.g. a relief or texture mesh) onto a "base"
    // mesh using alpha-weighted vertex displacement.  The overlay's Z values
    // are added to the base mesh within the specified blend radius.
    //
    //   Z_result = Z_base + alpha(dist) * Z_overlay_displacement
    //
    // where alpha is a smooth (cosine) falloff from 1 at the overlay vertex
    // to 0 at blendRadius distance.
    // -----------------------------------------------------------------------
    static MeshData blendMeshes(const MeshData& base,
                                  const MeshData& overlay,
                                  double          blendRadius = 5.0);

    // -----------------------------------------------------------------------
    // heightmapToZMap()
    //
    // Utility: convert a GrayscaleImage directly to a flat ZMap struct
    // (suitable for use with Verify::compare()) without building the full
    // triangulated mesh.
    // -----------------------------------------------------------------------
    static ZMap heightmapToZMap(const GrayscaleImage&      image,
                                  const ImageToReliefParams& p = {});

    // -----------------------------------------------------------------------
    // gaussianBlur()
    //
    // Apply a separable Gaussian blur to a single-channel float buffer.
    // Used internally for edge smoothing in imageToRelief().
    // -----------------------------------------------------------------------
    static std::vector<float>
        gaussianBlur(const std::vector<float>& input,
                      int width, int height,
                      double sigma);

private:
    // Build a raster pass at height `z` over a bounding rectangle
    static void buildRasterPass(Toolpath& tp,
                                  double xMin, double xMax,
                                  double yMin, double yMax,
                                  double z,
                                  double stepOver,
                                  double angle,
                                  double feedRate);

    // Check whether point (x,y) is inside a polygon boundary
    static bool insideBoundary(const std::vector<Geom::Vec2>& boundary,
                                 double x, double y);
};

// --------------------------------------------------------------------------
// ZMap (forward-declared above in Verify.h; re-use its definition here via
// the shared struct from Verify.h which is included by the .cpp).
// The Art module uses ZMap for heightmapToZMap() and internal computations.
// --------------------------------------------------------------------------
// ZMap is defined in simulation/Verify.h – include that header rather than
// redefining the struct here.
#include "../simulation/Verify.h"

#endif // ART_H
