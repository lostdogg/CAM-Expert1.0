#include "Viewport3D.h"
#include "../managers/ToolpathManager.h"
#include "../managers/SolidsManager.h"
#include "../cad/BRep.h"
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
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    float ambient[]  = {0.2f, 0.2f, 0.2f, 1.0f};
    float diffuse[]  = {0.8f, 0.8f, 0.8f, 1.0f};
    float specular[] = {0.4f, 0.4f, 0.4f, 1.0f};
    float lightPos[] = {100.0f, 200.0f, 300.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glClearColor(0.18f, 0.18f, 0.22f, 1.0f); // dark grey background

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

    drawGrid();
    drawAxes();
    drawStock();
    drawSolids();
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
        SetCapture(m_hwnd);
        return 0;
    case WM_LBUTTONUP:
        m_leftDown = false;
        ReleaseCapture();
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
            // Left drag: orbit
            m_camera.orbitX += dy * 0.5f;
            m_camera.orbitY += dx * 0.5f;
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
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        // Zoom by a fixed fraction of current distance per WHEEL_DELTA tick,
        // giving consistent angular rate regardless of distance.
        static constexpr float kZoomFraction = 0.12f; // 12% per wheel notch
        float ticks    = static_cast<float>(delta) / WHEEL_DELTA;
        float zoomStep = m_camera.distance * kZoomFraction * ticks;
        m_camera.distance -= zoomStep;
        if (m_camera.distance < 1.0f) m_camera.distance = 1.0f;
        redraw();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // prevent background erase flicker
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}
