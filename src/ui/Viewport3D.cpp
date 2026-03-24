#include "Viewport3D.h"
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
    glShadeModel(GL_SMOOTH);

    float ambient[]  = {0.2f, 0.2f, 0.2f, 1.0f};
    float diffuse[]  = {0.8f, 0.8f, 0.8f, 1.0f};
    float lightPos[] = {100.0f, 200.0f, 300.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
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

    // Placeholder: draw a sample billet (box) to represent stock
    glColor3f(0.6f, 0.6f, 0.8f);
    glBegin(GL_QUADS);
    // Front face
    glNormal3f(0,  -1, 0); glVertex3f(-50,-50,-50); glVertex3f(50,-50,-50);
                            glVertex3f(50,-50, 0);   glVertex3f(-50,-50, 0);
    // Back face
    glNormal3f(0,   1, 0); glVertex3f(-50, 50,-50); glVertex3f(-50, 50, 0);
                            glVertex3f(50,  50, 0);  glVertex3f(50,  50,-50);
    // Top face
    glNormal3f(0, 0,  1);  glVertex3f(-50,-50, 0);  glVertex3f(50,-50, 0);
                            glVertex3f(50,  50, 0);  glVertex3f(-50, 50, 0);
    // Bottom face
    glNormal3f(0, 0, -1);  glVertex3f(-50,-50,-50); glVertex3f(-50, 50,-50);
                            glVertex3f(50,  50,-50); glVertex3f(50, -50,-50);
    // Left face
    glNormal3f(-1, 0, 0);  glVertex3f(-50,-50,-50); glVertex3f(-50,-50, 0);
                            glVertex3f(-50, 50, 0);  glVertex3f(-50, 50,-50);
    // Right face
    glNormal3f( 1, 0, 0);  glVertex3f(50,-50,-50);  glVertex3f(50, 50,-50);
                            glVertex3f(50, 50, 0);   glVertex3f(50,-50, 0);
    glEnd();

    SwapBuffers(m_hDC);
}

// --------------------------------------------------------------------------
void Viewport3D::drawGrid() {
    glDisable(GL_LIGHTING);
    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; ++i) {
        float fi = static_cast<float>(i) * 10.0f;
        glVertex3f(fi, -100, 0); glVertex3f(fi, 100, 0);
        glVertex3f(-100, fi, 0); glVertex3f(100, fi, 0);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

// --------------------------------------------------------------------------
void Viewport3D::drawAxes() {
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0,0,0); glVertex3f(30,0,0);  // X red
    glColor3f(0, 1, 0); glVertex3f(0,0,0); glVertex3f(0,30,0);  // Y green
    glColor3f(0, 0, 1); glVertex3f(0,0,0); glVertex3f(0,0,30);  // Z blue
    glEnd();
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
    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        int dx = x - m_lastMouseX, dy = y - m_lastMouseY;
        if (m_leftDown) {
            m_camera.orbitX += dy * 0.5f;
            m_camera.orbitY += dx * 0.5f;
            redraw();
        }
        if (m_midDown) {
            m_camera.panX -= dx * 0.3f;
            m_camera.panY += dy * 0.3f;
            redraw();
        }
        m_lastMouseX = x; m_lastMouseY = y;
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        m_camera.distance -= delta * 0.1f;
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
