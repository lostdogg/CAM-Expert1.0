#pragma once
#ifndef SOLIDS_MANAGER_H
#define SOLIDS_MANAGER_H

#include "../cad/BRep.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// SolidsManager
//
// Tracks the history tree of 3D solid models built or imported into the
// session. Each solid has a name, visibility flag, and an ordered list of
// modelling operations (the "feature tree") that can be suppressed or
// rolled back to inspect intermediate states.
// --------------------------------------------------------------------------

struct SolidEntry {
    BRep::Solid solid;
    bool        visible   = true;
    bool        selected  = false;
    std::string layer;  // optional layer name for visibility grouping
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

    std::vector<int> selectedIndices() const;

    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    void notify();
    std::vector<SolidEntry> m_entries;
    ChangeCallback          m_onChange;
};

#endif // SOLIDS_MANAGER_H
