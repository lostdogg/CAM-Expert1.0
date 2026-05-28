#pragma once
#ifndef ROUTER_H
#define ROUTER_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include "../cad/MeshData.h"
#include "../cad/NurbsSurface.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// Router – CNC Routing / Panel Machining
//
// CNC routers operate on large flat panels (wood, plastics, composites,
// foams, aluminium sheet) and differ from machining centres in:
//
//  • Large work envelopes, often with vacuum table fixturing.
//  • Nested machining: multiple parts are arrayed onto a single sheet to
//    minimise material waste.
//  • Tabs / bridges: small uncut bridges hold parts in place while the
//    router cuts the perimeter; tabs are snapped off after unloading.
//  • Aggregate heads / tool changers: special multi-spindle heads for
//    boring, grooving, and sawing, often automatically swapped.
//  • 5-axis routing: compound-angle cutting for aerospace composite panels,
//    furniture profiles, and sign work.
//
// This module provides:
//   1. NestingSolver   – arranges part outlines on a sheet with configurable
//                        gap, grain direction, and rotation increments.
//   2. RouterStrategies – contour, pocket, and scoring strategies.
//   3. Tab manager     – inserts bridge stubs into a perimeter profile.
//   4. AggregateHead   – models a multi-spindle aggregate for multi-spindle ops.
//   5. Router5Axis     – 5-axis routing along NURBS surfaces for trimming.
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Sheet – the raw panel being routed
// --------------------------------------------------------------------------
struct RouterSheet {
    double width        = 2440.0;   // mm (e.g. 8 ft)
    double height       = 1220.0;   // mm (e.g. 4 ft)
    double thickness    = 18.0;     // mm
    bool   hasGrain     = false;    // if true, nesting respects grain direction
    double grainAngle   = 0.0;      // degrees – grain runs at this angle
};

// --------------------------------------------------------------------------
// PartOutline – one 2-D part profile to be nested
// --------------------------------------------------------------------------
struct PartOutline {
    std::vector<Geom::Vec2> boundary;   // closed 2-D chain
    int                     quantity    = 1;       // how many copies to nest
    double                  toolComp    = 0.0;     // router bit radius offset (mm)
    bool                    canRotate   = true;    // allow 90° rotation increments
    std::string             partId;
};

// --------------------------------------------------------------------------
// NestedPlacement – one placed instance of a part outline
// --------------------------------------------------------------------------
struct NestedPlacement {
    std::string  partId;
    Geom::Vec2   origin;        // bottom-left corner of the bounding box
    double       rotation;      // degrees (0, 90, 180, 270)
    int          copyIndex;     // which copy of the part this is
};

// --------------------------------------------------------------------------
// NestingResult – output of the nesting solver
// --------------------------------------------------------------------------
struct NestingResult {
    std::vector<NestedPlacement> placements;
    double  sheetUtilisation = 0.0;  // fraction of sheet area occupied by parts
    int     unplacedCount    = 0;    // parts that did not fit on this sheet
    double  totalScrapMm2    = 0.0;  // unused sheet area (mm²)
};

// --------------------------------------------------------------------------
// RouterTab – a bridge stub inserted into a profile
// --------------------------------------------------------------------------
struct RouterTab {
    double   profileDistance;  // mm along the perimeter where the tab starts
    double   width     = 4.0;  // mm – tab width (uncut length)
    double   height    = 2.0;  // mm – tab height above the bottom of the cut
    bool     isActive  = true;
};

// --------------------------------------------------------------------------
// AggregateHead – a multi-spindle router aggregate
// --------------------------------------------------------------------------
struct AggregateHead {
    enum class Type { Boring, Grooving, Saw, Sanding };
    Type    type            = Type::Boring;
    int     spindleCount    = 5;        // number of spindles in the bank
    double  spindleSpacing  = 32.0;     // mm centre-to-centre spacing
    double  spindleRPM      = 4000.0;   // rev/min
    double  diameter        = 10.0;     // mm tool diameter
    double  depth           = 10.0;     // mm depth per pass
    std::string description;
};

// --------------------------------------------------------------------------
// RouterParams – general routing operation parameters
// --------------------------------------------------------------------------
struct RouterParams {
    double  depth           = 18.0;     // mm – full cut-through depth
    double  stepDown        = 9.0;      // mm – Z increment per pass
    double  stepOver        = 0.5;      // fraction of bit diameter for pocketing
    double  feedRate        = 4000.0;   // mm/min
    double  plungeRate      = 800.0;    // mm/min
    double  stockAllowance  = 0.0;
    bool    climb           = true;     // climb (conventional = false)
    double  leadInRadius    = 3.0;      // tangential lead-in arc radius (mm)
    double  sheetGap        = 3.0;      // mm gap between nested parts
};

// --------------------------------------------------------------------------
// Router5AxisParams – 5-axis routing along a NURBS surface
// --------------------------------------------------------------------------
struct Router5AxisParams {
    double  normalOffset    = 0.0;      // mm – offset from surface normal
    double  leadAngle       = 5.0;      // tilt forward in feed direction
    double  sideAngle       = 0.0;      // tilt left/right
    double  stepOver        = 10.0;     // mm between passes along surface
    bool    reverseAlternate= true;     // zig-zag between passes
    double  feedRate        = 3000.0;   // mm/min
    double  plungeRate      = 500.0;    // mm/min
};

// --------------------------------------------------------------------------
// Router
// --------------------------------------------------------------------------
class Router {
public:
    explicit Router(RouterParams params = {});

    // -----------------------------------------------------------------------
    // contour()
    //
    // Profile-following (contour) path around a 2-D boundary with optional
    // multi-pass step-down.  Inserts any configured tabs into the perimeter.
    // -----------------------------------------------------------------------
    Toolpath contour(const std::vector<Geom::Vec2>& profile,
                      const RouterParams&             p,
                      const CuttingTool&              bit,
                      const CuttingParams&            cuts,
                      const std::vector<RouterTab>&   tabs = {}) const;

    // -----------------------------------------------------------------------
    // pocket()
    //
    // Concentric-offset pocketing within a closed boundary.
    // -----------------------------------------------------------------------
    Toolpath pocket(const std::vector<Geom::Vec2>& boundary,
                     const RouterParams&             p,
                     const CuttingTool&              bit,
                     const CuttingParams&            cuts) const;

    // -----------------------------------------------------------------------
    // scoreAndSnap()
    //
    // Generates a shallow V-groove scoring path that allows the part to be
    // snapped cleanly.  The score depth is typically 1/3 of the sheet thickness.
    // -----------------------------------------------------------------------
    Toolpath scoreAndSnap(const std::vector<Geom::Vec2>& line,
                           double                         sheetThickness,
                           const CuttingTool&             vBit,
                           const CuttingParams&           cuts) const;

    // -----------------------------------------------------------------------
    // aggregateOp()
    //
    // Generates a multi-spindle boring or grooving operation using an
    // AggregateHead.  Each spindle drills at its offset position.
    // Returns one Toolpath per spindle.
    // -----------------------------------------------------------------------
    std::vector<Toolpath>
        aggregateOp(const std::vector<Geom::Vec2>& positions,
                     const AggregateHead&            head,
                     double                         sheetDepth,
                     const CuttingParams&            cuts) const;

    // -----------------------------------------------------------------------
    // route5Axis()
    //
    // 5-axis surface-following routing path along a NURBS surface.
    // Generates parallel passes at `stepOver` spacing.
    // -----------------------------------------------------------------------
    Toolpath route5Axis(const NurbsSurface&    surf,
                         const Router5AxisParams& p,
                         const CuttingTool&     bit,
                         const CuttingParams&   cuts) const;

    // -----------------------------------------------------------------------
    // NestingSolver – static facility to nest part outlines on a sheet
    //
    // Strategy: bottom-left fill with guillotine splits.
    //   1. Sort parts by bounding-box area (largest first).
    //   2. Place each part at the lowest-leftmost free position on the sheet.
    //   3. After each placement, split the remaining free space into two
    //      rectangles (bottom strip and right strip).
    //   4. Try rotating the part 90° if it leads to better utilisation.
    // -----------------------------------------------------------------------
    static NestingResult nestParts(const std::vector<PartOutline>& parts,
                                    const RouterSheet&               sheet,
                                    double                           gapMm = 3.0);

    // -----------------------------------------------------------------------
    // insertTabs()
    //
    // Modifies `profile` to lift the Z axis over each tab location, leaving
    // a bridge of height `tab.height` above the bottom of the final pass.
    // Returns the profile split into segments around the tabs.
    // -----------------------------------------------------------------------
    static std::vector<std::vector<Geom::Vec2>>
        insertTabs(const std::vector<Geom::Vec2>& profile,
                   const std::vector<RouterTab>&  tabs);

    // -----------------------------------------------------------------------
    // autoGenerateTabs()
    //
    // Automatically distributes `tabCount` evenly-spaced tabs along the
    // perimeter of `profile`.
    // -----------------------------------------------------------------------
    static std::vector<RouterTab>
        autoGenerateTabs(const std::vector<Geom::Vec2>& profile,
                          int    tabCount  = 4,
                          double tabWidthMm = 4.0,
                          double tabHeightMm = 2.0);

    const RouterParams& params() const { return m_params; }
    void setParams(const RouterParams& p) { m_params = p; }

private:
    // Compute the perimeter length of a profile
    static double profilePerimeter(const std::vector<Geom::Vec2>& profile);

    // Build one step-down pass at a given Z level
    Toolpath buildContourPass(const std::vector<Geom::Vec2>& profile,
                               double                         z,
                               double                         feedRate,
                               double                         leadInR) const;

    // Offset a profile inward by `amount` mm
    static std::vector<Geom::Vec2>
        offsetInward(const std::vector<Geom::Vec2>& boundary, double amount);

    RouterParams m_params;
};

#endif // ROUTER_H
