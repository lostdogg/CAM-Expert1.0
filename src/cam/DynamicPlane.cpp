#include "DynamicPlane.h"
#include <cmath>

static constexpr double PI_DP = 3.14159265358979323846;

// --------------------------------------------------------------------------
DynamicPlane::DynamicPlane() { reset(); }

// --------------------------------------------------------------------------
void DynamicPlane::reset() {
    m_plane = CoordPlane{};   // identity: origin=(0,0,0), X/Y/Z = world axes
}

// --------------------------------------------------------------------------
// Snap origin to an arbitrary 3-D point.
// --------------------------------------------------------------------------
void DynamicPlane::snapToPoint(const Geom::Vec3& point) {
    m_plane.origin = point;
}

// --------------------------------------------------------------------------
// Derive the plane orientation from a solid face.
//
// The Z-axis is set perpendicular to the face (= the face outward normal).
// The X-axis is constructed orthogonal to Z by crossing with the World X-axis
// (or World Y-axis if the face normal is nearly parallel to World X).
// The Y-axis completes the right-handed triad: Y = Z × X.
// --------------------------------------------------------------------------
void DynamicPlane::snapToFace(const Geom::Vec3& faceNormal,
                               const Geom::Vec3& pointOnFace) {
    m_plane.origin = pointOnFace;
    m_plane.zAxis  = faceNormal.normalized();

    // Choose a reference vector that is not nearly parallel to zAxis
    const Geom::Vec3 worldX{1, 0, 0};
    const Geom::Vec3 worldY{0, 1, 0};
    Geom::Vec3 ref = (std::abs(m_plane.zAxis.dot(worldX)) < 0.9) ? worldX : worldY;

    m_plane.xAxis = m_plane.zAxis.cross(ref).normalized();
    m_plane.yAxis = m_plane.zAxis.cross(m_plane.xAxis).normalized();
}

// --------------------------------------------------------------------------
// Translate the Gnomon origin along one of its own local axes.
// --------------------------------------------------------------------------
void DynamicPlane::translateAlongAxis(GnomonAxis axis, double distanceMM) {
    const Geom::Vec3& dir = axisVec(axis);
    m_plane.origin = m_plane.origin + dir * distanceMM;
}

// --------------------------------------------------------------------------
// Rotate the plane about one of the Gnomon's own axes by angleDeg degrees.
//
// Uses Rodrigues' rotation formula:
//   v' = v·cos(θ) + (k×v)·sin(θ) + k·(k·v)·(1−cos(θ))
// where k is the unit rotation axis.
// --------------------------------------------------------------------------
void DynamicPlane::rotateAboutAxis(GnomonAxis axis, double angleDeg) {
    double rad = angleDeg * (PI_DP / 180.0);
    double c   = std::cos(rad);
    double s   = std::sin(rad);

    const Geom::Vec3& k = axisVec(axis);  // unit rotation axis

    auto rodrigues = [&](const Geom::Vec3& v) -> Geom::Vec3 {
        // v' = v*cos + (k×v)*sin + k*(k·v)*(1-cos)
        double kDotV = k.dot(v);
        Geom::Vec3 kCrossV = k.cross(v);
        return Geom::Vec3{
            v.x * c + kCrossV.x * s + k.x * kDotV * (1.0 - c),
            v.y * c + kCrossV.y * s + k.y * kDotV * (1.0 - c),
            v.z * c + kCrossV.z * s + k.z * kDotV * (1.0 - c)
        };
    };

    // Rotate the other two axes (not the rotation axis itself)
    switch (axis) {
    case GnomonAxis::X:
        m_plane.yAxis = rodrigues(m_plane.yAxis).normalized();
        m_plane.zAxis = rodrigues(m_plane.zAxis).normalized();
        break;
    case GnomonAxis::Y:
        m_plane.xAxis = rodrigues(m_plane.xAxis).normalized();
        m_plane.zAxis = rodrigues(m_plane.zAxis).normalized();
        break;
    case GnomonAxis::Z:
        m_plane.xAxis = rodrigues(m_plane.xAxis).normalized();
        m_plane.yAxis = rodrigues(m_plane.yAxis).normalized();
        break;
    }
}

// --------------------------------------------------------------------------
// Commit the current Gnomon state as a named plane in the PlanesManager.
// Returns the plane id, or -1 if the plane is not orthogonal.
// --------------------------------------------------------------------------
int DynamicPlane::commit(PlanesManager& mgr,
                          const std::string& name,
                          PlaneType type,
                          int wcsOffset,
                          std::string* outWarning) {
    std::string warning;
    if (!PlanesManager::validateOrthogonal(m_plane, warning)) {
        if (outWarning) *outWarning = warning;
        return -1;
    }

    CoordPlane p  = m_plane;
    p.name        = name;
    p.type        = type;
    p.wcsOffset   = wcsOffset;
    return mgr.addPlane(p);
}

// --------------------------------------------------------------------------
const Geom::Vec3& DynamicPlane::axisVec(GnomonAxis axis) const {
    switch (axis) {
    case GnomonAxis::X: return m_plane.xAxis;
    case GnomonAxis::Y: return m_plane.yAxis;
    default:            return m_plane.zAxis;
    }
}

Geom::Vec3& DynamicPlane::axisVec(GnomonAxis axis) {
    switch (axis) {
    case GnomonAxis::X: return m_plane.xAxis;
    case GnomonAxis::Y: return m_plane.yAxis;
    default:            return m_plane.zAxis;
    }
}
