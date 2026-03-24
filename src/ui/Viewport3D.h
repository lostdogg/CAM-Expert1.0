#pragma once
#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#include <windows.h>
#include "../cad/Geometry.h"
#include "../cam/Toolpath.h"
#include <vector>

// Forward declaration so we don't pull in ToolpathManager.h everywhere
class ToolpathManager;

// --------------------------------------------------------------------------
// Viewport3D
//
// The primary 3-D viewing viewport. Renders:
//   • CAD geometry (wireframe / shaded / translucent)
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

    // Provide a toolpath manager so the viewport can render toolpaths.
    // The pointer is non-owning; caller is responsible for lifetime.
    void setToolpathManager(const ToolpathManager* mgr);

    RenderMode renderMode() const { return m_renderMode; }
    HWND hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK ViewportProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool initOpenGL();
    void cleanupOpenGL();
    void render();
    void drawGrid();
    void drawAxes();
    void drawStock();
    void drawToolpaths();

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

    HWND                  m_hwnd      = nullptr;
    HDC                   m_hDC       = nullptr;
    HGLRC                 m_hGLRC     = nullptr;
    RenderMode            m_renderMode= RenderMode::Shaded;
    const ToolpathManager* m_toolpathMgr = nullptr;  // non-owning

    static constexpr const wchar_t* CLASS_NAME = L"CAMExpertViewport";
};

#endif // VIEWPORT3D_H
