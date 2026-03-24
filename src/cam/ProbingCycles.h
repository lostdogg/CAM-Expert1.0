#pragma once
#ifndef PROBING_CYCLES_H
#define PROBING_CYCLES_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// ProbingCycles – automated work-holding measurement
//
// A Probe is a high-precision touch-trigger sensor mounted in the spindle.
// Instead of a human using an "edge finder" and paper to find 0,0,0, the
// software generates a Probing Toolpath that communicates directly with the
// machine's controller macros to update the G54/G55/… register automatically.
//
// ── Available Cycles ──────────────────────────────────────────────────────
//
//  1. Single Point (Z-Surface)
//       The probe moves down in Z until it contacts the top of the stock.
//       The machine records the Z-position and overwrites the G54 Z-offset.
//       Controller variable: #5203 (Fanuc/Haas Z-register for G54).
//
//  2. Bore / Boss (3- or 4-point center-finding)
//       The probe touches the internal walls (bore) or external surface (boss)
//       at 3 or 4 equidistant angular positions (90° apart).
//       The software uses the circle equation
//           x² + y² + Dx + Ey + F = 0
//       to solve for the true center (h, k) and the diameter.
//       Controller variable update: #5221 = #135 (X), #5222 = #136 (Y) for G54.
//
//  3. Corner Finder (2+1 point method)
//       The probe touches two points along one axis and one point on the
//       perpendicular axis.  The software calculates the intersection of the
//       two lines to locate the exact corner—even if the stock is slightly
//       skewed in the vise.
//
// ── G-code Output ─────────────────────────────────────────────────────────
//
//  The probing G-code uses:
//    G65 P9xxx  – canned probing macro call (Renishaw / Haas convention)
//    #5221..    – G54 X/Y/Z machine variable registers
//    #135 / #136/ #137 – probe-result variables
//
// ── Result ────────────────────────────────────────────────────────────────
//
//  Each method returns:
//    • A Toolpath   – motion points for backplot animation
//    • A std::string – the G-code macro text ready to paste into the program
// --------------------------------------------------------------------------

struct ProbeResult {
    Toolpath    motionPath;   // probe approach / retract moves (for Backplot)
    std::string gcode;        // macro G-code text
};

// ── Probe configuration parameters ──────────────────────────────────────────
struct ProbeParams {
    double probeRadius = 1.0;    // stylus ball radius (mm)
    double safeZ       = 10.0;   // Z-height for rapid traversal (mm)
    double feedRate    = 200.0;  // probing feed rate (mm/min)
    double overshot    = 5.0;    // extra distance beyond expected contact (mm)
    int    wcsRegister = 54;     // G54=54, G55=55, … target offset to update
};

class ProbingCycles {
public:
    // 1. Single-point Z-surface probe
    //    Descends from safeZ at the given XY position until contact.
    //    Updates the G54 (or wcsRegister) Z-offset.
    //    expectedZ: approximate top-of-stock Z coordinate.
    static ProbeResult zSurface(const Geom::Vec2& xy,
                                 double            expectedZ,
                                 const ProbeParams& params = ProbeParams{});

    // 2. Bore (internal) center-finding
    //    Probes the bore wall at `numPoints` equidistant angular positions.
    //    numPoints must be 3 or 4.
    //    approximateCenter: estimated center position (X,Y) in WCS.
    //    approximateRadius: estimated bore radius (mm) — used for approach moves.
    //    Updates the G54 X and Y offsets.
    static ProbeResult bore(const Geom::Vec2& approximateCenter,
                             double            approximateRadius,
                             double            probeZ,
                             int               numPoints,
                             const ProbeParams& params = ProbeParams{});

    // 3. Boss (external) center-finding — same math as bore but probes outward.
    static ProbeResult boss(const Geom::Vec2& approximateCenter,
                             double            approximateRadius,
                             double            probeZ,
                             int               numPoints,
                             const ProbeParams& params = ProbeParams{});

    // 4. Corner finder (two points on X-axis face, one on Y-axis face)
    //    cornerEstimate: approximate corner position (X,Y) in WCS.
    //    stockXSize, stockYSize: approximate part dimensions (mm).
    //    Updates G54 X and Y offsets.
    static ProbeResult cornerFinder(const Geom::Vec2& cornerEstimate,
                                     double            stockXSize,
                                     double            stockYSize,
                                     double            probeZ,
                                     const ProbeParams& params = ProbeParams{});

    // ── Math helpers (public for unit testing) ──────────────────────────────

    // Fit a circle through 3+ points using the general equation
    //   x² + y² + Dx + Ey + F = 0
    // Returns the center (h, k) and radius r.
    // Returns false if the points are degenerate (collinear).
    static bool fitCircle(const std::vector<Geom::Vec2>& pts,
                          double& h, double& k, double& r);

private:
    // Build approach and retract motion points for one probe hit
    static void addProbeMove(Toolpath& tp,
                              const Geom::Vec3& safePos,
                              const Geom::Vec3& contactPos,
                              double feedRate);

    // Emit the Fanuc/Haas variable assignment to update a WCS register axis
    // axisVar: 1=X, 2=Y, 3=Z   (added to base: G54 X = #5221, Y=#5222, Z=#5203)
    static std::string updateWcsVariable(int wcsRegister, int axisVar,
                                          const std::string& sourceVar);
};

#endif // PROBING_CYCLES_H
