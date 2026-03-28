#pragma once
#ifndef WIREFRAME_SCENE_H
#define WIREFRAME_SCENE_H

#include "Geometry.h"
#include <vector>
#include <cmath>

// -------------------------------------------------------------------------
// Construction Plane (Cplane) – defines the "paper" you draw on.
// The Cplane controls how 2-D cursor coordinates are lifted into 3-D world
// space, and which direction the Z-depth offset is applied.
// -------------------------------------------------------------------------
enum class CplaneType {
    Top,     // XY plane  (normal = +Z) – the default "Top" view
    Front,   // XZ plane  (normal = -Y)
    Right,   // YZ plane  (normal = +X)
    Back,    // XZ plane  (normal = +Y)
    Bottom,  // XY plane  (normal = -Z)
    Left,    // YZ plane  (normal = -X)
    Count    // sentinel – keep last
};

// Short display name shown in the status bar ("TOP", "FRONT", …)
const wchar_t* cplaneName(CplaneType cp);

// Unit normal that points out of the construction plane toward the viewer
Geom::Vec3 cplaneNormal(CplaneType cp);

// -------------------------------------------------------------------------
// Snap types reported by AutoCursor
// -------------------------------------------------------------------------
enum class SnapType {
    None,
    Endpoint,     // end / vertex of a line, arc, or polygon edge
    Midpoint,     // midpoint of a line segment or arc
    ArcCenter,    // mathematical center of a circle or arc
    Intersection  // crossing point of two entities (placeholder – future)
};

struct SnapResult {
    SnapType   type     = SnapType::None;
    Geom::Vec3 position = {};
    double     distance = 1e30;   // distance from cursor in world units
};

// -------------------------------------------------------------------------
// Wireframe entity type discriminator
// -------------------------------------------------------------------------
enum class WfEntityType {
    Point,
    Line,
    Arc,        // partial arc (startAngle != endAngle)
    Circle,     // full circle
    Spline,
    Rectangle,  // 4-sided; vertices stored in pts[]
    Polygon,    // N-sided; vertices stored in pts[]
    Ellipse,
    Helix
};

// -------------------------------------------------------------------------
// WfEntity – one piece of wireframe geometry stored in the scene.
// The union of fields covers every supported entity type.
// -------------------------------------------------------------------------
struct WfEntity {
    WfEntityType type  = WfEntityType::Point;
    int          level = 1;   // LevelsManager level

    // Primary geometry points
    Geom::Vec3 p0 = {};   // point / line-start / arc-center / ellipse-center
    Geom::Vec3 p1 = {};   // line-end

    // Radial / angular parameters (arc, circle, ellipse, helix)
    double radius     = 0.0;   // primary radius (or helix radius)
    double radius2    = 0.0;   // ellipse semi-minor axis
    double startAngle = 0.0;   // radians
    double endAngle   = 0.0;   // radians

    // Helix-specific
    double pitch       = 0.0;
    double revolutions = 0.0;
    double height      = 0.0;  // total height = pitch * revolutions

    // Polygon / rectangle vertices; spline control points
    std::vector<Geom::Vec3> pts;

    // Generic integer count (spline ctrl-pt count, etc.)
    int count = 0;
};

// -------------------------------------------------------------------------
// WireframeScene – the central repository for all wireframe geometry.
// Manages the active Cplane and Z-depth so that newly created entities
// land on the correct construction plane at the correct depth.
// -------------------------------------------------------------------------
class WireframeScene {
public:
    WireframeScene() = default;

    // Entity management
    void addEntity(WfEntity e);
    void clear();
    int  entityCount() const { return static_cast<int>(m_entities.size()); }
    const std::vector<WfEntity>& entities() const { return m_entities; }

    // Construction plane & Z-depth
    CplaneType cplane() const  { return m_cplane; }
    double     zDepth() const  { return m_zDepth; }
    void setCplane(CplaneType cp) { m_cplane = cp; }
    void setZDepth(double z)      { m_zDepth = z;  }
    void cycleCplane();   // advance to the next Cplane in the sequence

    // Map 2-D in-plane (x,y) coordinates to a 3-D world point, honouring
    // the current Cplane orientation and Z-depth offset.
    Geom::Vec3 toWorld(double x, double y) const;

private:
    std::vector<WfEntity> m_entities;
    CplaneType            m_cplane = CplaneType::Top;
    double                m_zDepth = 0.0;
};

// -------------------------------------------------------------------------
// AutoCursor – snap-point detection against a WireframeScene.
//
// Searches every entity in the scene for geometric snap candidates
// (endpoints, midpoints, arc centres, intersections) that lie within a
// configurable tolerance radius of the supplied cursor position.
// The best match is returned according to snap priority.
// -------------------------------------------------------------------------
class AutoCursor {
public:
    // Default snap tolerance in world units (e.g. mm).
    static constexpr double kDefaultTolerance = 5.0;

    // Return the highest-priority snap point near 'cursor' in 'scene'.
    // Priority: Intersection > Endpoint > Midpoint > ArcCenter.
    // Returns a SnapResult with type==None if nothing is within tolerance.
    static SnapResult findSnap(const WireframeScene& scene,
                               const Geom::Vec3& cursor,
                               double tolerance = kDefaultTolerance);

    // Parse a "Fast Point" coordinate string typed by the user while a
    // wireframe tool is active.  Accepted formats:
    //   "2, 4, 0"  |  "2 4 0"  |  "2,4,0"   (X Y Z)
    //   "2, 4"     |  "2 4"                   (X Y, Z taken as 0)
    // Returns true and fills 'out' on success; returns false otherwise.
    static bool parseFastPoint(const wchar_t* text, Geom::Vec3& out);

private:
    // Per-entity candidate collectors
    static void collectLineSnaps  (const WfEntity&, const Geom::Vec3&, double,
                                   std::vector<SnapResult>&);
    static void collectArcSnaps   (const WfEntity&, const Geom::Vec3&, double,
                                   std::vector<SnapResult>&);
    static void collectCircleSnaps(const WfEntity&, const Geom::Vec3&, double,
                                   std::vector<SnapResult>&);
    static void collectPolySnaps  (const WfEntity&, const Geom::Vec3&, double,
                                   std::vector<SnapResult>&);
    static void collectSplineSnaps(const WfEntity&, const Geom::Vec3&, double,
                                   std::vector<SnapResult>&);
};

#endif // WIREFRAME_SCENE_H
