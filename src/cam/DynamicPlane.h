#pragma once
#ifndef DYNAMIC_PLANE_H
#define DYNAMIC_PLANE_H

#include "../cad/Geometry.h"
#include "../managers/PlanesManager.h"
#include <string>

// --------------------------------------------------------------------------
// DynamicPlane – interactive Gnomon-based coordinate-system builder
//
// The Dynamic Plane tool is a visual, interactive way to define a new
// coordinate system without typing raw numbers.  It exposes a "Gnomon"—a
// 3-D tripod—whose origin (centre ball) and axes can be manipulated:
//
//   Origin placement
//     snapToPoint()  – snap origin to an arbitrary 3-D point (corner,
//                       midpoint, arc-centre, …)
//     snapToFace()   – derive the plane from a flat solid face: origin at
//                       the face centroid, Z-axis perpendicular to the face.
//
//   Axis manipulation
//     translateAlongAxis() – drag the Gnomon along one of its own axes by a
//                            signed distance.
//     rotateAboutAxis()    – spin the plane around one of its axes by an
//                            angle in degrees (e.g. rotate Z by 90° to turn
//                            a "Top" plane into a "Side" plane).
//
//   Live coordinate display
//     liveCoords()  – returns the current X,Y,Z position of the Gnomon
//                     origin relative to the "World" origin (0,0,0).
//
//   Commit
//     commit()      – save the current gnomon state as a named CoordPlane
//                     into the supplied PlanesManager and return its id.
//
// The plane is validated for orthogonality before commit; a non-orthogonal
// configuration raises a warning (see PlanesManager::validateOrthogonal).
// --------------------------------------------------------------------------

// Axis selector used in translate / rotate calls
enum class GnomonAxis { X, Y, Z };

class DynamicPlane {
public:
    DynamicPlane();

    // ---- Origin placement ----

    // Snap the Gnomon origin to a specific 3-D point.
    void snapToPoint(const Geom::Vec3& point);

    // Align the plane to a flat face described by its outward normal and
    // a point on the face (typically the centroid).
    // The Z-axis of the new plane is set perpendicular to the face (= normal).
    // The X/Y axes are derived so that the triad remains orthogonal:
    //   • if |normal × worldX| > ε → xAxis = normalize(normal × worldX)
    //   • else                      → xAxis = normalize(normal × worldY)
    //   • yAxis = zAxis × xAxis
    void snapToFace(const Geom::Vec3& faceNormal,
                    const Geom::Vec3& pointOnFace);

    // ---- Axis manipulation ----

    // Translate the origin along one of the Gnomon's own axes.
    void translateAlongAxis(GnomonAxis axis, double distanceMM);

    // Rotate the plane about one of the Gnomon's own axes by angleDeg degrees.
    // Example: rotateAboutAxis(GnomonAxis::Z, 90) turns a Top plane into a
    //          Left-Side plane.
    void rotateAboutAxis(GnomonAxis axis, double angleDeg);

    // ---- Accessors ----

    // Current origin position in the World coordinate frame.
    const Geom::Vec3& origin() const { return m_plane.origin; }

    // Live World-space coordinates of the Gnomon origin (same as origin()).
    Geom::Vec3 liveCoords() const { return m_plane.origin; }

    // Access the working CoordPlane directly.
    const CoordPlane& plane() const { return m_plane; }

    // ---- Commit ----

    // Validate and save the plane into the PlanesManager.
    // Returns the new plane's id, or -1 if the plane is not orthogonal.
    // 'outWarning' is filled with the orthogonality diagnostic when id == -1.
    int commit(PlanesManager& mgr,
               const std::string& name,
               PlaneType type = PlaneType::WCS,
               int wcsOffset  = -1,
               std::string* outWarning = nullptr);

    // Reset the Gnomon to the world identity (origin at 0,0,0, axes = XYZ).
    void reset();

private:
    // Helper: return the Geom::Vec3 that corresponds to the chosen axis.
    const Geom::Vec3& axisVec(GnomonAxis axis) const;
    Geom::Vec3&       axisVec(GnomonAxis axis);

    CoordPlane m_plane;  // the working plane state
};

#endif // DYNAMIC_PLANE_H
