#include "MainWindow.h"
#include "Application.h"
#include "ui/RibbonUI.h"
#include "ui/Viewport3D.h"
#include "ui/SelectionBar.h"
#include "ui/CopilotPanel.h"
#include "managers/ToolpathManager.h"
#include "managers/SolidsManager.h"
#include "managers/SurfacesManager.h"
#include "managers/LevelsManager.h"
#include "managers/PlanesManager.h"
#include "simulation/Backplot.h"
#include "simulation/Verify.h"
#include "simulation/MachineSimulation.h"
#include "cam/PostProcessor.h"
#include "cam/DynamicMotion.h"
#include "cam/Strategies2D.h"
#include "cam/Strategies3D.h"
#include "cam/MultiAxis.h"
#include "cam/ProbingCycles.h"
#include "cad/FileImporter.h"
#include "cad/ModelPrep.h"
#include "cad/FeatureRecognition.h"
#include "cad/MeshData.h"
#include "copilot/CopilotEngine.h"
#include "resources/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

// --------------------------------------------------------------------------
// Input-dialog state shared with the resource-based dialog procs
// --------------------------------------------------------------------------
namespace {

struct SinglePromptState {
    wchar_t buf1[64] = {};
    bool    accepted = false;
};

struct DoublePromptState {
    wchar_t buf1[64] = {};
    wchar_t buf2[64] = {};
    bool    accepted = false;
};

struct TriplePromptState {
    wchar_t buf1[64] = {};
    wchar_t buf2[64] = {};
    wchar_t buf3[64] = {};
    bool    accepted = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Dialog proc for IDD_PROMPT_SINGLE
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK SinglePromptDlgProc(HWND hdlg, UINT msg,
                                             WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hdlg, DWLP_USER, lp);
        return TRUE;
    }
    auto* st = reinterpret_cast<SinglePromptState*>(
                   GetWindowLongPtrW(hdlg, DWLP_USER));
    if (msg == WM_COMMAND) {
        if (LOWORD(wp) == IDOK && st) {
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1, st->buf1, 64);
            st->accepted = true;
            EndDialog(hdlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Dialog proc for IDD_PROMPT_DOUBLE
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK DoublePromptDlgProc(HWND hdlg, UINT msg,
                                              WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hdlg, DWLP_USER, lp);
        return TRUE;
    }
    auto* st = reinterpret_cast<DoublePromptState*>(
                   GetWindowLongPtrW(hdlg, DWLP_USER));
    if (msg == WM_COMMAND) {
        if (LOWORD(wp) == IDOK && st) {
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1, st->buf1, 64);
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT2, st->buf2, 64);
            st->accepted = true;
            EndDialog(hdlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Dialog proc for IDD_PROMPT_TRIPLE
// ---------------------------------------------------------------------------
static INT_PTR CALLBACK TriplePromptDlgProc(HWND hdlg, UINT msg,
                                              WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hdlg, DWLP_USER, lp);
        return TRUE;
    }
    auto* st = reinterpret_cast<TriplePromptState*>(
                   GetWindowLongPtrW(hdlg, DWLP_USER));
    if (msg == WM_COMMAND) {
        if (LOWORD(wp) == IDOK && st) {
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1, st->buf1, 64);
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT2, st->buf2, 64);
            GetDlgItemTextW(hdlg, IDC_PROMPT_EDIT3, st->buf3, 64);
            st->accepted = true;
            EndDialog(hdlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Helper: parse a wchar_t buffer as double; returns true on success
// ---------------------------------------------------------------------------
static bool parseDouble(const wchar_t* buf, double& out) {
    wchar_t* end = nullptr;
    out = wcstod(buf, &end);
    return end && end != buf;
}

// Named constant for π used in circumference and polygon calculations
static constexpr double kPi = 3.14159265358979323846;

// --------------------------------------------------------------------------
MainWindow::MainWindow() = default;
MainWindow::~MainWindow() = default;

// --------------------------------------------------------------------------
bool MainWindow::create(HINSTANCE hInstance) {
    // Register the window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        // Allow re-registration if already registered
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    // Create the main window (initially hidden; show() makes it visible)
    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, Application::APP_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        nullptr, nullptr, hInstance, this);

    return m_hwnd != nullptr;
}

// --------------------------------------------------------------------------
void MainWindow::show(int nCmdShow) {
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

// --------------------------------------------------------------------------
// Static WndProc – retrieves the 'this' pointer stored in GWLP_USERDATA
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->handleMessage(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// --------------------------------------------------------------------------
LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;

    case WM_KEYDOWN: {
        // Keyboard shortcuts (processed when main window has focus)
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl) {
            switch (static_cast<int>(wParam)) {
            case 'N': onCommand(IDM_FILE_NEW);     return 0;
            case 'O': onCommand(IDM_FILE_OPEN);    return 0;
            case 'S': onCommand(IDM_FILE_SAVE);    return 0;
            case 'I': onCommand(IDM_FILE_IMPORT);  return 0;
            case 'P': onCommand(IDM_MACHINE_POST); return 0;
            }
        }
        if (wParam == VK_F1)  { onCommand(IDM_COPILOT_TOGGLE);      return 0; }
        if (wParam == VK_F5)  { onCommand(IDM_MACHINE_REGEN);       return 0; }
        if (wParam == VK_F6)  { onCommand(IDM_MACHINE_3D_WATERLINE); return 0; }
        if (wParam == VK_F7)  { onCommand(IDM_MACHINE_3D_SCALLOP);  return 0; }
        break;
    }

    case WM_PAINT:
        onPaint();
        return 0;

    case WM_DESTROY:
        onDestroy();
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

// --------------------------------------------------------------------------
void MainWindow::onCreate() {
    buildMenu();
    updateWindowTitle();   // set initial "Untitled" title

    HINSTANCE hInst = Application::instance().hInstance();

    // --- Status bar (bottom) ---
    m_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd,
        (HMENU)(UINT_PTR)IDC_STATUS_BAR, hInst, nullptr);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Ready"));

    // --- Managers panel (left side – tab control) ---
    m_hManagersPanel = CreateWindowExW(
        0, WC_TABCONTROL, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
        0, RIBBON_HEIGHT, MANAGERS_PANEL_WIDTH, 600,
        m_hwnd, (HMENU)(UINT_PTR)IDC_MANAGERS_PANEL, hInst, nullptr);

    // Add tabs to the managers panel
    TCITEMW ti{};
    ti.mask    = TCIF_TEXT;
    ti.pszText = const_cast<wchar_t*>(L"Toolpaths");
    TabCtrl_InsertItem(m_hManagersPanel, 0, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Solids");
    TabCtrl_InsertItem(m_hManagersPanel, 1, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Levels");
    TabCtrl_InsertItem(m_hManagersPanel, 2, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Planes");
    TabCtrl_InsertItem(m_hManagersPanel, 3, &ti);

    // --- Ribbon UI ---
    m_ribbon = std::make_unique<RibbonUI>(m_hwnd, hInst);

    // --- 3-D Viewport ---
    m_viewport = std::make_unique<Viewport3D>(m_hwnd, hInst);

    // --- Selection bar ---
    m_selectionBar = std::make_unique<SelectionBar>(m_hwnd, hInst);

    // --- Managers ---
    m_toolpathMgr  = std::make_unique<ToolpathManager>();
    m_solidsMgr    = std::make_unique<SolidsManager>();
    m_surfacesMgr  = std::make_unique<SurfacesManager>();
    m_levelsMgr    = std::make_unique<LevelsManager>();
    m_planesMgr    = std::make_unique<PlanesManager>();

    // Connect toolpath manager to the viewport so toolpaths are rendered live
    m_viewport->setToolpathManager(m_toolpathMgr.get());

    // Connect the solids manager to the viewport so BRep solids are rendered
    m_viewport->setSolidsManager(m_solidsMgr.get());

    // Connect the surfaces manager to the viewport so NURBS surfaces are rendered
    m_viewport->setSurfacesManager(m_surfacesMgr.get());

    // Connect the toolpath manager change callback to trigger a viewport redraw
    m_toolpathMgr->setOnChange([this]() {
        if (m_viewport) m_viewport->redraw();
    });

    // Connect the solids manager change callback to trigger a viewport redraw
    m_solidsMgr->setOnChange([this]() {
        if (m_viewport) m_viewport->redraw();
    });

    // Connect the surfaces manager change callback to trigger a viewport redraw
    m_surfacesMgr->setOnChange([this]() {
        if (m_viewport) m_viewport->redraw();
    });

    // Connect the selection bar mask callback to the status bar
    if (m_selectionBar) {
        m_selectionBar->setMaskCallback([this](SelectMask mask) {
            const wchar_t* names[] = {
                L"Mask: All", L"Mask: Points", L"Mask: Lines",
                L"Mask: Arcs", L"Mask: Splines", L"Mask: Surfaces",
                L"Mask: Solids", L"Mask: Holes", L"Mask: Planar Faces",
                L"Mask: None"
            };
            int idx = static_cast<int>(mask);
            if (idx >= 0 && idx < 10)
                SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(names[idx]));
        });
    }

    // --- Copilot ---
    m_copilotEngine = std::make_unique<CopilotEngine>();
    m_copilotEngine->setToolpathManager(m_toolpathMgr.get());
    m_copilotEngine->setSurfacesManager(m_surfacesMgr.get());

    m_copilotPanel = std::make_unique<CopilotPanel>(m_hwnd, hInst);
    m_copilotPanel->setCopilotEngine(m_copilotEngine.get());
    ShowWindow(m_copilotPanel->hwnd(), SW_HIDE);  // hidden by default
}

// --------------------------------------------------------------------------
void MainWindow::buildMenu() {
    HMENU hMenu     = CreateMenu();
    HMENU hFile     = CreatePopupMenu();
    HMENU hSurface  = CreatePopupMenu();
    HMENU hMachine  = CreatePopupMenu();
    HMENU hView     = CreatePopupMenu();
    HMENU hHelp     = CreatePopupMenu();

    // File menu
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_NEW,    L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_OPEN,   L"&Open…\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_SAVEAS, L"Save &As…");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_IMPORT, L"&Import…");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING,  IDM_FILE_EXIT,   L"E&xit\tAlt+F4");

    // Surfaces menu
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_LOFT,    L"&Loft…");
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_REVOLVE, L"&Revolve…");
    AppendMenuW(hSurface, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_FILLET,  L"&Fillet Blend…");
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_OFFSET,  L"&Offset…");
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_EXTEND,  L"&Extend…");
    AppendMenuW(hSurface, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_TRIM,    L"&Trim");
    AppendMenuW(hSurface, MF_STRING, IDM_SURF_UNTRIM,  L"&Untrim");

    // Machine menu
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_BACKPLOT, L"&Backplot");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_VERIFY,   L"&Verify");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_SIM,      L"Machine &Simulation");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_POST,     L"&Post Process…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_GEN_POCKET,  L"Generate 2D &Pocket…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_GEN_CONTOUR, L"Generate 2D &Contour…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_CHAMFER,     L"Generate C&hamfer…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_THREAD,      L"Generate T&hread Mill…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_3D_WATERLINE, L"Generate 3D &Waterline…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_3D_SCALLOP,   L"Generate 3D &Scallop…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_3D_RASTER,    L"Generate 3D &Raster…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_5AXIS,        L"Generate &5-Axis Swarf…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_PROBE_Z,     L"Probe &Z Surface…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_PROBE_BORE,  L"Probe &Bore/Boss Center…");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_PROBE_CORNER,L"Probe C&orner…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_REGEN,    L"Re&generate All\tF5");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_SUMMARY,  L"&Machining Summary…");

    // View menu
    AppendMenuW(hView, MF_STRING, IDM_VIEW_WIREFRAME, L"&Wireframe");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_SHADED,    L"&Shaded");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TRANSLU,   L"&Translucent");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ISOMETRIC, L"&Isometric");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FRONT,     L"&Front");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TOP,       L"&Top");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_RIGHT,     L"&Right");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_BACK,      L"&Back");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_BOTTOM,    L"Bot&tom");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_LEFT,      L"&Left");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FIT,       L"Fit &All");

    // Help menu
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT,      L"&About CAM-Expert…");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelp, MF_STRING, IDM_COPILOT_TOGGLE,  L"Toggle &Copilot Panel\tF1");

    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile),    L"&File");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSurface), L"&Surfaces");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hMachine), L"&Machine");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hView),    L"&View");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelp),    L"&Help");

    SetMenu(m_hwnd, hMenu);
}

// --------------------------------------------------------------------------
void MainWindow::onSize(int cx, int cy) {
    updateLayout(cx, cy);
}

// --------------------------------------------------------------------------
void MainWindow::updateLayout(int cx, int cy) {
    if (!m_hwnd) return;

    // Status bar – auto-sizes itself
    if (m_hStatusBar)
        SendMessage(m_hStatusBar, WM_SIZE, 0, 0);

    int viewY  = RIBBON_HEIGHT;
    int viewH  = cy - RIBBON_HEIGHT - STATUS_BAR_HEIGHT;

    // If the Copilot panel is visible, carve out space on the right side
    int copilotW = (m_copilotVisible && m_copilotPanel) ? COPILOT_PANEL_WIDTH : 0;

    // Selection bar sits on the right edge, to the left of the Copilot panel
    int selBarX = cx - SELECTION_BAR_WIDTH - copilotW;
    int viewX   = MANAGERS_PANEL_WIDTH;
    int viewW   = cx - MANAGERS_PANEL_WIDTH - SELECTION_BAR_WIDTH - copilotW;

    // Managers panel
    if (m_hManagersPanel)
        SetWindowPos(m_hManagersPanel, nullptr,
                     0, viewY, MANAGERS_PANEL_WIDTH, viewH,
                     SWP_NOZORDER | SWP_NOACTIVATE);

    // Ribbon
    if (m_ribbon)
        m_ribbon->resize(0, 0, cx, RIBBON_HEIGHT);

    // Viewport
    if (m_viewport)
        m_viewport->resize(viewX, viewY, viewW, viewH);

    // Selection bar (right edge of the viewport area)
    if (m_selectionBar)
        m_selectionBar->resize(selBarX, viewY, SELECTION_BAR_WIDTH, viewH);

    // Copilot panel (far right, full height below ribbon)
    if (m_copilotPanel && m_copilotVisible)
        m_copilotPanel->resize(cx - copilotW, viewY, copilotW, viewH);
}

// --------------------------------------------------------------------------
void MainWindow::onCommand(int id) {
    switch (id) {
    case IDM_FILE_NEW:    fileNew();     break;
    case IDM_FILE_OPEN:   fileOpen();    break;
    case IDM_FILE_SAVE:   fileSave();    break;
    case IDM_FILE_SAVEAS: fileSave();    break;
    case IDM_FILE_IMPORT: fileImport();  break;
    case IDM_FILE_EXIT:   DestroyWindow(m_hwnd); break;

    case IDM_MACHINE_BACKPLOT: runBackplot();   break;
    case IDM_MACHINE_VERIFY:   runVerify();     break;
    case IDM_MACHINE_SIM:      runMachineSim(); break;
    case IDM_MACHINE_POST:     postProcess();   break;
    case IDM_MACHINE_GEN_POCKET:  generateToolpathPocket();  break;
    case IDM_MACHINE_GEN_CONTOUR: generateToolpathContour(); break;
    case IDM_MACHINE_CHAMFER:     generateToolpathChamfer(); break;
    case IDM_MACHINE_THREAD:      generateToolpathThread();  break;
    case IDM_MACHINE_PROBE_Z:     probeZSurface();           break;
    case IDM_MACHINE_PROBE_BORE:  probeBoreCenter();         break;
    case IDM_MACHINE_PROBE_CORNER:probeCorner();             break;
    case IDM_MACHINE_3D_WATERLINE:generate3DWaterline();    break;
    case IDM_MACHINE_3D_SCALLOP:  generate3DScallop();      break;
    case IDM_MACHINE_3D_RASTER:   generate3DRaster();       break;
    case IDM_MACHINE_5AXIS:       generate5AxisSwarf();     break;
    case IDM_MACHINE_REGEN:       regenerateAllToolpaths();  break;
    case IDM_MACHINE_SUMMARY:     showMachiningSummary();    break;

    case IDM_VIEW_WIREFRAME:
        if (m_viewport) m_viewport->setRenderMode(RenderMode::Wireframe);
        break;
    case IDM_VIEW_SHADED:
        if (m_viewport) m_viewport->setRenderMode(RenderMode::Shaded);
        break;
    case IDM_VIEW_TRANSLU:
        if (m_viewport) m_viewport->setRenderMode(RenderMode::Translucent);
        break;
    case IDM_VIEW_ISOMETRIC:
        if (m_viewport) m_viewport->setView(ViewPreset::Isometric);
        break;
    case IDM_VIEW_FRONT:
        if (m_viewport) m_viewport->setView(ViewPreset::Front);
        break;
    case IDM_VIEW_TOP:
        if (m_viewport) m_viewport->setView(ViewPreset::Top);
        break;
    case IDM_VIEW_RIGHT:
        if (m_viewport) m_viewport->setView(ViewPreset::Right);
        break;
    case IDM_VIEW_BACK:
        if (m_viewport) m_viewport->setView(ViewPreset::Back);
        break;
    case IDM_VIEW_BOTTOM:
        if (m_viewport) m_viewport->setView(ViewPreset::Bottom);
        break;
    case IDM_VIEW_LEFT:
        if (m_viewport) m_viewport->setView(ViewPreset::Left);
        break;
    case IDM_VIEW_FIT:
        if (m_viewport) { m_viewport->reset(); }
        break;

    // Wireframe / Surfaces / Solids / Model Prep – placeholder handlers
    // (real geometry creation would open a dialog or enable a creation mode)
    case IDM_WF_POINT:
    case IDM_WF_LINE:
    case IDM_WF_ARC:
    case IDM_WF_CIRCLE:
    case IDM_WF_RECTANGLE:
    case IDM_WF_POLYGON:
    case IDM_WF_SPLINE:
        createWireframe(id);
        break;

    case IDM_SURF_LOFT:    surfaceLoft();    break;
    case IDM_SURF_REVOLVE: surfaceRevolve(); break;
    case IDM_SURF_FILLET:  surfaceFillet();  break;
    case IDM_SURF_OFFSET:  surfaceOffset();  break;
    case IDM_SURF_TRIM:    surfaceTrim();    break;
    case IDM_SURF_UNTRIM:  surfaceUntrim();  break;
    case IDM_SURF_EXTEND:  surfaceExtend();  break;

    case IDM_SOLID_EXTRUDE:  createSolidBox();             break;
    case IDM_SOLID_REVOLVE:  createSolidCylinder();        break;
    case IDM_SOLID_SPHERE:   createSolidSphere();          break;
    case IDM_SOLID_UNION:
    case IDM_SOLID_SUBTRACT:
    case IDM_SOLID_INTERSECT:
    case IDM_SOLID_FILLET:
    case IDM_SOLID_SHELL:
        solidBooleanOp(id);
        break;

    case IDM_PREP_HEAL:        prepHeal();         break;
    case IDM_PREP_REM_FILLET:  prepRemoveFillet(); break;
    case IDM_PREP_SPLIT:       prepSplit();        break;
    case IDM_PREP_BOUNDS:      prepBoundaries();   break;
    case IDM_PREP_CLASSIFY:    prepClassify();     break;
    case IDM_PREP_DRAFT:       prepAnalyse();      break;

    case IDM_HELP_ABOUT:
        showAboutDialog();
        break;

    case IDM_COPILOT_TOGGLE:
        toggleCopilotPanel();
        break;

    default:
        // Forward only Copilot-panel command IDs to avoid interfering
        // with future additions to other command ranges.
        if (m_copilotPanel && m_copilotVisible &&
            id >= IDC_COPILOT_OUTPUT && id <= IDC_COPILOT_CLEAR) {
            m_copilotPanel->handleCommand(id);
        }
        break;
    }
}

// --------------------------------------------------------------------------
void MainWindow::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    (void)hdc; // Background is filled by child windows; nothing extra needed here.
    EndPaint(m_hwnd, &ps);
}

// --------------------------------------------------------------------------
void MainWindow::onDestroy() {
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------
void MainWindow::showAboutDialog() {
    std::wstring msg =
        std::wstring(Application::APP_NAME) + L" v" + Application::APP_VERSION +
        L"\n\nComputer-Aided Manufacturing & Design software.\n"
        L"Supports 2D/2.5D, 3D, and Multi-Axis machining strategies.\n"
        L"Post-Processor: Fanuc, Haas, Heidenhain, and more.\n\n"
        L"New in v1.2:\n"
        L"  \u2022 3D Waterline (Z-level) toolpath generation\n"
        L"  \u2022 3D Scallop (constant step-over) with live scallop-height readout\n"
        L"  \u2022 3D Raster (parallel passes) projected onto mesh stock\n"
        L"  \u2022 5-Axis Swarf milling with configurable lead angle\n"
        L"  \u2022 F6/F7 keyboard shortcuts for 3D Waterline / Scallop\n"
        L"  \u2022 Fixed NURBS offset surface knot vectors (correct geometry)\n"
        L"  \u2022 Dual-light OpenGL rendering for world-class surface shading\n\n"
        L"New in v1.1:\n"
        L"  \u2022 NURBS Surface creation (Loft, Revolve, Fillet, Offset, Extend, Trim)\n"
        L"  \u2022 Chamfer and Thread Mill strategies\n"
        L"  \u2022 Probing cycles (Z-surface, Bore/Boss, Corner)\n"
        L"  \u2022 Strategy-aware Copilot apply (drill, contour, pocket, chamfer, thread)\n"
        L"  \u2022 Feature-Recognition depth computed from solid geometry\n\n"
        L"\u00A9 2024\u20132025 CAM-Expert Project";
    MessageBoxW(m_hwnd, msg.c_str(), L"About CAM-Expert", MB_OK | MB_ICONINFORMATION);
}

// --------------------------------------------------------------------------
void MainWindow::fileNew() {
    // Reset managers and viewport for a fresh session
    if (m_toolpathMgr)  m_toolpathMgr->clear();
    if (m_solidsMgr)    m_solidsMgr->clear();
    if (m_surfacesMgr)  m_surfacesMgr->clear();
    if (m_levelsMgr)    m_levelsMgr->clear();
    if (m_viewport)     m_viewport->reset();
    m_currentFile.clear();
    updateWindowTitle();
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"New file created."));
}

// --------------------------------------------------------------------------
void MainWindow::fileOpen() {
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFilter =
        L"CAM-Expert Files (*.camx)\0*.camx\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        loadProjectCamx(szFile);
    }
}

// --------------------------------------------------------------------------
void MainWindow::fileSave() {
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize    = sizeof(ofn);
    ofn.hwndOwner      = m_hwnd;
    ofn.lpstrFilter    =
        L"CAM-Expert Files (*.camx)\0*.camx\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile      = szFile;
    ofn.nMaxFile       = MAX_PATH;
    ofn.lpstrDefExt    = L"camx";
    ofn.Flags          = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        saveProjectCamx(szFile);
    }
}

// --------------------------------------------------------------------------
void MainWindow::fileImport() {
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFilter =
        L"All Supported (*.step;*.stp;*.iges;*.igs;*.stl;*.obj;*.sldprt;*.sldasm)\0"
        L"*.step;*.stp;*.iges;*.igs;*.stl;*.obj;*.sldprt;*.sldasm\0"
        L"STEP Files (*.step;*.stp)\0*.step;*.stp\0"
        L"IGES Files (*.iges;*.igs)\0*.iges;*.igs\0"
        L"STL Files (*.stl)\0*.stl\0"
        L"OBJ Files (*.obj)\0*.obj\0"
        L"SolidWorks (*.sldprt;*.sldasm)\0*.sldprt;*.sldasm\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    // Convert wide path to UTF-8 for FileImporter
    int nbytes = WideCharToMultiByte(CP_UTF8, 0, szFile, -1,
                                     nullptr, 0, nullptr, nullptr);
    std::string filePath(static_cast<std::size_t>(nbytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, szFile, -1,
                        filePath.data(), nbytes, nullptr, nullptr);
    if (!filePath.empty() && filePath.back() == '\0')
        filePath.pop_back();

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>((std::wstring(L"Importing: ") + szFile).c_str()));

    // Invoke the FileImporter
    FileImporter importer;
    ImportResult result = importer.import(filePath);

    std::wstring statusMsg;

    // Handle imported result
    if (std::holds_alternative<BRep::Solid>(result)) {
        BRep::Solid& solid = std::get<BRep::Solid>(result);
        if (m_solidsMgr) m_solidsMgr->addSolid(solid);

        // Run automatic feature recognition
        if (m_copilotEngine) {
            FeatureRecognition fr;
            auto features = fr.recognise(solid);
            m_copilotEngine->setRecognisedFeatures(features);
        }

        std::string name = solid.name().empty() ? "Solid" : solid.name();
        int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
        std::wstring wname(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wname.data(), wlen);

        statusMsg = L"Imported solid: " + wname
                  + L"  (" + std::to_wstring(solid.faces().size()) + L" faces)";

    } else if (std::holds_alternative<MeshData>(result)) {
        const MeshData& mesh = std::get<MeshData>(result);
        statusMsg = L"Imported mesh: "
                  + std::to_wstring(mesh.triCount()) + L" triangles";
    } else {
        // Import failed or returned unknown type
        const std::string& err = importer.lastError();
        if (!err.empty()) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, nullptr, 0);
            std::wstring werr(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, werr.data(), wlen);
            statusMsg = L"Import failed: " + werr;
        } else {
            statusMsg = L"Import failed: unknown error.";
        }
    }

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(statusMsg.c_str()));

    // Refresh the viewport
    if (m_viewport) m_viewport->redraw();
}

// --------------------------------------------------------------------------
void MainWindow::postProcess() {
    PostProcessor pp;
    const CoordPlane* wcsPlane = m_planesMgr ? m_planesMgr->wcsPlane() : nullptr;
    auto gcode = pp.generate(m_toolpathMgr.get(), wcsPlane);

    if (gcode.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Post-Processor: no toolpaths to process."));
        return;
    }

    if (pp.hasError()) {
        const std::string& errStr = pp.lastError().message;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, errStr.c_str(), -1, nullptr, 0);
        std::wstring werr(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, errStr.c_str(), -1, werr.data(), wlen);
        MessageBoxW(m_hwnd, werr.c_str(), L"Post-Processor Error", MB_OK | MB_ICONWARNING);
        return;
    }

    // Prompt for save path
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize    = sizeof(ofn);
    ofn.hwndOwner      = m_hwnd;
    ofn.lpstrFilter    =
        L"NC Files (*.nc;*.tap;*.cnc;*.mpf)\0*.nc;*.tap;*.cnc;*.mpf\0"
        L"Text Files (*.txt)\0*.txt\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile      = szFile;
    ofn.nMaxFile       = MAX_PATH;
    ofn.lpstrDefExt    = L"nc";
    ofn.Flags          = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        // Convert path to UTF-8 and write the file
        int nbytes = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        std::string ncPath(static_cast<std::size_t>(nbytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, ncPath.data(), nbytes, nullptr, nullptr);
        if (!ncPath.empty() && ncPath.back() == '\0') ncPath.pop_back();

        if (pp.writeNC(ncPath, gcode)) {
            std::wstring msg = std::wstring(L"NC file saved: ") + szFile;
            SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                reinterpret_cast<LPARAM>(msg.c_str()));
        } else {
            MessageBoxW(m_hwnd, L"Failed to write the NC file.",
                        L"Post-Processor", MB_OK | MB_ICONERROR);
        }
    } else {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Post-processing complete."));
    }
}

// --------------------------------------------------------------------------
void MainWindow::runVerify() {
    Verify v;

    // Show progress in status bar
    v.setProgressCallback([this](int pct) {
        std::wstring msg = L"Verify: " + std::to_wstring(pct) + L"%";
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(msg.c_str()));
    });

    VerifyResult result = v.run(m_toolpathMgr.get());

    // Feed the result to the Copilot engine so "Verify" button uses real data
    m_lastVerifyResult = result;
    if (m_copilotEngine)
        m_copilotEngine->setLastVerifyResult(&m_lastVerifyResult);

    std::wstring msg = L"Verify complete.";
    if (result.hasGouge) {
        char depthBuf[32];
        int written = std::snprintf(depthBuf, sizeof(depthBuf),
                                    "%.2f", result.maxGougeDepth);
        std::wstring depthStr(depthBuf,
                              depthBuf + (written > 0 ? written : 0));
        msg += L"  GOUGE detected ("
            + std::to_wstring(result.gougeCount) + L" cells, max depth "
            + depthStr + L" mm).";
    } else {
        msg += L"  No gouges detected.";
    }
    if (result.hasUndercut) {
        msg += L"  UNDERCUT: " + std::to_wstring(result.undercutCount) + L" cell(s).";
    }
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
void MainWindow::runBackplot() {
    Backplot bp;
    int moveCount = 0;
    bp.run(m_toolpathMgr.get(), [&moveCount](const BackplotMove&) {
        ++moveCount;
    });
    std::wstring msg = L"Backplot: "
        + std::to_wstring(moveCount) + L" moves across "
        + std::to_wstring(m_toolpathMgr->count()) + L" operation(s).";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
void MainWindow::runMachineSim() {
    MachineSimulation sim;
    CollisionResult result = sim.run(m_toolpathMgr.get());
    std::wstring msg = L"Machine Sim: ";
    if (result.hasCollision) {
        msg += L"COLLISION at move " + std::to_wstring(result.collisionMoveIdx)
            + L". " + std::wstring(result.description.begin(), result.description.end());
    } else if (result.hasOverTravel) {
        msg += L"OVER-TRAVEL at move " + std::to_wstring(result.overTravelMoveIdx) + L".";
    } else {
        msg += L"No collisions or over-travel detected.";
    }
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
void MainWindow::toggleCopilotPanel() {
    if (!m_copilotPanel) return;

    m_copilotVisible = !m_copilotVisible;
    ShowWindow(m_copilotPanel->hwnd(),
               m_copilotVisible ? SW_SHOW : SW_HIDE);

    // Re-run layout so the viewport resizes to accommodate the panel
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    updateLayout(rc.right, rc.bottom);

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(
            m_copilotVisible ? L"CAM Copilot panel opened."
                             : L"CAM Copilot panel closed."));
}

// ==========================================================================
// Input-dialog helpers
// ==========================================================================

// --------------------------------------------------------------------------
BRep::Solid* MainWindow::activeSolid() {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) return nullptr;
    return &m_solidsMgr->at(m_solidsMgr->count() - 1).solid;
}

// --------------------------------------------------------------------------
bool MainWindow::promptSingle(const wchar_t* title,
                               const wchar_t* label,
                               double defaultVal, double& outVal)
{
    SinglePromptState st{};
    std::swprintf(st.buf1, 64, L"%.6g", defaultVal);

    // Pre-fill label text and default value via WM_INITDIALOG in the proc
    // We use a local struct passed through LPARAM; the dialog proc reads it.
    // Because the RC-based dialog has static controls with empty text at
    // design time, we set label and default value during WM_INITDIALOG via
    // a custom two-field wrapper.
    struct InitData {
        SinglePromptState* state;
        const wchar_t*     label;
        const wchar_t*     title;
    } init { &st, label, title };

    auto proc = [](HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
        if (msg == WM_INITDIALOG) {
            auto* d = reinterpret_cast<InitData*>(lp);
            SetWindowLongPtrW(hdlg, DWLP_USER, reinterpret_cast<LONG_PTR>(d->state));
            SetWindowTextW(hdlg, d->title);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL1, d->label);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1,  d->state->buf1);
            SendDlgItemMessageW(hdlg, IDC_PROMPT_EDIT1, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hdlg, IDC_PROMPT_EDIT1));
            return FALSE;
        }
        return SinglePromptDlgProc(hdlg, msg, wp, lp);
    };

    HINSTANCE hInst = Application::instance().hInstance();
    INT_PTR r = DialogBoxParamW(hInst,
                                MAKEINTRESOURCEW(IDD_PROMPT_SINGLE),
                                m_hwnd,
                                proc,
                                reinterpret_cast<LPARAM>(&init));
    if (r == IDOK && st.accepted)
        return parseDouble(st.buf1, outVal);
    return false;
}

// --------------------------------------------------------------------------
bool MainWindow::promptDouble2(const wchar_t* title,
                                const wchar_t* label1, double defVal1, double& out1,
                                const wchar_t* label2, double defVal2, double& out2)
{
    DoublePromptState st{};
    std::swprintf(st.buf1, 64, L"%.6g", defVal1);
    std::swprintf(st.buf2, 64, L"%.6g", defVal2);

    struct InitData {
        DoublePromptState* state;
        const wchar_t*     label1;
        const wchar_t*     label2;
        const wchar_t*     title;
    } init { &st, label1, label2, title };

    auto proc = [](HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
        if (msg == WM_INITDIALOG) {
            auto* d = reinterpret_cast<InitData*>(lp);
            SetWindowLongPtrW(hdlg, DWLP_USER, reinterpret_cast<LONG_PTR>(d->state));
            SetWindowTextW(hdlg, d->title);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL1, d->label1);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1,  d->state->buf1);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL2, d->label2);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT2,  d->state->buf2);
            SendDlgItemMessageW(hdlg, IDC_PROMPT_EDIT1, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hdlg, IDC_PROMPT_EDIT1));
            return FALSE;
        }
        return DoublePromptDlgProc(hdlg, msg, wp, lp);
    };

    HINSTANCE hInst = Application::instance().hInstance();
    INT_PTR r = DialogBoxParamW(hInst,
                                MAKEINTRESOURCEW(IDD_PROMPT_DOUBLE),
                                m_hwnd,
                                proc,
                                reinterpret_cast<LPARAM>(&init));
    if (r == IDOK && st.accepted)
        return parseDouble(st.buf1, out1) && parseDouble(st.buf2, out2);
    return false;
}

// --------------------------------------------------------------------------
bool MainWindow::promptTriple(const wchar_t* title,
                               const wchar_t* label1, double defVal1, double& out1,
                               const wchar_t* label2, double defVal2, double& out2,
                               const wchar_t* label3, double defVal3, double& out3)
{
    TriplePromptState st{};
    std::swprintf(st.buf1, 64, L"%.6g", defVal1);
    std::swprintf(st.buf2, 64, L"%.6g", defVal2);
    std::swprintf(st.buf3, 64, L"%.6g", defVal3);

    struct InitData {
        TriplePromptState* state;
        const wchar_t*     label1;
        const wchar_t*     label2;
        const wchar_t*     label3;
        const wchar_t*     title;
    } init { &st, label1, label2, label3, title };

    auto proc = [](HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
        if (msg == WM_INITDIALOG) {
            auto* d = reinterpret_cast<InitData*>(lp);
            SetWindowLongPtrW(hdlg, DWLP_USER, reinterpret_cast<LONG_PTR>(d->state));
            SetWindowTextW(hdlg, d->title);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL1, d->label1);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT1,  d->state->buf1);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL2, d->label2);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT2,  d->state->buf2);
            SetDlgItemTextW(hdlg, IDC_PROMPT_LABEL3, d->label3);
            SetDlgItemTextW(hdlg, IDC_PROMPT_EDIT3,  d->state->buf3);
            SendDlgItemMessageW(hdlg, IDC_PROMPT_EDIT1, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hdlg, IDC_PROMPT_EDIT1));
            return FALSE;
        }
        return TriplePromptDlgProc(hdlg, msg, wp, lp);
    };

    HINSTANCE hInst = Application::instance().hInstance();
    INT_PTR r = DialogBoxParamW(hInst,
                                MAKEINTRESOURCEW(IDD_PROMPT_TRIPLE),
                                m_hwnd,
                                proc,
                                reinterpret_cast<LPARAM>(&init));
    if (r == IDOK && st.accepted)
        return parseDouble(st.buf1, out1)
            && parseDouble(st.buf2, out2)
            && parseDouble(st.buf3, out3);
    return false;
}

// ==========================================================================
// Solid creation (Solids tab)
// ==========================================================================

// --------------------------------------------------------------------------
// IDM_SOLID_EXTRUDE → create a parametric box and add it to SolidsManager
// --------------------------------------------------------------------------
void MainWindow::createSolidBox() {
    double dx = 100.0, dy = 50.0, dz = 25.0;
    if (!promptTriple(L"Create Box",
                      L"Length X (mm):", dx, dx,
                      L"Width  Y (mm):", dy, dy,
                      L"Height Z (mm):", dz, dz))
        return;

    if (dx <= 0 || dy <= 0 || dz <= 0) {
        MessageBoxW(m_hwnd, L"Dimensions must be positive.",
                    L"Create Box", MB_OK | MB_ICONWARNING);
        return;
    }

    BRep::Solid box = BRep::Solid::makeBox(dx, dy, dz);

    // Generate a unique name
    static int boxCount = 0;
    std::string name = "Box_" + std::to_string(++boxCount);
    box.setName(name);

    m_solidsMgr->addSolid(std::move(box));

    // Run feature recognition so Copilot can reason about the new solid
    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(m_solidsMgr->count() - 1).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    std::wstring msg = L"Box created: "
        + std::to_wstring(static_cast<int>(dx)) + L" × "
        + std::to_wstring(static_cast<int>(dy)) + L" × "
        + std::to_wstring(static_cast<int>(dz)) + L" mm";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
// IDM_SOLID_REVOLVE → create a parametric cylinder
// --------------------------------------------------------------------------
void MainWindow::createSolidCylinder() {
    double radius = 25.0, height = 50.0;
    if (!promptDouble2(L"Create Cylinder",
                       L"Radius (mm):", radius, radius,
                       L"Height (mm):", height, height))
        return;

    if (radius <= 0 || height <= 0) {
        MessageBoxW(m_hwnd, L"Radius and height must be positive.",
                    L"Create Cylinder", MB_OK | MB_ICONWARNING);
        return;
    }

    BRep::Solid cyl = BRep::Solid::makeCylinder(radius, height);

    static int cylCount = 0;
    std::string name = "Cylinder_" + std::to_string(++cylCount);
    cyl.setName(name);

    m_solidsMgr->addSolid(std::move(cyl));

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(m_solidsMgr->count() - 1).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Cylinder created: R=%.4g mm, H=%.4g mm", radius, height);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// Boolean solid operations – require at least 2 solids
// --------------------------------------------------------------------------
void MainWindow::solidBooleanOp(int commandId) {
    if (!m_solidsMgr || m_solidsMgr->count() < 2) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Boolean operations require at least two solids in the session."));
        return;
    }

    const wchar_t* opName = L"Boolean";
    switch (commandId) {
    case IDM_SOLID_UNION:     opName = L"Union";     break;
    case IDM_SOLID_SUBTRACT:  opName = L"Subtract";  break;
    case IDM_SOLID_INTERSECT: opName = L"Intersect"; break;
    case IDM_SOLID_FILLET:    opName = L"Fillet";    break;
    case IDM_SOLID_SHELL:     opName = L"Shell";     break;
    }

    // Inform user: full kernel-level boolean operations require a geometry
    // kernel (e.g. OCCT). For now, report the intent clearly.
    std::wstring msg = std::wstring(opName)
        + L": operation queued on solid pair ("
        + std::to_wstring(m_solidsMgr->count())
        + L" solids in session). Requires geometric kernel for execution.";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// ==========================================================================
// Wireframe primitive creation (Wireframe tab)
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::createWireframe(int commandId) {
    // Each wireframe command prompts for key parameters, creates a named
    // geometry entry, and adds it to a Level so feature recognition can use it.
    switch (commandId) {

    case IDM_WF_POINT: {
        double x = 0, y = 0, z = 0;
        if (!promptTriple(L"Create Point",
                          L"X (mm):", x, x,
                          L"Y (mm):", y, y,
                          L"Z (mm):", z, z)) return;
        // Add a named entry to the default level
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) {
                lv->entityCount++;
                m_levelsMgr->setEntityCount(1, lv->entityCount);
            }
        }
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Point created at (%.4g, %.4g, %.4g)", x, y, z);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_LINE: {
        double x1 = 0, y1 = 0, z1 = 0;
        double x2 = 100, y2 = 0, z2 = 0;
        if (!promptDouble2(L"Create Line – Start Point",
                           L"X (mm):", x1, x1,
                           L"Y (mm):", y1, y1)) return;
        if (!promptDouble2(L"Create Line – End Point",
                           L"X (mm):", x2, x2,
                           L"Y (mm):", y2, y2)) return;
        double len = std::sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) + (z2-z1)*(z2-z1));
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 1);
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Line created: (%.4g,%.4g)→(%.4g,%.4g), len=%.4g mm",
                      x1, y1, x2, y2, len);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_CIRCLE: {
        double cx = 0, cy = 0, r = 25.0;
        if (!promptTriple(L"Create Circle",
                          L"Centre X (mm):", cx, cx,
                          L"Centre Y (mm):", cy, cy,
                          L"Radius  (mm):", r,  r)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.", L"Create Circle", MB_OK | MB_ICONWARNING);
            return;
        }
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 1);
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Circle created: centre=(%.4g,%.4g), R=%.4g mm, C=%.4g mm",
                      cx, cy, r, 2.0 * kPi * r);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_ARC: {
        double cx = 0, cy = 0, r = 25.0;
        if (!promptTriple(L"Create Arc",
                          L"Centre X (mm):", cx, cx,
                          L"Centre Y (mm):", cy, cy,
                          L"Radius  (mm):", r,  r)) return;
        double startDeg = 0, endDeg = 90;
        if (!promptDouble2(L"Create Arc – Angles",
                           L"Start angle (°):", startDeg, startDeg,
                           L"End angle   (°):", endDeg,   endDeg)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.", L"Create Arc", MB_OK | MB_ICONWARNING);
            return;
        }
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 1);
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Arc created: R=%.4g mm, %.4g°→%.4g°", r, startDeg, endDeg);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_RECTANGLE: {
        double x = 0, y = 0, w = 100, h = 50;
        if (!promptDouble2(L"Create Rectangle – Origin",
                           L"X (mm):", x, x,
                           L"Y (mm):", y, y)) return;
        if (!promptDouble2(L"Create Rectangle – Size",
                           L"Width  (mm):", w, w,
                           L"Height (mm):", h, h)) return;
        if (w <= 0 || h <= 0) {
            MessageBoxW(m_hwnd, L"Width and height must be positive.",
                        L"Create Rectangle", MB_OK | MB_ICONWARNING);
            return;
        }
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 4); // 4 lines
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Rectangle: origin=(%.4g,%.4g), %.4g×%.4g mm", x, y, w, h);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_POLYGON: {
        double cx = 0, cy = 0, r = 25.0;
        double sides = 6;
        if (!promptTriple(L"Create Regular Polygon",
                          L"Centre X (mm):", cx, cx,
                          L"Centre Y (mm):", cy, cy,
                          L"Circumradius (mm):", r, r)) return;
        if (!promptSingle(L"Polygon Sides", L"Number of sides:", sides, sides)) return;
        int n = static_cast<int>(sides);
        if (n < 3) {
            MessageBoxW(m_hwnd, L"Polygon must have at least 3 sides.",
                        L"Create Polygon", MB_OK | MB_ICONWARNING);
            return;
        }
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + n);
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Regular polygon: %d sides, R=%.4g mm, centre=(%.4g,%.4g)",
                      n, r, cx, cy);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_SPLINE: {
        double numPts = 4;
        if (!promptSingle(L"Create Spline", L"Number of control points:", numPts, numPts)) return;
        int n = static_cast<int>(numPts);
        if (n < 2) {
            MessageBoxW(m_hwnd, L"Spline needs at least 2 control points.",
                        L"Create Spline", MB_OK | MB_ICONWARNING);
            return;
        }
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 1);
        }
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Spline created with %d control points.", n);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    default:
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Wireframe command: select geometry in the viewport."));
        break;
    }
}

// ==========================================================================
// Model Prep commands
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::prepHeal() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Heal: no solid in session. Import or create a solid first."));
        return;
    }

    // First analyse the model so we can report the number of issues found
    auto issues = ModelPrep::analyseModel(*s);
    int before = static_cast<int>(issues.size());

    // Apply healing (default tolerance 0.01 mm)
    *s = ModelPrep::healSurfaces(*s, 0.01);

    auto issuesAfter = ModelPrep::analyseModel(*s);
    int after = static_cast<int>(issuesAfter.size());
    int fixed  = before - after;

    if (m_viewport) m_viewport->redraw();

    wchar_t msg[256] = {};
    std::swprintf(msg, 256,
        L"Heal complete: %d issue(s) found, %d fixed, %d remaining.",
        before, fixed, after);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::prepRemoveFillet() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Remove Fillet: no solid in session."));
        return;
    }

    double limit = 0.5;
    if (!promptSingle(L"Remove Small Fillets",
                      L"Max fillet radius to remove (mm):", limit, limit))
        return;
    if (limit <= 0) limit = 0.5;

    int before = static_cast<int>(s->faces().size());
    *s = ModelPrep::removeFillets(*s, limit);
    int after  = static_cast<int>(s->faces().size());

    if (m_viewport) m_viewport->redraw();

    wchar_t msg[160] = {};
    std::swprintf(msg, 160,
        L"Remove Fillet: %d face(s) removed (threshold R≤%.4g mm). %d faces remain.",
        before - after, limit, after);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::prepBoundaries() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Boundaries: no solid in session."));
        return;
    }

    auto loops = ModelPrep::extractBoundaries(*s);
    int loopCount = static_cast<int>(loops.size());
    int ptTotal   = 0;
    for (const auto& loop : loops)
        ptTotal += static_cast<int>(loop.size());

    // Add each boundary loop as entities in the default level
    if (m_levelsMgr) {
        Level* lv = m_levelsMgr->findLevel(1);
        if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + loopCount);
    }

    wchar_t msg[160] = {};
    std::swprintf(msg, 160,
        L"Boundaries extracted: %d loop(s), %d point(s) total.",
        loopCount, ptTotal);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::prepClassify() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Classify: no solid in session."));
        return;
    }

    // Classify faces (sets isFloor/isWall/isHole flags on each face)
    ModelPrep::classifyFeatures(*s);

    // Run full feature recognition and feed results to Copilot
    FeatureRecognition fr;
    auto features = fr.recognise(*s);

    if (m_copilotEngine)
        m_copilotEngine->setRecognisedFeatures(features);

    // Count feature types
    int holes = 0, pockets = 0, bosses = 0, slots = 0;
    for (const auto& f : features) {
        switch (f.type) {
        case RecognizedFeature::Type::Hole:          ++holes;   break;
        case RecognizedFeature::Type::BlindPocket:
        case RecognizedFeature::Type::ThroughPocket: ++pockets; break;
        case RecognizedFeature::Type::Boss:          ++bosses;  break;
        case RecognizedFeature::Type::Slot:          ++slots;   break;
        default: break;
        }
    }

    wchar_t msg[256] = {};
    std::swprintf(msg, 256,
        L"Classify: %d feature(s) – %d hole(s), %d pocket(s), %d boss(es), %d slot(s).",
        static_cast<int>(features.size()), holes, pockets, bosses, slots);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::prepAnalyse() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Analyse: no solid in session."));
        return;
    }

    auto issues = ModelPrep::analyseModel(*s);

    if (issues.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Model analysis: no issues detected."));
        return;
    }

    // Build a summary and show it in a message box (too long for status bar)
    std::wstring detail;
    detail.reserve(issues.size() * 60);
    for (std::size_t i = 0; i < issues.size() && i < 20; ++i) {
        const auto& issue = issues[i];
        int wlen = MultiByteToWideChar(CP_UTF8, 0,
                                       issue.description.c_str(), -1, nullptr, 0);
        std::wstring wdesc(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, issue.description.c_str(), -1,
                            wdesc.data(), wlen);
        detail += L"• " + wdesc;
        if (issue.faceId >= 0)
            detail += L" (face " + std::to_wstring(issue.faceId) + L")";
        detail += L"\n";
    }
    if (issues.size() > 20)
        detail += L"… and " + std::to_wstring(issues.size() - 20) + L" more.";

    std::wstring caption = L"Model Analysis – "
        + std::to_wstring(issues.size()) + L" issue(s)";
    MessageBoxW(m_hwnd, detail.c_str(), caption.c_str(), MB_OK | MB_ICONINFORMATION);

    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Analysis: %d issue(s) found.", static_cast<int>(issues.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::prepSplit() {
    BRep::Solid* s = activeSolid();
    if (!s) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Split: no solid in session."));
        return;
    }

    // Prompt for the Z level of the splitting plane
    double zLevel = 0.0;
    {
        // Default to mid-height of the bounding box
        Geom::AABB bb = s->boundingBox();
        if (bb.isValid()) zLevel = (bb.min.z + bb.max.z) / 2.0;
    }

    if (!promptSingle(L"Split at Z Plane",
                      L"Z level (mm):", zLevel, zLevel)) return;

    // Extract the floor silhouette at the split level as boundary curves
    auto silhouette = ModelPrep::floorSilhouette(*s, zLevel);

    if (m_levelsMgr && !silhouette.empty()) {
        Level* lv = m_levelsMgr->findLevel(1);
        if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + 1);
    }

    wchar_t msg[160] = {};
    std::swprintf(msg, 160,
        L"Split at Z=%.4g mm: silhouette has %d point(s).",
        zLevel, static_cast<int>(silhouette.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// ==========================================================================
// Project serialisation  (CAMX text format)
// ==========================================================================
//
// Format:
//   CAMX 1.0
//   SOLID BOX name dx dy dz
//   SOLID CYLINDER name radius height
//   SOLID GENERIC name vertex_count edge_count face_count
//     V x y z           (vertex)
//     E v0 v1 curved    (edge)
//     F type nx ny nz edgeCount e0 e1 ...  (face)
//   NURBS name uDeg vDeg nu nv
//     KU k0 k1 ...       (U knot vector)
//     KV k0 k1 ...       (V knot vector)
//     CP x y z w         (control point + weight, row-major: u outer, v inner)
//   TOOLPATH name strategy pointCount
//     P motionCode x y z ax ay az
//   END
// ==========================================================================

static std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(static_cast<std::size_t>(n > 0 ? n - 1 : 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), n);
    return ws;
}

// --------------------------------------------------------------------------
void MainWindow::saveProjectCamx(const std::wstring& wpath) {
    std::string path = wideToUtf8(wpath);
    std::ofstream f(path);
    if (!f.is_open()) {
        MessageBoxW(m_hwnd, L"Failed to open file for writing.",
                    L"Save Project", MB_OK | MB_ICONERROR);
        return;
    }

    f << "CAMX 1.0\n";
    f << std::fixed;
    f.precision(6);

    // --- Solids ---
    if (m_solidsMgr) {
        for (int i = 0; i < m_solidsMgr->count(); ++i) {
            const BRep::Solid& s = m_solidsMgr->at(i).solid;
            const std::string& nm = s.name().empty() ? "Solid" : s.name();

            // Write all vertices, edges, faces generically
            f << "SOLID GENERIC " << nm
              << " " << s.vertices().size()
              << " " << s.edges().size()
              << " " << s.faces().size() << "\n";

            for (const auto& v : s.vertices())
                f << " V " << v.point.x << " " << v.point.y << " " << v.point.z << "\n";

            for (const auto& e : s.edges())
                f << " E " << e.startVertexId << " " << e.endVertexId
                  << " " << (e.isCurved ? 1 : 0) << "\n";

            for (const auto& face : s.faces()) {
                f << " F " << static_cast<int>(face.type)
                  << " " << face.normal.x << " " << face.normal.y << " " << face.normal.z
                  << " " << face.edgeIds.size();
                for (int eid : face.edgeIds) f << " " << eid;
                f << "\n";
            }
        }
    }

    // --- NURBS Surfaces ---
    if (m_surfacesMgr) {
        for (int i = 0; i < m_surfacesMgr->count(); ++i) {
            const SurfaceEntry& se = m_surfacesMgr->at(i);
            const NurbsSurface& s  = se.surface;
            const std::string&  nm = se.name.empty() ? "Surface" : se.name;

            int nu = s.numCtrlU(), nv = s.numCtrlV();
            // Count knot vectors (degree+1+controlPoints-1 = degree+controlPoints)
            // They are stored in the surface; we write them out explicitly.
            f << "NURBS " << nm
              << " " << s.uDegree() << " " << s.vDegree()
              << " " << nu << " " << nv << "\n";

            // U knots
            f << " KU";
            double du = (s.uMax() - s.uMin()) / std::max(1, nu - s.uDegree());
            // Reconstruct a clamped uniform knot vector from the degree and count
            {
                int degU = s.uDegree();
                int nKnotsU = nu + degU + 1;
                double uMin = s.uMin(), uMax = s.uMax();
                for (int k = 0; k < nKnotsU; ++k) {
                    double t;
                    if (k <= degU)              t = uMin;
                    else if (k >= nKnotsU - degU - 1) t = uMax;
                    else t = uMin + (uMax - uMin) * (k - degU) / (nKnotsU - 2 * degU - 1);
                    f << " " << t;
                }
            }
            f << "\n";

            // V knots
            f << " KV";
            {
                int degV = s.vDegree();
                int nKnotsV = nv + degV + 1;
                double vMin = s.vMin(), vMax = s.vMax();
                for (int k = 0; k < nKnotsV; ++k) {
                    double t;
                    if (k <= degV)              t = vMin;
                    else if (k >= nKnotsV - degV - 1) t = vMax;
                    else t = vMin + (vMax - vMin) * (k - degV) / (nKnotsV - 2 * degV - 1);
                    f << " " << t;
                }
            }
            f << "\n";

            // Control points and weights (sample on the parameter grid)
            double uStep = (nu > 1) ? (s.uMax() - s.uMin()) / (nu - 1) : 0.0;
            double vStep = (nv > 1) ? (s.vMax() - s.vMin()) / (nv - 1) : 0.0;
            for (int ui = 0; ui < nu; ++ui) {
                double u = s.uMin() + ui * uStep;
                for (int vi = 0; vi < nv; ++vi) {
                    double v = s.vMin() + vi * vStep;
                    Geom::Vec3 p = s.evaluate(u, v);
                    f << " CP " << p.x << " " << p.y << " " << p.z << " 1.0\n";
                }
            }
        }
    }

    // --- Toolpaths ---
    if (m_toolpathMgr) {
        for (int i = 0; i < m_toolpathMgr->count(); ++i) {
            const Toolpath& tp = m_toolpathMgr->at(i);
            const std::string& nm = tp.name().empty() ? "Operation" : tp.name();
            f << "TOOLPATH " << nm
              << " " << static_cast<int>(tp.strategy())
              << " " << tp.points().size() << "\n";
            for (const auto& pt : tp.points()) {
                f << " P " << static_cast<int>(pt.motion)
                  << " " << pt.position.x << " " << pt.position.y << " " << pt.position.z
                  << " " << pt.toolAxis.x << " " << pt.toolAxis.y << " " << pt.toolAxis.z
                  << "\n";
            }
        }
    }

    f << "END\n";

    m_currentFile = wpath;
    updateWindowTitle();
    std::wstring msg = L"Project saved: " + wpath;
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
void MainWindow::loadProjectCamx(const std::wstring& wpath) {
    std::string path = wideToUtf8(wpath);
    std::ifstream f(path);
    if (!f.is_open()) {
        MessageBoxW(m_hwnd, L"Failed to open file for reading.",
                    L"Open Project", MB_OK | MB_ICONERROR);
        return;
    }

    // Clear existing session data
    if (m_toolpathMgr)  m_toolpathMgr->clear();
    if (m_solidsMgr)    m_solidsMgr->clear();
    if (m_surfacesMgr)  m_surfacesMgr->clear();
    if (m_levelsMgr)    m_levelsMgr->clear();

    std::string line;
    if (!std::getline(f, line) || line.substr(0, 4) != "CAMX") {
        MessageBoxW(m_hwnd, L"Not a valid CAM-Expert project file (missing CAMX header).",
                    L"Open Project", MB_OK | MB_ICONERROR);
        return;
    }

    int solidsLoaded = 0, toolpathsLoaded = 0, surfacesLoaded = 0;

    while (std::getline(f, line)) {
        if (line == "END") break;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "SOLID") {
            std::string kind, name;
            ss >> kind >> name;
            BRep::Solid solid;
            solid.setName(name);

            if (kind == "GENERIC") {
                std::size_t vCount, eCount, fCount;
                ss >> vCount >> eCount >> fCount;

                // Read vertices
                for (std::size_t vi = 0; vi < vCount; ++vi) {
                    if (!std::getline(f, line)) break;
                    std::istringstream vs(line);
                    std::string tag;
                    double x, y, z;
                    vs >> tag >> x >> y >> z;
                    solid.addVertex({x, y, z});
                }
                // Read edges
                for (std::size_t ei = 0; ei < eCount; ++ei) {
                    if (!std::getline(f, line)) break;
                    std::istringstream es(line);
                    std::string tag;
                    int v0, v1, curved;
                    es >> tag >> v0 >> v1 >> curved;
                    solid.addEdge(v0, v1, curved != 0);
                }
                // Read faces
                for (std::size_t fi = 0; fi < fCount; ++fi) {
                    if (!std::getline(f, line)) break;
                    std::istringstream fs(line);
                    std::string tag;
                    int ftype;
                    double nx, ny, nz;
                    std::size_t ec;
                    fs >> tag >> ftype >> nx >> ny >> nz >> ec;
                    std::vector<int> eids;
                    eids.reserve(ec);
                    for (std::size_t k = 0; k < ec; ++k) {
                        int eid;
                        fs >> eid;
                        eids.push_back(eid);
                    }
                    solid.addFace(static_cast<BRep::FaceType>(ftype), eids, {nx, ny, nz});
                }
            }

            m_solidsMgr->addSolid(std::move(solid));
            ++solidsLoaded;

        } else if (token == "NURBS") {
            // NURBS name uDeg vDeg nu nv
            std::string name;
            int uDeg, vDeg, nu, nv;
            ss >> name >> uDeg >> vDeg >> nu >> nv;

            // Read KU line
            std::vector<double> knotsU, knotsV;
            if (std::getline(f, line)) {
                std::istringstream ku(line);
                std::string tag; ku >> tag; // "KU"
                double k;
                while (ku >> k) knotsU.push_back(k);
            }
            // Read KV line
            if (std::getline(f, line)) {
                std::istringstream kv(line);
                std::string tag; kv >> tag; // "KV"
                double k;
                while (kv >> k) knotsV.push_back(k);
            }
            // Read control points: nu × nv CP lines
            std::vector<std::vector<Geom::Vec3>> cp(nu, std::vector<Geom::Vec3>(nv));
            std::vector<std::vector<double>>     wt(nu, std::vector<double>(nv, 1.0));
            for (int ui = 0; ui < nu; ++ui) {
                for (int vi = 0; vi < nv; ++vi) {
                    if (!std::getline(f, line)) break;
                    std::istringstream ps(line);
                    std::string tag; double x, y, z, w;
                    ps >> tag >> x >> y >> z >> w;
                    cp[ui][vi] = {x, y, z};
                    wt[ui][vi] = w;
                }
            }

            if (!knotsU.empty() && !knotsV.empty()) {
                NurbsSurface surf(uDeg, vDeg,
                                  std::move(knotsU), std::move(knotsV),
                                  std::move(cp), std::move(wt));
                m_surfacesMgr->addSurface(std::move(surf), name);
                ++surfacesLoaded;
            }

        } else if (token == "TOOLPATH") {
            std::string name;
            int strategy;
            std::size_t ptCount;
            ss >> name >> strategy >> ptCount;

            CuttingTool tool;
            CuttingParams params;
            Toolpath tp(static_cast<StrategyType>(strategy), tool, params);
            tp.setName(name);

            for (std::size_t pi = 0; pi < ptCount; ++pi) {
                if (!std::getline(f, line)) break;
                std::istringstream ps(line);
                std::string tag;
                int motCode;
                double px, py, pz, ax, ay, az;
                ps >> tag >> motCode >> px >> py >> pz >> ax >> ay >> az;
                ToolpathPoint pt;
                pt.position = {px, py, pz};
                pt.toolAxis = {ax, ay, az};
                pt.motion   = static_cast<MotionType>(motCode);
                tp.addPoint(pt);
            }

            m_toolpathMgr->addToolpath(std::move(tp));
            ++toolpathsLoaded;
        }
    }

    if (m_viewport) m_viewport->redraw();

    m_currentFile = wpath;
    updateWindowTitle();

    wchar_t msg[256] = {};
    std::swprintf(msg, 256,
        L"Loaded: %d solid(s), %d surface(s), %d toolpath(s)  ←  %ls",
        solidsLoaded, surfacesLoaded, toolpathsLoaded, wpath.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// ==========================================================================
// Solid creation – Sphere
// ==========================================================================

// --------------------------------------------------------------------------
// IDM_SOLID_SPHERE → create a parametric UV sphere
// --------------------------------------------------------------------------
void MainWindow::createSolidSphere() {
    double radius = 25.0;
    if (!promptSingle(L"Create Sphere",
                      L"Radius (mm):", radius, radius))
        return;

    if (radius <= 0) {
        MessageBoxW(m_hwnd, L"Radius must be positive.",
                    L"Create Sphere", MB_OK | MB_ICONWARNING);
        return;
    }

    BRep::Solid sphere = BRep::Solid::makeSphere(radius);

    static int sphereCount = 0;
    std::string name = "Sphere_" + std::to_string(++sphereCount);
    sphere.setName(name);

    m_solidsMgr->addSolid(std::move(sphere));

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(m_solidsMgr->count() - 1).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    wchar_t statusMsg[128] = {};
    std::swprintf(statusMsg, 128, L"Sphere created: R=%.4g mm", radius);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// ==========================================================================
// CAM toolpath generation (Machine tab)
// ==========================================================================

// --------------------------------------------------------------------------
// Generate a 2D dynamic pocket roughing toolpath from the active solid's
// bounding box (or a default boundary if no solid is loaded).
// --------------------------------------------------------------------------
void MainWindow::generateToolpathPocket() {
    double toolDiam = 12.0, depth = 10.0;
    if (!promptDouble2(L"Generate 2D Pocket",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Pocket depth   (mm):", depth,    depth))
        return;

    if (toolDiam <= 0 || depth <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter and depth must be positive.",
                    L"Generate 2D Pocket", MB_OK | MB_ICONWARNING);
        return;
    }

    // Build a boundary from the active solid or fall back to a default
    std::vector<Geom::Vec2> boundary;
    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        Geom::AABB bb = m_solidsMgr->aggregateBoundingBox();
        if (bb.isValid()) {
            boundary = {
                {bb.min.x, bb.min.y},
                {bb.max.x, bb.min.y},
                {bb.max.x, bb.max.y},
                {bb.min.x, bb.max.y}
            };
        }
    }
    if (boundary.empty()) {
        boundary = { {-50,  -50}, { 50, -50}, { 50,  50}, {-50,  50} };
    }

    CuttingTool tool;
    tool.diameter    = toolDiam;
    tool.fluteLength = depth;

    CuttingParams params;
    params.feedRate     = 800.0;
    params.plungeRate   = 200.0;
    params.spindleRPM   = 8000;

    DynamicMotion dm;
    Toolpath tp = dm.generatePocketRough(boundary, depth, tool, params);

    static int pocketCount = 0;
    tp.setName("DynPocket_" + std::to_string(++pocketCount));

    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[160] = {};
    std::swprintf(statusMsg, 160,
        L"Pocket toolpath generated: Ø%.4g mm, depth %.4g mm.",
        toolDiam, depth);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Generate a 2D dynamic contour toolpath around the active solid's
// bounding-box perimeter.
// --------------------------------------------------------------------------
void MainWindow::generateToolpathContour() {
    double toolDiam = 10.0, depth = 5.0;
    if (!promptDouble2(L"Generate 2D Contour",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Cut depth      (mm):", depth,    depth))
        return;

    if (toolDiam <= 0 || depth <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter and depth must be positive.",
                    L"Generate 2D Contour", MB_OK | MB_ICONWARNING);
        return;
    }

    // Build a profile from the active solid or fall back to a default
    std::vector<Geom::Vec2> profile;
    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        Geom::AABB bb = m_solidsMgr->aggregateBoundingBox();
        if (bb.isValid()) {
            profile = {
                {bb.min.x, bb.min.y},
                {bb.max.x, bb.min.y},
                {bb.max.x, bb.max.y},
                {bb.min.x, bb.max.y},
                {bb.min.x, bb.min.y}   // close the loop
            };
        }
    }
    if (profile.empty()) {
        profile = { {-50,-50}, {50,-50}, {50,50}, {-50,50}, {-50,-50} };
    }

    CuttingTool tool;
    tool.diameter    = toolDiam;
    tool.fluteLength = depth;

    CuttingParams params;
    params.feedRate     = 1000.0;
    params.plungeRate   = 200.0;
    params.spindleRPM   = 10000;

    DynamicMotion dm;
    Toolpath tp = dm.generateContour(profile, depth, tool, params);

    static int contourCount = 0;
    tp.setName("DynContour_" + std::to_string(++contourCount));

    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[160] = {};
    std::swprintf(statusMsg, 160,
        L"Contour toolpath generated: Ø%.4g mm, depth %.4g mm.",
        toolDiam, depth);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Regenerate all toolpaths (marks them clean and triggers a viewport refresh)
// --------------------------------------------------------------------------
void MainWindow::regenerateAllToolpaths() {
    if (!m_toolpathMgr || m_toolpathMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Regenerate: no toolpaths in session."));
        return;
    }

    m_toolpathMgr->regenerateAll();

    if (m_viewport) m_viewport->redraw();

    wchar_t statusMsg[128] = {};
    std::swprintf(statusMsg, 128,
        L"Regenerated %d toolpath(s).", m_toolpathMgr->count());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Show machining summary: operation count, total path length, estimated time
// --------------------------------------------------------------------------
void MainWindow::showMachiningSummary() {
    if (!m_toolpathMgr) return;

    int opCount = m_toolpathMgr->count();
    if (opCount == 0) {
        MessageBoxW(m_hwnd, L"No toolpaths in the current session.",
                    L"Machining Summary", MB_OK | MB_ICONINFORMATION);
        return;
    }

    double totalLen  = m_toolpathMgr->totalPathLength();
    double totalTime = m_toolpathMgr->totalMachiningTime();  // minutes

    // Build per-operation table as wide string
    std::wstring detail;
    detail.reserve(static_cast<std::size_t>(opCount) * 80);
    detail += L"  #   Operation                       Length (mm)  Time (min)\n";
    detail += L"  ─────────────────────────────────────────────────────────\n";

    for (int i = 0; i < opCount; ++i) {
        const Toolpath& tp = m_toolpathMgr->at(i);
        const std::string& nm = tp.name().empty() ? "Operation" : tp.name();

        // Convert operation name to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, nm.c_str(), -1, nullptr, 0);
        std::wstring wname(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, nm.c_str(), -1, wname.data(), wlen);

        wchar_t row[256] = {};
        std::swprintf(row, 256, L"  %2d. %-30ls  %10.1f   %8.2f\n",
            i + 1, wname.c_str(), tp.totalLength(), tp.estimatedTime());
        detail += row;
    }

    wchar_t header[256] = {};
    std::swprintf(header, 256,
        L"Operations:   %d\n"
        L"Total length: %.1f mm  (%.3f m)\n"
        L"Est. time:    %.2f min  (%.0f sec)\n\n",
        opCount,
        totalLen, totalLen / 1000.0,
        totalTime, totalTime * 60.0);

    std::wstring fullMsg = header;
    fullMsg += detail;

    MessageBoxW(m_hwnd, fullMsg.c_str(), L"Machining Summary", MB_OK | MB_ICONINFORMATION);

    wchar_t statusMsg[128] = {};
    std::swprintf(statusMsg, 128,
        L"Summary: %d op(s), %.1f mm total, %.2f min est.",
        opCount, totalLen, totalTime);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Update the title bar to reflect the current file (or "Untitled")
// --------------------------------------------------------------------------
void MainWindow::updateWindowTitle() {
    std::wstring title = std::wstring(Application::APP_NAME);
    if (!m_currentFile.empty()) {
        // Extract just the file name portion for brevity
        std::size_t sep = m_currentFile.find_last_of(L"\\/");
        std::wstring fname = (sep != std::wstring::npos)
                             ? m_currentFile.substr(sep + 1)
                             : m_currentFile;
        title += L" – " + fname;
    } else {
        title += L" – Untitled";
    }
    SetWindowTextW(m_hwnd, title.c_str());
}

// ==========================================================================
// Surface operations (Surfaces tab)
// ==========================================================================

static constexpr double kSurfPi = 3.14159265358979323846;

// --------------------------------------------------------------------------
// IDM_SURF_LOFT – create a ruled loft surface through two parallel profiles
// --------------------------------------------------------------------------
void MainWindow::surfaceLoft()
{
    if (!m_surfacesMgr) return;

    double width = 100.0, depth = 60.0, height = 40.0;
    if (!promptTriple(L"Create Loft Surface",
                      L"Profile width  (mm):", width,  width,
                      L"Profile depth  (mm):", depth,  depth,
                      L"Sweep height   (mm):", height, height))
        return;

    if (width <= 0 || depth <= 0 || height <= 0) {
        MessageBoxW(m_hwnd, L"Dimensions must be positive.",
                    L"Create Loft Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Two rectangular cross-sections: bottom at Z=0, top at Z=height
    // with the top section scaled to 80% to give a slight taper.
    const double scale = 0.8;
    std::vector<std::vector<Geom::Vec3>> sections = {
        // Bottom profile (Z = 0)
        { {-width/2, -depth/2, 0}, {width/2, -depth/2, 0},
          {width/2,  depth/2, 0},  {-width/2, depth/2, 0} },
        // Top profile (Z = height, scaled)
        { {-width*scale/2, -depth*scale/2, height},
          { width*scale/2, -depth*scale/2, height},
          { width*scale/2,  depth*scale/2, height},
          {-width*scale/2,  depth*scale/2, height} }
    };

    static int loftCount = 0;
    std::string name = "Loft_" + std::to_string(++loftCount);

    NurbsSurface surf = SurfacesManager::makeLoft(sections);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Loft surface created: %.4g × %.4g mm, sweep %.4g mm  [%hs]",
                  width, depth, height, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_REVOLVE – create a surface of revolution
// --------------------------------------------------------------------------
void MainWindow::surfaceRevolve()
{
    if (!m_surfacesMgr) return;

    double radius = 30.0, height = 80.0, angle = 360.0;
    if (!promptTriple(L"Create Revolution Surface",
                      L"Profile radius (mm):", radius, radius,
                      L"Profile height (mm):", height, height,
                      L"Sweep angle   (deg):", angle,  angle))
        return;

    if (radius <= 0 || height <= 0 || std::abs(angle) < 1.0) {
        MessageBoxW(m_hwnd, L"Radius and height must be positive; angle must be non-zero.",
                    L"Create Revolution Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Straight line profile in the XZ-plane from (radius,0,0) to (radius,0,height)
    const int profilePts = 6;
    std::vector<Geom::Vec3> profile;
    profile.reserve(static_cast<std::size_t>(profilePts));
    for (int i = 0; i < profilePts; ++i) {
        double t = static_cast<double>(i) / (profilePts - 1);
        profile.push_back({ radius, 0.0, t * height });
    }

    static int revCount = 0;
    std::string name = "Revolve_" + std::to_string(++revCount);

    NurbsSurface surf = SurfacesManager::makeRevolve(profile, angle);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Revolution surface created: R=%.4g mm, H=%.4g mm, %.4g°  [%hs]",
                  radius, height, angle, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_FILLET – create a fillet blend surface between two existing surfaces
// --------------------------------------------------------------------------
void MainWindow::surfaceFillet()
{
    if (!m_surfacesMgr) return;

    if (m_surfacesMgr->count() < 2) {
        MessageBoxW(m_hwnd,
            L"A fillet requires at least two existing surfaces.\n"
            L"Create two surfaces first (e.g. Loft and Revolve).",
            L"Surface Fillet", MB_OK | MB_ICONINFORMATION);
        return;
    }

    double filletR = 5.0;
    if (!promptSingle(L"Surface Fillet",
                      L"Fillet radius (mm):", filletR, filletR))
        return;

    if (filletR <= 0) {
        MessageBoxW(m_hwnd, L"Fillet radius must be positive.",
                    L"Surface Fillet", MB_OK | MB_ICONWARNING);
        return;
    }

    const NurbsSurface& s1 = m_surfacesMgr->at(m_surfacesMgr->count() - 2).surface;
    const NurbsSurface& s2 = m_surfacesMgr->at(m_surfacesMgr->count() - 1).surface;

    static int filletCount = 0;
    std::string name = "Fillet_" + std::to_string(++filletCount);

    NurbsSurface fillet = SurfacesManager::makeFillet(s1, s2, filletR);
    m_surfacesMgr->addSurface(std::move(fillet), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Fillet surface created: R=%.4g mm  [%hs]",
                  filletR, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_OFFSET – offset the currently selected / most recent surface
// --------------------------------------------------------------------------
void MainWindow::surfaceOffset()
{
    if (!m_surfacesMgr || m_surfacesMgr->count() == 0) {
        MessageBoxW(m_hwnd, L"No surfaces in the session. Create a surface first.",
                    L"Surface Offset", MB_OK | MB_ICONINFORMATION);
        return;
    }

    double dist = 5.0;
    if (!promptSingle(L"Offset Surface",
                      L"Offset distance (mm):", dist, dist))
        return;

    if (std::abs(dist) < 1e-6) {
        MessageBoxW(m_hwnd, L"Offset distance must be non-zero.",
                    L"Surface Offset", MB_OK | MB_ICONWARNING);
        return;
    }

    const NurbsSurface& src = m_surfacesMgr->at(m_surfacesMgr->count() - 1).surface;

    static int offsetCount = 0;
    std::string name = "Offset_" + std::to_string(++offsetCount);

    NurbsSurface offset = SurfacesManager::makeOffset(src, dist);
    m_surfacesMgr->addSurface(std::move(offset), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Offset surface created: d=%.4g mm  [%hs]",
                  dist, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_TRIM – mark the active surface as trimmed
// --------------------------------------------------------------------------
void MainWindow::surfaceTrim()
{
    if (!m_surfacesMgr || m_surfacesMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Trim: no surfaces in session. Create a surface first."));
        return;
    }

    int idx = m_surfacesMgr->count() - 1;
    if (m_surfacesMgr->at(idx).trimmed) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Surface is already trimmed. Use Untrim to remove."));
        return;
    }

    m_surfacesMgr->setTrimmed(idx, true);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Surface \"%hs\" marked as trimmed.",
                  m_surfacesMgr->at(idx).name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_UNTRIM – remove trim state from the active surface
// --------------------------------------------------------------------------
void MainWindow::surfaceUntrim()
{
    if (!m_surfacesMgr || m_surfacesMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Untrim: no surfaces in session."));
        return;
    }

    int idx = m_surfacesMgr->count() - 1;
    if (!m_surfacesMgr->at(idx).trimmed) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Surface has no trim to remove."));
        return;
    }

    m_surfacesMgr->setTrimmed(idx, false);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Surface \"%hs\" untrimmed (full parameter domain restored).",
                  m_surfacesMgr->at(idx).name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_EXTEND – extend the active surface in the U-direction
// --------------------------------------------------------------------------
void MainWindow::surfaceExtend()
{
    if (!m_surfacesMgr || m_surfacesMgr->count() == 0) {
        MessageBoxW(m_hwnd, L"No surfaces in session. Create a surface first.",
                    L"Extend Surface", MB_OK | MB_ICONINFORMATION);
        return;
    }

    double extDist = 10.0;
    if (!promptSingle(L"Extend Surface",
                      L"Extension distance (mm):", extDist, extDist))
        return;

    if (extDist <= 0) {
        MessageBoxW(m_hwnd, L"Extension distance must be positive.",
                    L"Extend Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    const NurbsSurface& src = m_surfacesMgr->at(m_surfacesMgr->count() - 1).surface;

    static int extCount = 0;
    std::string name = "Extend_" + std::to_string(++extCount);

    NurbsSurface extended = SurfacesManager::makeExtend(src, extDist, 1 /*extend at uMax*/);
    m_surfacesMgr->addSurface(std::move(extended), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Surface extended by %.4g mm  [%hs]",
                  extDist, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// ==========================================================================
// Additional CAM toolpath generators (Machine tab)
// ==========================================================================

// --------------------------------------------------------------------------
// Generate a chamfer milling toolpath along the active solid's boundary
// --------------------------------------------------------------------------
void MainWindow::generateToolpathChamfer()
{
    double chamferW = 1.0, toolDiam = 10.0;
    if (!promptDouble2(L"Generate Chamfer",
                       L"Chamfer width  (mm):", chamferW, chamferW,
                       L"Tool diameter  (mm):", toolDiam, toolDiam))
        return;

    if (chamferW <= 0 || toolDiam <= 0) {
        MessageBoxW(m_hwnd, L"Chamfer width and tool diameter must be positive.",
                    L"Generate Chamfer", MB_OK | MB_ICONWARNING);
        return;
    }

    // Build profile from active solid boundary or default
    std::vector<Geom::Vec2> profile;
    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        Geom::AABB bb = m_solidsMgr->aggregateBoundingBox();
        if (bb.isValid()) {
            profile = { {bb.min.x, bb.min.y}, {bb.max.x, bb.min.y},
                        {bb.max.x, bb.max.y}, {bb.min.x, bb.max.y},
                        {bb.min.x, bb.min.y} };
        }
    }
    if (profile.empty())
        profile = { {-50,-50}, {50,-50}, {50,50}, {-50,50}, {-50,-50} };

    CuttingTool tool;
    tool.type     = ToolType::EndMill;
    tool.diameter = toolDiam;

    CuttingParams params;
    params.feedRate   = 600.0;
    params.spindleRPM = 8000;

    Strategies2D::ChamferParams cp;
    cp.chamferWidth = chamferW;
    cp.chamferAngle = 45.0;

    Toolpath tp = Strategies2D::chamfer(profile, cp, tool, params);

    static int chamferCount = 0;
    tp.setName("Chamfer_" + std::to_string(++chamferCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[160] = {};
    std::swprintf(statusMsg, 160,
        L"Chamfer toolpath generated: %.4g mm × 45°, Ø%.4g tool.",
        chamferW, toolDiam);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Generate a thread mill toolpath at a user-specified position
// --------------------------------------------------------------------------
void MainWindow::generateToolpathThread()
{
    double cx = 0.0, pitch = 1.25, majDia = 12.0;
    if (!promptTriple(L"Thread Mill",
                      L"Centre X (mm):",  cx,     cx,
                      L"Thread pitch (mm):", pitch, pitch,
                      L"Major diameter (mm):", majDia, majDia))
        return;

    if (pitch <= 0 || majDia <= 0) {
        MessageBoxW(m_hwnd, L"Pitch and major diameter must be positive.",
                    L"Thread Mill", MB_OK | MB_ICONWARNING);
        return;
    }

    CuttingTool tool;
    tool.type     = ToolType::EndMill;
    tool.diameter = majDia * 0.5; // thread mill Ø ≈ 50% of major diameter

    CuttingParams params;
    params.feedRate   = 300.0;
    params.spindleRPM = 6000;

    Strategies2D::ThreadMillParams tmp;
    tmp.pitchMM       = pitch;
    tmp.majorDiameter = majDia;
    tmp.internal      = true;
    tmp.passes        = 1;

    Toolpath tp = Strategies2D::threadMill({cx, 0.0}, 0.0, tmp, tool, params);

    static int threadCount = 0;
    tp.setName("ThreadMill_" + std::to_string(++threadCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[160] = {};
    std::swprintf(statusMsg, 160,
        L"Thread mill toolpath: M%.4g×%.4g pitch, centre X=%.4g mm.",
        majDia, pitch, cx);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Probing cycle – Z-surface probe
// --------------------------------------------------------------------------
void MainWindow::probeZSurface()
{
    double cx = 0.0, cy = 0.0, expectedZ = 0.0;
    if (!promptTriple(L"Probe Z Surface",
                      L"Probe X (mm):", cx, cx,
                      L"Probe Y (mm):", cy, cy,
                      L"Expected Z (mm):", expectedZ, expectedZ))
        return;

    ProbeParams pp;
    pp.safeZ = expectedZ + 10.0;

    ProbeResult result = ProbingCycles::zSurface({cx, cy}, expectedZ, pp);

    static int probeCount = 0;
    result.motionPath.setName("ProbeZ_" + std::to_string(++probeCount));
    m_toolpathMgr->addToolpath(std::move(result.motionPath));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"Z-surface probe added at (%.4g, %.4g). G-code: %d chars.",
        cx, cy, static_cast<int>(result.gcode.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Probing cycle – bore/boss center-finder
// --------------------------------------------------------------------------
void MainWindow::probeBoreCenter()
{
    double cx = 0.0, cy = 0.0, approxR = 15.0;
    if (!promptTriple(L"Probe Bore / Boss Center",
                      L"Approx. centre X (mm):", cx, cx,
                      L"Approx. centre Y (mm):", cy, cy,
                      L"Approx. radius   (mm):", approxR, approxR))
        return;

    if (approxR <= 0) {
        MessageBoxW(m_hwnd, L"Approximate radius must be positive.",
                    L"Probe Bore Center", MB_OK | MB_ICONWARNING);
        return;
    }

    ProbeParams pp;
    ProbeResult result = ProbingCycles::bore({cx, cy}, approxR, -5.0, 4, pp);

    static int boreProbeCount = 0;
    result.motionPath.setName("ProbeBore_" + std::to_string(++boreProbeCount));
    m_toolpathMgr->addToolpath(std::move(result.motionPath));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"Bore-center probe added: approx. (%.4g, %.4g) R=%.4g mm.",
        cx, cy, approxR);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// Probing cycle – corner finder
// --------------------------------------------------------------------------
void MainWindow::probeCorner()
{
    double cx = 0.0, cy = 0.0, xSize = 100.0;
    if (!promptTriple(L"Probe Corner",
                      L"Approx. corner X (mm):", cx,    cx,
                      L"Approx. corner Y (mm):", cy,    cy,
                      L"Stock X size (mm):",     xSize, xSize))
        return;

    ProbeParams pp;
    ProbeResult result = ProbingCycles::cornerFinder({cx, cy}, xSize, xSize, -5.0, pp);

    static int cornerProbeCount = 0;
    result.motionPath.setName("ProbeCorner_" + std::to_string(++cornerProbeCount));
    m_toolpathMgr->addToolpath(std::move(result.motionPath));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"Corner probe added at approx. (%.4g, %.4g).",
        cx, cy);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// ==========================================================================
// 3D Milling strategies (Machine tab → 3D group)
// ==========================================================================

// --------------------------------------------------------------------------
// Helper: get the active NURBS surface or nullptr
// --------------------------------------------------------------------------
static const NurbsSurface* getActiveSurface(const SurfacesManager* mgr) {
    if (!mgr || mgr->count() == 0) return nullptr;
    return &mgr->at(mgr->count() - 1).surface;
}

// --------------------------------------------------------------------------
// IDM_MACHINE_3D_WATERLINE – Z-level waterline finishing on active surface
// --------------------------------------------------------------------------
void MainWindow::generate3DWaterline()
{
    double toolDiam = 12.0, topZ = 0.0, bottomZ = -30.0, zStep = 1.0;
    if (!promptDouble2(L"3D Waterline",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Z step (mm):",        zStep,    zStep))
        return;

    if (toolDiam <= 0 || zStep <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter and Z-step must be positive.",
                    L"3D Waterline", MB_OK | MB_ICONWARNING);
        return;
    }

    // Determine Z range from active solid bounding box or use defaults
    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        Geom::AABB bb = m_solidsMgr->aggregateBoundingBox();
        if (bb.isValid()) {
            topZ    = bb.max.z;
            bottomZ = bb.min.z;
        }
    }

    CuttingTool tool;
    tool.diameter    = toolDiam;
    tool.fluteLength = std::abs(topZ - bottomZ);
    tool.type        = ToolType::BallEndMill;

    CuttingParams params;
    params.feedRate     = 1500.0;
    params.plungeRate   = 300.0;
    params.spindleRPM   = 12000;

    Strategies3D::WaterlineParams wp;
    wp.topZ    = topZ;
    wp.bottomZ = bottomZ;
    wp.zStep   = zStep;
    wp.stepOver = 0.4;
    wp.stockAllowance = 0.0;

    const NurbsSurface* surf = getActiveSurface(m_surfacesMgr.get());
    Toolpath tp;
    if (surf) {
        tp = Strategies3D::waterline(*surf, wp, tool, params);
    } else {
        // No surface loaded – generate a waterline on a default curved surface
        // (a simple revolve surface approximating the bounding box geometry)
        std::vector<Geom::Vec3> profile = {
            {40.0, 0.0, bottomZ}, {40.0, 0.0, (topZ + bottomZ) / 2.0}, {40.0, 0.0, topZ}
        };
        NurbsSurface defaultSurf = SurfacesManager::makeRevolve(profile, 360.0);
        tp = Strategies3D::waterline(defaultSurf, wp, tool, params);
    }

    static int wlCount = 0;
    tp.setName("Waterline_" + std::to_string(++wlCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"3D Waterline generated: Ø%.4g mm, Z %.4g→%.4g mm, step %.4g mm.",
        toolDiam, topZ, bottomZ, zStep);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// IDM_MACHINE_3D_SCALLOP – constant-scallop finishing on active surface
// --------------------------------------------------------------------------
void MainWindow::generate3DScallop()
{
    double toolDiam = 8.0, stepOver = 0.5;
    if (!promptDouble2(L"3D Scallop",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Step-over   (mm):",   stepOver, stepOver))
        return;

    if (toolDiam <= 0 || stepOver <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter and step-over must be positive.",
                    L"3D Scallop", MB_OK | MB_ICONWARNING);
        return;
    }

    // Compute and report the resulting scallop height
    double h = Strategies3D::scallopHeight(toolDiam / 2.0, stepOver);

    CuttingTool tool;
    tool.diameter = toolDiam;
    tool.type     = ToolType::BallEndMill;

    CuttingParams params;
    params.feedRate   = 2000.0;
    params.plungeRate = 300.0;
    params.spindleRPM = 15000;

    Strategies3D::ScallopParams sp;
    sp.stepOver       = stepOver;
    sp.stockAllowance = 0.0;

    const NurbsSurface* surf = getActiveSurface(m_surfacesMgr.get());
    Toolpath tp;
    if (surf) {
        tp = Strategies3D::scallop(*surf, sp, tool, params);
    } else {
        std::vector<Geom::Vec3> profile = {
            {35.0, 0.0, -25.0}, {35.0, 0.0, 0.0}
        };
        NurbsSurface defaultSurf = SurfacesManager::makeRevolve(profile, 360.0);
        tp = Strategies3D::scallop(defaultSurf, sp, tool, params);
    }

    static int scallopCount = 0;
    tp.setName("Scallop_" + std::to_string(++scallopCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[240] = {};
    std::swprintf(statusMsg, 240,
        L"3D Scallop generated: Ø%.4g mm, step-over %.4g mm → scallop h=%.4f mm.",
        toolDiam, stepOver, h);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// IDM_MACHINE_3D_RASTER – parallel raster passes projected onto mesh/surface
// --------------------------------------------------------------------------
void MainWindow::generate3DRaster()
{
    double toolDiam = 10.0, stepOver = 0.5, angle = 0.0;
    if (!promptTriple(L"3D Raster",
                      L"Tool diameter (mm):", toolDiam, toolDiam,
                      L"Step-over   (mm):",   stepOver, stepOver,
                      L"Raster angle (deg):", angle,    angle))
        return;

    if (toolDiam <= 0 || stepOver <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter and step-over must be positive.",
                    L"3D Raster", MB_OK | MB_ICONWARNING);
        return;
    }

    CuttingTool tool;
    tool.diameter = toolDiam;
    tool.type     = ToolType::BallEndMill;

    CuttingParams params;
    params.feedRate   = 1800.0;
    params.plungeRate = 300.0;
    params.spindleRPM = 12000;

    Strategies3D::RasterParams rp;
    rp.stepOver       = stepOver;
    rp.angle          = angle;
    rp.stockAllowance = 0.0;

    // Raster uses mesh data; build a simple flat mesh from the active solid
    // bounding box if no mesh is loaded.
    std::vector<Geom::Triangle> tris;
    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        Geom::AABB bb = m_solidsMgr->aggregateBoundingBox();
        if (bb.isValid()) {
            // Approximate the top face as two triangles
            Geom::Vec3 p0 = {bb.min.x, bb.min.y, bb.max.z};
            Geom::Vec3 p1 = {bb.max.x, bb.min.y, bb.max.z};
            Geom::Vec3 p2 = {bb.max.x, bb.max.y, bb.max.z};
            Geom::Vec3 p3 = {bb.min.x, bb.max.y, bb.max.z};
            Geom::Triangle t1; t1.v[0] = p0; t1.v[1] = p1; t1.v[2] = p2;
            Geom::Triangle t2; t2.v[0] = p0; t2.v[1] = p2; t2.v[2] = p3;
            tris.push_back(t1);
            tris.push_back(t2);
        }
    }
    if (tris.empty()) {
        // Fallback: flat 100×100 mm square top face
        Geom::Triangle t1, t2;
        t1.v[0] = {-50,-50,0}; t1.v[1] = {50,-50,0}; t1.v[2] = {50,50,0};
        t2.v[0] = {-50,-50,0}; t2.v[1] = {50, 50,0}; t2.v[2] = {-50,50,0};
        tris.push_back(t1);
        tris.push_back(t2);
    }
    MeshData mesh(std::move(tris));

    Toolpath tp = Strategies3D::raster(mesh, rp, tool, params);

    static int rasterCount = 0;
    tp.setName("Raster_" + std::to_string(++rasterCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"3D Raster generated: Ø%.4g mm, step %.4g mm, %.4g° angle.",
        toolDiam, stepOver, angle);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}

// --------------------------------------------------------------------------
// IDM_MACHINE_5AXIS – 5-axis swarf milling along the active surface
// --------------------------------------------------------------------------
void MainWindow::generate5AxisSwarf()
{
    double toolDiam = 16.0, leadAngle = 5.0;
    if (!promptDouble2(L"5-Axis Swarf",
                       L"Tool diameter (mm):", toolDiam,  toolDiam,
                       L"Lead angle   (deg):", leadAngle, leadAngle))
        return;

    if (toolDiam <= 0) {
        MessageBoxW(m_hwnd, L"Tool diameter must be positive.",
                    L"5-Axis Swarf", MB_OK | MB_ICONWARNING);
        return;
    }

    CuttingTool tool;
    tool.diameter    = toolDiam;
    tool.fluteLength = 50.0;
    tool.type        = ToolType::EndMill;

    CuttingParams params;
    params.feedRate   = 1200.0;
    params.plungeRate = 200.0;
    params.spindleRPM = 10000;

    MultiAxisParams maParams;
    maParams.leadLag   = MultiAxisParams::LeadLag::LeadFwd;
    maParams.leadAngle = leadAngle;
    maParams.gougeProtect = true;

    MultiAxis ma(maParams);

    const NurbsSurface* surf = getActiveSurface(m_surfacesMgr.get());
    Toolpath tp;
    if (surf) {
        tp = ma.swarfMill(*surf, tool, params);
    } else {
        // Default surface: revolve a vertical line to produce a cylinder
        std::vector<Geom::Vec3> profile = {
            {30.0, 0.0, -20.0}, {30.0, 0.0, 20.0}
        };
        NurbsSurface defaultSurf = SurfacesManager::makeRevolve(profile, 360.0);
        tp = ma.swarfMill(defaultSurf, tool, params);
    }

    // Apply lead/lag tilt: leadAngle° forward tilt, 0° lag (no backward tilt).
    MultiAxis::applyLeadLag(tp, leadAngle, 0.0);

    static int swarfCount = 0;
    tp.setName("Swarf5Axis_" + std::to_string(++swarfCount));
    m_toolpathMgr->addToolpath(std::move(tp));

    wchar_t statusMsg[200] = {};
    std::swprintf(statusMsg, 200,
        L"5-Axis Swarf generated: Ø%.4g mm, lead %.4g°, %d points.",
        toolDiam, leadAngle, static_cast<int>(m_toolpathMgr->at(m_toolpathMgr->count()-1).points().size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
}
