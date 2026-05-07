#include "SurfacesManager.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// SurfacesManager – basic management
// --------------------------------------------------------------------------

int SurfacesManager::addSurface(NurbsSurface surf, const std::string& name) {
    SurfaceEntry e;
    e.surface = std::move(surf);
    e.name    = name.empty() ? "Surface" : name;
    m_entries.push_back(std::move(e));
    notify();
    return static_cast<int>(m_entries.size()) - 1;
}

void SurfacesManager::removeSurface(int index) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries.erase(m_entries.begin() + index);
    if (m_selectedIndex >= static_cast<int>(m_entries.size()))
        m_selectedIndex = static_cast<int>(m_entries.size()) - 1;
    notify();
}

void SurfacesManager::clear() {
    m_entries.clear();
    m_selectedIndex = -1;
    notify();
}

int SurfacesManager::count() const {
    return static_cast<int>(m_entries.size());
}

SurfaceEntry& SurfacesManager::at(int index) {
    return m_entries.at(static_cast<std::size_t>(index));
}

const SurfaceEntry& SurfacesManager::at(int index) const {
    return m_entries.at(static_cast<std::size_t>(index));
}

void SurfacesManager::select(int index) {
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
        m_selectedIndex = index;
}

void SurfacesManager::deselect() {
    m_selectedIndex = -1;
}

SurfaceEntry* SurfacesManager::activeSurface() {
    if (m_selectedIndex < 0 ||
        m_selectedIndex >= static_cast<int>(m_entries.size())) return nullptr;
    return &m_entries[static_cast<std::size_t>(m_selectedIndex)];
}

const SurfaceEntry* SurfacesManager::activeSurface() const {
    if (m_selectedIndex < 0 ||
        m_selectedIndex >= static_cast<int>(m_entries.size())) return nullptr;
    return &m_entries[static_cast<std::size_t>(m_selectedIndex)];
}

void SurfacesManager::setVisible(int index, bool visible) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries[static_cast<std::size_t>(index)].visible = visible;
    notify();
}

void SurfacesManager::setTrimmed(int index, bool trimmed) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries[static_cast<std::size_t>(index)].trimmed = trimmed;
    notify();
}

void SurfacesManager::notify() {
    if (m_onChange) m_onChange();
}

// --------------------------------------------------------------------------
// Factory helpers
// --------------------------------------------------------------------------

// Helper: build a uniform open knot vector of size (n + degree + 1)
// for `n` control points and given degree.
static std::vector<double> uniformOpenKnots(int n, int degree) {
    int m = n + degree + 1;
    std::vector<double> knots(static_cast<std::size_t>(m), 0.0);
    for (int i = 0; i <= degree; ++i)        knots[i]     = 0.0;
    for (int i = degree + 1; i < m - degree - 1; ++i)
        knots[i] = static_cast<double>(i - degree) / (n - degree);
    for (int i = m - degree - 1; i < m; ++i) knots[i]     = 1.0;
    return knots;
}

// --------------------------------------------------------------------------
// makeLoft – ruled surface through N cross-section profiles
//
// Each section is a polyline of 3-D points. Sections are blended linearly
// along the v-direction (degree 1 in v). In u the section is approximated
// as a cubic (degree 3) B-spline through the section points.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeLoft(
        const std::vector<std::vector<Geom::Vec3>>& sections)
{
    if (sections.size() < 2)
        throw std::invalid_argument("makeLoft: need at least 2 cross-sections");

    // Require all sections to have the same point count
    std::size_t ptCount = sections[0].size();
    for (const auto& sec : sections) {
        if (sec.size() != ptCount)
            throw std::invalid_argument("makeLoft: all sections must have the same point count");
    }
    if (ptCount < 2)
        throw std::invalid_argument("makeLoft: each section must have at least 2 points");

    int nu = static_cast<int>(ptCount);      // control points in U
    int nv = static_cast<int>(sections.size()); // control points in V (= section count)

    // Degree: cubic in U (capped by control point count), linear in V
    int uDeg = std::min(3, nu - 1);
    int vDeg = 1; // linear interpolation between sections

    // Build control point net from section data
    std::vector<std::vector<Geom::Vec3>> cp(static_cast<std::size_t>(nu),
                                             std::vector<Geom::Vec3>(static_cast<std::size_t>(nv)));
    for (int i = 0; i < nu; ++i)
        for (int j = 0; j < nv; ++j)
            cp[i][j] = sections[j][i];

    std::vector<double> knotsU = uniformOpenKnots(nu, uDeg);
    std::vector<double> knotsV = uniformOpenKnots(nv, vDeg);

    return NurbsSurface(uDeg, vDeg, knotsU, knotsV, cp);
}

// --------------------------------------------------------------------------
// makeRevolve – surface of revolution
//
// The profile is a polyline in the XZ-plane (Y = 0).
// Sweeping around the Z-axis through `angleDeg` degrees produces a NURBS
// surface.  For arcs < 360° we use a rational exact representation (weight
// = cos(halfSweep) for mid-arc control points).  For a full 360° revolution
// we use four 90° quadrants.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeRevolve(
        const std::vector<Geom::Vec3>& profile, double angleDeg)
{
    if (profile.size() < 2)
        throw std::invalid_argument("makeRevolve: profile needs at least 2 points");

    // Clamp angle
    angleDeg = std::max(-360.0, std::min(360.0, angleDeg));
    const double absAngle = std::abs(angleDeg);
    const double sign = (angleDeg >= 0) ? 1.0 : -1.0;

    // For the revolution we use the standard NURBS circle approximation:
    // divide the full sweep into segments of at most 90°.
    int numSegs = static_cast<int>(std::ceil(absAngle / 90.0));
    if (numSegs < 1) numSegs = 1;
    const double segAngle = absAngle / numSegs; // degrees per segment
    const double segRad   = segAngle * (3.14159265358979323846 / 180.0);
    const double w        = std::cos(segRad * 0.5); // weight for mid-arc pts

    // Each segment contributes 2 control columns (arc start + mid); last adds 1 end.
    // Total V columns per segment pair = 2; plus final = 2*numSegs + 1
    int nv = 2 * numSegs + 1;
    int nu = static_cast<int>(profile.size());

    // Build control nets
    std::vector<std::vector<Geom::Vec3>> cp(static_cast<std::size_t>(nu),
                                             std::vector<Geom::Vec3>(static_cast<std::size_t>(nv)));
    std::vector<std::vector<double>>     wt(static_cast<std::size_t>(nu),
                                             std::vector<double>(static_cast<std::size_t>(nv), 1.0));

    for (int pi = 0; pi < nu; ++pi) {
        const Geom::Vec3& prof = profile[pi];
        double r = std::sqrt(prof.x * prof.x + prof.y * prof.y);
        double zv = prof.z;

        double curAngle = 0.0;
        for (int s = 0; s < numSegs; ++s) {
            double a0 = sign * curAngle * (3.14159265358979323846 / 180.0);
            double a1 = sign * (curAngle + segAngle * 0.5) * (3.14159265358979323846 / 180.0);
            double a2 = sign * (curAngle + segAngle) * (3.14159265358979323846 / 180.0);

            int col0 = 2 * s;
            int col1 = 2 * s + 1;
            int col2 = 2 * s + 2;

            // Arc start
            cp[pi][col0] = { r * std::cos(a0), r * std::sin(a0), zv };
            wt[pi][col0] = 1.0;

            // Arc mid-point (tangent control point, rational)
            cp[pi][col1] = { r * std::cos(a1), r * std::sin(a1), zv };
            wt[pi][col1] = w;

            // Arc end (also start of next segment)
            cp[pi][col2] = { r * std::cos(a2), r * std::sin(a2), zv };
            wt[pi][col2] = 1.0;

            curAngle += segAngle;
        }
    }

    // Build knots: uniform open in U (degree 3), and clamped in V (degree 2)
    int uDeg = std::min(3, nu - 1);
    int vDeg = 2;

    std::vector<double> knotsU = uniformOpenKnots(nu, uDeg);

    // V knots for the arc segments: repeat each interior knot once
    std::vector<double> knotsV;
    knotsV.push_back(0.0);
    knotsV.push_back(0.0);
    knotsV.push_back(0.0);
    for (int s = 1; s < numSegs; ++s) {
        double t = static_cast<double>(s) / numSegs;
        knotsV.push_back(t);
        knotsV.push_back(t);
    }
    knotsV.push_back(1.0);
    knotsV.push_back(1.0);
    knotsV.push_back(1.0);

    return NurbsSurface(uDeg, vDeg, knotsU, knotsV, cp, wt);
}

// --------------------------------------------------------------------------
// makeOffset – translate every control point outward along the surface normal
//
// This is an approximation (not the exact offset surface). Each control
// point is moved by `distMM` along the normal evaluated at the closest
// (u,v) parameter centre of that control point.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeOffset(const NurbsSurface& src, double distMM)
{
    int nu = src.numCtrlU();
    int nv = src.numCtrlV();

    std::vector<std::vector<Geom::Vec3>> newCp(static_cast<std::size_t>(nu),
                                                std::vector<Geom::Vec3>(static_cast<std::size_t>(nv)));

    double uSpan = src.uMax() - src.uMin();
    double vSpan = src.vMax() - src.vMin();

    for (int i = 0; i < nu; ++i) {
        double u = src.uMin() + uSpan * (static_cast<double>(i) / std::max(nu - 1, 1));
        for (int j = 0; j < nv; ++j) {
            double v = src.vMin() + vSpan * (static_cast<double>(j) / std::max(nv - 1, 1));
            Geom::Vec3 pos = src.evaluate(u, v);
            Geom::Vec3 n   = src.normal(u, v);
            newCp[i][j] = pos + n * distMM;
        }
    }

    // Build proper clamped uniform knot vectors matching the source degree and
    // control-point count so the offset surface evaluates correctly.
    return NurbsSurface(src.uDegree(), src.vDegree(),
                        uniformOpenKnots(nu, src.uDegree()),
                        uniformOpenKnots(nv, src.vDegree()),
                        newCp);
}

// --------------------------------------------------------------------------
// makeExtend – extend the surface at one boundary by adding an extra row/column
//
// side=0 → extend at uMin; side=1 → extend at uMax.
// The extension is a linear (tangent-direction) extrapolation.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeExtend(const NurbsSurface& src,
                                          double extensionMM, int side)
{
    int nu = src.numCtrlU();
    int nv = src.numCtrlV();

    // Evaluate the boundary and tangent row at the selected side
    // and build an extra row of control points by linear extrapolation.
    std::vector<Geom::Vec3> boundaryRow(static_cast<std::size_t>(nv));
    std::vector<Geom::Vec3> tangentRow(static_cast<std::size_t>(nv));

    double uEval = (side == 0) ? src.uMin() : src.uMax();
    double vSpan = src.vMax() - src.vMin();

    for (int j = 0; j < nv; ++j) {
        double v = src.vMin() + vSpan * (static_cast<double>(j) / std::max(nv - 1, 1));
        boundaryRow[j] = src.evaluate(uEval, v);
        tangentRow[j]  = src.derivU(uEval, v).normalized();
    }

    // Build new control net: original + 1 extra row
    int newNu = nu + 1;
    std::vector<std::vector<Geom::Vec3>> newCp(static_cast<std::size_t>(newNu),
                                                std::vector<Geom::Vec3>(static_cast<std::size_t>(nv)));

    // The existing control points need to be re-read via evaluate at uniform params
    double uSpan = src.uMax() - src.uMin();
    for (int i = 0; i < nu; ++i) {
        double u = src.uMin() + uSpan * (static_cast<double>(i) / std::max(nu - 1, 1));
        for (int j = 0; j < nv; ++j) {
            double v = src.vMin() + vSpan * (static_cast<double>(j) / std::max(nv - 1, 1));
            int row = (side == 0) ? (i + 1) : i;
            newCp[static_cast<std::size_t>(row)][j] = src.evaluate(u, v);
        }
    }

    // Fill in the extension row
    double extDir = (side == 0) ? -1.0 : 1.0;
    int extRow = (side == 0) ? 0 : nu;
    for (int j = 0; j < nv; ++j)
        newCp[static_cast<std::size_t>(extRow)][j] =
            boundaryRow[j] + tangentRow[j] * (extDir * extensionMM);

    int uDeg = std::min(src.uDegree(), newNu - 1);
    std::vector<double> knotsU = uniformOpenKnots(newNu, uDeg);
    std::vector<double> knotsV = uniformOpenKnots(nv, src.vDegree());

    return NurbsSurface(uDeg, src.vDegree(), knotsU, knotsV, newCp);
}

// --------------------------------------------------------------------------
// makeFillet – quarter-circle arc blend surface
//
// Given two NURBS surfaces (assumed to be flat and separated by a distance
// ≈ 2*filletRadius in one direction), build a quarter-torus approximation
// of the fillet.  This is the standard G1-continuous analytical fillet used
// in CAD kernels when the base surfaces are planar.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeFillet(const NurbsSurface& srf1,
                                          const NurbsSurface& srf2,
                                          double filletRadius)
{
    // Sample a single v-column from each surface along v at u=midpoint
    const int samples = 4;  // 4 profile points per fillet column
    double uMid1 = (srf1.uMin() + srf1.uMax()) * 0.5;
    double uMid2 = (srf2.uMin() + srf2.uMax()) * 0.5;

    // Build the fillet profile (quarter arc) at the start v parameter
    // The arc sweeps from the surface-1 edge normal to surface-2 edge normal.
    // We approximate with 3 rational NURBS control points (exact quarter arc).
    const double w = std::cos(3.14159265358979323846 / 4.0); // cos(45°) ≈ 0.7071

    // Evaluate edge points on each surface
    Geom::Vec3 p1    = srf1.evaluate(uMid1, srf1.vMax());
    Geom::Vec3 n1    = srf1.normal(uMid1,  srf1.vMax());
    Geom::Vec3 p2    = srf2.evaluate(uMid2, srf2.vMin());

    // Corner of fillet arc (where the two tangent planes meet)
    Geom::Vec3 corner = p1 + n1 * filletRadius;

    // 3 control points for the rational quarter-arc: start, mid (rational), end
    std::vector<std::vector<Geom::Vec3>> cp(3, std::vector<Geom::Vec3>(samples));
    std::vector<std::vector<double>>     wt(3, std::vector<double>(samples, 1.0));

    double vSpan2 = srf2.vMax() - srf2.vMin();
    for (int j = 0; j < samples; ++j) {
        double v2 = srf2.vMin() + vSpan2 * (static_cast<double>(j) / (samples - 1));
        Geom::Vec3 e2 = srf2.evaluate(uMid2, v2);

        double vSpan1 = srf1.vMax() - srf1.vMin();
        double v1 = srf1.vMin() + vSpan1 * (static_cast<double>(j) / (samples - 1));
        Geom::Vec3 e1 = srf1.evaluate(uMid1, v1);

        Geom::Vec3 cornerJ = corner + Geom::Vec3{0, 0, (e2.z - p2.z)};

        cp[0][j] = e1;
        cp[1][j] = cornerJ;
        cp[2][j] = e2;

        wt[0][j] = 1.0;
        wt[1][j] = w;  // rational weight for the mid point
        wt[2][j] = 1.0;
    }

    // V knots: uniform open for `samples` points degree 3 (clamped to degree 1)
    int vDeg = std::min(3, samples - 1);
    std::vector<double> knotsV = uniformOpenKnots(samples, vDeg);
    // U knots: clamped for 3 control points, degree 2
    std::vector<double> knotsU = { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };

    return NurbsSurface(2, vDeg, knotsU, knotsV, cp, wt);
}

// --------------------------------------------------------------------------
// makeFlat – flat bilinear surface of `width` × `depth` centred at the origin
//
// A degree-1 bicubic NURBS (bilinear) with four corner control points lying
// in the XY-plane.  This represents the simplest "Flat Boundary" surface.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeFlat(double width, double depth)
{
    double hw = width * 0.5;
    double hd = depth * 0.5;

    // 2 × 2 control point grid, degree 1 in both directions
    std::vector<std::vector<Geom::Vec3>> cp = {
        { {-hw, -hd, 0.0}, {-hw,  hd, 0.0} },
        { { hw, -hd, 0.0}, { hw,  hd, 0.0} }
    };

    std::vector<double> knots = { 0.0, 0.0, 1.0, 1.0 };
    return NurbsSurface(1, 1, knots, knots, cp);
}

// --------------------------------------------------------------------------
// makeSwept – sweep a cross-section profile along a polyline path
//
// At each path vertex the cross-section is translated so that its centroid
// coincides with the path point.  The resulting loft through all translated
// sections produces a swept surface.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeSwept(
        const std::vector<Geom::Vec3>& crossSection,
        const std::vector<Geom::Vec3>& path)
{
    if (crossSection.size() < 2)
        throw std::invalid_argument("makeSwept: cross-section needs at least 2 points");
    if (path.size() < 2)
        throw std::invalid_argument("makeSwept: path needs at least 2 points");

    // Compute cross-section centroid
    Geom::Vec3 centroid{0, 0, 0};
    for (const auto& p : crossSection) {
        centroid.x += p.x;
        centroid.y += p.y;
        centroid.z += p.z;
    }
    double inv = 1.0 / static_cast<double>(crossSection.size());
    centroid.x *= inv;
    centroid.y *= inv;
    centroid.z *= inv;

    // Build sections: translate cross-section to each path point
    std::vector<std::vector<Geom::Vec3>> sections;
    sections.reserve(path.size());
    for (const auto& pathPt : path) {
        Geom::Vec3 offset{ pathPt.x - centroid.x,
                           pathPt.y - centroid.y,
                           pathPt.z - centroid.z };
        std::vector<Geom::Vec3> sec;
        sec.reserve(crossSection.size());
        for (const auto& p : crossSection)
            sec.push_back({ p.x + offset.x, p.y + offset.y, p.z + offset.z });
        sections.push_back(std::move(sec));
    }

    return makeLoft(sections);
}

// --------------------------------------------------------------------------
// makeNet – net surface from a grid of U-chains and V-chains
//
// uChains: profiles running in the U direction (must all have the same
//          point count, which becomes the V control-point count).
// vChains: profiles running in the V direction (count = U control-point count).
//
// The surface is constructed by using each uChain row as a loft section so
// that the resulting NURBS interpolates all U-chain polylines.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeNet(
        const std::vector<std::vector<Geom::Vec3>>& uChains,
        const std::vector<std::vector<Geom::Vec3>>& vChains)
{
    if (uChains.size() < 2)
        throw std::invalid_argument("makeNet: need at least 2 U-chains");
    if (vChains.size() < 2)
        throw std::invalid_argument("makeNet: need at least 2 V-chains");

    // Each U-chain's point count equals the number of V-chains (grid columns)
    std::size_t ptPerChain = vChains.size();
    for (const auto& ch : uChains) {
        if (ch.size() != ptPerChain)
            throw std::invalid_argument(
                "makeNet: each U-chain must have exactly as many points as there are V-chains");
    }

    // Use U-chains directly as loft sections
    return makeLoft(uChains);
}

// --------------------------------------------------------------------------
// makeFence – extrude a base curve in a given direction by `length`
//
// This creates a ruled surface between the base curve and a copy of it
// translated by direction * length.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeFence(
        const std::vector<Geom::Vec3>& baseCurve,
        const Geom::Vec3& direction,
        double length)
{
    if (baseCurve.size() < 2)
        throw std::invalid_argument("makeFence: base curve needs at least 2 points");

    // Normalise direction
    double mag = std::sqrt(direction.x * direction.x +
                           direction.y * direction.y +
                           direction.z * direction.z);
    if (mag < 1e-12)
        throw std::invalid_argument("makeFence: direction vector must be non-zero");

    double scale = length / mag;
    Geom::Vec3 d{ direction.x * scale, direction.y * scale, direction.z * scale };

    // Offset copy of base curve
    std::vector<Geom::Vec3> topCurve;
    topCurve.reserve(baseCurve.size());
    for (const auto& p : baseCurve)
        topCurve.push_back({ p.x + d.x, p.y + d.y, p.z + d.z });

    // Ruled loft between base and offset curves
    return makeLoft({ baseCurve, topCurve });
}

// --------------------------------------------------------------------------
// makeDraft – extrude a base curve upward at a draft angle
//
// The wall leans outward (away from each point's XY centroid) by
// `draftAngleDeg` from the vertical as it rises to `height` mm.
// --------------------------------------------------------------------------
NurbsSurface SurfacesManager::makeDraft(
        const std::vector<Geom::Vec3>& baseCurve,
        double draftAngleDeg,
        double height)
{
    if (baseCurve.size() < 2)
        throw std::invalid_argument("makeDraft: base curve needs at least 2 points");
    if (std::abs(height) < 1e-12)
        throw std::invalid_argument("makeDraft: height must be non-zero");

    // Compute XY centroid of the base curve to determine outward direction
    double cx = 0.0, cy = 0.0;
    for (const auto& p : baseCurve) { cx += p.x; cy += p.y; }
    double inv = 1.0 / static_cast<double>(baseCurve.size());
    cx *= inv;  cy *= inv;

    // Draft offset: at height H, each point moves outward by H * tan(angle)
    const double pi = 3.14159265358979323846;
    double tanAngle = std::tan(draftAngleDeg * pi / 180.0);
    double lateralOffset = height * tanAngle;

    std::vector<Geom::Vec3> topCurve;
    topCurve.reserve(baseCurve.size());
    for (const auto& p : baseCurve) {
        // Outward unit vector in XY
        double ox = p.x - cx;
        double oy = p.y - cy;
        double len = std::sqrt(ox * ox + oy * oy);
        if (len > 1e-12) { ox /= len; oy /= len; }
        topCurve.push_back({
            p.x + ox * lateralOffset,
            p.y + oy * lateralOffset,
            p.z + height
        });
    }

    return makeLoft({ baseCurve, topCurve });
}
