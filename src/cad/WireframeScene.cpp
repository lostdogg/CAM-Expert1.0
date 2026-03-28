#include "WireframeScene.h"
#include <algorithm>
#include <cwchar>

// =========================================================================
// Cplane helpers
// =========================================================================

const wchar_t* cplaneName(CplaneType cp) {
    switch (cp) {
    case CplaneType::Top:    return L"TOP";
    case CplaneType::Front:  return L"FRONT";
    case CplaneType::Right:  return L"RIGHT";
    case CplaneType::Back:   return L"BACK";
    case CplaneType::Bottom: return L"BOTTOM";
    case CplaneType::Left:   return L"LEFT";
    default:                 return L"TOP";
    }
}

Geom::Vec3 cplaneNormal(CplaneType cp) {
    switch (cp) {
    case CplaneType::Top:    return { 0,  0,  1 };
    case CplaneType::Front:  return { 0, -1,  0 };
    case CplaneType::Right:  return { 1,  0,  0 };
    case CplaneType::Back:   return { 0,  1,  0 };
    case CplaneType::Bottom: return { 0,  0, -1 };
    case CplaneType::Left:   return {-1,  0,  0 };
    default:                 return { 0,  0,  1 };
    }
}

// =========================================================================
// WireframeScene
// =========================================================================

void WireframeScene::addEntity(WfEntity e) {
    m_entities.push_back(std::move(e));
}

void WireframeScene::clear() {
    m_entities.clear();
}

void WireframeScene::cycleCplane() {
    int next = (static_cast<int>(m_cplane) + 1) % static_cast<int>(CplaneType::Count);
    m_cplane = static_cast<CplaneType>(next);
}

Geom::Vec3 WireframeScene::toWorld(double x, double y) const {
    // Maps the 2-D (x,y) in-plane coordinates to 3-D world space, offset
    // by m_zDepth along the plane normal.
    switch (m_cplane) {
    case CplaneType::Top:
    case CplaneType::Bottom:
        // In-plane X→world X, in-plane Y→world Y; depth along world Z.
        return { x, y, m_zDepth };
    case CplaneType::Front:
    case CplaneType::Back:
        // In-plane X→world X, in-plane Y→world Z; depth along world Y.
        return { x, m_zDepth, y };
    case CplaneType::Right:
    case CplaneType::Left:
        // In-plane X→world Y, in-plane Y→world Z; depth along world X.
        return { m_zDepth, x, y };
    default:
        return { x, y, m_zDepth };
    }
}

// =========================================================================
// AutoCursor – internal helpers
// =========================================================================

static double dist3(const Geom::Vec3& a, const Geom::Vec3& b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static void trySnap(std::vector<SnapResult>& out,
                    SnapType t,
                    const Geom::Vec3& pos,
                    const Geom::Vec3& cursor,
                    double tol) {
    double d = dist3(pos, cursor);
    if (d <= tol) {
        SnapResult r;
        r.type     = t;
        r.position = pos;
        r.distance = d;
        out.push_back(r);
    }
}

// -------------------------------------------------------------------------
void AutoCursor::collectLineSnaps(const WfEntity& e,
                                  const Geom::Vec3& cursor,
                                  double tol,
                                  std::vector<SnapResult>& out) {
    // Endpoints
    trySnap(out, SnapType::Endpoint, e.p0, cursor, tol);
    trySnap(out, SnapType::Endpoint, e.p1, cursor, tol);
    // Midpoint
    Geom::Vec3 mid = { (e.p0.x + e.p1.x) * 0.5,
                       (e.p0.y + e.p1.y) * 0.5,
                       (e.p0.z + e.p1.z) * 0.5 };
    trySnap(out, SnapType::Midpoint, mid, cursor, tol);
}

// -------------------------------------------------------------------------
void AutoCursor::collectArcSnaps(const WfEntity& e,
                                 const Geom::Vec3& cursor,
                                 double tol,
                                 std::vector<SnapResult>& out) {
    // Arc center
    trySnap(out, SnapType::ArcCenter, e.p0, cursor, tol);
    // Start and end points
    Geom::Vec3 startPt = { e.p0.x + e.radius * std::cos(e.startAngle),
                           e.p0.y + e.radius * std::sin(e.startAngle),
                           e.p0.z };
    Geom::Vec3 endPt   = { e.p0.x + e.radius * std::cos(e.endAngle),
                           e.p0.y + e.radius * std::sin(e.endAngle),
                           e.p0.z };
    trySnap(out, SnapType::Endpoint, startPt, cursor, tol);
    trySnap(out, SnapType::Endpoint, endPt,   cursor, tol);
    // Midpoint of the arc
    double midAngle = (e.startAngle + e.endAngle) * 0.5;
    Geom::Vec3 midPt = { e.p0.x + e.radius * std::cos(midAngle),
                         e.p0.y + e.radius * std::sin(midAngle),
                         e.p0.z };
    trySnap(out, SnapType::Midpoint, midPt, cursor, tol);
}

// -------------------------------------------------------------------------
void AutoCursor::collectCircleSnaps(const WfEntity& e,
                                    const Geom::Vec3& cursor,
                                    double tol,
                                    std::vector<SnapResult>& out) {
    // Center
    trySnap(out, SnapType::ArcCenter, e.p0, cursor, tol);
    // Four quadrant snap points (0°, 90°, 180°, 270°)
    static const double angles[4] = { 0.0, 1.5707963, 3.1415926, 4.7123889 };
    for (double a : angles) {
        Geom::Vec3 q = { e.p0.x + e.radius * std::cos(a),
                         e.p0.y + e.radius * std::sin(a),
                         e.p0.z };
        trySnap(out, SnapType::Endpoint, q, cursor, tol);
    }
}

// -------------------------------------------------------------------------
void AutoCursor::collectPolySnaps(const WfEntity& e,
                                  const Geom::Vec3& cursor,
                                  double tol,
                                  std::vector<SnapResult>& out) {
    if (e.pts.empty()) return;
    for (size_t i = 0; i < e.pts.size(); ++i) {
        // Vertices are endpoints
        trySnap(out, SnapType::Endpoint, e.pts[i], cursor, tol);
        // Midpoint of each edge
        size_t j = (i + 1) % e.pts.size();
        Geom::Vec3 mid = { (e.pts[i].x + e.pts[j].x) * 0.5,
                           (e.pts[i].y + e.pts[j].y) * 0.5,
                           (e.pts[i].z + e.pts[j].z) * 0.5 };
        trySnap(out, SnapType::Midpoint, mid, cursor, tol);
    }
}

// -------------------------------------------------------------------------
void AutoCursor::collectSplineSnaps(const WfEntity& e,
                                    const Geom::Vec3& cursor,
                                    double tol,
                                    std::vector<SnapResult>& out) {
    for (const auto& cp : e.pts)
        trySnap(out, SnapType::Endpoint, cp, cursor, tol);
}

// =========================================================================
// AutoCursor::findSnap
// =========================================================================

SnapResult AutoCursor::findSnap(const WireframeScene& scene,
                                const Geom::Vec3& cursor,
                                double tolerance) {
    std::vector<SnapResult> candidates;
    candidates.reserve(64);

    for (const WfEntity& e : scene.entities()) {
        switch (e.type) {
        case WfEntityType::Point:
            trySnap(candidates, SnapType::Endpoint, e.p0, cursor, tolerance);
            break;
        case WfEntityType::Line:
            collectLineSnaps(e, cursor, tolerance, candidates);
            break;
        case WfEntityType::Arc:
            collectArcSnaps(e, cursor, tolerance, candidates);
            break;
        case WfEntityType::Circle:
            collectCircleSnaps(e, cursor, tolerance, candidates);
            break;
        case WfEntityType::Rectangle:
        case WfEntityType::Polygon:
            collectPolySnaps(e, cursor, tolerance, candidates);
            break;
        case WfEntityType::Spline:
            collectSplineSnaps(e, cursor, tolerance, candidates);
            break;
        case WfEntityType::Ellipse:
            // Snap to center and four axis-points
            trySnap(candidates, SnapType::ArcCenter, e.p0, cursor, tolerance);
            {
                Geom::Vec3 q0 = { e.p0.x + e.radius,  e.p0.y,           e.p0.z };
                Geom::Vec3 q1 = { e.p0.x,              e.p0.y + e.radius2, e.p0.z };
                Geom::Vec3 q2 = { e.p0.x - e.radius,  e.p0.y,           e.p0.z };
                Geom::Vec3 q3 = { e.p0.x,              e.p0.y - e.radius2, e.p0.z };
                trySnap(candidates, SnapType::Endpoint, q0, cursor, tolerance);
                trySnap(candidates, SnapType::Endpoint, q1, cursor, tolerance);
                trySnap(candidates, SnapType::Endpoint, q2, cursor, tolerance);
                trySnap(candidates, SnapType::Endpoint, q3, cursor, tolerance);
            }
            break;
        case WfEntityType::Helix:
            // Snap to base center and top center
            trySnap(candidates, SnapType::Endpoint, e.p0, cursor, tolerance);
            {
                Geom::Vec3 top = { e.p0.x, e.p0.y, e.p0.z + e.height };
                trySnap(candidates, SnapType::Endpoint, top, cursor, tolerance);
            }
            break;
        }
    }

    if (candidates.empty())
        return SnapResult{};

    // Sort by priority (lower = higher priority), then by distance within
    // the same priority tier.
    auto priority = [](SnapType t) -> int {
        switch (t) {
        case SnapType::Intersection: return 0;
        case SnapType::Endpoint:     return 1;
        case SnapType::Midpoint:     return 2;
        case SnapType::ArcCenter:    return 3;
        default:                     return 4;
        }
    };
    std::sort(candidates.begin(), candidates.end(),
              [&](const SnapResult& a, const SnapResult& b) {
                  int pa = priority(a.type), pb = priority(b.type);
                  if (pa != pb) return pa < pb;
                  return a.distance < b.distance;
              });

    return candidates.front();
}

// =========================================================================
// AutoCursor::parseFastPoint
// =========================================================================

bool AutoCursor::parseFastPoint(const wchar_t* text, Geom::Vec3& out) {
    if (!text) return false;
    double x = 0.0, y = 0.0, z = 0.0;

    // Try "X , Y , Z"  or  "X,Y,Z"  or  "X Y Z"
    int n = std::swscanf(text, L" %lf , %lf , %lf", &x, &y, &z);
    if (n == 3) { out = {x, y, z}; return true; }

    n = std::swscanf(text, L" %lf,%lf,%lf", &x, &y, &z);
    if (n == 3) { out = {x, y, z}; return true; }

    n = std::swscanf(text, L" %lf %lf %lf", &x, &y, &z);
    if (n == 3) { out = {x, y, z}; return true; }

    // Two-value form: X Y or X,Y  (Z defaults to 0)
    n = std::swscanf(text, L" %lf , %lf", &x, &y);
    if (n == 2) { out = {x, y, 0.0}; return true; }

    n = std::swscanf(text, L" %lf %lf", &x, &y);
    if (n == 2) { out = {x, y, 0.0}; return true; }

    return false;
}
