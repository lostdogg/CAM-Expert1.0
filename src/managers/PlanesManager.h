#pragma once
#ifndef PLANES_MANAGER_H
#define PLANES_MANAGER_H

#include "../cad/Geometry.h"
#include <string>
#include <vector>
#include <functional>

// --------------------------------------------------------------------------
// PlanesManager
//
// Manages coordinate systems used for multi-axis machining:
//
//  • WCS (Work Coordinate System) – the datum the part is fixtured to.
//    Corresponds to G54–G59 offsets on the machine.
//
//  • Tool Plane – the orientation of the spindle axis relative to the WCS.
//    Determines A/B/C rotation angles.
//
//  • Construction Plane (C-Plane) – a temporary datum used for creating
//    geometry. Equivalent to a "sketch plane" in parametric CAD.
//
// Multiple planes can be stored and selected. The active plane determines
// which coordinate frame is used for toolpath generation and simulation.
// --------------------------------------------------------------------------

enum class PlaneType { WCS, ToolPlane, ConstructionPlane, Custom };

struct CoordPlane {
    int         id       = 0;
    std::string name;
    PlaneType   type     = PlaneType::WCS;

    // Basis vectors (default = standard XYZ)
    Geom::Vec3  origin{};
    Geom::Vec3  xAxis{1, 0, 0};
    Geom::Vec3  yAxis{0, 1, 0};
    Geom::Vec3  zAxis{0, 0, 1};

    // G54-G59 work-offset number (for WCS planes)
    int         wcsOffset = 54;

    // Build the 4×4 transform matrix for this plane
    Geom::Mat4 toMatrix() const;

    // Normal = zAxis
    Geom::Vec3 normal() const { return zAxis; }
};

class PlanesManager {
public:
    PlanesManager();

    int  addPlane(CoordPlane plane);
    void removePlane(int planeId);
    void clear();

    CoordPlane*       findPlane(int id);
    const CoordPlane* findPlane(int id) const;

    void setActive(int planeId);
    int  activePlaneId() const { return m_activePlaneId; }
    const CoordPlane* activePlane() const { return findPlane(m_activePlaneId); }

    const std::vector<CoordPlane>& planes() const { return m_planes; }
    int count() const { return static_cast<int>(m_planes.size()); }

    // Predefined standard planes
    static CoordPlane standardTop();
    static CoordPlane standardFront();
    static CoordPlane standardRight();
    static CoordPlane standardIsometric();

    // Rename an existing plane
    void renamePlane(int id, const std::string& name);

    // Update the WCS offset (G54-G59) for a WCS-type plane
    void setWcsOffset(int id, int wcsOffsetNumber);

    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    void notify();
    int  nextId();

    std::vector<CoordPlane> m_planes;
    int                     m_activePlaneId = 1;
    int                     m_nextId        = 1;
    ChangeCallback          m_onChange;
};

#endif // PLANES_MANAGER_H
