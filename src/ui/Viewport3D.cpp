#include "Viewport3D.h"
#include "../managers/ToolpathManager.h"
#include "../managers/SolidsManager.h"
#include "../managers/SurfacesManager.h"
#include "../cad/BRep.h"
#include "../cad/WireframeScene.h"
#include <gl/gl.h>
#include <gl/glu.h>
#include <cmath>

// --------------------------------------------------------------------------
Viewport3D::Viewport3D(HWND parent, HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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
void Viewport3D::setWireframeScene(const WireframeScene* scene) {
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

    glClearColor(0.16f, 0.17f, 0.21f, 1.0f); // deep blue-grey background

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

    SwapBuffers(m_hDC);
}

// --------------------------------------------------------------------------
// Viewport visualization constants
static constexpr int   kGridHighlightInterval = 5;     // every Nth grid line is brighter
static constexpr float kMinAxisLength         = 0.5f;  // minimum length to treat toolAxis as valid
static constexpr double kToolAxisTickLength   = 8.0;   // mm – length of tool-axis tick marks
static constexpr int   kAxisTickInterval      = 10;    // draw a tick every N toolpath points

// --------------------------------------------------------------------------
void Viewport3D::drawGrid() {
    glDisable(GL_LIGHTING);
    glColor3f(0.30f, 0.30f, 0.30f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -20; i <= 20; ++i) {
        float fi = static_cast<float>(i) * 10.0f;
        // Highlight every kGridHighlightInterval-th line slightly brighter
        float bright = (i % kGridHighlightInterval == 0) ? 0.45f : 0.30f;
        glColor3f(bright, bright, bright);
        glVertex3f(fi, -200, 0); glVertex3f(fi, 200, 0);
        glVertex3f(-200, fi, 0); glVertex3f(200, fi, 0);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
void Viewport3D::drawAxes() {
    glDisable(GL_LIGHTING);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    // X – red
    glColor3f(1.0f, 0.2f, 0.2f); glVertex3f(0,0,0); glVertex3f(40,0,0);
    // Y – green
    glColor3f(0.2f, 1.0f, 0.2f); glVertex3f(0,0,0); glVertex3f(0,40,0);
    // Z – blue
    glColor3f(0.2f, 0.4f, 1.0f); glVertex3f(0,0,0); glVertex3f(0,0,40);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
// Draw a semi-transparent stock billet (100×100×50 mm box centred at origin)
void Viewport3D::drawStock() {
    if (m_renderMode == RenderMode::Wireframe) {
        glDisable(GL_LIGHTING);
        glColor3f(0.5f, 0.5f, 0.7f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.55f, 0.55f, 0.75f,
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
                glColor4f(0.55f, 0.65f, 0.80f,
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
            glColor3f(0.30f, 0.35f, 0.50f);  // dark edge overlay in shaded mode

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

    glDisable(GL_LIGHTING);
    glLineWidth(1.5f);

    for (const WfEntity& e : m_wfScene->entities()) {
        switch (e.type) {

        case WfEntityType::Point:
            glColor3f(1.0f, 1.0f, 0.0f);   // yellow
            glPointSize(kPointSz);
            glBegin(GL_POINTS);
            glVertex3d(e.p0.x, e.p0.y, e.p0.z);
            glEnd();
            break;

        case WfEntityType::Line:
            glColor3f(0.0f, 1.0f, 0.8f);   // cyan
            glBegin(GL_LINES);
            glVertex3d(e.p0.x, e.p0.y, e.p0.z);
            glVertex3d(e.p1.x, e.p1.y, e.p1.z);
            glEnd();
            break;

        case WfEntityType::Arc: {
            glColor3f(0.6f, 1.0f, 0.2f);   // lime
            glBegin(GL_LINE_STRIP);
            double span = e.endAngle - e.startAngle;
            // Handle wrap-around (ensure positive sweep)
            if (span <= 0.0) span += kTwoPi;
            for (int i = 0; i <= kArcSegs; ++i) {
                double t = e.startAngle + span * i / kArcSegs;
                double px = e.p0.x + e.radius * std::cos(t);
                double py = e.p0.y + e.radius * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            break;
        }

        case WfEntityType::Circle: {
            glColor3f(0.6f, 1.0f, 0.2f);   // lime
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < kArcSegs; ++i) {
                double t  = kTwoPi * i / kArcSegs;
                double px = e.p0.x + e.radius * std::cos(t);
                double py = e.p0.y + e.radius * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            break;
        }

        case WfEntityType::Ellipse: {
            glColor3f(0.8f, 0.6f, 1.0f);   // lavender
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < kArcSegs; ++i) {
                double t  = kTwoPi * i / kArcSegs;
                double px = e.p0.x + e.radius  * std::cos(t);
                double py = e.p0.y + e.radius2 * std::sin(t);
                glVertex3d(px, py, e.p0.z);
            }
            glEnd();
            break;
        }

        case WfEntityType::Spline:
            // Draw line segments between consecutive control points as a
            // polyline approximation of the spline.
            if (e.pts.size() >= 2) {
                glColor3f(1.0f, 0.7f, 0.0f);   // orange
                glBegin(GL_LINE_STRIP);
                for (const auto& cp : e.pts)
                    glVertex3d(cp.x, cp.y, cp.z);
                glEnd();
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
                glColor3f(0.0f, 1.0f, 0.8f);   // cyan
                glBegin(GL_LINE_LOOP);
                for (const auto& v : e.pts)
                    glVertex3d(v.x, v.y, v.z);
                glEnd();
            }
            break;

        case WfEntityType::Helix: {
            glColor3f(1.0f, 0.5f, 0.2f);   // coral
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
            break;
        }

        }  // switch
    }  // for each entity

    glLineWidth(1.0f);
    glPointSize(1.0f);
    glEnable(GL_LIGHTING);
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
        m_lastMouseX = LOWORD(lParam);
        m_lastMouseY = HIWORD(lParam);
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
        // Start inertia if there is meaningful velocity
        if ((m_spinVelX * m_spinVelX + m_spinVelY * m_spinVelY) > kInertiaStop * kInertiaStop) {
            m_inertiaTimer = SetTimer(m_hwnd, kInertiaTimerId, 16, nullptr); // ~60 fps
        }
        return 0;
    case WM_MBUTTONDOWN:
        m_midDown    = true;
        m_lastMouseX = LOWORD(lParam);
        m_lastMouseY = HIWORD(lParam);
        SetCapture(m_hwnd);
        return 0;
    case WM_MBUTTONUP:
        m_midDown = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        m_rightDown  = true;
        m_lastMouseX = LOWORD(lParam);
        m_lastMouseY = HIWORD(lParam);
        SetCapture(m_hwnd);
        return 0;
    case WM_RBUTTONUP:
        m_rightDown = false;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        int dx = x - m_lastMouseX, dy = y - m_lastMouseY;
        if (m_leftDown) {
            // Left drag: orbit (dynamic rotation)
            float vx = dy * 0.5f;
            float vy = dx * 0.5f;
            m_camera.orbitX += vx;
            m_camera.orbitY += vy;
            // Track velocity for inertia
            m_spinVelX = vx;
            m_spinVelY = vy;
            redraw();
        }
        if (m_midDown || m_rightDown) {
            // Middle/right drag: pan
            m_camera.panX -= dx * 0.3f;
            m_camera.panY += dy * 0.3f;
            redraw();
        }
        m_lastMouseX = x; m_lastMouseY = y;
        return 0;
    }
    case WM_MOUSEWHEEL: {
        // Determine which modifier keys are held
        bool ctrl  = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
        bool shift = (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)   != 0;
        int  delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float ticks = static_cast<float>(delta) / WHEEL_DELTA;

        if (ctrl && shift) {
            // Ctrl+Shift+Wheel → horizontal pan
            m_camera.panX += ticks * 10.0f;
        } else if (ctrl) {
            // Ctrl+Wheel → rotate around Y axis (yaw / spin)
            float rot = ticks * 8.0f;
            m_camera.orbitY += rot;
            // Kick off a small inertia spin
            m_spinVelX = 0.0f;
            m_spinVelY = rot;
            if (m_inertiaTimer) KillTimer(m_hwnd, kInertiaTimerId);
            m_inertiaTimer = SetTimer(m_hwnd, kInertiaTimerId, 16, nullptr);
        } else if (shift) {
            // Shift+Wheel → rotate around X axis (pitch / tilt)
            float rot = ticks * 8.0f;
            m_camera.orbitX += rot;
            m_spinVelX = rot;
            m_spinVelY = 0.0f;
            if (m_inertiaTimer) KillTimer(m_hwnd, kInertiaTimerId);
            m_inertiaTimer = SetTimer(m_hwnd, kInertiaTimerId, 16, nullptr);
        } else {
            // Plain wheel → zoom (12% of current distance per notch)
            static constexpr float kZoomFraction = 0.12f;
            float zoomStep = m_camera.distance * kZoomFraction * ticks;
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
