#pragma once
#ifndef NURBS_SURFACE_H
#define NURBS_SURFACE_H

#include "Geometry.h"
#include <vector>

// --------------------------------------------------------------------------
// NURBS Surface (Non-Uniform Rational B-Spline)
//
// A general tensor-product NURBS surface of degree (uDegree × vDegree).
// The CAM engine uses the surface normal at a given (u,v) parameter to
// calculate gouge-free tool positioning in 3-D milling.
// --------------------------------------------------------------------------
class NurbsSurface {
public:
    NurbsSurface() = default;

    // Construct a NURBS surface with explicit control net
    NurbsSurface(int uDegree, int vDegree,
                 std::vector<double> knotsU,
                 std::vector<double> knotsV,
                 std::vector<std::vector<Geom::Vec3>> controlPoints,
                 std::vector<std::vector<double>>     weights = {});

    // Evaluate position at parameter (u, v)
    Geom::Vec3 evaluate(double u, double v) const;

    // Evaluate outward-pointing normal at (u, v)
    Geom::Vec3 normal(double u, double v) const;

    // Evaluate partial derivatives
    Geom::Vec3 derivU(double u, double v) const;
    Geom::Vec3 derivV(double u, double v) const;

    // Tessellate the surface into a triangle mesh for rendering
    // (resU × resV quads → 2 × resU × resV triangles)
    std::vector<Geom::Triangle> tessellate(int resU = 20, int resV = 20) const;

    // Parameter domain
    double uMin() const { return m_knotsU.front(); }
    double uMax() const { return m_knotsU.back(); }
    double vMin() const { return m_knotsV.front(); }
    double vMax() const { return m_knotsV.back(); }

    int uDegree() const { return m_uDegree; }
    int vDegree() const { return m_vDegree; }

    int numCtrlU() const { return static_cast<int>(m_cp.size()); }
    int numCtrlV() const { return m_cp.empty() ? 0 : static_cast<int>(m_cp[0].size()); }

private:
    // B-spline basis function (recursive Cox-de Boor)
    double basisFunc(int i, int p, double t,
                     const std::vector<double>& knots) const;

    // Weighted (homogeneous) evaluation helper
    Geom::Vec3 evalHomogeneous(double u, double v) const;

    int                                  m_uDegree = 3;
    int                                  m_vDegree = 3;
    std::vector<double>                  m_knotsU;
    std::vector<double>                  m_knotsV;
    std::vector<std::vector<Geom::Vec3>> m_cp;      // control points
    std::vector<std::vector<double>>     m_weights;  // NURBS weights (1 = B-spline)
};

#endif // NURBS_SURFACE_H
