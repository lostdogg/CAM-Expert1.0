#include "ConstraintSolver.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr double kEps = 1e-8;
constexpr double kPi  = 3.14159265358979323846;

static Geom::Vec3 midpoint(const Geom::Vec3& a, const Geom::Vec3& b) {
    return {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5};
}
}

int ConstraintSolver::addConstraint(SketchConstraint c) {
    c.id = m_nextId++;
    m_constraints.push_back(std::move(c));
    m_dirty = true;
    return m_constraints.back().id;
}

bool ConstraintSolver::removeConstraint(int id) {
    auto it = std::remove_if(m_constraints.begin(), m_constraints.end(),
                             [id](const SketchConstraint& c) { return c.id == id; });
    if (it == m_constraints.end()) return false;
    m_constraints.erase(it, m_constraints.end());
    m_dirty = true;
    return true;
}

void ConstraintSolver::clearConstraints() {
    m_constraints.clear();
    m_dirty = true;
}

bool ConstraintSolver::getRefPoint(const std::vector<WfEntity>& entities,
                                   const SketchRef& r,
                                   Geom::Vec3& out) {
    if (r.entityIndex < 0 || r.entityIndex >= static_cast<int>(entities.size()))
        return false;
    const auto& e = entities[static_cast<std::size_t>(r.entityIndex)];
    switch (e.type) {
    case WfEntityType::Point:
    case WfEntityType::Arc:
    case WfEntityType::Circle:
    case WfEntityType::Ellipse:
    case WfEntityType::Helix:
        out = e.p0; return true;
    case WfEntityType::Line:
        out = (r.selector == 1) ? e.p1 : e.p0;
        return true;
    case WfEntityType::Rectangle:
    case WfEntityType::Polygon:
    case WfEntityType::Spline:
        if (r.selector < 0 || r.selector >= static_cast<int>(e.pts.size()))
            return false;
        out = e.pts[static_cast<std::size_t>(r.selector)];
        return true;
    default:
        return false;
    }
}

bool ConstraintSolver::setRefPoint(std::vector<WfEntity>& entities,
                                   const SketchRef& r,
                                   const Geom::Vec3& p) {
    if (r.entityIndex < 0 || r.entityIndex >= static_cast<int>(entities.size()))
        return false;
    auto& e = entities[static_cast<std::size_t>(r.entityIndex)];
    switch (e.type) {
    case WfEntityType::Point:
    case WfEntityType::Arc:
    case WfEntityType::Circle:
    case WfEntityType::Ellipse:
    case WfEntityType::Helix:
        e.p0 = p; return true;
    case WfEntityType::Line:
        if (r.selector == 1) e.p1 = p;
        else e.p0 = p;
        return true;
    case WfEntityType::Rectangle:
    case WfEntityType::Polygon:
    case WfEntityType::Spline:
        if (r.selector < 0 || r.selector >= static_cast<int>(e.pts.size()))
            return false;
        e.pts[static_cast<std::size_t>(r.selector)] = p;
        return true;
    default:
        return false;
    }
}

bool ConstraintSolver::getLineEndpoints(const std::vector<WfEntity>& entities,
                                        int idx,
                                        Geom::Vec3& p0,
                                        Geom::Vec3& p1) {
    if (idx < 0 || idx >= static_cast<int>(entities.size()))
        return false;
    const auto& e = entities[static_cast<std::size_t>(idx)];
    if (e.type != WfEntityType::Line)
        return false;
    p0 = e.p0;
    p1 = e.p1;
    return true;
}

bool ConstraintSolver::setLineEndpoints(std::vector<WfEntity>& entities,
                                        int idx,
                                        const Geom::Vec3& p0,
                                        const Geom::Vec3& p1) {
    if (idx < 0 || idx >= static_cast<int>(entities.size()))
        return false;
    auto& e = entities[static_cast<std::size_t>(idx)];
    if (e.type != WfEntityType::Line)
        return false;
    e.p0 = p0;
    e.p1 = p1;
    return true;
}

bool ConstraintSolver::applyConstraint(const SketchConstraint& c,
                                       std::vector<WfEntity>& entities,
                                       SolveResult& result) const {
    if (!c.enabled) return true;

    Geom::Vec3 a{}, b{};
    switch (c.type) {
    case SketchConstraintType::Coincident: {
        if (!getRefPoint(entities, c.a, a) || !getRefPoint(entities, c.b, b))
            return false;
        Geom::Vec3 m = midpoint(a, b);
        return setRefPoint(entities, c.a, m) && setRefPoint(entities, c.b, m);
    }
    case SketchConstraintType::FixPoint: {
        // Keep reference A where it is currently; acts as anchor for other constraints.
        if (!getRefPoint(entities, c.a, a))
            return false;
        return setRefPoint(entities, c.a, a);
    }
    case SketchConstraintType::Horizontal: {
        Geom::Vec3 p0, p1;
        if (!getLineEndpoints(entities, c.a.entityIndex, p0, p1))
            return false;
        p1.y = p0.y;
        return setLineEndpoints(entities, c.a.entityIndex, p0, p1);
    }
    case SketchConstraintType::Vertical: {
        Geom::Vec3 p0, p1;
        if (!getLineEndpoints(entities, c.a.entityIndex, p0, p1))
            return false;
        p1.x = p0.x;
        return setLineEndpoints(entities, c.a.entityIndex, p0, p1);
    }
    case SketchConstraintType::Distance: {
        if (!getRefPoint(entities, c.a, a) || !getRefPoint(entities, c.b, b))
            return false;
        auto d = b - a;
        double len = d.length();
        if (len < kEps) {
            b.x += c.value;
            return setRefPoint(entities, c.b, b);
        }
        Geom::Vec3 dir = d * (1.0 / len);
        Geom::Vec3 m   = midpoint(a, b);
        Geom::Vec3 half = dir * (std::max(0.0, c.value) * 0.5);
        return setRefPoint(entities, c.a, m - half) && setRefPoint(entities, c.b, m + half);
    }
    case SketchConstraintType::EqualLength: {
        Geom::Vec3 a0, a1, b0, b1;
        if (!getLineEndpoints(entities, c.a.entityIndex, a0, a1) ||
            !getLineEndpoints(entities, c.b.entityIndex, b0, b1))
            return false;
        auto va = a1 - a0;
        auto vb = b1 - b0;
        double la = va.length();
        double lb = vb.length();
        if (la < kEps || lb < kEps) return false;
        double target = (la + lb) * 0.5;
        Geom::Vec3 da = va * (1.0 / la);
        Geom::Vec3 db = vb * (1.0 / lb);
        Geom::Vec3 ma = midpoint(a0, a1);
        Geom::Vec3 mb = midpoint(b0, b1);
        bool okA = setLineEndpoints(entities, c.a.entityIndex, ma - da * (target * 0.5), ma + da * (target * 0.5));
        bool okB = setLineEndpoints(entities, c.b.entityIndex, mb - db * (target * 0.5), mb + db * (target * 0.5));
        return okA && okB;
    }
    case SketchConstraintType::Parallel:
    case SketchConstraintType::Perpendicular:
    case SketchConstraintType::Angle: {
        Geom::Vec3 a0, a1, b0, b1;
        if (!getLineEndpoints(entities, c.a.entityIndex, a0, a1) ||
            !getLineEndpoints(entities, c.b.entityIndex, b0, b1))
            return false;
        auto va = a1 - a0;
        auto vb = b1 - b0;
        double la = va.length();
        double lb = vb.length();
        if (la < kEps || lb < kEps) return false;
        Geom::Vec3 da = va * (1.0 / la);
        double targetAngleRad = 0.0;
        if (c.type == SketchConstraintType::Parallel) targetAngleRad = 0.0;
        else if (c.type == SketchConstraintType::Perpendicular) targetAngleRad = 0.5 * kPi;
        else targetAngleRad = c.value * (kPi / 180.0);

        // Rotate A direction to target and apply to B while preserving B midpoint and length.
        double cs = std::cos(targetAngleRad);
        double sn = std::sin(targetAngleRad);
        Geom::Vec3 desired{
            da.x * cs - da.y * sn,
            da.x * sn + da.y * cs,
            0.0
        };
        if (desired.length() < kEps)
            return false;
        desired = desired.normalized();
        Geom::Vec3 mb = midpoint(b0, b1);
        return setLineEndpoints(entities, c.b.entityIndex,
                                mb - desired * (lb * 0.5),
                                mb + desired * (lb * 0.5));
    }
    case SketchConstraintType::Radius: {
        if (c.a.entityIndex < 0 || c.a.entityIndex >= static_cast<int>(entities.size()))
            return false;
        auto& e = entities[static_cast<std::size_t>(c.a.entityIndex)];
        if (e.type != WfEntityType::Arc && e.type != WfEntityType::Circle)
            return false;
        e.radius = std::max(0.0, c.value);
        return true;
    }
    default:
        result.diagnostics.push_back({c.id, true, "Unsupported constraint type."});
        result.underConstrained = true;
        return true;
    }
}

SolveResult ConstraintSolver::solve(std::vector<WfEntity>& entities, int maxIterations) {
    SolveResult res;
    if (m_inEdit) {
        res.status = SolveResult::Status::DeferredRebuild;
        res.diagnostics.push_back({0, true, "Solve deferred while sketch is in edit mode."});
        return res;
    }
    if (entities.empty()) {
        res.status = SolveResult::Status::InvalidInput;
        res.diagnostics.push_back({0, false, "No sketch entities available for solve."});
        return res;
    }
    if (m_constraints.empty()) {
        res.status = SolveResult::Status::SolvedWithWarnings;
        res.underConstrained = true;
        res.diagnostics.push_back({0, true, "No constraints defined; sketch is unconstrained."});
        m_dirty = false;
        return res;
    }

    int iterations = std::max(1, maxIterations);
    for (int iter = 0; iter < iterations; ++iter) {
        int appliedThisIter = 0;
        bool failed = false;
        for (const auto& c : m_constraints) {
            if (!c.enabled) continue;
            if (applyConstraint(c, entities, res)) {
                ++appliedThisIter;
            } else {
                failed = true;
                res.diagnostics.push_back({c.id, false, "Constraint application failed due to invalid references or geometry type."});
            }
        }
        res.appliedCount += appliedThisIter;
        res.iterations = iter + 1;
        if (!failed) break;
        if (iter + 1 == iterations) {
            res.status = SolveResult::Status::Infeasible;
            res.overConstrained = true;
        }
    }

    if (res.status != SolveResult::Status::Infeasible) {
        res.status = res.diagnostics.empty() ? SolveResult::Status::Solved
                                             : SolveResult::Status::SolvedWithWarnings;
    }
    m_dirty = false;
    return res;
}
