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
//  • Tool Plane (Tplane) – the orientation of the spindle axis relative to
//    the WCS.  Determines A/B/C rotation angles output in the G-code.
//    In standard 3-axis milling WCS == Tplane.  In tombstone / 5-axis work
//    they diverge: keeping WCS on "Top" while the Tplane is set to "Front"
//    causes the post-processor to output a rotary move (e.g. B90.) so the
//    machine indexes to the second face without re-probing G54.
//
//  • Construction Plane (Cplane) – a temporary datum used for creating
//    geometry.  Equivalent to a "sketch pad" in parametric CAD.
//
// Work Offset Numbering (integer-based):
//   wcsOffset value  →  G-code output
//       -1           →  G54  (post-processor default, auto-assigned)
//        0           →  G54  (explicit first offset)
//        1           →  G55  (second vise / fixture)
//        2           →  G56  (third fixture)
//        3+          →  G57, G58, … (higher registers)
//
// The mapping is: G-code number = 54 + max(wcsOffset, 0)
//   except -1 which also maps to G54.
//
// View Sheets: named snapshots of (WCS id, Cplane id, Tplane id) that
//   can be recalled instantly to flip between set-up operations.
//
// Associativity: when the origin of the active WCS plane changes, all
//   toolpaths referencing that plane are dirtied automatically via the
//   DirtyCallback.
//
// Orthogonality: isOrthogonal() checks that the three basis vectors are
//   mutually perpendicular (dot-products ≈ 0 and cross-product ≈ 1).
//   A "Plane not orthogonal" warning is issued when this fails.
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

    // Integer-based work-offset selector (WCS planes only):
    //   -1  → G54 default (post auto-assigns)
    //    0  → G54 explicit
    //    1  → G55
    //    2  → G56
    //    n  → G(54+n)  for n >= 0
    int         wcsOffset = -1;

    // Build the 4×4 transform matrix for this plane
    Geom::Mat4 toMatrix() const;

    // Normal = zAxis
    Geom::Vec3 normal() const { return zAxis; }

    // Convert the integer wcsOffset to the actual G-code number (54..99)
    int gCodeOffsetNumber() const;

    // Verify that xAxis, yAxis, zAxis are mutually orthogonal (within tol)
    bool isOrthogonal(double tol = 1e-6) const;
};

// --------------------------------------------------------------------------
// ViewSheet – a named snapshot of WCS / Cplane / Tplane IDs.
// Clicking a View Sheet instantly restores the saved plane combination,
// e.g. "Op 1" = WCS:1, Cplane:1, Tplane:1; "Op 2" = WCS:2, Cplane:2, Tplane:3.
// --------------------------------------------------------------------------
struct ViewSheet {
    std::string name;
    int         wcsPlaneId    = 0;
    int         cplanePlaneId = 0;
    int         tplanePlaneId = 0;
};

class PlanesManager {
public:
    PlanesManager();

    // --- Plane CRUD ---
    int  addPlane(CoordPlane plane);
    void removePlane(int planeId);
    void clear();

    CoordPlane*       findPlane(int id);
    const CoordPlane* findPlane(int id) const;

    // --- Active plane selection ---
    void setActive(int planeId);
    int  activePlaneId() const { return m_activePlaneId; }
    const CoordPlane* activePlane() const { return findPlane(m_activePlaneId); }

    // --- Dedicated WCS / Cplane / Tplane slots ---
    // In 3-axis work all three point to the same plane; in multi-axis they differ.
    void setWcsPlane  (int id);
    void setCplane    (int id);
    void setTplane    (int id);
    int  wcsPlaneId   () const { return m_wcsPlaneId;    }
    int  cplanePlaneId() const { return m_cplanePlaneId; }
    int  tplanePlaneId() const { return m_tplanePlaneId; }
    const CoordPlane* wcsPlane  () const { return findPlane(m_wcsPlaneId);    }
    const CoordPlane* cplane    () const { return findPlane(m_cplanePlaneId); }
    const CoordPlane* tplane    () const { return findPlane(m_tplanePlaneId); }

    const std::vector<CoordPlane>& planes() const { return m_planes; }
    int count() const { return static_cast<int>(m_planes.size()); }

    // --- Predefined standard planes ---
    static CoordPlane standardTop();
    static CoordPlane standardFront();
    static CoordPlane standardRight();
    static CoordPlane standardIsometric();

    // --- Plane editing ---
    void renamePlane(int id, const std::string& name);

    // Set the integer work-offset selector (-1, 0, 1, 2, …) for a WCS plane.
    // Use -1 to let the post-processor default to G54.
    // Use 0 for explicit G54, 1 for G55, 2 for G56, etc.
    void setWcsOffset(int id, int wcsOffsetValue);

    // Move the origin of a plane; automatically dirtied associated toolpaths.
    void setOrigin(int id, const Geom::Vec3& origin);

    // Orthogonality check – returns false and fills 'msg' if not orthogonal
    static bool validateOrthogonal(const CoordPlane& p, std::string& msg);

    // --- View Sheets ---
    int  addViewSheet(const ViewSheet& vs);
    void removeViewSheet(int index);
    void activateViewSheet(int index);
    const std::vector<ViewSheet>& viewSheets() const { return m_viewSheets; }

    // --- Change / dirty callbacks ---
    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

    // Called whenever a WCS origin moves – allows ToolpathManager to dirty ops.
    using DirtyCallback = std::function<void(int wcsPlaneId)>;
    void setOnWcsDirty(DirtyCallback cb) { m_onWcsDirty = std::move(cb); }

private:
    void notify();
    void notifyDirty(int wcsPlaneId);
    int  nextId();

    std::vector<CoordPlane> m_planes;
    std::vector<ViewSheet>  m_viewSheets;

    int m_activePlaneId  = 1;
    int m_wcsPlaneId     = 1;
    int m_cplanePlaneId  = 1;
    int m_tplanePlaneId  = 1;
    int m_nextId         = 1;

    ChangeCallback m_onChange;
    DirtyCallback  m_onWcsDirty;
};

#endif // PLANES_MANAGER_H
