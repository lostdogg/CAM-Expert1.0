#pragma once
#ifndef SOLIDS_MANAGER_H
#define SOLIDS_MANAGER_H

#include "../cad/BRep.h"
#include "../cad/Geometry.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// FeatureOpType – discriminator for each operation stored in the history tree.
// --------------------------------------------------------------------------
enum class FeatureOpType {
    // Create group (from wireframe chains)
    Extrude,
    Revolve,
    Sweep,
    Loft,
    Thicken,
    // Primitives group (direct, no wireframe needed)
    Block,
    Cylinder,
    Sphere,
    Cone,
    Torus,
    // Modify group (applied to an existing solid)
    Fillet,
    Chamfer,
    Shell,
    Draft,
    Trim,
    // Boolean group
    BoolUnion,
    BoolSubtract,
    BoolIntersect,
    // Advanced / specialized
    Hole,
    Impression,
    FromSurfaces
};

// --------------------------------------------------------------------------
// BodyOpType – how the new volume relates to the existing solid session.
// --------------------------------------------------------------------------
enum class BodyOpType {
    CreateBody,  // brand-new independent solid body
    AddBoss,     // merge (add material) into the target solid
    CutBody      // subtract (remove material) from the target solid
};

// --------------------------------------------------------------------------
// FeatureOp – one branch in the parametric history tree.
//
// Stores the operation type, its parameters, the body-operation mode, and
// an optional link to the wireframe chain (profile) used as input.  When a
// wireframe entity is edited the solid engine can re-evaluate all FeatureOps
// that reference it (associativity).
// --------------------------------------------------------------------------
struct FeatureOp {
    FeatureOpType opType    = FeatureOpType::Block;
    BodyOpType    bodyOp    = BodyOpType::CreateBody;
    std::string   label;            // human-readable name shown in the tree
    bool          suppressed = false; // suppressed ops are skipped on rebuild

    // Index into WireframeScene::entities() for the driving profile/path
    // (-1 means no wireframe link; op uses param1/2/3 directly).
    int           wfChainIdx = -1;

    // Generic parametric payload.  Interpretation depends on opType:
    //   Extrude/Block:   param1=dx, param2=dy, param3=dz
    //   Revolve/Cylinder: param1=radius, param2=height
    //   Sphere:          param1=radius
    //   Cone:            param1=baseRadius, param2=topRadius, param3=height
    //   Torus:           param1=majorRadius, param2=minorRadius
    //   Fillet/Shell/Draft: param1=primary value (radius/thickness/angle)
    //   Chamfer:         param1=distance1, param2=distance2
    //   Hole:            param1=diameter, param2=depth
    //   Loft:            param1=number of sections
    //   Thicken:         param1=thickness
    double param1 = 0.0;
    double param2 = 0.0;
    double param3 = 0.0;
};

// --------------------------------------------------------------------------
// SolidsManager
//
// Tracks the history tree of 3D solid models built or imported into the
// session. Each solid has a name, visibility flag, and an ordered list of
// modelling operations (the "feature tree") that can be suppressed or
// rolled back to inspect intermediate states.
// --------------------------------------------------------------------------

struct SolidEntry {
    BRep::Solid            solid;
    bool                   visible   = true;
    bool                   selected  = false;
    std::string            layer;      // optional layer name for visibility grouping
    std::vector<FeatureOp> history;    // ordered feature/operation tree
};

class SolidsManager {
public:
    SolidsManager() = default;

    void addSolid(BRep::Solid solid, const std::string& name = "");
    void removeSolid(int index);
    void clear();

    int               count()     const;
    SolidEntry&       at(int i);
    const SolidEntry& at(int i)   const;

    void setVisible(int index, bool visible);
    void setSelected(int index, bool selected);
    void clearSelection();

    // Rename the solid at the given index
    void renameSolid(int index, const std::string& name);

    // Assign the solid to a named layer
    void setLayer(int index, const std::string& layer);

    // Return indices of all solids on the given layer
    std::vector<int> indicesOnLayer(const std::string& layer) const;

    // Return the aggregate bounding box of all visible solids
    Geom::AABB aggregateBoundingBox() const;

    std::vector<int> selectedIndices() const;

    // ------------------------------------------------------------------
    // Feature / history-tree management
    // ------------------------------------------------------------------

    // Append a FeatureOp to the history of an existing solid.
    // Returns the index of the new feature, or -1 on error.
    int addFeature(int solidIndex, const FeatureOp& op);

    // Toggle the suppressed flag on a feature.  Suppressed operations are
    // skipped when the solid is rebuilt from its history.
    void suppressFeature(int solidIndex, int featureIndex, bool suppressed);

    // Replace the parameters / label of an existing feature.
    // Returns true on success.
    bool editFeature(int solidIndex, int featureIndex, const FeatureOp& op);

    // Remove a feature from the history tree entirely.
    void removeFeature(int solidIndex, int featureIndex);

    // Number of features in the history of a solid.
    int featureCount(int solidIndex) const;

    // Accessors for individual features.
    const FeatureOp& getFeature(int solidIndex, int featureIndex) const;
    FeatureOp&       getFeature(int solidIndex, int featureIndex);

    // Wireframe associativity: return the indices of all solids whose history
    // contains at least one FeatureOp that references the given wireframe
    // entity index.
    std::vector<int> solidsDrivenBy(int wfEntityIndex) const;

    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    void notify();
    bool validSolidIndex  (int solidIndex)   const;
    bool validFeatureIndex(int solidIndex, int featureIndex) const;

    std::vector<SolidEntry> m_entries;
    ChangeCallback          m_onChange;
};

#endif // SOLIDS_MANAGER_H
