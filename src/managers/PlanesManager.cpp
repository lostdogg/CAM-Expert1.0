#include "PlanesManager.h"
#include <algorithm>

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
PlanesManager::PlanesManager() {
    // Default WCS (G54, top/XY plane)
    CoordPlane wcs;
    wcs.id        = 1;
    wcs.name      = "WCS (Top)";
    wcs.type      = PlaneType::WCS;
    wcs.wcsOffset = 54;
    m_planes.push_back(wcs);
    m_nextId        = 2;
    m_activePlaneId = 1;
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
    m_nextId = 1;
    // Re-add default WCS
    CoordPlane wcs;
    wcs.id = 1; wcs.name = "WCS (Top)"; wcs.type = PlaneType::WCS;
    m_planes.push_back(wcs);
    m_nextId = 2;
    m_activePlaneId = 1;
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
int  PlanesManager::nextId() { return m_nextId++; }
void PlanesManager::notify() { if (m_onChange) m_onChange(); }

// --------------------------------------------------------------------------
void PlanesManager::renamePlane(int id, const std::string& name) {
    if (auto* p = findPlane(id)) { p->name = name; notify(); }
}

void PlanesManager::setWcsOffset(int id, int wcsOffsetNumber) {
    if (auto* p = findPlane(id)) { p->wcsOffset = wcsOffsetNumber; notify(); }
}
