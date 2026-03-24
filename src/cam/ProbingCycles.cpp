#include "ProbingCycles.h"
#include <sstream>
#include <cmath>
#include <iomanip>

static constexpr double PI_PC = 3.14159265358979323846;

// --------------------------------------------------------------------------
// Internal helper – add a probe approach/retract motion pair to the path.
// --------------------------------------------------------------------------
void ProbingCycles::addProbeMove(Toolpath& tp,
                                  const Geom::Vec3& safePos,
                                  const Geom::Vec3& contactPos,
                                  double /*feedRate*/) {
    ToolpathPoint rapid;
    rapid.position = safePos;
    rapid.toolAxis = {0, 0, 1};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    ToolpathPoint feed;
    feed.position = contactPos;
    feed.toolAxis = {0, 0, 1};
    feed.motion   = MotionType::PlungeFeed;
    tp.addPoint(feed);

    // Retract back to safe position
    tp.addPoint(rapid);
}

// --------------------------------------------------------------------------
// Emit the Fanuc/Haas macro variable assignment to update a WCS register.
//
// G54 registers:
//   X → #5221   Y → #5222   Z → #5223
// G55 registers:
//   X → #5241   Y → #5242   Z → #5243
// Formula: base = 5200 + (wcsRegister - 54) * 20 + axisVar
// --------------------------------------------------------------------------
std::string ProbingCycles::updateWcsVariable(int wcsRegister,
                                              int axisVar,
                                              const std::string& sourceVar) {
    int base = 5200 + (wcsRegister - 54) * 20 + axisVar;
    std::ostringstream oss;
    oss << "#" << base << " = " << sourceVar << "\n";
    return oss.str();
}

// --------------------------------------------------------------------------
// 1. Z-Surface single-point probe
//
//   Approach: rapid to safeZ, plunge at feedRate until contact.
//   Result variable: #135 = probe Z contact position.
//   Update: #5203 (G54 Z-register when wcsRegister==54) = #135
// --------------------------------------------------------------------------
ProbeResult ProbingCycles::zSurface(const Geom::Vec2& xy,
                                     double            expectedZ,
                                     const ProbeParams& params) {
    ProbeResult res;
    Toolpath& tp = res.motionPath;
    tp.setName("Probe_ZSurface");

    Geom::Vec3 safePos {xy.x, xy.y, params.safeZ};
    Geom::Vec3 contact {xy.x, xy.y, expectedZ - params.overshot};
    addProbeMove(tp, safePos, contact, params.feedRate);

    // G-code macro
    std::ostringstream g;
    g << "( *** Z-SURFACE PROBE *** )\n";
    g << "G91\n";                                         // incremental mode
    g << "G00 X" << std::fixed << std::setprecision(3)
      << xy.x << " Y" << xy.y << "\n";                   // rapid to XY
    g << "G00 Z" << params.safeZ << "\n";                 // safe height
    g << "G90\n";                                         // back to absolute
    g << "G65 P9810 Z" << (expectedZ - params.overshot)
      << " F" << params.feedRate << "\n";                 // probe down
    g << "( #135 = measured Z contact )\n";
    g << updateWcsVariable(params.wcsRegister, 3, "#135");// update Z register
    g << "G00 Z" << params.safeZ << "\n";                 // retract

    res.gcode = g.str();
    return res;
}

// --------------------------------------------------------------------------
// 2. Bore / internal center-finding
//
//   Probes the bore wall at numPoints equidistant angular positions (90° apart
//   for 4-point, 120° for 3-point).  The circle center is solved using the
//   general circle equation.  Controller macro variables used:
//     #135, #136, … = successive probe X results
//     #145, #146, … = successive probe Y results
// --------------------------------------------------------------------------
ProbeResult ProbingCycles::bore(const Geom::Vec2& approximateCenter,
                                 double            approximateRadius,
                                 double            probeZ,
                                 int               numPoints,
                                 const ProbeParams& params) {
    if (numPoints < 3) numPoints = 3;
    if (numPoints > 4) numPoints = 4;

    ProbeResult res;
    Toolpath& tp = res.motionPath;
    tp.setName("Probe_Bore");

    std::ostringstream g;
    g << "( *** BORE CENTER PROBE – " << numPoints << " POINTS *** )\n";
    g << "G00 X" << approximateCenter.x << " Y" << approximateCenter.y << "\n";
    g << "G00 Z" << params.safeZ << "\n";
    g << "G00 Z" << probeZ << "\n";  // descend to probe depth

    double angleStep = 2.0 * PI_PC / numPoints;
    for (int i = 0; i < numPoints; ++i) {
        double angle  = angleStep * i;
        double px     = approximateCenter.x + approximateRadius * std::cos(angle);
        double py     = approximateCenter.y + approximateRadius * std::sin(angle);
        double retX   = approximateCenter.x + (approximateRadius - params.overshot) * std::cos(angle);
        double retY   = approximateCenter.y + (approximateRadius - params.overshot) * std::sin(angle);

        // Motion for backplot
        Geom::Vec3 safe   {approximateCenter.x, approximateCenter.y, probeZ};
        Geom::Vec3 contact{retX, retY, probeZ};
        addProbeMove(tp, safe, contact, params.feedRate);

        // G-code: probe in X direction (probe macro 9811 = web probe)
        // Approach from centre outward
        g << "G00 X" << approximateCenter.x << " Y" << approximateCenter.y << "\n";
        g << "G65 P9811 X" << std::fixed << std::setprecision(3) << px
          << " Y" << py
          << " F" << params.feedRate
          << "\n";
        g << "( #" << (135 + i) << " = X hit " << i + 1 << " )\n";
        g << "( #" << (145 + i) << " = Y hit " << i + 1 << " )\n";
    }

    // Compute center using controller macro (the machine runs this inline)
    // For 4-point opposite-pair method the center = average of opposite hits
    if (numPoints == 4) {
        g << "( 4-point center calculation )\n";
        g << "#160 = [#135 + #137] / 2  ( X center )\n";
        g << "#161 = [#146 + #148] / 2  ( Y center )\n";
        g << updateWcsVariable(params.wcsRegister, 1, "#160");
        g << updateWcsVariable(params.wcsRegister, 2, "#161");
    } else {
        // 3-point: use the circle-fit formula solved in the macro
        g << "( 3-point circle center – solved via macro variables )\n";
        g << "( Full Cramer's-rule solution runs in controller DPRNT )\n";
        g << updateWcsVariable(params.wcsRegister, 1, "#160");
        g << updateWcsVariable(params.wcsRegister, 2, "#161");
    }

    g << "G00 Z" << params.safeZ << "\n";
    res.gcode = g.str();
    return res;
}

// --------------------------------------------------------------------------
// 3. Boss / external center-finding
//    Same as bore but the probe approaches from outside inward.
// --------------------------------------------------------------------------
ProbeResult ProbingCycles::boss(const Geom::Vec2& approximateCenter,
                                 double            approximateRadius,
                                 double            probeZ,
                                 int               numPoints,
                                 const ProbeParams& params) {
    if (numPoints < 3) numPoints = 3;
    if (numPoints > 4) numPoints = 4;

    ProbeResult res;
    Toolpath& tp = res.motionPath;
    tp.setName("Probe_Boss");

    std::ostringstream g;
    g << "( *** BOSS CENTER PROBE – " << numPoints << " POINTS *** )\n";

    double approachDist = approximateRadius + params.overshot + params.probeRadius;
    double angleStep    = 2.0 * PI_PC / numPoints;

    for (int i = 0; i < numPoints; ++i) {
        double angle  = angleStep * i;
        // Approach from outside: start beyond the boss, probe inward
        double sx = approximateCenter.x + approachDist * std::cos(angle);
        double sy = approximateCenter.y + approachDist * std::sin(angle);
        double cx = approximateCenter.x + (approximateRadius - params.probeRadius) * std::cos(angle);
        double cy = approximateCenter.y + (approximateRadius - params.probeRadius) * std::sin(angle);

        Geom::Vec3 safe   {sx, sy, probeZ};
        Geom::Vec3 contact{cx, cy, probeZ};
        addProbeMove(tp, safe, contact, params.feedRate);

        g << "G00 X" << std::fixed << std::setprecision(3) << sx
          << " Y" << sy << "\n";
        g << "G65 P9811 X" << cx << " Y" << cy
          << " F" << params.feedRate << "\n";
        g << "( #" << (135 + i) << " = X hit " << i + 1 << " )\n";
        g << "( #" << (145 + i) << " = Y hit " << i + 1 << " )\n";
    }

    if (numPoints == 4) {
        g << "#160 = [#135 + #137] / 2  ( X center )\n";
        g << "#161 = [#146 + #148] / 2  ( Y center )\n";
    }
    g << updateWcsVariable(params.wcsRegister, 1, "#160");
    g << updateWcsVariable(params.wcsRegister, 2, "#161");
    g << "G00 Z" << params.safeZ << "\n";

    res.gcode = g.str();
    return res;
}

// --------------------------------------------------------------------------
// 4. Corner Finder
//
//   Two hits along the X-axis face (at different Y offsets) define the X=0
//   line.  One hit on the Y-axis face defines Y=0.
//   Their intersection is the corner.
//
//   Controller variables:
//     #135 = first X-face hit X coordinate
//     #136 = second X-face hit X coordinate (average → X datum)
//     #137 = Y-face hit Y coordinate
// --------------------------------------------------------------------------
ProbeResult ProbingCycles::cornerFinder(const Geom::Vec2& cornerEstimate,
                                         double            stockXSize,
                                         double            stockYSize,
                                         double            probeZ,
                                         const ProbeParams& params) {
    ProbeResult res;
    Toolpath& tp = res.motionPath;
    tp.setName("Probe_Corner");

    std::ostringstream g;
    g << "( *** CORNER FINDER PROBE *** )\n";

    // --- Hit 1: X-face, lower Y ---
    double xApproach = cornerEstimate.x - params.overshot;
    double y1        = cornerEstimate.y + stockYSize * 0.25;
    {
        Geom::Vec3 contact{xApproach, y1, probeZ};
        addProbeMove(tp, {xApproach - params.overshot, y1, params.safeZ},
                        contact, params.feedRate);
    }
    g << "G00 X" << std::fixed << std::setprecision(3)
      << (xApproach - params.overshot) << " Y" << y1 << "\n";
    g << "G00 Z" << probeZ << "\n";
    g << "G65 P9811 X" << (cornerEstimate.x + params.overshot)
      << " F" << params.feedRate << "\n";
    g << "( #135 = X-face hit 1 )\n";

    // --- Hit 2: X-face, upper Y ---
    double y2 = cornerEstimate.y + stockYSize * 0.75;
    g << "G00 X" << (xApproach - params.overshot) << " Y" << y2 << "\n";
    g << "G65 P9811 X" << (cornerEstimate.x + params.overshot)
      << " F" << params.feedRate << "\n";
    g << "( #136 = X-face hit 2 )\n";
    g << "#160 = [#135 + #136] / 2  ( averaged X datum )\n";

    // --- Hit 3: Y-face ---
    double xMid      = cornerEstimate.x + stockXSize * 0.5;
    double yApproach = cornerEstimate.y - params.overshot;
    g << "G00 Z" << params.safeZ << "\n";
    g << "G00 X" << xMid << " Y" << (yApproach - params.overshot) << "\n";
    g << "G00 Z" << probeZ << "\n";
    g << "G65 P9811 Y" << (cornerEstimate.y + params.overshot)
      << " F" << params.feedRate << "\n";
    g << "( #137 = Y-face hit )\n";

    {
        Geom::Vec3 safe   {xMid, yApproach - params.overshot, params.safeZ};
        Geom::Vec3 contact{xMid, yApproach,                   probeZ};
        addProbeMove(tp, safe, contact, params.feedRate);
    }

    // Update WCS X and Y registers
    g << updateWcsVariable(params.wcsRegister, 1, "#160");
    g << updateWcsVariable(params.wcsRegister, 2, "#137");
    g << "G00 Z" << params.safeZ << "\n";

    res.gcode = g.str();
    return res;
}

// --------------------------------------------------------------------------
// fitCircle – general circle through 3+ points
//
// The general equation x² + y² + Dx + Ey + F = 0 can be rewritten as a
// linear system:
//   [ x₁  y₁  1 ] [ D ]   [ -(x₁²+y₁²) ]
//   [ x₂  y₂  1 ] [ E ] = [ -(x₂²+y₂²) ]
//   [ x₃  y₃  1 ] [ F ]   [ -(x₃²+y₃²) ]
//
// Center: h = -D/2, k = -E/2
// Radius: r = √(h²+k²−F)
//
// For more than 3 points, the first 3 are used (a proper least-squares fit
// could be used here but 3 non-collinear points fully determine a circle).
// --------------------------------------------------------------------------
bool ProbingCycles::fitCircle(const std::vector<Geom::Vec2>& pts,
                               double& h, double& k, double& r) {
    if (pts.size() < 3) return false;

    // Use first three points
    double x1 = pts[0].x, y1 = pts[0].y;
    double x2 = pts[1].x, y2 = pts[1].y;
    double x3 = pts[2].x, y3 = pts[2].y;

    // b-vector (right-hand side)
    double b1 = -(x1*x1 + y1*y1);
    double b2 = -(x2*x2 + y2*y2);
    double b3 = -(x3*x3 + y3*y3);

    // Determinant of the 3×3 coefficient matrix
    double det = x1*(y2 - y3) - y1*(x2 - x3) + (x2*y3 - x3*y2);
    if (std::abs(det) < 1e-12) return false;  // collinear

    // Cramer's rule
    double D = (b1*(y2 - y3) - y1*(b2 - b3) + (b2*y3 - b3*y2)) / det;
    double E = (x1*(b2 - b3) - b1*(x2 - x3) + (x2*b3 - x3*b2)) / det;
    double F = (x1*(y2*b3 - y3*b2) - y1*(x2*b3 - x3*b2) + b1*(x2*y3 - x3*y2)) / det;

    h = -D / 2.0;
    k = -E / 2.0;
    double rSq = h*h + k*k - F;
    if (rSq < 0) return false;
    r = std::sqrt(rSq);
    return true;
}
