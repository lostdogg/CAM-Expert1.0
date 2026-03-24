#pragma once
#ifndef TOOLPATH_MANAGER_H
#define TOOLPATH_MANAGER_H

#include "../cam/Toolpath.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// ToolpathManager
//
// The most important tab in the Managers panel. It stores the ordered list
// of all machining operations for the current job. Each operation is a
// Toolpath object that can be independently regenerated, reordered, or
// deleted. The manager also tracks the current stock material state between
// operations so that subsequent operations can take the "remaining stock"
// into account.
// --------------------------------------------------------------------------

class ToolpathManager {
public:
    ToolpathManager() = default;

    // --- Operation list management ---
    void addToolpath(Toolpath tp);
    void insertToolpath(int index, Toolpath tp);
    void removeToolpath(int index);
    void moveUp(int index);
    void moveDown(int index);
    void clear();

    // --- Accessors ---
    int                           count()         const;
    const Toolpath&               at(int index)   const;
    Toolpath&                     at(int index);
    const std::vector<Toolpath>&  toolpaths()     const { return m_toolpaths; }
    std::vector<Toolpath>&        toolpaths()           { return m_toolpaths; }

    // --- Selection ---
    void   select(int index);
    void   deselect();
    int    selectedIndex() const { return m_selectedIndex; }
    bool   hasSelection()  const { return m_selectedIndex >= 0; }

    // --- Regeneration ---
    // Regenerate a single (dirty) operation
    void regenerate(int index);
    // Regenerate all dirty operations
    void regenerateAll();

    // --- Statistics ---
    double totalMachiningTime()  const;  // seconds
    double totalPathLength()     const;  // mm

    // --- Change notification ---
    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

    // --- Stock simulation ---
    // Mark the stock as "updated through" a given operation index
    void setStockValidThrough(int idx) { m_stockValidThrough = idx; }
    int  stockValidThrough() const     { return m_stockValidThrough; }

private:
    void notifyChange();

    std::vector<Toolpath> m_toolpaths;
    int                   m_selectedIndex    = -1;
    int                   m_stockValidThrough= -1;
    ChangeCallback        m_onChange;
};

#endif // TOOLPATH_MANAGER_H
