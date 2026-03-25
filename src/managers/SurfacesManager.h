#pragma once
#ifndef SURFACES_MANAGER_H
#define SURFACES_MANAGER_H

#include "../cad/NurbsSurface.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// SurfacesManager
//
// Tracks all NURBS surfaces created or imported into the current session.
// Each surface entry holds the surface object, a display name, and visibility
// state.  The manager is accessed by both the Viewport3D (read-only render)
// and the Surface-tab commands in MainWindow (read-write creation/deletion).
//
// A surface can be:
//   • Procedurally created (loft, revolve, offset, fillet, extend)
//   • Imported from a CAD file that carries explicit NURBS data
//   • Derived from a B-Rep solid via the Model Prep engine
// --------------------------------------------------------------------------

struct SurfaceEntry {
    NurbsSurface surface;
    std::string  name;
    bool         visible  = true;
    bool         selected = false;
    bool         trimmed  = false; // true when the surface has been trimmed
};

class SurfacesManager {
public:
    SurfacesManager() = default;

    // --- Population ---
    // Add a surface; returns its index.
    int addSurface(NurbsSurface surf, const std::string& name = "Surface");

    // Remove a surface by index.
    void removeSurface(int index);

    // Remove all surfaces.
    void clear();

    // --- Accessors ---
    int                count() const;
    SurfaceEntry&       at(int index);
    const SurfaceEntry& at(int index) const;
    const std::vector<SurfaceEntry>& surfaces() const { return m_entries; }

    // --- Selection ---
    void select(int index);
    void deselect();
    int  selectedIndex() const { return m_selectedIndex; }
    bool hasSelection()  const { return m_selectedIndex >= 0; }

    // Return a pointer to the currently selected surface, or nullptr.
    SurfaceEntry*       activeSurface();
    const SurfaceEntry* activeSurface() const;

    // --- Visibility ---
    void setVisible(int index, bool visible);

    // --- Trim state ---
    void setTrimmed(int index, bool trimmed);

    // --- Change notification ---
    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

    // --- Factory helpers -------------------------------------------------
    // Build a ruled loft surface (degree 1 in v-direction) through N
    // cross-section profiles, each supplied as a set of 3-D points.
    // Returns a bicubic NURBS that approximates the loft.
    static NurbsSurface makeLoft(
        const std::vector<std::vector<Geom::Vec3>>& sections);

    // Build a NURBS surface of revolution for a polyline profile
    // rotated `angleDeg` degrees around the Z-axis.
    static NurbsSurface makeRevolve(
        const std::vector<Geom::Vec3>& profile,
        double angleDeg = 360.0);

    // Offset every control point along the evaluation normal by `distMM`.
    static NurbsSurface makeOffset(const NurbsSurface& src, double distMM);

    // Extend the surface in the u-direction by `extensionMM` on one end.
    // side=0 → extend at uMin; side=1 → extend at uMax.
    static NurbsSurface makeExtend(const NurbsSurface& src,
                                   double extensionMM,
                                   int side = 1);

    // Build a simple circular-arc fillet blend surface between two
    // flat-parallel offset surfaces (analytical approximation).
    static NurbsSurface makeFillet(const NurbsSurface& srf1,
                                   const NurbsSurface& srf2,
                                   double filletRadius);

private:
    void notify();

    std::vector<SurfaceEntry> m_entries;
    int                       m_selectedIndex = -1;
    ChangeCallback            m_onChange;
};

#endif // SURFACES_MANAGER_H
