#pragma once
#ifndef TRANSFORM_TOOLPATH_H
#define TRANSFORM_TOOLPATH_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include "../managers/PlanesManager.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// TransformToolpath – replicate a "Source" operation across the workspace
//
// Instead of re-selecting geometry for every part on a tombstone or fixture
// plate, you program one operation and use the Transform tool to generate
// the required copies with the correct G-code output.
//
// ---- Transformation Methods ----
//
//   Translate  – moves copies along a linear vector (X, Y, or Z).
//                Used for grids, pallets, and straight vise arrangements.
//
//   Rotate     – sweeps copies around a centre point at a fixed angular
//                increment.  Used for circular bolt patterns and rotary-table
//                fixtures.
//
//   Mirror     – flips the toolpath across an axis.  Critical for
//                left-hand / right-hand mirrored parts.
//                *** Note: mirroring reverses climb vs. conventional
//                cutting direction.  Set compensateClimb=true to invert
//                the arc direction so the cutter load remains correct.
//
// ---- Output Types ----
//
//   Coordinate  – the post-processor outputs a new Work Offset for every
//                 copy (G54 for copy 0, G55 for copy 1, G56 for copy 2, …).
//                 The XYZ coordinates stay the same relative to each offset;
//                 only the register number changes.
//
//   Toolpath    – keeps the same Work Offset (G54) but mathematically shifts
//                 all XYZ coordinates for each copy.  Suitable for parts in
//                 the same fixture when the controller does not have extra
//                 offset registers.
//
//   ToolPlane   – used in 4/5-axis positional work.  The tool-approach angle
//                 changes for each copy while the part stays in the same
//                 fixture.  The post outputs B-axis (or A-axis) rotation
//                 moves between copies.
//
// ---- Subprogram Integration ----
//
//   When useSubprogram == true the transform emits an M98 call at each copy
//   location and writes the cutting logic once as an M99-terminated
//   subprogram.  This keeps the NC file compact for large production runs.
//
// --------------------------------------------------------------------------

enum class TransformOutputType { Coordinate, Toolpath, ToolPlane };

struct TransformResult {
    // One entry per copy of the source operation
    struct Copy {
        Toolpath    toolpath;          // transformed toolpath for this copy
        CoordPlane  wcsPlane;          // work offset plane (Coordinate mode)
        int         subprogramNumber;  // valid when useSubprogram==true
    };

    std::vector<Copy>   copies;
    bool                useSubprogram = false;
    int                 subprogramNumber = 9000; // M98 P9000 …

    // For Coordinate output: generate the work-offset preamble block
    // (G54, G55, G56, …) for each copy in sequence.
    std::string workOffsetSequence() const;

    // Generate the full subprogram body (M99 at end)
    std::string subprogramBody() const;
};

// --------------------------------------------------------------------------
class TransformToolpath {
public:
    struct TranslateParams {
        Geom::Vec3 delta;        // offset applied to each successive copy
        int        copies = 1;   // number of additional copies (total = 1 + copies)
    };

    struct RotateParams {
        Geom::Vec3 centre;              // rotation centre point
        Geom::Vec3 axis{0, 0, 1};      // rotation axis (default: Z)
        double     angleDegIncrement;   // angular step per copy (degrees)
        int        copies = 1;
    };

    struct MirrorParams {
        enum class Axis { X, Y, Z, Custom };
        Axis       mirrorAxis   = Axis::X;
        Geom::Vec3 customNormal{1, 0, 0}; // used when mirrorAxis==Custom
        Geom::Vec3 mirrorOrigin{};
        bool       compensateClimb = true; // invert arc directions after mirror
    };

    // ---- Translate ----
    // Produce (1 + params.copies) total copies of 'source' spaced by params.delta.
    static TransformResult translate(const Toolpath&      source,
                                     const TranslateParams& params,
                                     TransformOutputType    outputType = TransformOutputType::Toolpath,
                                     bool                   useSubprogram = false,
                                     int                    firstWcsOffset = 0);

    // ---- Rotate ----
    // Produce (1 + params.copies) total copies rotated about params.centre.
    static TransformResult rotate(const Toolpath&     source,
                                   const RotateParams&  params,
                                   TransformOutputType  outputType = TransformOutputType::Toolpath,
                                   bool                 useSubprogram = false,
                                   int                  firstWcsOffset = 0);

    // ---- Mirror ----
    // Produce the source + one mirrored copy.
    static TransformResult mirror(const Toolpath&    source,
                                   const MirrorParams& params,
                                   TransformOutputType outputType = TransformOutputType::Toolpath,
                                   int                 mirrorWcsOffset = 1);

private:
    // Apply a 4×4 matrix to all points in a toolpath (in-place)
    static void applyMatrix(Toolpath& tp, const Geom::Mat4& m);

    // Build a CoordPlane for the n-th copy in Coordinate output mode
    static CoordPlane makeCopyPlane(const Geom::Vec3& origin, int wcsOffset,
                                    const std::string& name);

    // Invert arc direction (ArcCW↔ArcCCW) for all points in a toolpath
    static void invertArcs(Toolpath& tp);
};

#endif // TRANSFORM_TOOLPATH_H
