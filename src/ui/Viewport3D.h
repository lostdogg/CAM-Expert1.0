#pragma once
#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#include <windows.h>
#include "../cad/Geometry.h"
#include "../cad/WireframeScene.h"
#include "../cam/Toolpath.h"
#include <vector>
#include <functional>

// Forward declaration so we don't pull in manager headers everywhere
class ToolpathManager;
class SolidsManager;
class SurfacesManager;
// WireframeScene is fully included above via WireframeScene.h

// --------------------------------------------------------------------------
// Viewport3D
//
// The primary 3-D viewing viewport. Renders:
//   • CAD geometry (wireframe / shaded / translucent)
//   • NURBS surfaces (wireframe mesh or shaded)
//   • Toolpath curves (colour-coded by motion type)
//   • Stock simulation result (Z-map mesh)
//   • Machine axes and coordinate triad
//
// Uses an OpenGL pixel format for GPU-accelerated rendering.
// Camera supports orbit (left-drag), pan (middle-drag), and zoom (wheel).
// --------------------------------------------------------------------------

enum class RenderMode { Wireframe, Shaded, Translucent };
enum class ViewPreset  { Isometric, Front, Top, Right, Back, Bottom, Left };

class Viewport3D {
public:
    Viewport3D(HWND parent, HINSTANCE hInstance);
    ~Viewport3D();

    void resize(int x, int y, int width, int height);
    void reset();   // reset camera to isometric view
    void redraw();

    void setRenderMode(RenderMode mode);
    void setView(ViewPreset preset);
    void zoomSelected();             // F2 – zoom to fit any selected / visible geometry
    void toggleGrid();               // F4 – show/hide the ground grid
    void toggleGnomon();             // F5 – show/hide the dynamic gnomon (axes triad)

    // Provide a toolpath manager so the viewport can render toolpaths.
    // The pointer is non-owning; caller is responsible for lifetime.
    void setToolpathManager(const ToolpathManager* mgr);

    // Provide a solids manager so the viewport can render BRep solid bodies.
    // The pointer is non-owning; caller is responsible for lifetime.
    void setSolidsManager(const SolidsManager* mgr);

    // Provide a surfaces manager so the viewport can render NURBS surfaces.
    // The pointer is non-owning; caller is responsible for lifetime.
    void setSurfacesManager(const SurfacesManager* mgr);

    // Provide the wireframe scene so the viewport can render all WfEntity
    // objects (lines, arcs, circles, splines, polygons, etc.) and show
    // AutoCursor snap highlights on hover.
    // The pointer is non-owning; caller is responsible for lifetime.
    void setWireframeScene(const WireframeScene* scene);

    RenderMode renderMode() const { return m_renderMode; }
    bool       gridVisible()   const { return m_showGrid; }
    bool       gnomonVisible() const { return m_showGnomon; }
    HWND hwnd() const { return m_hwnd; }

    // Callback fired on WM_MOUSEMOVE with the cursor's estimated world-space
    // position projected onto the Z=0 construction plane.
    using CoordCallback = std::function<void(double x, double y, double z)>;
    void setCoordCallback(CoordCallback cb) { m_coordCb = std::move(cb); }

    // Callback fired when the user right-clicks without dragging, passing
    // screen coordinates so the caller can show a context menu.
    using ContextMenuCallback = std::function<void(int screenX, int screenY)>;
    void setContextMenuCallback(ContextMenuCallback cb) { m_contextMenuCb = std::move(cb); }

private:
    static LRESULT CALLBACK ViewportProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool initOpenGL();
    void cleanupOpenGL();
    void render();
    void drawBackground();
    void drawGrid();
    void drawAxes();
    void drawStock();
    void drawSolids();     // render BRep solid bodies from SolidsManager
    void drawSurfaces();   // render NURBS surfaces from SurfacesManager
    void drawToolpaths();
    void drawWireframe();  // render all wireframe entities from WireframeScene

    // Returns an RGB colour triple for a given motion type
    static void motionColor(MotionType mt, float& r, float& g, float& b);

    // Camera state
    struct Camera {
        float orbitX    =  30.0f;  // degrees
        float orbitY    = -45.0f;
        float distance  = 300.0f;
        float panX      =   0.0f;
        float panY      =   0.0f;
    } m_camera;

    // Mouse interaction
    bool  m_leftDown  = false;
    bool  m_midDown   = false;
    bool  m_rightDown = false;
    int   m_lastMouseX = 0, m_lastMouseY = 0;
    int   m_rightClickStartX = 0, m_rightClickStartY = 0;  // for context-menu drag detection

    // Spin inertia (applied via WM_TIMER after a fast left-drag or wheel rotation)
    float       m_spinVelX      = 0.0f;
    float       m_spinVelY      = 0.0f;
    UINT_PTR    m_inertiaTimer  = 0;
    static constexpr UINT_PTR kInertiaTimerId = 1;
    static constexpr float    kInertiaDamping = 0.88f;  // velocity multiplied each tick
    static constexpr float    kInertiaStop    = 0.05f;  // velocity below this stops the timer

    HWND                   m_hwnd        = nullptr;
    HDC                    m_hDC         = nullptr;
    HGLRC                  m_hGLRC       = nullptr;
    RenderMode             m_renderMode  = RenderMode::Shaded;
    bool                   m_showGrid    = true;   // toggled by F4
    bool                   m_showGnomon  = true;   // toggled by F5
    const ToolpathManager* m_toolpathMgr = nullptr;  // non-owning
    const SolidsManager*   m_solidsMgr   = nullptr;  // non-owning
    const SurfacesManager* m_surfacesMgr = nullptr;  // non-owning
    const WireframeScene*  m_wfScene     = nullptr;  // non-owning

    CoordCallback       m_coordCb;        // cursor world-space position callback
    ContextMenuCallback m_contextMenuCb;  // right-click context menu callback

    static constexpr const wchar_t* CLASS_NAME = L"CAMExpertViewport";
};

#endif // VIEWPORT3D_H
