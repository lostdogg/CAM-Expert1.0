#pragma once
#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

#include "WireframeScene.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// Constraint solver for parametric sketch-driven wireframe modeling.
//
// Scope:
//  - Point/line geometric constraints (coincident, horizontal, vertical, ...)
//  - Edit lifecycle hooks (beginEdit/endEdit/queueRebuild)
//  - Solve diagnostics for under/over-constrained behavior
// --------------------------------------------------------------------------

enum class SketchConstraintType {
    Coincident,
    Horizontal,
    Vertical,
    Parallel,
    Perpendicular,
    EqualLength,
    Distance,
    Angle,
    Radius,
    FixPoint
};

// Selector into an entity:
//  - Point entity: selector ignored (uses p0)
//  - Line entity: selector 0 => p0, selector 1 => p1
//  - Arc/Circle entity: selector ignored (uses p0 center) for point constraints
//  - Polygon/Rectangle/Spline: selector indexes pts[]
struct SketchRef {
    int entityIndex = -1;
    int selector    = 0;
};

struct SketchConstraint {
    int                  id      = 0;
    SketchConstraintType type    = SketchConstraintType::Coincident;
    SketchRef            a{};
    SketchRef            b{};
    double               value   = 0.0; // distance(mm), angle(deg), radius(mm)
    bool                 enabled = true;
};

struct SolveDiagnostic {
    int         constraintId = 0;
    bool        warning      = false;
    std::string message;
};

struct SolveResult {
    enum class Status {
        Solved,
        SolvedWithWarnings,
        Infeasible,
        InvalidInput,
        DeferredRebuild
    };

    Status                      status         = Status::Solved;
    int                         iterations     = 0;
    int                         appliedCount   = 0;
    bool                        underConstrained = false;
    bool                        overConstrained  = false;
    std::vector<SolveDiagnostic> diagnostics;
};

class ConstraintSolver {
public:
    ConstraintSolver() = default;

    // Constraint set management
    int  addConstraint(SketchConstraint c);
    bool removeConstraint(int id);
    void clearConstraints();
    const std::vector<SketchConstraint>& constraints() const { return m_constraints; }

    // Edit lifecycle
    void beginEdit() { m_inEdit = true; }
    void endEdit()   { m_inEdit = false; m_dirty = true; }
    void queueRebuild() { m_dirty = true; }
    bool needsRebuild() const { return m_dirty; }

    // Solve constraints in-place against sketch entities.
    SolveResult solve(std::vector<WfEntity>& entities, int maxIterations = 24);

private:
    static bool getRefPoint(const std::vector<WfEntity>& entities,
                            const SketchRef& r,
                            Geom::Vec3& out);
    static bool setRefPoint(std::vector<WfEntity>& entities,
                            const SketchRef& r,
                            const Geom::Vec3& p);

    static bool getLineEndpoints(const std::vector<WfEntity>& entities,
                                 int idx,
                                 Geom::Vec3& p0,
                                 Geom::Vec3& p1);
    static bool setLineEndpoints(std::vector<WfEntity>& entities,
                                 int idx,
                                 const Geom::Vec3& p0,
                                 const Geom::Vec3& p1);

    bool applyConstraint(const SketchConstraint& c,
                         std::vector<WfEntity>& entities,
                         SolveResult& result) const;

    std::vector<SketchConstraint> m_constraints;
    int                           m_nextId = 1;
    bool                          m_inEdit = false;
    bool                          m_dirty  = true;
};

#endif // CONSTRAINT_SOLVER_H

