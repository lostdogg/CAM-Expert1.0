#pragma once
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <cmath>
#include <vector>
#include <array>
#include <string>

namespace Geom {

// --------------------------------------------------------------------------
// Fundamental math types
// --------------------------------------------------------------------------
struct Vec2 {
    double x = 0, y = 0;
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(double s)      const { return {x*s,   y*s};   }
    double dot(const Vec2& o)     const { return x*o.x + y*o.y;  }
    double length()               const { return std::sqrt(x*x + y*y); }
    Vec2   normalized()           const { double l=length(); return l>0?Vec2{x/l,y/l}:Vec2{}; }
    bool operator==(const Vec2& o) const {
        return std::abs(x-o.x)<1e-9 && std::abs(y-o.y)<1e-9;
    }
};

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s)      const { return {x*s,   y*s,   z*s};   }
    Vec3 operator-()              const { return {-x, -y, -z};          }
    double dot(const Vec3& o)     const { return x*o.x + y*o.y + z*o.z; }
    Vec3   cross(const Vec3& o)   const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    double length()   const { return std::sqrt(x*x + y*y + z*z); }
    Vec3   normalized() const {
        double l = length();
        return l > 0 ? Vec3{x/l, y/l, z/l} : Vec3{};
    }
    bool operator==(const Vec3& o) const {
        return std::abs(x-o.x)<1e-9 && std::abs(y-o.y)<1e-9 && std::abs(z-o.z)<1e-9;
    }
};

// --------------------------------------------------------------------------
// Axis-Aligned Bounding Box
// --------------------------------------------------------------------------
struct AABB {
    Vec3 min{1e30, 1e30, 1e30};
    Vec3 max{-1e30,-1e30,-1e30};

    void expand(const Vec3& p) {
        if (p.x < min.x) min.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.z < min.z) min.z = p.z;
        if (p.x > max.x) max.x = p.x;
        if (p.y > max.y) max.y = p.y;
        if (p.z > max.z) max.z = p.z;
    }

    Vec3 center() const {
        return {(min.x+max.x)/2, (min.y+max.y)/2, (min.z+max.z)/2};
    }

    bool isValid() const { return min.x <= max.x; }
};

// --------------------------------------------------------------------------
// Plane defined by a point and a normal
// --------------------------------------------------------------------------
struct Plane {
    Vec3 origin;
    Vec3 normal;
    explicit Plane(Vec3 o = {}, Vec3 n = {0,0,1}) : origin(o), normal(n.normalized()) {}
    double distanceTo(const Vec3& p) const { return (p - origin).dot(normal); }
};

// --------------------------------------------------------------------------
// Ray for intersection tests
// --------------------------------------------------------------------------
struct Ray {
    Vec3 origin;
    Vec3 direction;   // must be normalized
    Vec3 at(double t) const { return origin + direction * t; }
};

// --------------------------------------------------------------------------
// A single triangle (mesh face)
// --------------------------------------------------------------------------
struct Triangle {
    Vec3 v[3];
    Vec3 normal() const {
        return (v[1]-v[0]).cross(v[2]-v[0]).normalized();
    }
};

// --------------------------------------------------------------------------
// 4×4 transformation matrix (column-major, OpenGL-compatible)
// --------------------------------------------------------------------------
struct Mat4 {
    double m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    static Mat4 identity() { return {}; }

    static Mat4 translate(Vec3 t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(double s) {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = s;
        return r;
    }

    // Rotation about an arbitrary axis by angleRad radians (Rodrigues' formula)
    static Mat4 rotate(Vec3 axis, double angleRad);

    Vec3 transformPoint(const Vec3& p) const {
        double w = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
        return {
            (m[0]*p.x + m[4]*p.y + m[8] *p.z + m[12]) / w,
            (m[1]*p.x + m[5]*p.y + m[9] *p.z + m[13]) / w,
            (m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]) / w
        };
    }

    Vec3 transformVector(const Vec3& v) const {
        return {
            m[0]*v.x + m[4]*v.y + m[8] *v.z,
            m[1]*v.x + m[5]*v.y + m[9] *v.z,
            m[2]*v.x + m[6]*v.y + m[10]*v.z
        };
    }
};

// --------------------------------------------------------------------------
// Non-inline geometry utilities (defined in Geometry.cpp)
// --------------------------------------------------------------------------

double clamp(double v, double lo, double hi);
double lerp(double a, double b, double t);
Vec3   lerp3(const Vec3& a, const Vec3& b, double t);

// Closest point on segment [p0,p1] to point p; optionally returns parameter t
Vec3   closestPointOnSegment(const Vec3& p, const Vec3& p0, const Vec3& p1,
                              double* tOut = nullptr);

// Area of triangle with vertices a,b,c
double triArea(const Vec3& a, const Vec3& b, const Vec3& c);

// Möller–Trumbore ray-triangle intersection; returns true on hit, sets tHit
bool   rayTriangleIntersect(const Ray& ray, const Triangle& tri,
                             double& tHit, double epsilon = 1e-9);

// AABB / overlap tests
bool   pointInAABB(const Vec3& p, const AABB& box);
bool   aabbsOverlap(const AABB& a, const AABB& b);

// Compose two 4×4 column-major matrices
Mat4   matMul(const Mat4& A, const Mat4& B);

} // namespace Geom

#endif // GEOMETRY_H
