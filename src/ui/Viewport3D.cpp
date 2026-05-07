#include "Viewport3D.h"
#include "../managers/ToolpathManager.h"
#include "../managers/SolidsManager.h"
#include "../managers/SurfacesManager.h"
#include "../cad/BRep.h"
#include "../cad/WireframeScene.h"
#include <gl/gl.h>
#include <gl/glu.h>
#include <algorithm>
#include <cmath>

// --------------------------------------------------------------------------
Viewport3D::Viewport3D(HWND parent, HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = ViewportProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, CLASS_NAME, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        parent, nullptr, hInstance, this);

    initOpenGL();
}

// --------------------------------------------------------------------------
Viewport3D::~Viewport3D() {
    if (m_inertiaTimer) {
        KillTimer(m_hwnd, kInertiaTimerId);
        m_inertiaTimer = 0;
    }
    cleanupOpenGL();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// --------------------------------------------------------------------------
void Viewport3D::setToolpathManager(const ToolpathManager* mgr) {
    m_toolpathMgr = mgr;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setSolidsManager(const SolidsManager* mgr) {
    m_solidsMgr = mgr;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setSurfacesManager(const SurfacesManager* mgr) {
    m_surfacesMgr = mgr;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setWireframeScene(WireframeScene* scene) {
    m_wfScene = scene;
    redraw();
}

// --------------------------------------------------------------------------
bool Viewport3D::initOpenGL() {
    m_hDC = GetDC(m_hwnd);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(m_hDC, &pfd);
    if (!fmt) return false;
    SetPixelFormat(m_hDC, fmt, &pfd);

    m_hGLRC = wglCreateContext(m_hDC);
    if (!m_hGLRC) return false;
    wglMakeCurrent(m_hDC, m_hGLRC);

    // Basic OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);   // secondary fill light for world-class shading
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    // Primary key light (upper-right front)
    float ambient[]   = {0.18f, 0.18f, 0.20f, 1.0f};
    float diffuse[]   = {0.85f, 0.85f, 0.85f, 1.0f};
    float specular[]  = {0.50f, 0.50f, 0.50f, 1.0f};
    float lightPos[]  = {100.0f, 200.0f, 300.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Secondary fill light (lower-left back, cooler, dimmer)
    float fillAmbient[]  = {0.0f,  0.0f,  0.0f,  1.0f};
    float fillDiffuse[]  = {0.28f, 0.30f, 0.38f, 1.0f}; // slightly blue-tinted
    float fillSpecular[] = {0.0f,  0.0f,  0.0f,  1.0f};
    float fillPos[]      = {-150.0f, -100.0f, -80.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT,  fillAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  fillDiffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fillSpecular);
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);

    glClearColor(0.08f, 0.11f, 0.19f, 1.0f); // fallback clear under gradient

    return true;
}

// --------------------------------------------------------------------------
void Viewport3D::cleanupOpenGL() {
    if (m_hGLRC) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hGLRC);
        m_hGLRC = nullptr;
    }
    if (m_hDC && m_hwnd) {
        ReleaseDC(m_hwnd, m_hDC);
        m_hDC = nullptr;
    }
}

// --------------------------------------------------------------------------
void Viewport3D::resize(int x, int y, int width, int height) {
    SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
    if (m_hGLRC && width > 0 && height > 0) {
        wglMakeCurrent(m_hDC, m_hGLRC);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, static_cast<double>(width) / height, 0.1, 10000.0);
        glMatrixMode(GL_MODELVIEW);
    }
}

// --------------------------------------------------------------------------
void Viewport3D::reset() {
    m_camera = Camera{};
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setRenderMode(RenderMode mode) {
    m_renderMode = mode;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setView(ViewPreset preset) {
    switch (preset) {
    case ViewPreset::Isometric: m_camera.orbitX =  30; m_camera.orbitY = -45; break;
    case ViewPreset::Top:       m_camera.orbitX =  90; m_camera.orbitY =   0; break;
    case ViewPreset::Front:     m_camera.orbitX =   0; m_camera.orbitY =   0; break;
    case ViewPreset::Right:     m_camera.orbitX =   0; m_camera.orbitY = -90; break;
    case ViewPreset::Back:      m_camera.orbitX =   0; m_camera.orbitY = 180; break;
    case ViewPreset::Bottom:    m_camera.orbitX = -90; m_camera.orbitY =   0; break;
    case ViewPreset::Left:      m_camera.orbitX =   0; m_camera.orbitY =  90; break;
    }
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::toggleGrid() {
    m_showGrid = !m_showGrid;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::toggleGnomon() {
    m_showGnomon = !m_showGnomon;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::setSelectionFilter(SelectionFilter filter) {
    m_selectionFilter = filter;
}

// --------------------------------------------------------------------------
void Viewport3D::setSelectionMode(bool selectMode) {
    m_selectMode = selectMode;
}

// --------------------------------------------------------------------------
void Viewport3D::zoomSelected() {
    // No selection system yet; zoom to a tight fit of the visible scene
    m_camera.distance = 180.0f;
    m_camera.panX     = 0.0f;
    m_camera.panY     = 0.0f;
    redraw();
}

// --------------------------------------------------------------------------
void Viewport3D::redraw() {
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

// --------------------------------------------------------------------------
// Map a MotionType to an RGB display colour
void Viewport3D::motionColor(MotionType mt, float& r, float& g, float& b) {
    switch (mt) {
    case MotionType::Rapid:
    case MotionType::MicroLift:
        r = 1.0f; g = 1.0f; b = 0.0f; break;  // yellow – rapid traverse
    case MotionType::PlungeFeed:
        r = 1.0f; g = 0.3f; b = 0.3f; break;  // red – plunge
    case MotionType::Retract:
        r = 0.3f; g = 0.5f; b = 1.0f; break;  // blue – retract
    case MotionType::ArcCW:
    case MotionType::ArcCCW:
        r = 0.8f; g = 0.4f; b = 1.0f; break;  // purple – arc motion
    case MotionType::Dwell:
        r = 1.0f; g = 0.5f; b = 0.0f; break;  // orange – dwell
    default:
        r = 0.0f; g = 1.0f; b = 0.0f; break;  // green – linear feed
    }
}

// --------------------------------------------------------------------------
void Viewport3D::render() {
    if (!m_hGLRC) return;
    wglMakeCurrent(m_hDC, m_hGLRC);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackground();
    glLoadIdentity();

    // Camera transform
    glTranslatef(-m_camera.panX, -m_camera.panY, -m_camera.distance);
    glRotatef(m_camera.orbitX, 1, 0, 0);
    glRotatef(m_camera.orbitY, 0, 0, 1);

    // Render mode state
    switch (m_renderMode) {
    case RenderMode::Wireframe:
        glDisable(GL_LIGHTING);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        break;
    case RenderMode::Translucent:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_LIGHTING);
        break;
    default: // Shaded
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_LIGHTING);
        break;
    }

    if (m_showGrid)   drawGrid();
    if (m_showGnomon) drawAxes();
    drawStock();
    drawSolids();
    drawSurfaces();
    drawWireframe();
    drawToolpaths();
    if (m_dragSelecting) drawSelectionWindowOverlay();

    SwapBuffers(m_hDC);
}

// --------------------------------------------------------------------------
void Viewport3D::drawBackground() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBegin(GL_QUADS);
    glColor3f(0.08f, 0.11f, 0.19f);
    glVertex2f(-1.0f,  1.0f);
    glVertex2f( 1.0f,  1.0f);
    glColor3f(0.20f, 0.28f, 0.42f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f(-1.0f, -1.0f);
    glEnd();

    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// --------------------------------------------------------------------------
// Viewport visualization constants
static constexpr int   kGridHighlightInterval = 5;     // every Nth grid line is brighter
static constexpr float kMinAxisLength         = 0.5f;  // minimum length to treat toolAxis as valid
static constexpr double kToolAxisTickLength   = 8.0;   // mm – length of tool-axis tick marks
static constexpr int   kAxisTickInterval      = 10;    // draw a tick every N toolpath points

// Right-click interaction constants
static constexpr int    kContextMenuThreshold = 5;     // px radius below which RMB is a click, not a pan
static constexpr double kRayPlaneEpsilon      = 1e-10; // near-zero threshold for ray/Z=0 plane check

// --------------------------------------------------------------------------
void Viewport3D::drawGrid() {
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -20; i <= 20; ++i) {
        float fi = static_cast<float>(i) * 10.0f;
        // Highlight every kGridHighlightInterval-th line slightly brighter
        if (i % kGridHighlightInterval == 0)
            glColor3f(0.28f, 0.30f, 0.40f);
        else
            glColor3f(0.18f, 0.20f, 0.28f);
        glVertex3f(fi, -200, 0); glVertex3f(fi, 200, 0);
        glVertex3f(-200, fi, 0); glVertex3f(200, fi, 0);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
void Viewport3D::drawAxes() {
    glDisable(GL_LIGHTING);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    // X – red
    glColor3f(1.0f, 0.18f, 0.18f); glVertex3f(0,0,0); glVertex3f(40,0,0);
    // Y – green
    glColor3f(0.10f, 0.95f, 0.22f); glVertex3f(0,0,0); glVertex3f(0,40,0);
    // Z – blue
    glColor3f(0.18f, 0.45f, 1.0f); glVertex3f(0,0,0); glVertex3f(0,0,40);
    glEnd();

    constexpr float axisTip = 40.0f;
    constexpr float headBase = 34.0f;
    constexpr float headHalf = 1.8f;
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.18f, 0.18f);
    glVertex3f(axisTip, 0.0f, 0.0f); glVertex3f(headBase,  headHalf,  headHalf); glVertex3f(headBase, -headHalf,  headHalf);
    glVertex3f(axisTip, 0.0f, 0.0f); glVertex3f(headBase, -headHalf,  headHalf); glVertex3f(headBase, -headHalf, -headHalf);
    glVertex3f(axisTip, 0.0f, 0.0f); glVertex3f(headBase, -headHalf, -headHalf); glVertex3f(headBase,  headHalf, -headHalf);
    glVertex3f(axisTip, 0.0f, 0.0f); glVertex3f(headBase,  headHalf, -headHalf); glVertex3f(headBase,  headHalf,  headHalf);

    glColor3f(0.10f, 0.95f, 0.22f);
    glVertex3f(0.0f, axisTip, 0.0f); glVertex3f( headHalf, headBase,  headHalf); glVertex3f( headHalf, headBase, -headHalf);
    glVertex3f(0.0f, axisTip, 0.0f); glVertex3f( headHalf, headBase, -headHalf); glVertex3f(-headHalf, headBase, -headHalf);
    glVertex3f(0.0f, axisTip, 0.0f); glVertex3f(-headHalf, headBase, -headHalf); glVertex3f(-headHalf, headBase,  headHalf);
    glVertex3f(0.0f, axisTip, 0.0f); glVertex3f(-headHalf, headBase,  headHalf); glVertex3f( headHalf, headBase,  headHalf);

    glColor3f(0.18f, 0.45f, 1.0f);
    glVertex3f(0.0f, 0.0f, axisTip); glVertex3f( headHalf,  headHalf, headBase); glVertex3f(-headHalf,  headHalf, headBase);
    glVertex3f(0.0f, 0.0f, axisTip); glVertex3f(-headHalf,  headHalf, headBase); glVertex3f(-headHalf, -headHalf, headBase);
    glVertex3f(0.0f, 0.0f, axisTip); glVertex3f(-headHalf, -headHalf, headBase); glVertex3f( headHalf, -headHalf, headBase);
    glVertex3f(0.0f, 0.0f, axisTip); glVertex3f( headHalf, -headHalf, headBase); glVertex3f( headHalf,  headHalf, headBase);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
// Draw a semi-transparent stock billet (100×100×50 mm box centred at origin)
void Viewport3D::drawStock() {
    if (m_renderMode == RenderMode::Wireframe) {
        glDisable(GL_LIGHTING);
        glColor3f(0.60f, 0.63f, 0.68f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.60f, 0.63f, 0.68f,
                  m_renderMode == RenderMode::Translucent ? 0.35f : 0.80f);
        glEnable(GL_LIGHTING);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glBegin(GL_QUADS);
    // Bottom face (Z = -50)
    glNormal3f(0, 0, -1);
    glVertex3f(-50,-50,-50); glVertex3f( 50,-50,-50);
    glVertex3f( 50, 50,-50); glVertex3f(-50, 50,-50);
    // Top face (Z = 0)
    glNormal3f(0, 0, 1);
    glVertex3f(-50,-50, 0); glVertex3f(-50, 50, 0);
    glVertex3f( 50, 50, 0); glVertex3f( 50,-50, 0);
    // Front face (Y = -50)
    glNormal3f(0, -1, 0);
    glVertex3f(-50,-50,-50); glVertex3f(-50,-50, 0);
    glVertex3f( 50,-50, 0);  glVertex3f( 50,-50,-50);
    // Back face (Y = 50)
    glNormal3f(0, 1, 0);
    glVertex3f(-50, 50,-50); glVertex3f( 50, 50,-50);
    glVertex3f( 50, 50, 0);  glVertex3f(-50, 50, 0);
    // Left face (X = -50)
    glNormal3f(-1, 0, 0);
    glVertex3f(-50,-50,-50); glVertex3f(-50, 50,-50);
    glVertex3f(-50, 50, 0);  glVertex3f(-50,-50, 0);
    // Right face (X = 50)
    glNormal3f(1, 0, 0);
    glVertex3f(50,-50,-50); glVertex3f( 50,-50, 0);
    glVertex3f(50, 50, 0);  glVertex3f( 50, 50,-50);
    glEnd();

    if (m_renderMode != RenderMode::Wireframe) {
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_LIGHTING);
    }
}

// --------------------------------------------------------------------------
// Render BRep solid bodies from SolidsManager (shaded faces + wireframe edges)
// --------------------------------------------------------------------------
void Viewport3D::drawSolids() {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) return;

    for (int i = 0; i < m_solidsMgr->count(); ++i) {
        const SolidEntry& entry = m_solidsMgr->at(i);
        if (!entry.visible) continue;

        const BRep::Solid&            solid  = entry.solid;
        const std::vector<BRep::Vertex>& verts = solid.vertices();
        const std::vector<BRep::Edge>&   edges = solid.edges();
        const std::vector<BRep::Face>&   faces = solid.faces();

        if (verts.empty()) continue;

        // --- Shaded face pass (skip in wireframe mode) ---
        if (m_renderMode != RenderMode::Wireframe) {
            glEnable(GL_LIGHTING);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            if (m_renderMode == RenderMode::Translucent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            // Colour: gold for selected, steel-blue for normal
            if (entry.selected)
                glColor4f(0.85f, 0.65f, 0.15f,
                          m_renderMode == RenderMode::Translucent ? 0.50f : 0.90f);
            else
                glColor4f(0.52f, 0.64f, 0.80f,
                          m_renderMode == RenderMode::Translucent ? 0.40f : 0.85f);

            for (const auto& face : faces) {
                // Render Planar and Spherical faces via edge-vertex polygon.
                // Cylindrical/Conical/Toroidal faces have interleaved multi-quad
                // edge lists and are shown only as wireframe edges below.
                if (face.type == BRep::FaceType::Cylindrical ||
                    face.type == BRep::FaceType::Conical     ||
                    face.type == BRep::FaceType::Toroidal    ||
                    face.type == BRep::FaceType::NURBS)
                    continue;
                if (face.edgeIds.empty()) continue;

                glNormal3d(face.normal.x, face.normal.y, face.normal.z);
                glBegin(GL_POLYGON);
                for (int eid : face.edgeIds) {
                    if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
                    int vid = edges[eid].startVertexId;
                    if (vid < 0 || vid >= static_cast<int>(verts.size())) continue;
                    const Geom::Vec3& pos = verts[vid].point;
                    glVertex3d(pos.x, pos.y, pos.z);
                }
                glEnd();
            }

            if (m_renderMode == RenderMode::Translucent)
                glDisable(GL_BLEND);
        }

        // --- Wireframe edge pass (always drawn) ---
        glDisable(GL_LIGHTING);
        glLineWidth(1.2f);
        if (m_renderMode == RenderMode::Wireframe)
            glColor3f(0.70f, 0.75f, 0.92f);  // bright in wireframe mode
        else if (entry.selected)
            glColor3f(0.95f, 0.75f, 0.10f);  // gold silhouette when selected
        else
            glColor3f(0.35f, 0.40f, 0.58f);  // dark edge overlay in shaded mode

        glBegin(GL_LINES);
        for (const auto& edge : edges) {
            int sv = edge.startVertexId, ev = edge.endVertexId;
            if (sv < 0 || sv >= static_cast<int>(verts.size())) continue;
            if (ev < 0 || ev >= static_cast<int>(verts.size())) continue;
            const Geom::Vec3& p0 = verts[sv].point;
            const Geom::Vec3& p1 = verts[ev].point;
            glVertex3d(p0.x, p0.y, p0.z);
            glVertex3d(p1.x, p1.y, p1.z);
        }
        glEnd();

        glLineWidth(1.0f);
        glEnable(GL_LIGHTING);
    }
}

// --------------------------------------------------------------------------
// Draw all NURBS surfaces as tessellated triangle meshes.
// In Wireframe mode each surface is drawn as a 20×20 parameter-space grid.
// In Shaded / Translucent mode triangles are filled using the surface normal
// for per-face lighting, with a distinct teal/cyan colour to distinguish
// surfaces from solid bodies.
void Viewport3D::drawSurfaces() {
    if (!m_surfacesMgr || m_surfacesMgr->count() == 0) return;

    const int RES = 20; // tessellation resolution (RES × RES quads)

    for (int si = 0; si < m_surfacesMgr->count(); ++si) {
        const SurfaceEntry& entry = m_surfacesMgr->at(si);
        if (!entry.visible) continue;

        const NurbsSurface& surf = entry.surface;

        double uMin = surf.uMin(), uMax = surf.uMax();
        double vMin = surf.vMin(), vMax = surf.vMax();

        if (m_renderMode == RenderMode::Wireframe) {
            // U-isolines
            glDisable(GL_LIGHTING);
            glColor3f(0.0f, 0.7f, 0.8f);  // cyan-teal
            glLineWidth(1.2f);
            for (int i = 0; i <= RES; ++i) {
                double u = uMin + (uMax - uMin) * i / RES;
                glBegin(GL_LINE_STRIP);
                for (int j = 0; j <= RES; ++j) {
                    double v = vMin + (vMax - vMin) * j / RES;
                    Geom::Vec3 p = surf.evaluate(u, v);
                    glVertex3d(p.x, p.y, p.z);
                }
                glEnd();
            }
            // V-isolines
            for (int j = 0; j <= RES; ++j) {
                double v = vMin + (vMax - vMin) * j / RES;
                glBegin(GL_LINE_STRIP);
                for (int i = 0; i <= RES; ++i) {
                    double u = uMin + (uMax - uMin) * i / RES;
                    Geom::Vec3 p = surf.evaluate(u, v);
                    glVertex3d(p.x, p.y, p.z);
                }
                glEnd();
            }
        } else {
            // Shaded / Translucent – tessellate into GL_TRIANGLES
            if (m_renderMode == RenderMode::Translucent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(0.0f, 0.75f, 0.85f, 0.45f);
            } else {
                glColor3f(0.0f, 0.75f, 0.85f);
            }
            glEnable(GL_LIGHTING);

            // Teal-ish material for surfaces
            GLfloat diff[]   = { 0.0f, 0.70f, 0.80f, m_renderMode == RenderMode::Translucent ? 0.45f : 1.0f };
            GLfloat spec[]   = { 0.5f, 0.9f,  1.0f,  1.0f };
            GLfloat shininess = 64.0f;
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, diff);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

            glBegin(GL_TRIANGLES);
            for (int i = 0; i < RES; ++i) {
                double u0 = uMin + (uMax - uMin) *  i      / RES;
                double u1 = uMin + (uMax - uMin) * (i + 1) / RES;
                for (int j = 0; j < RES; ++j) {
                    double v0 = vMin + (vMax - vMin) *  j      / RES;
                    double v1 = vMin + (vMax - vMin) * (j + 1) / RES;

                    Geom::Vec3 p00 = surf.evaluate(u0, v0);
                    Geom::Vec3 p10 = surf.evaluate(u1, v0);
                    Geom::Vec3 p01 = surf.evaluate(u0, v1);
                    Geom::Vec3 p11 = surf.evaluate(u1, v1);

                    Geom::Vec3 n00 = surf.normal(u0, v0);
                    Geom::Vec3 n10 = surf.normal(u1, v0);
                    Geom::Vec3 n01 = surf.normal(u0, v1);
                    Geom::Vec3 n11 = surf.normal(u1, v1);

                    // Triangle 1: p00, p10, p11
                    glNormal3d(n00.x, n00.y, n00.z); glVertex3d(p00.x, p00.y, p00.z);
                    glNormal3d(n10.x, n10.y, n10.z); glVertex3d(p10.x, p10.y, p10.z);
                    glNormal3d(n11.x, n11.y, n11.z); glVertex3d(p11.x, p11.y, p11.z);

                    // Triangle 2: p00, p11, p01
                    glNormal3d(n00.x, n00.y, n00.z); glVertex3d(p00.x, p00.y, p00.z);
                    glNormal3d(n11.x, n11.y, n11.z); glVertex3d(p11.x, p11.y, p11.z);
                    glNormal3d(n01.x, n01.y, n01.z); glVertex3d(p01.x, p01.y, p01.z);
                }
            }
            glEnd();

            if (m_renderMode == RenderMode::Translucent)
                glDisable(GL_BLEND);
        }
    }
}

// --------------------------------------------------------------------------
// Draw all wireframe entities from the WireframeScene.
// Lines are rendered in cyan-green; arcs/circles in yellow-green;
// points as small GL_POINTS; polygons/rectangles as closed loops.
void Viewport3D::drawWireframe() {
    if (!m_wfScene || m_wfScene->entityCount() == 0) return;

    static constexpr int   kArcSegs  = 64;   // tessellation segments per full circle
    static constexpr float kPointSz  = 5.0f;
    static constexpr double kTwoPi   = 6.28318530717959;

    // Selection highlight colour: bright gold
    static constexpr float kSelR = 0.95f, kSelG = 0.80f, kSelB = 0.10f;

    glDisable(GL_LIGHTING);
    glLineWidth(1.5f);

    const auto& entities = m_wfScene->entities();
    for (int entityIdx = 0; entityIdx < static_cast<int>(entities.size()); ++entityIdx) {
        const WfEntity& e = entities[entityIdx];
        bool selected = m_wfScene->isSelected(entityIdx);
        bool hovered  = (entityIdx == m_hoverEntity);

        switch (e.type) {

        case WfEntityType::Point:
            glColor3f(selected ? kSelR : (hovered ? 0.95f : 1.0f),
                      selected ? kSelG : (hovered ? 0.95f : 1.0f),
                      selected ? kSelB : (hovered ? 0.30f : 0.0f));   // gold/hover/yellow
            glPointSize(selected ? kPointSz + 2.0f : (hovered ? kPointSz + 1.0f : kPointSz));
            glBegin(GL_POINTS);
            glVertex3d(e.p0.x, e.p0.y, e.p0.z);
            glEnd();
            break;

        case WfEntityType::Line:
            glColor3f(selected ? kSelR : (hovered ? 0.3f : 0.0f),
                      selected ? kSelG : 1.0f,
                      selected ? kSelB : (hovered ? 1.0f : 0.8f));   // gold/hover/cyan
            glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
            glBegin(GL_LINES);
            glVertex3d(e.p0.x, e.p0.y, e.p0.z);
            glVertex3d(e.p1.x, e.p1.y, e.p1.z);
            glEnd();
            glLineWidth(1.5f);
            break;

        case WfEntityType::Arc: {
            glColor3f(selected ? kSelR : (hovered ? 0.8f : 0.6f),
                      selected ? kSelG : 1.0f,
                      selected ? kSelB : (hovered ? 0.4f : 0.2f));   // gold/hover/lime
            glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
            glBegin(GL_LINE_STRIP);
            double span = e.endAngle - e.startAngle;
            // Ensure we always sweep the short (CCW) way around.
            // When endAngle < startAngle the arc wraps past 0 radians.
            if (span < 0.0) span += kTwoPi;
            // Degenerate arc (start == end after wrap correction) → skip.
            if (span < 1e-9) break;
            for (int i = 0; i <= kArcSegs; ++i) {
                double t = e.startAngle + span * i / kArcSegs;
                double px = e.p0.x + e.radius * std::cos(t);
                double py = e.p0.y + e.radius * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            glLineWidth(1.5f);
            break;
        }

        case WfEntityType::Circle: {
            glColor3f(selected ? kSelR : (hovered ? 0.8f : 0.6f),
                      selected ? kSelG : 1.0f,
                      selected ? kSelB : (hovered ? 0.4f : 0.2f));   // gold/hover/lime
            glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < kArcSegs; ++i) {
                double t  = kTwoPi * i / kArcSegs;
                double px = e.p0.x + e.radius * std::cos(t);
                double py = e.p0.y + e.radius * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            glLineWidth(1.5f);
            break;
        }

        case WfEntityType::Ellipse: {
            glColor3f(selected ? kSelR : (hovered ? 0.9f : 0.8f),
                      selected ? kSelG : (hovered ? 0.8f : 0.6f),
                      selected ? kSelB : 1.0f);   // gold/hover/lavender
            glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < kArcSegs; ++i) {
                double t  = kTwoPi * i / kArcSegs;
                double px = e.p0.x + e.radius  * std::cos(t);
                double py = e.p0.y + e.radius2 * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            glLineWidth(1.5f);
            break;
        }

        case WfEntityType::Spline:
            // Draw line segments between consecutive control points as a
            // polyline approximation of the spline.
            if (e.pts.size() >= 2) {
                glColor3f(selected ? kSelR : 1.0f,
                          selected ? kSelG : (hovered ? 0.85f : 0.7f),
                          selected ? kSelB : (hovered ? 0.2f : 0.0f));   // gold/hover/orange
                glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
                glBegin(GL_LINE_STRIP);
                for (const auto& cp : e.pts)
                    glVertex3d(cp.x, cp.y, cp.z);
                glEnd();
                glLineWidth(1.5f);
                // Draw control points as small dots
                glColor3f(1.0f, 1.0f, 1.0f);
                glPointSize(4.0f);
                glBegin(GL_POINTS);
                for (const auto& cp : e.pts)
                    glVertex3d(cp.x, cp.y, cp.z);
                glEnd();
            }
            break;

        case WfEntityType::Rectangle:
        case WfEntityType::Polygon:
            if (!e.pts.empty()) {
                glColor3f(selected ? kSelR : 0.0f,
                          selected ? kSelG : 1.0f,
                          selected ? kSelB : (hovered ? 1.0f : 0.8f));   // gold/hover/cyan
                glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
                glBegin(GL_LINE_LOOP);
                for (const auto& v : e.pts)
                    glVertex3d(v.x, v.y, v.z);
                glEnd();
                glLineWidth(1.5f);
            }
            break;

        case WfEntityType::Helix: {
            glColor3f(selected ? kSelR : 1.0f,
                      selected ? kSelG : (hovered ? 0.65f : 0.5f),
                      selected ? kSelB : 0.2f);   // gold/hover/coral
            glLineWidth(selected ? 2.5f : (hovered ? 2.2f : 1.5f));
            static constexpr int kHelixSegs = 32;  // segments per revolution
            int totalSegs = static_cast<int>(e.revolutions * kHelixSegs);
            if (totalSegs < 2) totalSegs = 2;
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= totalSegs; ++i) {
                double frac  = static_cast<double>(i) / totalSegs;
                double angle = kTwoPi * e.revolutions * frac;
                double px    = e.p0.x + e.radius * std::cos(angle);
                double py    = e.p0.y + e.radius * std::sin(angle);
                double pz    = e.p0.z + e.height * frac;
                glVertex3d(px, py, pz);
            }
            glEnd();
            glLineWidth(1.5f);
            break;
        }

        }  // switch
    }  // for each entity

    glLineWidth(1.0f);
    glPointSize(1.0f);
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
void Viewport3D::drawSelectionWindowOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    glOrtho(0.0, static_cast<double>(std::max(1L, rc.right - rc.left)),
            static_cast<double>(std::max(1L, rc.bottom - rc.top)), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glColor3f(0.95f, 0.90f, 0.25f);

    glBegin(GL_LINE_LOOP);
    glVertex2i(m_selectStartX, m_selectStartY);
    glVertex2i(m_selectEndX,   m_selectStartY);
    glVertex2i(m_selectEndX,   m_selectEndY);
    glVertex2i(m_selectStartX, m_selectEndY);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// --------------------------------------------------------------------------
bool Viewport3D::entityMatchesFilter(const WfEntity& e) const {
    switch (m_selectionFilter) {
    case SelectionFilter::All:    return true;
    case SelectionFilter::Points: return e.type == WfEntityType::Point;
    case SelectionFilter::Lines:  return e.type == WfEntityType::Line;
    case SelectionFilter::Arcs:   return e.type == WfEntityType::Arc || e.type == WfEntityType::Circle;
    case SelectionFilter::Splines:return e.type == WfEntityType::Spline;
    case SelectionFilter::None:   return false;
    default:                      return true; // unsupported filters default to permissive
    }
}

// --------------------------------------------------------------------------
bool Viewport3D::projectPoint(const Geom::Vec3& p, double& sx, double& sy, double& sz) const {
    if (!m_hGLRC) return false;
    GLint viewport[4] = {};
    GLdouble modelview[16] = {};
    GLdouble projection[16] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    GLdouble px = 0.0, py = 0.0, pz = 0.0;
    if (!gluProject(p.x, p.y, p.z, modelview, projection, viewport, &px, &py, &pz))
        return false;
    sx = px;
    sy = viewport[3] - py;
    sz = pz;
    return true;
}

// --------------------------------------------------------------------------
std::vector<Geom::Vec3> Viewport3D::sampleEntityPoints(const WfEntity& e) const {
    std::vector<Geom::Vec3> pts;
    static constexpr int kArcSegs = 24;
    static constexpr double kTwoPi = 6.28318530717959;
    switch (e.type) {
    case WfEntityType::Point:
        pts.push_back(e.p0);
        break;
    case WfEntityType::Line:
        pts.push_back(e.p0);
        pts.push_back(e.p1);
        break;
    case WfEntityType::Arc: {
        double span = e.endAngle - e.startAngle;
        if (span < 0.0) span += kTwoPi;
        for (int i = 0; i <= kArcSegs; ++i) {
            double t = e.startAngle + span * i / kArcSegs;
            pts.push_back({ e.p0.x + e.radius * std::cos(t),
                            e.p0.y + e.radius * std::sin(t), e.p0.z });
        }
        break;
    }
    case WfEntityType::Circle:
    case WfEntityType::Ellipse:
        for (int i = 0; i <= kArcSegs; ++i) {
            double t = kTwoPi * i / kArcSegs;
            double rx = e.radius;
            double ry = (e.type == WfEntityType::Ellipse) ? e.radius2 : e.radius;
            pts.push_back({ e.p0.x + rx * std::cos(t), e.p0.y + ry * std::sin(t), e.p0.z });
        }
        break;
    case WfEntityType::Spline:
    case WfEntityType::Rectangle:
    case WfEntityType::Polygon:
        pts = e.pts;
        break;
    case WfEntityType::Helix: {
        int segs = std::max(8, static_cast<int>(e.revolutions * 24.0));
        for (int i = 0; i <= segs; ++i) {
            double u = static_cast<double>(i) / segs;
            double a = kTwoPi * e.revolutions * u;
            pts.push_back({ e.p0.x + e.radius * std::cos(a),
                            e.p0.y + e.radius * std::sin(a),
                            e.p0.z + e.height * u });
        }
        break;
    }
    }
    return pts;
}

// --------------------------------------------------------------------------
int Viewport3D::hitTestEntityAt(int x, int y, double tolerancePx) const {
    if (!m_wfScene || m_wfScene->entityCount() == 0 || !m_hGLRC) return -1;
    wglMakeCurrent(m_hDC, m_hGLRC);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-m_camera.panX, -m_camera.panY, -m_camera.distance);
    glRotatef(m_camera.orbitX, 1, 0, 0);
    glRotatef(m_camera.orbitY, 0, 0, 1);

    const auto& entities = m_wfScene->entities();
    const double tolSq = tolerancePx * tolerancePx;
    int bestIdx = -1;
    double best = tolSq;

    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        if (!entityMatchesFilter(entities[i])) continue;
        auto samples = sampleEntityPoints(entities[i]);
        for (const auto& p : samples) {
            double sx = 0.0, sy = 0.0, sz = 0.0;
            if (!projectPoint(p, sx, sy, sz)) continue;
            double dx = sx - x;
            double dy = sy - y;
            double d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

// --------------------------------------------------------------------------
std::vector<int> Viewport3D::collectWindowSelection(int x0, int y0, int x1, int y1, bool crossing) const {
    std::vector<int> out;
    if (!m_wfScene || !m_hGLRC) return out;
    wglMakeCurrent(m_hDC, m_hGLRC);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-m_camera.panX, -m_camera.panY, -m_camera.distance);
    glRotatef(m_camera.orbitX, 1, 0, 0);
    glRotatef(m_camera.orbitY, 0, 0, 1);

    const int left   = std::min(x0, x1);
    const int right  = std::max(x0, x1);
    const int top    = std::min(y0, y1);
    const int bottom = std::max(y0, y1);

    const auto& entities = m_wfScene->entities();
    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        const auto& e = entities[i];
        if (!entityMatchesFilter(e)) continue;
        auto samples = sampleEntityPoints(e);
        if (samples.empty()) continue;
        bool anyInside = false;
        bool allInside = true;
        for (const auto& p : samples) {
            double sx = 0.0, sy = 0.0, sz = 0.0;
            if (!projectPoint(p, sx, sy, sz)) continue;
            bool inside = (sx >= left && sx <= right && sy >= top && sy <= bottom);
            anyInside |= inside;
            allInside &= inside;
        }
        if ((crossing && anyInside) || (!crossing && allInside))
            out.push_back(i);
    }
    return out;
}

// --------------------------------------------------------------------------
// Draw all toolpath moves colour-coded by motion type
void Viewport3D::drawToolpaths() {
    if (!m_toolpathMgr || m_toolpathMgr->count() == 0) return;

    glDisable(GL_LIGHTING);
    glLineWidth(1.5f);

    for (int opIdx = 0; opIdx < m_toolpathMgr->count(); ++opIdx) {
        const auto& tp  = m_toolpathMgr->at(opIdx);
        const auto& pts = tp.points();
        if (pts.size() < 2) continue;

        glBegin(GL_LINE_STRIP);
        for (const auto& pt : pts) {
            float r, g, b;
            motionColor(pt.motion, r, g, b);
            glColor3f(r, g, b);
            glVertex3d(pt.position.x, pt.position.y, pt.position.z);
        }
        glEnd();

        // Draw tool-axis tick marks at every kAxisTickInterval-th point (multi-axis paths)
        bool hasAxisInfo = false;
        for (const auto& pt : pts) {
            if (pt.toolAxis.length() > kMinAxisLength) { hasAxisInfo = true; break; }
        }
        if (hasAxisInfo) {
            glColor3f(0.9f, 0.9f, 0.4f);
            glBegin(GL_LINES);
            for (std::size_t i = 0; i < pts.size(); i += kAxisTickInterval) {
                const auto& pt = pts[i];
                if (pt.toolAxis.length() < kMinAxisLength) continue;
                double tx = pt.position.x, ty = pt.position.y, tz = pt.position.z;
                double ex = tx + pt.toolAxis.x * kToolAxisTickLength;
                double ey = ty + pt.toolAxis.y * kToolAxisTickLength;
                double ez = tz + pt.toolAxis.z * kToolAxisTickLength;
                glVertex3d(tx, ty, tz);
                glVertex3d(ex, ey, ez);
            }
            glEnd();
        }
    }

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
// Static WndProc
LRESULT CALLBACK Viewport3D::ViewportProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam) {
    Viewport3D* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<Viewport3D*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<Viewport3D*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Viewport3D::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
        render();
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        if (m_hGLRC && w > 0 && h > 0) {
            wglMakeCurrent(m_hDC, m_hGLRC);
            glViewport(0, 0, w, h);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(45.0, static_cast<double>(w) / h, 0.1, 10000.0);
            glMatrixMode(GL_MODELVIEW);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        m_leftDown   = true;
        m_selectStartX = m_lastMouseX = LOWORD(lParam);
        m_selectStartY = m_lastMouseY = HIWORD(lParam);
        m_selectEndX   = m_selectStartX;
        m_selectEndY   = m_selectStartY;
        m_dragSelecting = false;
        // Cancel any ongoing inertia spin when user grabs the model
        if (m_inertiaTimer) {
            KillTimer(m_hwnd, kInertiaTimerId);
            m_inertiaTimer = 0;
        }
        m_spinVelX = 0.0f;
        m_spinVelY = 0.0f;
        SetCapture(m_hwnd);
        return 0;
    case WM_LBUTTONUP:
        m_leftDown = false;
        ReleaseCapture();
        if (m_wfScene) {
            const bool crossing = m_selectEndX < m_selectStartX;
            const int dxSel = m_selectEndX - m_selectStartX;
            const int dySel = m_selectEndY - m_selectStartY;
            const bool isBox = (dxSel * dxSel + dySel * dySel) > 16;

            if (isBox) {
                auto picked = collectWindowSelection(m_selectStartX, m_selectStartY, m_selectEndX, m_selectEndY, crossing);
                if (m_selectMode) {
                    m_wfScene->clearSelection();
                    for (int idx : picked) m_wfScene->selectEntity(idx);
                } else {
                    for (int idx : picked) m_wfScene->deselectEntity(idx);
                }
            } else {
                int hit = hitTestEntityAt(LOWORD(lParam), HIWORD(lParam));
                if (hit >= 0) {
                    if (m_selectMode) {
                        auto chain = (m_autoChainEnabled ? m_wfScene->autoChainFrom(hit) : std::vector<int>{});
                        if (chain.empty()) {
                            m_wfScene->clearSelection();
                            m_wfScene->selectEntity(hit);
                        } else {
                            m_wfScene->clearSelection();
                            for (int idx : chain) m_wfScene->selectEntity(idx);
                        }
                    } else {
                        auto chain = (m_autoChainEnabled ? m_wfScene->autoChainFrom(hit) : std::vector<int>{});
                        if (chain.empty()) {
                            m_wfScene->deselectEntity(hit);
                        } else {
                            for (int idx : chain) m_wfScene->deselectEntity(idx);
                        }
                    }
                } else if (m_selectMode) {
                    m_wfScene->clearSelection();
                }
            }
            redraw();
        }
        m_dragSelecting = false;
        return 0;
    case WM_MBUTTONDOWN:
        m_midDown    = true;
        m_lastMouseX = LOWORD(lParam);
        m_lastMouseY = HIWORD(lParam);
        SetCapture(m_hwnd);
        return 0;
    case WM_MBUTTONDBLCLK:
        zoomSelected();
        return 0;
    case WM_MBUTTONUP:
        m_midDown = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        m_rightDown  = true;
        m_lastMouseX = LOWORD(lParam);
        m_lastMouseY = HIWORD(lParam);
        m_rightClickStartX = m_lastMouseX;
        m_rightClickStartY = m_lastMouseY;
        SetCapture(m_hwnd);
        return 0;
    case WM_RBUTTONUP: {
        m_rightDown = false;
        ReleaseCapture();
        // Distinguish a click (no significant drag) from a pan gesture.
        // If movement was less than 5 px in either axis, treat as a click
        // and fire the context menu callback.
        int dxCtx = LOWORD(lParam) - m_rightClickStartX;
        int dyCtx = HIWORD(lParam) - m_rightClickStartY;
        if (m_contextMenuCb &&
            dxCtx * dxCtx + dyCtx * dyCtx <= kContextMenuThreshold * kContextMenuThreshold) {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ClientToScreen(m_hwnd, &pt);
            m_contextMenuCb(pt.x, pt.y);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        int dx = x - m_lastMouseX, dy = y - m_lastMouseY;
        if (m_leftDown) {
            m_selectEndX = x;
            m_selectEndY = y;
            m_dragSelecting = true;
            redraw();
        }
        if (m_midDown) {
            bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (ctrl) {
                // Ctrl + MMB drag: smooth precision zoom
                float zoomFactor = 1.0f + static_cast<float>(-dy) * 0.01f;
                if (zoomFactor < 0.1f) zoomFactor = 0.1f;
                m_camera.distance *= zoomFactor;
                if (m_camera.distance < 1.0f) m_camera.distance = 1.0f;
            } else if (shift) {
                // Shift + MMB drag: pan
                m_camera.panX -= dx * 0.3f;
                m_camera.panY += dy * 0.3f;
            } else {
                // MMB drag: orbit
                float vx = dy * 0.5f;
                float vy = dx * 0.5f;
                m_camera.orbitX += vx;
                m_camera.orbitY += vy;
            }
            redraw();
        }
        if (m_rightDown) {
            // Right drag: pan
            m_camera.panX -= dx * 0.3f;
            m_camera.panY += dy * 0.3f;
            redraw();
        }
        m_lastMouseX = x; m_lastMouseY = y;

        if (!m_leftDown && !m_midDown && !m_rightDown) {
            int hover = hitTestEntityAt(x, y, 8.0);
            if (hover != m_hoverEntity) {
                m_hoverEntity = hover;
                redraw();
            }
        }

        // Fire live-coordinate callback: unproject screen point to world space
        // and intersect the resulting ray with the Z=0 construction plane.
        if (m_coordCb && m_hGLRC) {
            wglMakeCurrent(m_hDC, m_hGLRC);

            GLint    viewport[4]  = {};
            GLdouble modelview[16]  = {};
            GLdouble projection[16] = {};
            glGetIntegerv(GL_VIEWPORT,        viewport);
            glGetDoublev (GL_MODELVIEW_MATRIX,  modelview);
            glGetDoublev (GL_PROJECTION_MATRIX, projection);

            // Flip Y: OpenGL origin is at bottom-left, Windows at top-left
            GLdouble winX = static_cast<GLdouble>(x);
            GLdouble winY = static_cast<GLdouble>(viewport[3] - y);

            GLdouble wx0, wy0, wz0;  // near-plane intersection
            GLdouble wx1, wy1, wz1;  // far-plane intersection
            gluUnProject(winX, winY, 0.0, modelview, projection, viewport,
                         &wx0, &wy0, &wz0);
            gluUnProject(winX, winY, 1.0, modelview, projection, viewport,
                         &wx1, &wy1, &wz1);

            // Ray–plane intersection: find t where z == 0
            double dz = wz1 - wz0;
            double wx, wy, wz;
            if (std::abs(dz) > kRayPlaneEpsilon) {
                double t = -wz0 / dz;
                wx = wx0 + t * (wx1 - wx0);
                wy = wy0 + t * (wy1 - wy0);
                wz = 0.0;
            } else {
                // Ray is parallel to z=0; report near-plane point
                wx = wx0; wy = wy0; wz = wz0;
            }
            m_coordCb(wx, wy, wz);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        // Determine which modifier keys are held
        bool ctrl  = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
        int  delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float ticks = static_cast<float>(delta) / WHEEL_DELTA;

        if (ctrl && shift) {
            // Ctrl+Shift+Wheel → horizontal pan
            m_camera.panX += ticks * 10.0f;
        } else {
            // Wheel zoom: stepped increment; Ctrl+wheel reduces increment for precision.
            static constexpr float kZoomFraction = 0.12f;
            static constexpr float kPrecisionZoomFraction = 0.04f;
            float frac = ctrl ? kPrecisionZoomFraction : kZoomFraction;
            float zoomStep = m_camera.distance * frac * ticks;
            m_camera.distance -= zoomStep;
            if (m_camera.distance < 1.0f) m_camera.distance = 1.0f;
        }
        redraw();
        return 0;
    }
    case WM_MOUSEHWHEEL: {
        // Native horizontal scroll (trackpad two-finger swipe / tilt-wheel) → pan X
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float ticks = static_cast<float>(delta) / WHEEL_DELTA;
        m_camera.panX += ticks * 10.0f;
        redraw();
        return 0;
    }
    case WM_TIMER: {
        if (wParam == kInertiaTimerId) {
            // Dampen and apply spin inertia
            m_spinVelX *= kInertiaDamping;
            m_spinVelY *= kInertiaDamping;
            m_camera.orbitX += m_spinVelX;
            m_camera.orbitY += m_spinVelY;
            redraw();
            float speed = m_spinVelX * m_spinVelX + m_spinVelY * m_spinVelY;
            if (speed < kInertiaStop * kInertiaStop) {
                KillTimer(m_hwnd, kInertiaTimerId);
                m_inertiaTimer = 0;
                m_spinVelX = 0.0f;
                m_spinVelY = 0.0f;
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // prevent background erase flicker
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}
