#include "PlanesManager.h"
#include <algorithm>
#include <cmath>
#include <sstream>

// --------------------------------------------------------------------------
// CoordPlane helpers
// --------------------------------------------------------------------------

Geom::Mat4 CoordPlane::toMatrix() const {
    Geom::Mat4 m;
    // Column-major order: each column is a basis vector
    m.m[0]  = xAxis.x; m.m[1]  = xAxis.y; m.m[2]  = xAxis.z; m.m[3]  = 0;
    m.m[4]  = yAxis.x; m.m[5]  = yAxis.y; m.m[6]  = yAxis.z; m.m[7]  = 0;
    m.m[8]  = zAxis.x; m.m[9]  = zAxis.y; m.m[10] = zAxis.z; m.m[11] = 0;
    m.m[12] = origin.x;m.m[13] = origin.y;m.m[14] = origin.z;m.m[15] = 1;
    return m;
}

// --------------------------------------------------------------------------
// Work Offset Numbering:
//   wcsOffset == -1  →  G54  (default, post auto-assigns)
//   wcsOffset ==  0  →  G54  (explicit)
//   wcsOffset ==  1  →  G55
//   wcsOffset ==  2  →  G56
//   wcsOffset ==  n  →  G(54+n)   (n >= 0)
// --------------------------------------------------------------------------
int CoordPlane::gCodeOffsetNumber() const {
    int idx = (wcsOffset < 0) ? 0 : wcsOffset;
    return 54 + idx;
}

// --------------------------------------------------------------------------
// Orthogonality check
// X·Y ≈ 0, X·Z ≈ 0, Y·Z ≈ 0  AND all three are unit vectors
// --------------------------------------------------------------------------
bool CoordPlane::isOrthogonal(double tol) const {
    auto isUnit = [tol](const Geom::Vec3& v) {
        return std::abs(v.length() - 1.0) < tol;
    };
    if (!isUnit(xAxis) || !isUnit(yAxis) || !isUnit(zAxis)) return false;
    if (std::abs(xAxis.dot(yAxis)) > tol) return false;
    if (std::abs(xAxis.dot(zAxis)) > tol) return false;
    if (std::abs(yAxis.dot(zAxis)) > tol) return false;
    return true;
}

// --------------------------------------------------------------------------
PlanesManager::PlanesManager() {
    // Default WCS (G54 default, top/XY plane)
    CoordPlane wcs;
    wcs.id        = 1;
    wcs.name      = "WCS (Top)";
    wcs.type      = PlaneType::WCS;
    wcs.wcsOffset = -1;   // post-processor default → G54
    m_planes.push_back(wcs);
    m_nextId        = 2;
    m_activePlaneId = 1;
    m_wcsPlaneId    = 1;
    m_cplanePlaneId = 1;
    m_tplanePlaneId = 1;
}

// --------------------------------------------------------------------------
int PlanesManager::addPlane(CoordPlane plane) {
    plane.id = nextId();
    m_planes.push_back(plane);
    notify();
    return plane.id;
}

void PlanesManager::removePlane(int planeId) {
    m_planes.erase(
        std::remove_if(m_planes.begin(), m_planes.end(),
            [planeId](const CoordPlane& p){ return p.id == planeId; }),
        m_planes.end());
    if (m_activePlaneId == planeId && !m_planes.empty())
        m_activePlaneId = m_planes[0].id;
    notify();
}

void PlanesManager::clear() {
    m_planes.clear();
    m_viewSheets.clear();
    m_nextId = 1;
    // Re-add default WCS
    CoordPlane wcs;
    wcs.id = 1; wcs.name = "WCS (Top)"; wcs.type = PlaneType::WCS;
    wcs.wcsOffset = -1;
    m_planes.push_back(wcs);
    m_nextId        = 2;
    m_activePlaneId = 1;
    m_wcsPlaneId    = 1;
    m_cplanePlaneId = 1;
    m_tplanePlaneId = 1;
    notify();
}

CoordPlane* PlanesManager::findPlane(int id) {
    for (auto& p : m_planes)
        if (p.id == id) return &p;
    return nullptr;
}

const CoordPlane* PlanesManager::findPlane(int id) const {
    for (const auto& p : m_planes)
        if (p.id == id) return &p;
    return nullptr;
}

void PlanesManager::setActive(int planeId) {
    if (findPlane(planeId)) {
        m_activePlaneId = planeId;
        notify();
    }
}

// --------------------------------------------------------------------------
// Dedicated WCS / Cplane / Tplane slot setters
// --------------------------------------------------------------------------
void PlanesManager::setWcsPlane(int id) {
    if (findPlane(id)) { m_wcsPlaneId = id; notify(); }
}

void PlanesManager::setCplane(int id) {
    if (findPlane(id)) { m_cplanePlaneId = id; notify(); }
}

void PlanesManager::setTplane(int id) {
    if (findPlane(id)) { m_tplanePlaneId = id; notify(); }
}

// --------------------------------------------------------------------------
// Standard planes
// --------------------------------------------------------------------------
CoordPlane PlanesManager::standardTop() {
    CoordPlane p;
    p.name   = "Top (XY)";
    p.type   = PlaneType::ConstructionPlane;
    p.xAxis  = {1,0,0}; p.yAxis = {0,1,0}; p.zAxis = {0,0,1};
    return p;
}

CoordPlane PlanesManager::standardFront() {
    CoordPlane p;
    p.name   = "Front (XZ)";
    p.type   = PlaneType::ConstructionPlane;
    p.xAxis  = {1,0,0}; p.yAxis = {0,0,1}; p.zAxis = {0,-1,0};
    return p;
}

CoordPlane PlanesManager::standardRight() {
    CoordPlane p;
    p.name   = "Right (YZ)";
    p.type   = PlaneType::ConstructionPlane;
    p.xAxis  = {0,1,0}; p.yAxis = {0,0,1}; p.zAxis = {1,0,0};
    return p;
}

CoordPlane PlanesManager::standardIsometric() {
    CoordPlane p;
    p.name   = "Isometric";
    p.type   = PlaneType::ConstructionPlane;
    double c = 0.7071;
    p.xAxis  = {c,  -c, 0};
    p.yAxis  = {c*0.577, c*0.577, 0.816};
    p.zAxis  = {-0.408, -0.408, 0.816};
    return p;
}

// --------------------------------------------------------------------------
int  PlanesManager::nextId()  { return m_nextId++; }
void PlanesManager::notify()  { if (m_onChange)   m_onChange(); }
void PlanesManager::notifyDirty(int id) { if (m_onWcsDirty) m_onWcsDirty(id); }

// --------------------------------------------------------------------------
void PlanesManager::renamePlane(int id, const std::string& name) {
    if (auto* p = findPlane(id)) { p->name = name; notify(); }
}

// --------------------------------------------------------------------------
// Set integer work-offset value for a WCS plane.
// value: -1 = G54 default, 0 = G54, 1 = G55, 2 = G56, etc.
// --------------------------------------------------------------------------
void PlanesManager::setWcsOffset(int id, int wcsOffsetValue) {
    if (auto* p = findPlane(id)) { p->wcsOffset = wcsOffsetValue; notify(); }
}

// --------------------------------------------------------------------------
// Move the origin of a plane – automatically dirtied associated toolpaths.
// --------------------------------------------------------------------------
void PlanesManager::setOrigin(int id, const Geom::Vec3& origin) {
    if (auto* p = findPlane(id)) {
        p->origin = origin;
        notify();
        if (p->type == PlaneType::WCS)
            notifyDirty(id);
    }
}

// --------------------------------------------------------------------------
// Orthogonality validation
// --------------------------------------------------------------------------
bool PlanesManager::validateOrthogonal(const CoordPlane& p, std::string& msg) {
    if (p.isOrthogonal()) return true;
    std::ostringstream oss;
    oss << "Plane \"" << p.name << "\" is not orthogonal: "
        << "X·Y=" << p.xAxis.dot(p.yAxis)
        << " X·Z=" << p.xAxis.dot(p.zAxis)
        << " Y·Z=" << p.yAxis.dot(p.zAxis);
    msg = oss.str();
    return false;
}

// --------------------------------------------------------------------------
// View Sheets
// --------------------------------------------------------------------------
int PlanesManager::addViewSheet(const ViewSheet& vs) {
    m_viewSheets.push_back(vs);
    notify();
    return static_cast<int>(m_viewSheets.size()) - 1;
}

void PlanesManager::removeViewSheet(int index) {
    if (index >= 0 && index < static_cast<int>(m_viewSheets.size())) {
        m_viewSheets.erase(m_viewSheets.begin() + index);
        notify();
    }
}

// Activating a View Sheet instantly restores the saved WCS/Cplane/Tplane.
void PlanesManager::activateViewSheet(int index) {
    if (index < 0 || index >= static_cast<int>(m_viewSheets.size())) return;
    const ViewSheet& vs = m_viewSheets[index];
    if (findPlane(vs.wcsPlaneId))    m_wcsPlaneId    = vs.wcsPlaneId;
    if (findPlane(vs.cplanePlaneId)) m_cplanePlaneId = vs.cplanePlaneId;
    if (findPlane(vs.tplanePlaneId)) m_tplanePlaneId = vs.tplanePlaneId;
    m_activePlaneId = m_wcsPlaneId;
    notify();
}

