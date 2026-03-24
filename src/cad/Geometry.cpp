#include "Geometry.h"
#include <algorithm>

namespace Geom {

// --------------------------------------------------------------------------
// clamp – restrict v to [lo, hi]
// --------------------------------------------------------------------------
double clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// --------------------------------------------------------------------------
// lerp – linear interpolation between a and b by parameter t in [0,1]
// --------------------------------------------------------------------------
double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

Vec3 lerp3(const Vec3& a, const Vec3& b, double t) {
    return { lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t) };
}

// --------------------------------------------------------------------------
// closestPointOnSegment
// Returns the closest point on segment [p0,p1] to point p, along with the
// parameter t in [0,1] such that result = p0 + t*(p1-p0).
// --------------------------------------------------------------------------
Vec3 closestPointOnSegment(const Vec3& p, const Vec3& p0, const Vec3& p1,
                            double* tOut) {
    Vec3   d  = p1 - p0;
    double dd = d.dot(d);
    if (dd < 1e-18) {
        if (tOut) *tOut = 0.0;
        return p0;
    }
    double t = clamp((p - p0).dot(d) / dd, 0.0, 1.0);
    if (tOut) *tOut = t;
    return p0 + d * t;
}

// --------------------------------------------------------------------------
// triArea – area of a triangle given its three vertices
// --------------------------------------------------------------------------
double triArea(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).cross(c - a).length() * 0.5;
}

// --------------------------------------------------------------------------
// rayTriangleIntersect – Möller–Trumbore algorithm
// Returns true if the ray hits the triangle, and sets tHit to the distance
// along the ray. Epsilon is used to reject near-parallel rays.
// --------------------------------------------------------------------------
bool rayTriangleIntersect(const Ray& ray, const Triangle& tri,
                           double& tHit, double epsilon) {
    const Vec3& v0 = tri.v[0];
    const Vec3& v1 = tri.v[1];
    const Vec3& v2 = tri.v[2];

    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 h     = ray.direction.cross(edge2);
    double a   = edge1.dot(h);

    if (a > -epsilon && a < epsilon)
        return false; // Ray is parallel to the triangle

    double f = 1.0 / a;
    Vec3   s = ray.origin - v0;
    double u = f * s.dot(h);
    if (u < 0.0 || u > 1.0)
        return false;

    Vec3   q = s.cross(edge1);
    double v = f * ray.direction.dot(q);
    if (v < 0.0 || u + v > 1.0)
        return false;

    double t = f * edge2.dot(q);
    if (t < epsilon)
        return false; // Intersection behind the ray origin

    tHit = t;
    return true;
}

// --------------------------------------------------------------------------
// pointInAABB – test whether a point lies inside an axis-aligned bounding box
// --------------------------------------------------------------------------
bool pointInAABB(const Vec3& p, const AABB& box) {
    return p.x >= box.min.x && p.x <= box.max.x &&
           p.y >= box.min.y && p.y <= box.max.y &&
           p.z >= box.min.z && p.z <= box.max.z;
}

// --------------------------------------------------------------------------
// aabbsOverlap – test whether two AABBs intersect
// --------------------------------------------------------------------------
bool aabbsOverlap(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

// --------------------------------------------------------------------------
// Mat4 multiply – compose two 4×4 column-major matrices
// --------------------------------------------------------------------------
Mat4 matMul(const Mat4& A, const Mat4& B) {
    Mat4 C;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            double s = 0;
            for (int k = 0; k < 4; ++k)
                s += A.m[k * 4 + row] * B.m[col * 4 + k];
            C.m[col * 4 + row] = s;
        }
    return C;
}

// --------------------------------------------------------------------------
// Mat4 rotation about an arbitrary axis (Rodrigues' formula, angle in radians)
// --------------------------------------------------------------------------
Mat4 Mat4::rotate(Vec3 axis, double angleRad) {
    axis = axis.normalized();
    double c = std::cos(angleRad);
    double s = std::sin(angleRad);
    double t = 1.0 - c;
    double x = axis.x, y = axis.y, z = axis.z;

    Mat4 r;
    r.m[0]  = t*x*x + c;   r.m[4]  = t*x*y - s*z; r.m[8]  = t*x*z + s*y; r.m[12] = 0;
    r.m[1]  = t*x*y + s*z; r.m[5]  = t*y*y + c;   r.m[9]  = t*y*z - s*x; r.m[13] = 0;
    r.m[2]  = t*x*z - s*y; r.m[6]  = t*y*z + s*x; r.m[10] = t*z*z + c;   r.m[14] = 0;
    r.m[3]  = 0;            r.m[7]  = 0;            r.m[11] = 0;           r.m[15] = 1;
    return r;
}

} // namespace Geom
