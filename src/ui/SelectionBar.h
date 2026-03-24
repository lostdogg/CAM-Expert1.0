#pragma once
#ifndef SELECTION_BAR_H
#define SELECTION_BAR_H

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

// --------------------------------------------------------------------------
// SelectionBar
//
// A narrow toolbar (typically on the right edge of the managers panel) that
// provides Quick Masks for filtering geometry selections:
//   • Select all points / lines / arcs / splines / surfaces / solids
//   • Select by type (e.g. "holes only", "planar faces only")
//   • Select by diameter / size filter
//
// Each mask button sends a command that the application processes to filter
// the current selection set.
// --------------------------------------------------------------------------

enum class SelectMask {
    All, Points, Lines, Arcs, Splines,
    Surfaces, Solids, Holes, PlanarFaces, None
};

class SelectionBar {
public:
    SelectionBar(HWND parent, HINSTANCE hInstance);
    ~SelectionBar();

    void resize(int x, int y, int width, int height);

    using MaskCallback = std::function<void(SelectMask)>;
    void setMaskCallback(MaskCallback cb) { m_callback = std::move(cb); }

    SelectMask currentMask() const { return m_currentMask; }

    HWND hwnd() const { return m_hwnd; }

private:
    HWND       m_hwnd        = nullptr;
    SelectMask m_currentMask = SelectMask::All;
    MaskCallback m_callback;
};

#endif // SELECTION_BAR_H
