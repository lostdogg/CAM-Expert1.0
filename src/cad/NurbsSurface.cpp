#include "NurbsSurface.h"
#include <stdexcept>
#include <cmath>

// --------------------------------------------------------------------------
NurbsSurface::NurbsSurface(int uDegree, int vDegree,
                             std::vector<double> knotsU,
                             std::vector<double> knotsV,
                             std::vector<std::vector<Geom::Vec3>> cp,
                             std::vector<std::vector<double>>     weights)
    : m_uDegree(uDegree), m_vDegree(vDegree),
      m_knotsU(std::move(knotsU)), m_knotsV(std::move(knotsV)),
      m_cp(std::move(cp)), m_weights(std::move(weights))
{
    // If weights are not supplied, default all to 1.0 (B-spline)
    if (m_weights.empty()) {
        m_weights.assign(m_cp.size(),
            std::vector<double>(m_cp.empty() ? 0 : m_cp[0].size(), 1.0));
    }
}

// --------------------------------------------------------------------------
// Cox-de Boor recursive B-spline basis function N_{i,p}(t)
// --------------------------------------------------------------------------
double NurbsSurface::basisFunc(int i, int p, double t,
                                const std::vector<double>& knots) const {
    if (p == 0)
        return (t >= knots[i] && t < knots[i+1]) ? 1.0 : 0.0;

    double left = 0.0, right = 0.0;
    double denom1 = knots[i+p]   - knots[i];
    double denom2 = knots[i+p+1] - knots[i+1];

    if (denom1 > 1e-12)
        left  = ((t - knots[i]) / denom1) * basisFunc(i,   p-1, t, knots);
    if (denom2 > 1e-12)
        right = ((knots[i+p+1] - t) / denom2) * basisFunc(i+1, p-1, t, knots);

    return left + right;
}

// --------------------------------------------------------------------------
Geom::Vec3 NurbsSurface::evalHomogeneous(double u, double v) const {
    // Clamp to domain
    double uMax = m_knotsU.back()  - 1e-12;
    double vMax = m_knotsV.back()  - 1e-12;
    u = std::max(m_knotsU.front(), std::min(u, uMax));
    v = std::max(m_knotsV.front(), std::min(v, vMax));

    int n = numCtrlU();
    int m = numCtrlV();

    Geom::Vec3 result{};
    double     wSum = 0.0;

    for (int i = 0; i < n; ++i) {
        double Ni = basisFunc(i, m_uDegree, u, m_knotsU);
        if (std::abs(Ni) < 1e-14) continue;
        for (int j = 0; j < m; ++j) {
            double Nj = basisFunc(j, m_vDegree, v, m_knotsV);
            if (std::abs(Nj) < 1e-14) continue;
            double w = m_weights[i][j];
            double b = Ni * Nj * w;
            result   = result + m_cp[i][j] * b;
            wSum    += b;
        }
    }

    if (wSum > 1e-12)
        result = result * (1.0 / wSum);
    return result;
}

// --------------------------------------------------------------------------
Geom::Vec3 NurbsSurface::evaluate(double u, double v) const {
    return evalHomogeneous(u, v);
}

// --------------------------------------------------------------------------
Geom::Vec3 NurbsSurface::derivU(double u, double v) const {
    const double h = (uMax() - uMin()) * 1e-5;
    return (evaluate(u+h, v) - evaluate(u-h, v)) * (1.0 / (2.0*h));
}

Geom::Vec3 NurbsSurface::derivV(double u, double v) const {
    const double h = (vMax() - vMin()) * 1e-5;
    return (evaluate(u, v+h) - evaluate(u, v-h)) * (1.0 / (2.0*h));
}

// --------------------------------------------------------------------------
Geom::Vec3 NurbsSurface::normal(double u, double v) const {
    return derivU(u, v).cross(derivV(u, v)).normalized();
}

// --------------------------------------------------------------------------
std::vector<Geom::Triangle> NurbsSurface::tessellate(int resU, int resV) const {
    std::vector<Geom::Triangle> tris;
    tris.reserve(2 * resU * resV);

    double du = (uMax() - uMin()) / resU;
    double dv = (vMax() - vMin()) / resV;

    for (int i = 0; i < resU; ++i) {
        for (int j = 0; j < resV; ++j) {
            double u0 = uMin() + i * du;
            double u1 = u0 + du;
            double v0 = vMin() + j * dv;
            double v1 = v0 + dv;

            Geom::Vec3 p00 = evaluate(u0, v0);
            Geom::Vec3 p10 = evaluate(u1, v0);
            Geom::Vec3 p01 = evaluate(u0, v1);
            Geom::Vec3 p11 = evaluate(u1, v1);

            tris.push_back({p00, p10, p11});
            tris.push_back({p00, p11, p01});
        }
    }
    return tris;
}
