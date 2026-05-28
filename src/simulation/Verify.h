#pragma once
#ifndef VERIFY_H
#define VERIFY_H

#include "../cam/Toolpath.h"
#include "../cad/MeshData.h"
#include "../cad/ZMap.h"
#include "../managers/ToolpathManager.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// Verify – solid stock simulation
// --------------------------------------------------------------------------

struct VerifyResult {
    bool   hasGouge     = false;
    bool   hasUndercut  = false;
    int    gougeCount   = 0;
    int    undercutCount= 0;
    double maxGougeDepth   = 0; // mm
    double maxUndercutDepth= 0; // mm

    std::vector<Geom::Vec3> gougeLocations;
    std::vector<Geom::Vec3> undercutLocations;
};

// --------------------------------------------------------------------------
// StockCompare – heat-map comparison of as-machined stock vs. target CAD model
//
// After simulation finishes, StockCompare overlays the machined ZMap against
// the original target mesh and generates a per-cell deviation value.
// The colour convention matches the problem statement:
//   Green  → within tolerance  (|deviation| <= tolerance)
//   Red    → Gouge             (machined surface is INSIDE the target, deviation < -tolerance)
//   Blue   → Excess material   (machined surface is OUTSIDE the target, deviation > +tolerance)
// --------------------------------------------------------------------------

enum class StockCompareColor {
    Green,  // within tolerance
    Red,    // gouge – material removed that should stay
    Blue    // excess – material that should have been removed
};

struct StockCompareCell {
    double           deviation = 0.0; // mm, negative = gouge, positive = excess
    StockCompareColor color    = StockCompareColor::Green;
    double           x        = 0.0; // world X centre of cell
    double           y        = 0.0; // world Y centre of cell
};

struct StockCompareResult {
    std::vector<StockCompareCell> cells;
    double minDeviation  = 0.0; // most negative = deepest gouge
    double maxDeviation  = 0.0; // most positive = largest excess
    int    gougeCount    = 0;
    int    excessCount   = 0;
    double tolerance     = 0.0254; // mm — default 0.0254 mm ≈ 0.001 inch
};

class StockCompare {
public:
    // tolerance in mm (e.g. 0.0254 mm ≈ 0.001 inch)
    explicit StockCompare(double toleranceMm = 0.0254);

    // Compare the as-machined ZMap to the target mesh.
    // Returns per-cell deviation and summary statistics.
    StockCompareResult compare(const ZMap& stock, const MeshData& target) const;

    // Return the deviation at a specific world (x,y) location.
    // Interpolates from the nearest cell centre.
    static double deviationAt(const StockCompareResult& result,
                               const ZMap& stock,
                               double worldX, double worldY);

private:
    double m_tolerance; // mm
};

// Options defined outside Verify class to avoid GCC nested-struct default-arg issue
struct VerifyOptions {
    double stockXMin = -60, stockXMax = 60;
    double stockYMin = -60, stockYMax = 60;
    double stockZTop =  0,  stockZBot =-50;
    int    zMapRes   = 512;
    double gougeTol  = 0.01;
    // Simulation resolution: coarse = faster but may miss tiny gouges,
    // fine = slower but catches sub-micron deviations
    enum class Resolution { Coarse, Medium, Fine } resolution = Resolution::Medium;
};

class Verify {
public:
    using Options = VerifyOptions;

    explicit Verify(Options opts = Options{});

    // Run the full simulation against all operations in the manager.
    // Populates the internal ZMap stock model.
    VerifyResult run(const ToolpathManager* mgr);

    // Legacy compare: checks stock against a target mesh using the simplified
    // minimum-Z heuristic.  For a full heat-map analysis use StockCompare.
    VerifyResult compare(const ZMap& stock, const MeshData& target) const;

    // Step-through: simulate exactly one move and return whether a gouge
    // occurred on that specific move.  Intended for frame-by-frame inspection.
    bool stepMove(const ToolpathPoint& from, const ToolpathPoint& to,
                  const CuttingTool& tool);

    const ZMap& stockMap() const { return m_stockMap; }
    static MeshData zMapToMesh(const ZMap& zmap);

    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback cb) { m_progress = std::move(cb); }

private:
    // Subtract the swept volume of one tool move from the Z-map.
    void subtractMove(const ToolpathPoint& from, const ToolpathPoint& to,
                       const CuttingTool& tool, ZMap& zmap);

    // Check a single move for a gouge against the current stock surface.
    // Returns true if the move would remove material below stockZBot.
    bool detectGouge(const ToolpathPoint& from, const ToolpathPoint& to,
                     const CuttingTool& tool, const ZMap& zmap) const;

    Options          m_opts;
    ZMap             m_stockMap;
    ProgressCallback m_progress;
};

// --------------------------------------------------------------------------
// §4.6 – ProbeSimulation
//
// Validates a probing toolpath against the current stock model (ZMap).
// Checks that:
//  1. The probe contact point lies within the expected measurement window
//     (defined by toleranceMm around the nominal target surface).
//  2. The probe approach moves do not collide with the stock surface before
//     the intended touch point.
//  3. The stylus shank does not gouge the stock during the approach.
//
// ProbeSimulation is designed to work with Renishaw Productivity+ cycles
// or any similar canned probing output.
// --------------------------------------------------------------------------

struct ProbeContact {
    Geom::Vec3  nominal;        // expected contact point (from CAD)
    Geom::Vec3  actual;         // simulated contact point (from stock ZMap)
    double      deviation;      // signed deviation (actual.z - nominal.z)  mm
    bool        withinTol;      // |deviation| <= tolerance
    bool        earlyContact;   // probe touched before intended point
};

struct ProbeSimResult {
    std::vector<ProbeContact> contacts;
    int    contactCount     = 0;
    int    outOfTolCount    = 0;
    int    earlyContactCount= 0;
    double maxDeviation     = 0.0;
    double minDeviation     = 0.0;
    bool   hasCollision     = false;
};

struct ProbeSimOptions {
    double toleranceMm        = 0.025; // ±mm tolerance around nominal
    double stylusRadius       = 1.0;   // mm – probe ball radius
    double shanksRadius       = 2.0;   // mm – probe shank radius (for collision)
    double approachFeedMmMin  = 300.0; // mm/min – simulated approach feed
    bool   checkShankCollision = true; // check shank as well as ball
};

class ProbeSimulation {
public:
    explicit ProbeSimulation(ProbeSimOptions opts = {});

    // Simulate a probe toolpath against the given stock ZMap.
    // Each Linear move in `probePath` is treated as a probing move.
    // Returns the simulated contact points and deviation report.
    ProbeSimResult simulate(const Toolpath& probePath,
                             const ZMap&     stock) const;

    // Quick check: is a single probe contact within tolerance?
    static bool isWithinTolerance(const ProbeContact& c, double toleranceMm);

private:
    // Find the Z at which the probe ball first contacts the stock surface
    // along the move from `from` to `to`.
    // Returns true and sets `contactZ` if a contact is found within the move.
    bool findContact(const Geom::Vec3& from,
                      const Geom::Vec3& to,
                      double stylusRadius,
                      const ZMap& stock,
                      Geom::Vec3& contactPt) const;

    ProbeSimOptions m_opts;
};

#endif // VERIFY_H
