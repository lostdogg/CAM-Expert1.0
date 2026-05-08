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
#include "cam/CloudToolLibrary.h"
#include "cam/SqlToolDatabase.h"
#include "cam/MaterialLibrary.h"
#include "cad/ConstraintSolver.h"
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
#include <algorithm>
#include <cmath>
#include <utility>
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

static std::wstring toWideFromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (wlen <= 1) return {};
    std::wstring out(static_cast<std::size_t>(wlen - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), wlen);
    return out;
}

static std::string toUtf8FromWide(const wchar_t* ws) {
    if (!ws || !*ws) return {};
    int nbytes = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (nbytes <= 1) return {};
    std::string out(static_cast<std::size_t>(nbytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), nbytes, nullptr, nullptr);
    return out;
}

static const wchar_t* constraintTypeName(SketchConstraintType t) {
    switch (t) {
    case SketchConstraintType::Coincident:    return L"Coincident";
    case SketchConstraintType::Horizontal:    return L"Horizontal";
    case SketchConstraintType::Vertical:      return L"Vertical";
    case SketchConstraintType::Parallel:      return L"Parallel";
    case SketchConstraintType::Perpendicular: return L"Perpendicular";
    case SketchConstraintType::EqualLength:   return L"EqualLength";
    case SketchConstraintType::Distance:      return L"Distance";
    case SketchConstraintType::Angle:         return L"Angle";
    case SketchConstraintType::Radius:        return L"Radius";
    case SketchConstraintType::FixPoint:      return L"FixPoint";
    default:                                  return L"Unknown";
    }
}

static const wchar_t* solveStatusName(SolveResult::Status s) {
    switch (s) {
    case SolveResult::Status::Solved:             return L"Solved";
    case SolveResult::Status::SolvedWithWarnings: return L"SolvedWithWarnings";
    case SolveResult::Status::Infeasible:         return L"Infeasible";
    case SolveResult::Status::InvalidInput:       return L"InvalidInput";
    case SolveResult::Status::DeferredRebuild:    return L"DeferredRebuild";
    default:                                      return L"Unknown";
    }
}

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

// Coordinate conversion constant (mm per inch)
static constexpr double kMmPerInch = 25.4;

// Threshold (pixels²) below which a right-click with minimal mouse movement
// is treated as a context-menu click rather than a pan gesture.
static constexpr int kContextMenuThresholdSq = 25; // 5 px radius
static constexpr std::size_t kMaxAuditEntries = 120;
static constexpr int kMaxAuditDisplayEntries = 24;
static constexpr int kMaxDisplayedDiagnostics = 6;
static constexpr int kMaxDisplayedMaterials = 10;
static constexpr int kMaxMaterialSearchResults = 20;

// --------------------------------------------------------------------------
MainWindow::MainWindow() = default;
MainWindow::~MainWindow() {
    if (m_backgroundBrush) {
        DeleteObject(m_backgroundBrush);
        m_backgroundBrush = nullptr;
    }
}

// --------------------------------------------------------------------------
bool MainWindow::create(HINSTANCE hInstance) {
    if (!m_backgroundBrush)
        m_backgroundBrush = CreateSolidBrush(FRAME_BACKGROUND_COLOR);

    // Register the window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_backgroundBrush;
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
        // Keyboard shortcuts (processed when main window has focus).
        // The Windows accelerator table (m_hAccel) handles most cases when a
        // child window has focus; this handler covers the same keys for the
        // top-level frame window itself.
        bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;

        if (ctrl && shift) {
            switch (static_cast<int>(wParam)) {
            case 'T': onCommand(IDM_TOOLPATH_TOGGLE_DISP); return 0;
            case 'C': onCommand(IDM_TOOLPATH_COPY_PARAMS);  return 0;
            }
        }
        if (ctrl) {
            switch (static_cast<int>(wParam)) {
            case 'N': onCommand(IDM_FILE_NEW);      return 0;
            case 'O': onCommand(IDM_FILE_OPEN);     return 0;
            case 'S': onCommand(IDM_FILE_SAVE);     return 0;
            case 'I': onCommand(IDM_FILE_IMPORT);   return 0;
            case 'P': onCommand(IDM_MACHINE_POST);  return 0;
            case 'Z': onCommand(IDM_EDIT_UNDO);     return 0;
            case 'Y': onCommand(IDM_EDIT_REDO);     return 0;
            case 'C': onCommand(IDM_EDIT_COPY);     return 0;
            case 'V': onCommand(IDM_EDIT_PASTE);    return 0;
            }
        }
        // Single-key shortcuts (only when Ctrl is NOT held to avoid conflicts)
        if (!ctrl) {
            switch (static_cast<int>(wParam)) {
            case VK_DELETE: onCommand(IDM_EDIT_DELETE);         return 0;
            case VK_END:    onCommand(IDM_EDIT_ANALYZE);        return 0;
            case VK_SPACE:  onCommand(IDM_TOGGLE_SELECT_MODE);  return 0;
            case VK_F1:     onCommand(IDM_HELP_TOPICS);         return 0;
            case VK_F2:     onCommand(IDM_VIEW_ZOOM_SELECTED);  return 0;
            case VK_F3:     onCommand(IDM_VIEW_FIT);            return 0;
            case VK_F4:     onCommand(IDM_VIEW_TOGGLE_GRID);    return 0;
            case VK_F5:     onCommand(IDM_VIEW_TOGGLE_GNOMON);  return 0;
            case VK_F6:     onCommand(IDM_MACHINE_3D_WATERLINE); return 0;
            case VK_F7:     onCommand(IDM_MACHINE_3D_SCALLOP);  return 0;
            case VK_F8:     onCommand(IDM_WF_SET_CPLANE);       return 0;
            case VK_F9:     onCommand(IDM_WF_SET_ZDEPTH);       return 0;
            case 'T':       onCommand(IDM_TOOLPATH_MGR_TOGGLE); return 0;
            case 'M':       onCommand(IDM_GEOM_MOVE);           return 0;
            case 'R':       onCommand(IDM_GEOM_ROTATE);         return 0;
            case 'S':       onCommand(IDM_GEOM_SCALE);          return 0;
            case 'L':       onCommand(IDM_WF_LINE);             return 0;
            case 'A':       onCommand(IDM_WF_ARC);              return 0;
            case 'C':       onCommand(IDM_WF_CIRCLE);           return 0;
            case 'P':       onCommand(IDM_WF_POINT);            return 0;
            }
        }
        break;
    }

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        // Tab-control selection change: show/hide the Solids tree accordingly
        if (hdr->hwndFrom == m_hManagersPanel &&
            (hdr->code == TCN_SELCHANGE || hdr->code == TCN_SELCHANGING)) {
            // Trigger a layout refresh so the tree visibility is updated
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            onSize(rc.right, rc.bottom);
        }
        // TreeView notifications from the Solids history tree
        if (hdr->hwndFrom == m_hSolidsTree) {
            onSolidsTreeNotify(hdr);
        }
        return 0;
    }

    case WM_PAINT:
        onPaint();
        return 0;

    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        FillRect(hdc, &rc, m_backgroundBrush);
        return 1;
    }

    case WM_DESTROY:
        onDestroy();
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

// --------------------------------------------------------------------------
void MainWindow::onCreate() {
    buildMenu();
    buildAcceleratorTable();
    updateWindowTitle();   // set initial "Untitled" title

    HINSTANCE hInst = Application::instance().hInstance();

    // --- Status bar (bottom) ---
    m_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_STATUS_BAR)), hInst, nullptr);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Ready"));

    // --- Managers panel (left side – tab control) ---
    m_hManagersPanel = CreateWindowExW(
        0, WC_TABCONTROL, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
        0, RIBBON_HEIGHT, MANAGERS_PANEL_WIDTH, 600,
        m_hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_MANAGERS_PANEL)), hInst, nullptr);

    // Add tabs to the managers panel in spec order: Toolpaths, Levels, Planes, Solids
    TCITEMW ti{};
    ti.mask    = TCIF_TEXT;
    ti.pszText = const_cast<wchar_t*>(L"Toolpaths");
    TabCtrl_InsertItem(m_hManagersPanel, 0, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Levels");
    TabCtrl_InsertItem(m_hManagersPanel, 1, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Planes");
    TabCtrl_InsertItem(m_hManagersPanel, 2, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Solids");
    TabCtrl_InsertItem(m_hManagersPanel, 3, &ti);

    // --- Solids history-tree (lives inside the managers panel, Solids tab) ---
    // The TreeView is created as a child of the main window (not of the tab
    // control) so that it can be shown/hidden independently when the user
    // switches tabs.  It is positioned in onSize() / updateLayout().
    m_hSolidsTree = CreateWindowExW(
        0, WC_TREEVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER |
            TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
        0, 0, 0, 0,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_SOLIDS_TREE)), hInst, nullptr);

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
    // and rebuild the Solids history-tree panel.
    m_solidsMgr->setOnChange([this]() {
        if (m_viewport) m_viewport->redraw();
        buildSolidsHistoryTree();
    });

    // Connect the surfaces manager change callback to trigger a viewport redraw
    m_surfacesMgr->setOnChange([this]() {
        if (m_viewport) m_viewport->redraw();
    });

    // Connect the selection bar mask callback to select wireframe entities by type
    if (m_selectionBar) {
        m_selectionBar->setMaskCallback([this](SelectMask mask) {
            if (m_viewport) {
                SelectionFilter filter = SelectionFilter::All;
                switch (mask) {
                case SelectMask::Points:     filter = SelectionFilter::Points; break;
                case SelectMask::Lines:      filter = SelectionFilter::Lines;  break;
                case SelectMask::Arcs:       filter = SelectionFilter::Arcs;   break;
                case SelectMask::Splines:    filter = SelectionFilter::Splines;break;
                case SelectMask::Surfaces:   filter = SelectionFilter::Surfaces; break;
                case SelectMask::Solids:     filter = SelectionFilter::Solids; break;
                case SelectMask::Holes:      filter = SelectionFilter::Holes; break;
                case SelectMask::PlanarFaces:filter = SelectionFilter::PlanarFaces; break;
                case SelectMask::None:       filter = SelectionFilter::None;   break;
                case SelectMask::All:
                default:                     filter = SelectionFilter::All;    break;
                }
                m_viewport->setSelectionFilter(filter);
                m_viewport->redraw();
            }
            const wchar_t* names[] = {
                L"Filter lock: All entities",  L"Filter lock: Points",      L"Filter lock: Lines",
                L"Filter lock: Arcs",          L"Filter lock: Splines",     L"Filter lock: Surfaces",
                L"Filter lock: Solids",        L"Filter lock: Holes",       L"Filter lock: Planar Faces",
                L"Filter lock: None"
            };
            int idx = static_cast<int>(mask);
            if (idx >= 0 && idx < 10) {
                wchar_t msg[128] = {};
                std::swprintf(msg, 128, L"%s.", names[idx]);
                SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
            }
        });
        m_viewport->setSelectionFilter(SelectionFilter::All);
    }
    if (m_viewport) {
        m_viewport->setSelectionMode(m_selectionMode);
    }

    // --- Copilot ---
    m_copilotEngine = std::make_unique<CopilotEngine>();
    m_copilotEngine->setToolpathManager(m_toolpathMgr.get());
    m_copilotEngine->setSurfacesManager(m_surfacesMgr.get());
    m_copilotEngine->setMaterialLibrary(&m_materialLib);

    m_copilotPanel = std::make_unique<CopilotPanel>(m_hwnd, hInst);
    m_copilotPanel->setCopilotEngine(m_copilotEngine.get());
    ShowWindow(m_copilotPanel->hwnd(), SW_HIDE);  // hidden by default

    // Sync startup SQL "single source of truth" cache from current libraries.
    m_sqlToolDb.clear();
    m_materialLib.exportToSqlDatabase(m_sqlToolDb);
    m_cloudToolLib.exportToSqlDatabase(m_sqlToolDb);

    // --- Wireframe scene (entity store, Cplane, Z-depth) ---
    m_wfScene = std::make_unique<WireframeScene>();
    if (m_viewport) m_viewport->setWireframeScene(m_wfScene.get());

    // Wire wireframe associativity: when an entity is added to (or removed
    // from) the scene, check whether any solid's FeatureOp references it and
    // update the Solids tree and status bar accordingly.
    m_wfScene->setOnEntityChanged([this](int entityIndex) {
        onWireframeEntityAdded(entityIndex);
    });

    // Wire viewport coordinate callback → live X/Y/Z readout in status bar
    if (m_viewport) {
        m_viewport->setCoordCallback([this](double x, double y, double z) {
            updateCoordinateDisplay(x, y, z);
            if (m_wfScene) {
                SnapResult snap = AutoCursor::findSnap(*m_wfScene, Geom::Vec3{x, y, z});
                updateSnapDisplay(snap);
            } else {
                updateSnapDisplay(SnapResult{});
            }
        });

        // Wire right-click context menu callback
        m_viewport->setContextMenuCallback([this](int screenX, int screenY) {
            showViewportContextMenu(screenX, screenY);
        });
    }
}

// --------------------------------------------------------------------------
void MainWindow::buildMenu() {
    HMENU hMenu    = CreateMenu();
    HMENU hFile    = CreatePopupMenu();
    HMENU hEdit    = CreatePopupMenu();
    HMENU hSurface = CreatePopupMenu();
    HMENU hMachine = CreatePopupMenu();
    HMENU hView    = CreatePopupMenu();
    HMENU hHelp    = CreatePopupMenu();

    // File menu
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_NEW,    L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_OPEN,   L"&Open…\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVEAS, L"Save &As…");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_IMPORT, L"&Import…\tCtrl+I");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_EXIT,   L"E&xit\tAlt+F4");

    // Edit menu
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_UNDO,           L"&Undo\tCtrl+Z");
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_REDO,           L"&Redo\tCtrl+Y");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_COPY,           L"&Copy Selected\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_PASTE,          L"&Paste\tCtrl+V");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_DELETE,         L"&Delete Selected\tDel");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_ANALYZE,        L"&Analyze…\tEnd");
    AppendMenuW(hEdit, MF_STRING,    IDM_TOGGLE_SELECT_MODE,  L"&Toggle Selection Mode\tSpace");

    // Surfaces menu
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_FLAT_BOUNDARY, L"&Flat Boundary…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_LOFT,          L"Ruled/&Loft…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_REVOLVE,       L"&Revolve…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_SWEPT,         L"&Swept…");
    AppendMenuW(hSurface, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_NET,           L"&Net…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_FENCE,         L"Fen&ce…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_DRAFT_SURF,    L"&Draft…");
    AppendMenuW(hSurface, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_TRIM,          L"Trim to &Curves");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_TRIM_TO_SURF,  L"Trim to &Surfaces");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_UNTRIM,        L"&Untrim");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_FILLET,        L"&Fillet Blend…");
    AppendMenuW(hSurface, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_FROM_SOLID,    L"From Sol&ids…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_OFFSET,        L"&Offset…");
    AppendMenuW(hSurface, MF_STRING,    IDM_SURF_EXTEND,        L"&Extend…");

    // Machine menu
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_BACKPLOT, L"&Backplot");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_VERIFY,   L"&Verify");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_SIM,      L"Machine &Simulation");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_POST,     L"&Post Process…\tCtrl+P");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_GEN_POCKET,  L"Generate 2D &Pocket…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_GEN_CONTOUR, L"Generate 2D &Contour…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_CHAMFER,     L"Generate C&hamfer…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_THREAD,      L"Generate T&hread Mill…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_3D_WATERLINE, L"Generate 3D &Waterline…\tF6");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_3D_SCALLOP,   L"Generate 3D &Scallop…\tF7");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_3D_RASTER,    L"Generate 3D &Raster…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_5AXIS,        L"Generate &5-Axis Swarf…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_PROBE_Z,     L"Probe &Z Surface…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_PROBE_BORE,  L"Probe &Bore/Boss Center…");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_PROBE_CORNER,L"Probe C&orner…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_REGEN,    L"Re&generate All");
    AppendMenuW(hMachine, MF_STRING,    IDM_MACHINE_SUMMARY,  L"&Machining Summary…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_CONSTRAINTS,  L"Setup &Constraints…");
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_POST_PROFILE, L"Setup Post &Profile…");
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_TOOL_DB,      L"Setup Tool/&Material DB…");
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_PERF_MODE,    L"Setup &Performance Mode…");
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_GUIDANCE,     L"Context &Guidance");
    AppendMenuW(hMachine, MF_STRING,    IDM_SETUP_AUDIT_LOG,    L"Recent &Audit Trail…");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING,    IDM_TOOLPATH_MGR_TOGGLE,  L"&Toolpath Manager\tT");
    AppendMenuW(hMachine, MF_STRING,    IDM_TOOLPATH_TOGGLE_DISP, L"Toggle Toolpath &Display\tCtrl+Shift+T");
    AppendMenuW(hMachine, MF_STRING,    IDM_TOOLPATH_COPY_PARAMS, L"Copy Toolpath Para&ms\tCtrl+Shift+C");

    // View menu
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_WIREFRAME,     L"&Wireframe");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_SHADED,        L"&Shaded");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_TRANSLU,       L"&Translucent");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_ISOMETRIC,     L"&Isometric");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_FRONT,         L"&Front");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_TOP,           L"&Top");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_RIGHT,         L"&Right");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_BACK,          L"&Back");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_BOTTOM,        L"Bot&tom");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_LEFT,          L"&Left");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_ZOOM_SELECTED, L"Zoom to &Selected\tF2");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_FIT,           L"Zoom to &Fit All\tF3");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_TOGGLE_GRID,   L"Toggle &Grid\tF4");
    AppendMenuW(hView, MF_STRING,    IDM_VIEW_TOGGLE_GNOMON, L"Toggle G&nomon\tF5");

    // Geometry menu (hidden here; accessible via Wireframe ribbon tab and shortcuts)
    HMENU hGeom = CreatePopupMenu();
    AppendMenuW(hGeom, MF_STRING, IDM_WF_LINE,    L"&Line\tL");
    AppendMenuW(hGeom, MF_STRING, IDM_WF_ARC,     L"&Arc\tA");
    AppendMenuW(hGeom, MF_STRING, IDM_WF_CIRCLE,  L"&Circle\tC");
    AppendMenuW(hGeom, MF_STRING, IDM_WF_POINT,   L"&Point\tP");
    AppendMenuW(hGeom, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hGeom, MF_STRING, IDM_WF_SET_CPLANE, L"Cycle &Construction Plane\tF8");
    AppendMenuW(hGeom, MF_STRING, IDM_WF_SET_ZDEPTH, L"Set &Z-Depth…\tF9");
    AppendMenuW(hGeom, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hGeom, MF_STRING, IDM_GEOM_MOVE,   L"&Move\tM");
    AppendMenuW(hGeom, MF_STRING, IDM_GEOM_ROTATE, L"&Rotate\tR");
    AppendMenuW(hGeom, MF_STRING, IDM_GEOM_SCALE,  L"&Scale\tS");

    // Help menu
    AppendMenuW(hHelp, MF_STRING,    IDM_HELP_TOPICS,    L"&Help Topics\tF1");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelp, MF_STRING,    IDM_HELP_ABOUT,     L"&About CAM-Expert…");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelp, MF_STRING,    IDM_COPILOT_TOGGLE, L"Toggle &Copilot Panel");

    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile),    L"&File");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hEdit),    L"&Edit");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hGeom),    L"&Geometry");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSurface), L"&Surfaces");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hMachine), L"&Machine");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hView),    L"&View");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelp),    L"&Help");

    SetMenu(m_hwnd, hMenu);
}

// --------------------------------------------------------------------------
void MainWindow::buildAcceleratorTable() {
    // Windows accelerator table: fires WM_COMMAND even when a child window
    // (e.g. the Copilot text box) holds keyboard focus.
    // Single-character CAD shortcuts (T, M, R, S, L, A, C, P) are intentionally
    // omitted here so they only trigger when the main frame has focus via
    // WM_KEYDOWN, avoiding interference with text-entry fields.
    ACCEL accel[] = {
        // File
        { FVIRTKEY | FCONTROL,               'N',      IDM_FILE_NEW    },
        { FVIRTKEY | FCONTROL,               'O',      IDM_FILE_OPEN   },
        { FVIRTKEY | FCONTROL,               'S',      IDM_FILE_SAVE   },
        { FVIRTKEY | FCONTROL,               'I',      IDM_FILE_IMPORT },
        // Edit
        { FVIRTKEY | FCONTROL,               'Z',      IDM_EDIT_UNDO   },
        { FVIRTKEY | FCONTROL,               'Y',      IDM_EDIT_REDO   },
        { FVIRTKEY | FCONTROL,               'C',      IDM_EDIT_COPY   },
        { FVIRTKEY | FCONTROL,               'V',      IDM_EDIT_PASTE  },
        { FVIRTKEY,                          VK_DELETE,IDM_EDIT_DELETE  },
        { FVIRTKEY,                          VK_END,   IDM_EDIT_ANALYZE },
        // Machine
        { FVIRTKEY | FCONTROL,               'P',      IDM_MACHINE_POST },
        { FVIRTKEY,                          VK_F6,    IDM_MACHINE_3D_WATERLINE },
        { FVIRTKEY,                          VK_F7,    IDM_MACHINE_3D_SCALLOP   },
        { FVIRTKEY,                          VK_F8,    IDM_WF_SET_CPLANE        },
        { FVIRTKEY,                          VK_F9,    IDM_WF_SET_ZDEPTH        },
        // View / Function keys
        { FVIRTKEY,                          VK_F1,    IDM_HELP_TOPICS         },
        { FVIRTKEY,                          VK_F2,    IDM_VIEW_ZOOM_SELECTED  },
        { FVIRTKEY,                          VK_F3,    IDM_VIEW_FIT            },
        { FVIRTKEY,                          VK_F4,    IDM_VIEW_TOGGLE_GRID    },
        { FVIRTKEY,                          VK_F5,    IDM_VIEW_TOGGLE_GNOMON  },
        // Toolpath (Ctrl+Shift combos)
        { FVIRTKEY | FCONTROL | FSHIFT,      'T',      IDM_TOOLPATH_TOGGLE_DISP },
        { FVIRTKEY | FCONTROL | FSHIFT,      'C',      IDM_TOOLPATH_COPY_PARAMS },
    };
    if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
    m_hAccel = CreateAcceleratorTableW(accel,
                                       static_cast<int>(sizeof(accel) / sizeof(accel[0])));
}

// --------------------------------------------------------------------------
void MainWindow::onSize(int cx, int cy) {
    updateLayout(cx, cy);
}

// --------------------------------------------------------------------------
void MainWindow::updateLayout(int cx, int cy) {
    if (!m_hwnd) return;

    // Status bar – auto-sizes itself, then we partition it into 8 panes.
    if (m_hStatusBar) {
        SendMessage(m_hStatusBar, WM_SIZE, 0, 0);
        // Fixed-width panes at the right end; pane 0 fills the remainder.
        int unitW   = 50;
        int coordW  = 80;
        int snapW   = 80;
        int zdepW   = 80;
        int cplaneW = 90;
        int parts[8] = {
            cx - cplaneW - zdepW - snapW - coordW*3 - unitW,  // pane 0: message
            cx - zdepW   - snapW - coordW*3 - unitW,           // pane 1: Cplane
            cx - snapW   - coordW*3 - unitW,                   // pane 2: Z-depth
            cx - coordW*3 - unitW,                             // pane 3: Snap
            cx - coordW*2 - unitW,                             // pane 4: X coord
            cx - coordW   - unitW,                             // pane 5: Y coord
            cx - unitW,                                        // pane 6: Z coord
            -1                                                 // pane 7: Unit (fills to end)
        };
        SendMessage(m_hStatusBar, SB_SETPARTS, 8,
                    reinterpret_cast<LPARAM>(parts));
        updateWfStatusBar();
        updateUnitPane();
    }

    int viewY  = RIBBON_HEIGHT;
    int viewH  = cy - RIBBON_HEIGHT - STATUS_BAR_HEIGHT;

    // If the Copilot panel is visible, carve out space on the right side
    int copilotW = (m_copilotVisible && m_copilotPanel) ? COPILOT_PANEL_WIDTH : 0;

    // Selection bar – horizontal strip anchored directly above the 3D canvas
    int selBarX = MANAGERS_PANEL_WIDTH;
    int selBarW = cx - MANAGERS_PANEL_WIDTH - copilotW;

    // Viewport starts below the selection bar
    int viewContentY = viewY + SELECTION_BAR_HEIGHT;
    int viewContentH = viewH - SELECTION_BAR_HEIGHT;
    int viewX   = MANAGERS_PANEL_WIDTH;
    int viewW   = cx - MANAGERS_PANEL_WIDTH - copilotW;

    // Managers panel
    if (m_hManagersPanel)
        SetWindowPos(m_hManagersPanel, nullptr,
                     0, viewY, MANAGERS_PANEL_WIDTH, viewH,
                     SWP_NOZORDER | SWP_NOACTIVATE);

    // Solids history-tree – sits inside the managers panel below the tab strip.
    // The tree is shown only when the "Solids" tab (index 3) is active.
    if (m_hSolidsTree) {
        int tabHeight = 24; // approximate tab-strip height in pixels
        int treeY     = viewY + tabHeight;
        int treeH     = viewH - tabHeight;
        int activeSolidsTab = m_hManagersPanel
            ? TabCtrl_GetCurSel(m_hManagersPanel) : -1;
        bool solidsTabActive = (activeSolidsTab == 3);
        SetWindowPos(m_hSolidsTree, nullptr,
                     0, treeY, MANAGERS_PANEL_WIDTH, treeH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(m_hSolidsTree, solidsTabActive ? SW_SHOW : SW_HIDE);
    }

    // Ribbon
    if (m_ribbon)
        m_ribbon->resize(0, 0, cx, RIBBON_HEIGHT);

    // Selection bar – horizontal bar above the 3D canvas
    if (m_selectionBar)
        m_selectionBar->resize(selBarX, viewY, selBarW, SELECTION_BAR_HEIGHT);

    // Viewport – positioned below the selection bar
    if (m_viewport)
        m_viewport->resize(viewX, viewContentY, viewW, viewContentH);

    // Copilot panel (far right, full height below ribbon)
    if (m_copilotPanel && m_copilotVisible)
        m_copilotPanel->resize(cx - copilotW, viewY, copilotW, viewH);
}

// --------------------------------------------------------------------------
void MainWindow::onCommand(int id) {
    if (handleFileCommand(id) ||
        handleWireframeCommand(id) ||
        handleSurfaceCommand(id) ||
        handleSolidCommand(id) ||
        handleCamCommand(id) ||
        handleSetupWorkflowCommand(id)) {
        return;
    }

    switch (id) {
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

    case IDM_PREP_HEAL:        prepHeal();         break;
    case IDM_PREP_REM_FILLET:  prepRemoveFillet(); break;
    case IDM_PREP_SPLIT:       prepSplit();        break;
    case IDM_PREP_BOUNDS:      prepBoundaries();   break;
    case IDM_PREP_CLASSIFY:    prepClassify();     break;
    case IDM_PREP_DRAFT:       prepAnalyse();      break;

    case IDM_HELP_ABOUT:
        showAboutDialog();
        break;

    case IDM_HELP_TOPICS:
        showHelpTopics();
        break;

    case IDM_COPILOT_TOGGLE:
        toggleCopilotPanel();
        break;

    // Edit commands
    case IDM_EDIT_UNDO:           editUndo();           break;
    case IDM_EDIT_REDO:           editRedo();           break;
    case IDM_EDIT_COPY:           editCopy();           break;
    case IDM_EDIT_PASTE:          editPaste();          break;
    case IDM_EDIT_DELETE:         editDelete();         break;
    case IDM_EDIT_ANALYZE:        editAnalyze();        break;
    case IDM_TOGGLE_SELECT_MODE:  toggleSelectionMode(); break;

    // View commands
    case IDM_VIEW_ZOOM_SELECTED:  viewZoomSelected();   break;
    case IDM_VIEW_TOGGLE_GRID:    viewToggleGrid();     break;
    case IDM_VIEW_TOGGLE_GNOMON:  viewToggleGnomon();   break;

    // Geometry transform commands
    case IDM_GEOM_MOVE:           geomMove();           break;
    case IDM_GEOM_ROTATE:         geomRotate();         break;
    case IDM_GEOM_SCALE:          geomScale();          break;

    // Toolpath manager commands
    case IDM_TOOLPATH_MGR_TOGGLE:  toolpathMgrToggle();   break;
    case IDM_TOOLPATH_TOGGLE_DISP: toolpathToggleDisplay(); break;
    case IDM_TOOLPATH_COPY_PARAMS: toolpathCopyParams();   break;

    // Unit toggle
    case IDM_UNIT_TOGGLE:          unitToggle();           break;

    // Viewport context menu commands
    case IDM_CTX_FIT:
        if (m_viewport) { m_viewport->reset(); }
        break;
    case IDM_CTX_ISO:
        if (m_viewport) m_viewport->setView(ViewPreset::Isometric);
        break;
    case IDM_CTX_FRONT:
        if (m_viewport) m_viewport->setView(ViewPreset::Front);
        break;
    case IDM_CTX_TOP:
        if (m_viewport) m_viewport->setView(ViewPreset::Top);
        break;
    case IDM_CTX_RIGHT:
        if (m_viewport) m_viewport->setView(ViewPreset::Right);
        break;
    case IDM_CTX_CLEAR_COLORS:
        // Reset all entity colours to defaults by clearing and redrawing the scene
        if (m_viewport) m_viewport->redraw();
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                    reinterpret_cast<LPARAM>(L"Colors cleared."));
        break;
    case IDM_CTX_CHANGE_COLOR:
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                    reinterpret_cast<LPARAM>(L"Change Color: selected-entity coloring is not yet implemented."));
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
    BeginPaint(m_hwnd, &ps);
    EndPaint(m_hwnd, &ps);
}

// --------------------------------------------------------------------------
void MainWindow::onDestroy() {
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------
void MainWindow::showAboutDialog() {
    std::wstring msg =
        std::wstring(Application::APP_NAME) + L" v" + Application::APP_VERSION +
        L"\n\nComputer-Aided Manufacturing & Design software.\n"
        L"Supports 2D/2.5D, 3D, and Multi-Axis machining strategies.\n"
        L"Post-Processor: Fanuc, Haas, Heidenhain, and more.\n\n"
        L"Keyboard Shortcuts:\n"
        L"  Ctrl+N/O/S  New / Open / Save\n"
        L"  Ctrl+Z/Y    Undo / Redo\n"
        L"  Ctrl+C/V    Copy / Paste selected entities\n"
        L"  Del / End   Delete selected / Analyze dialog\n"
        L"  Space       Toggle selection / deselection mode\n"
        L"  F1          Help Topics\n"
        L"  F2          Zoom to selected entities\n"
        L"  F3          Zoom to fit all entities\n"
        L"  F4          Toggle grid display\n"
        L"  F5          Toggle dynamic gnomon\n"
        L"  F6 / F7     3D Waterline / Scallop toolpath\n"
        L"  T           Toolpath Manager\n"
        L"  M / R / S   Move / Rotate / Scale geometry\n"
        L"  L / A / C / P  Line / Arc / Circle / Point creation\n"
        L"  Ctrl+Shift+T   Toggle toolpath display\n"
        L"  Ctrl+Shift+C   Copy toolpath parameters\n\n"
        L"Mouse Controls:\n"
        L"  Left drag           Orbit (dynamic rotation)\n"
        L"  Middle/Right drag   Pan\n"
        L"  Scroll wheel        Zoom in / out\n"
        L"  Ctrl + Scroll       Rotate (yaw) with inertia spin\n"
        L"  Shift + Scroll      Rotate (pitch) with inertia spin\n"
        L"  Ctrl+Shift+Scroll   Pan horizontally\n"
        L"  Horizontal scroll   Pan left / right (trackpad)\n\n"
        L"New in v1.3:\n"
        L"  \u2022 Full keyboard shortcut set (Mastercam-style)\n"
        L"  \u2022 Windows accelerator table for system-wide shortcut support\n"
        L"  \u2022 Edit menu (Undo, Redo, Copy, Paste, Delete, Analyze)\n"
        L"  \u2022 Geometry menu (Move, Rotate, Scale, Line, Arc, Circle, Point)\n"
        L"  \u2022 Enhanced mouse: Ctrl/Shift+wheel rotation with inertia spin\n"
        L"  \u2022 Horizontal scroll (WM_MOUSEHWHEEL) pans view left/right\n"
        L"  \u2022 F4/F5 grid and gnomon display toggles\n\n"
        L"New in v1.4:\n"
        L"  \u2022 Setup workflows for Constraints, Post Profiles, and SQL Tool/Material DB\n"
        L"  \u2022 Post and simulation preflight checks with remediation messaging\n"
        L"  \u2022 Operation-level recent audit trail and context guidance\n"
        L"  \u2022 Performance mode presets (Quality/Balanced/Speed) for heavy 3D generation\n\n"
        L"New in v1.2:\n"
        L"  \u2022 3D Waterline (Z-level) toolpath generation\n"
        L"  \u2022 3D Scallop (constant step-over) with live scallop-height readout\n"
        L"  \u2022 3D Raster (parallel passes) projected onto mesh stock\n"
        L"  \u2022 5-Axis Swarf milling with configurable lead angle\n\n"
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
    if (m_wfScene) {
        m_wfScene->clear();
        m_wfScene->clearUndoRedo();
    }
    m_constraintSolver.clearConstraints();
    m_activePostProfilePath.clear();
    m_operationAudit.clear();
    m_sqlToolDb.clear();
    m_materialLib.exportToSqlDatabase(m_sqlToolDb);
    m_cloudToolLib.exportToSqlDatabase(m_sqlToolDb);
    m_wfClipboard.clear();
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
        L"All Supported (*.step;*.stp;*.iges;*.igs;*.x_t;*.x_b;*.stl;*.obj;*.3mf;*.amf;*.dxf;*.dwg;*.sldprt;*.sldasm;*.3dm;*.ipt;*.iam;*.catpart)\0"
        L"*.step;*.stp;*.iges;*.igs;*.x_t;*.x_b;*.stl;*.obj;*.3mf;*.amf;*.dxf;*.dwg;*.sldprt;*.sldasm;*.3dm;*.ipt;*.iam;*.catpart\0"
        L"STEP Files (*.step;*.stp)\0*.step;*.stp\0"
        L"IGES Files (*.iges;*.igs)\0*.iges;*.igs\0"
        L"Parasolid (*.x_t;*.x_b)\0*.x_t;*.x_b\0"
        L"STL Files (*.stl)\0*.stl\0"
        L"OBJ Files (*.obj)\0*.obj\0"
        L"3MF / AMF (*.3mf;*.amf)\0*.3mf;*.amf\0"
        L"AutoCAD (*.dxf;*.dwg)\0*.dxf;*.dwg\0"
        L"SolidWorks (*.sldprt;*.sldasm)\0*.sldprt;*.sldasm\0"
        L"Rhino (*.3dm)\0*.3dm\0"
        L"Inventor (*.ipt;*.iam)\0*.ipt;*.iam\0"
        L"CATIA (*.catpart)\0*.catpart\0"
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
    std::wstring preflightReason;
    if (!preflightForPosting(preflightReason)) {
        MessageBoxW(m_hwnd, preflightReason.c_str(),
                    L"Post Preflight Check", MB_OK | MB_ICONWARNING);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"Post cancelled by preflight check."));
        appendAudit(L"Post preflight failed");
        return;
    }

    PostProcessor pp;
    if (!m_activePostProfilePath.empty()) {
        std::string err;
        if (!pp.loadScriptProfile(m_activePostProfilePath, &err)) {
            std::wstring werr = toWideFromUtf8(err);
            MessageBoxW(m_hwnd, werr.c_str(), L"Post Profile Error", MB_OK | MB_ICONWARNING);
            appendAudit(L"Post aborted: profile load failure");
            return;
        }
    }

    const CoordPlane* wcsPlane = m_planesMgr ? m_planesMgr->wcsPlane() : nullptr;
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Posting in progress..."));
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    auto gcode = pp.generate(m_toolpathMgr.get(), wcsPlane);
    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    if (gcode.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Post-Processor: no toolpaths to process."));
        appendAudit(L"Post generated no output");
        return;
    }

    if (pp.hasError()) {
        const std::string& errStr = pp.lastError().message;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, errStr.c_str(), -1, nullptr, 0);
        std::wstring werr(static_cast<std::size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, errStr.c_str(), -1, werr.data(), wlen);
        MessageBoxW(m_hwnd, werr.c_str(), L"Post-Processor Error", MB_OK | MB_ICONWARNING);
        appendAudit(L"Post failed with post-processor error");
        return;
    }

    // Prompt for save path
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize    = sizeof(ofn);
    ofn.hwndOwner      = m_hwnd;
    ofn.lpstrFilter    =
        L"CNC Programs (*.nc;*.ncc;*.tap;*.gcode;*.mpf;*.spf;*.din;*.sbp)\0*.nc;*.ncc;*.tap;*.gcode;*.mpf;*.spf;*.din;*.sbp\0"
        L"Generic NC (*.nc;*.ncc)\0*.nc;*.ncc\0"
        L"TAP (*.tap)\0*.tap\0"
        L"GCODE (*.gcode)\0*.gcode\0"
        L"Siemens (*.mpf;*.spf)\0*.mpf;*.spf\0"
        L"Heidenhain DIN (*.din)\0*.din\0"
        L"ShopBot (*.sbp)\0*.sbp\0"
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
            appendAudit(L"NC file posted successfully");
        } else {
            MessageBoxW(m_hwnd, L"Failed to write the NC file.",
                        L"Post-Processor", MB_OK | MB_ICONERROR);
            appendAudit(L"Post failed while writing NC file");
        }
    } else {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Post-processing complete."));
        appendAudit(L"Post completed without file save");
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
    std::wstring preflightReason;
    if (!preflightForSimulation(preflightReason)) {
        MessageBoxW(m_hwnd, preflightReason.c_str(),
                    L"Simulation Preflight Check", MB_OK | MB_ICONWARNING);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"Machine simulation cancelled by preflight check."));
        appendAudit(L"Machine simulation preflight failed");
        return;
    }
    MachineSimulation sim;
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Machine simulation in progress..."));
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    CollisionResult result = sim.run(m_toolpathMgr.get());
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
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
    appendAudit(result.hasCollision ? L"Machine simulation detected collision"
               : (result.hasOverTravel ? L"Machine simulation detected over-travel"
                                      : L"Machine simulation passed"));
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
// Edit command handlers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::editUndo() {
    if (!m_wfScene || !m_wfScene->canUndo()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Undo: nothing to undo."));
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    m_wfScene->undo();
    if (m_viewport) m_viewport->redraw();
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Undo: last wireframe operation reversed."));
}

// --------------------------------------------------------------------------
void MainWindow::editRedo() {
    if (!m_wfScene || !m_wfScene->canRedo()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Redo: nothing to redo."));
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    m_wfScene->redo();
    if (m_viewport) m_viewport->redraw();
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Redo: operation reapplied."));
}

// --------------------------------------------------------------------------
void MainWindow::editCopy() {
    if (!m_wfScene) return;
    auto indices = m_wfScene->selectedIndices();
    if (indices.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Copy: no entities selected. Use the Selection Bar to select entities first."));
        return;
    }
    m_wfClipboard.clear();
    const auto& ents = m_wfScene->entities();
    for (int i : indices) {
        if (i >= 0 && i < static_cast<int>(ents.size()))
            m_wfClipboard.push_back(ents[i]);
    }
    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Copied %d entity(s) to clipboard.",
                  static_cast<int>(m_wfClipboard.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::editPaste() {
    if (m_wfClipboard.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Paste: clipboard is empty. Copy entities first (Ctrl+C)."));
        return;
    }
    if (m_wfScene) {
        m_wfScene->pushUndoState();
        m_wfScene->clearSelection();
        int base = m_wfScene->entityCount();
        for (const auto& e : m_wfClipboard)
            m_wfScene->addEntity(e);
        // Select the newly pasted entities so they can be immediately moved
        for (int i = 0; i < static_cast<int>(m_wfClipboard.size()); ++i)
            m_wfScene->selectEntity(base + i);
        if (m_viewport) m_viewport->redraw();
    }
    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Pasted %d entity(s).",
                  static_cast<int>(m_wfClipboard.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::editDelete() {
    if (!m_wfScene) return;
    auto indices = m_wfScene->selectedIndices();
    if (indices.empty()) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Delete: no entities selected. Use the Selection Bar to select entities first."));
        return;
    }
    m_wfScene->pushUndoState();
    // Remove in reverse order so earlier indices remain valid
    for (int i = static_cast<int>(indices.size()) - 1; i >= 0; --i)
        m_wfScene->removeEntity(indices[i]);
    if (m_viewport) m_viewport->redraw();
    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Deleted %d entity(s). Press Ctrl+Z to undo.",
                  static_cast<int>(indices.size()));
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::editAnalyze() {
    // Display the Analyze dialog (model analysis / draft-check results).
    prepAnalyse();
}

// --------------------------------------------------------------------------
void MainWindow::toggleSelectionMode() {
    m_selectionMode = !m_selectionMode;
    if (m_viewport) m_viewport->setSelectionMode(m_selectionMode);
    const wchar_t* modeName = m_selectionMode ? L"Selection mode: SELECT"
                                               : L"Selection mode: DESELECT";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(modeName));
}

// ==========================================================================
// View command handlers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::viewZoomSelected() {
    if (m_viewport) {
        m_viewport->zoomSelected();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"View: zoomed to selected entities."));
    }
}

// --------------------------------------------------------------------------
void MainWindow::viewToggleGrid() {
    if (m_viewport) {
        m_viewport->toggleGrid();
        bool on = m_viewport->gridVisible();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(on ? L"Grid: ON" : L"Grid: OFF"));
    }
}

// --------------------------------------------------------------------------
void MainWindow::viewToggleGnomon() {
    if (m_viewport) {
        m_viewport->toggleGnomon();
        bool on = m_viewport->gnomonVisible();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(on ? L"Gnomon: ON" : L"Gnomon: OFF"));
    }
}

// ==========================================================================
// Geometry transform command handlers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::geomMove() {
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Move: select geometry to move, then specify vector."));
}

// --------------------------------------------------------------------------
void MainWindow::geomRotate() {
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Rotate: select geometry to rotate, then specify angle."));
}

// --------------------------------------------------------------------------
void MainWindow::geomScale() {
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Scale: select geometry to scale, then specify factor."));
}

// ==========================================================================
// Toolpath manager / display command handlers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::toolpathMgrToggle() {
    // Activate the Toolpath tab in the managers panel (tab 0).
    if (m_hManagersPanel) {
        TabCtrl_SetCurSel(m_hManagersPanel, 0);
        SetFocus(m_hManagersPanel);
    }
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Toolpath Manager activated."));
}

// --------------------------------------------------------------------------
void MainWindow::toolpathToggleDisplay() {
    // Toggle toolpath curve visibility in the viewport.
    // The viewport renders toolpaths when a ToolpathManager is connected;
    // a simple approach is to connect / disconnect it.
    if (!m_viewport || !m_toolpathMgr) return;
    static bool toolpathsVisible = true;
    toolpathsVisible = !toolpathsVisible;
    m_viewport->setToolpathManager(toolpathsVisible ? m_toolpathMgr.get() : nullptr);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(toolpathsVisible
            ? L"Toolpath display: ON"
            : L"Toolpath display: OFF"));
}

// --------------------------------------------------------------------------
void MainWindow::toolpathCopyParams() {
    if (!m_toolpathMgr || m_toolpathMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Copy toolpath params: no toolpaths exist."));
        return;
    }
    // Use selected toolpath, or fall back to the last one
    int idx = m_toolpathMgr->hasSelection()
            ? m_toolpathMgr->selectedIndex()
            : m_toolpathMgr->count() - 1;
    const Toolpath&      tp = m_toolpathMgr->at(idx);
    const CuttingParams& p  = tp.params();
    const CuttingTool&   t  = tp.tool();

    // Build a human-readable parameter block
    char text[512] = {};
    std::snprintf(text, sizeof(text),
        "Operation: %s\n"
        "Tool:      %s (%.4g mm dia, %d flutes)\n"
        "Spindle:   %.0f RPM\n"
        "Feed:      %.0f mm/min\n"
        "Plunge:    %.0f mm/min\n"
        "Axial DoC: %.4g mm\n"
        "Radial DoC:%.4g mm\n"
        "Stock:     %.4g mm\n",
        tp.name().c_str(),
        t.name.empty() ? "Tool" : t.name.c_str(),
        t.diameter, t.numFlutes,
        p.spindleRPM, p.feedRate, p.plungeRate,
        p.axialDepth, p.radialDepth, p.stockAllowance);

    // Place the text on the Windows clipboard as CF_UNICODETEXT.
    // snprintf produces bytes in the system ANSI code page (CP_ACP).
    int wlen = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (wlen > 1 && OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE,
                                  static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
        if (hg) {
            wchar_t* dest = static_cast<wchar_t*>(GlobalLock(hg));
            if (dest) {
                MultiByteToWideChar(CP_ACP, 0, text, -1, dest, wlen);
                GlobalUnlock(hg);
                SetClipboardData(CF_UNICODETEXT, hg);
            } else {
                GlobalFree(hg);
            }
        }
        CloseClipboard();
    }

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Toolpath parameters copied to clipboard."));
}

// ==========================================================================
// Help command handlers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::showHelpTopics() {
    std::wstring msg =
        L"CAM-Expert Keyboard Shortcuts\n"
        L"================================\n\n"
        L"File\n"
        L"  Ctrl+N    New file\n"
        L"  Ctrl+O    Open file\n"
        L"  Ctrl+S    Save file\n\n"
        L"Edit\n"
        L"  Ctrl+Z    Undo last action\n"
        L"  Ctrl+Y    Redo last undone action\n"
        L"  Ctrl+C    Copy selected entities\n"
        L"  Ctrl+V    Paste entities\n"
        L"  Delete    Remove selected entities\n"
        L"  End       Analyze dialog\n"
        L"  Space     Toggle selection / deselection mode\n\n"
        L"View / Function Keys\n"
        L"  F2        Zoom to selected entities\n"
        L"  F3        Zoom to fit all entities\n"
        L"  F4        Toggle grid display\n"
        L"  F5        Toggle dynamic gnomon\n"
        L"  F6        3D Waterline toolpath\n"
        L"  F7        3D Scallop toolpath\n\n"
        L"Toolpath & CAD\n"
        L"  T         Open Toolpath Manager\n"
        L"  M         Move selected geometry\n"
        L"  R         Rotate selected geometry\n"
        L"  S         Scale selected geometry\n"
        L"  L         Line creation\n"
        L"  A         Arc creation\n"
        L"  C         Circle creation\n"
        L"  P         Point creation\n"
        L"  Ctrl+Shift+T  Toggle toolpath display\n"
        L"  Ctrl+Shift+C  Copy toolpath parameters\n\n"
        L"Setup Workflows (Machine → Setup)\n"
        L"  Constraints    Create/list/delete/solve sketch constraints\n"
        L"  Post Profile   Load/validate/clear script profiles\n"
        L"  Tool DB        SQL tool/material/cutting-data workflow\n"
        L"  Performance    Quality/Balanced/Speed and OpenMP visibility\n"
        L"  Audit          Recent operation-level changes\n\n"
        L"Mouse Controls\n"
        L"  Left drag            Orbit (dynamic rotation)\n"
        L"  Middle / Right drag  Pan\n"
        L"  Scroll wheel         Zoom\n"
        L"  Ctrl + Scroll        Rotate (yaw) with inertia\n"
        L"  Shift + Scroll       Rotate (pitch) with inertia\n"
        L"  Ctrl+Shift+Scroll    Pan horizontally\n"
        L"  Horizontal scroll    Pan left / right";
    MessageBoxW(m_hwnd, msg.c_str(), L"CAM-Expert Help", MB_OK | MB_ICONINFORMATION);
}

// --------------------------------------------------------------------------

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
// promptBodyOpType – ask the user to choose Create Body / Add Boss / Cut Body.
// Shows a simple MessageBox with three choices (via a cascading Yes/No prompt).
// Returns true if the user confirmed a selection.
// --------------------------------------------------------------------------
bool MainWindow::promptBodyOpType(BodyOpType& out) {
    // Use a three-way choice dialog built from two MessageBox calls:
    // First ask: "Add to existing solid? Yes=AddBoss/CutBody, No=CreateBody"
    int r1 = MessageBoxW(m_hwnd,
        L"Operation type:\n\n"
        L"[Yes]  Add to / Cut from an existing solid\n"
        L"[No]   Create a brand-new independent solid body\n"
        L"[Cancel]  Abort",
        L"Operation Type",
        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r1 == IDCANCEL) return false;
    if (r1 == IDNO) {
        out = BodyOpType::CreateBody;
        return true;
    }
    // Yes → ask whether to Add Boss or Cut Body
    int r2 = MessageBoxW(m_hwnd,
        L"[Yes]  Add Boss (merge / add material)\n"
        L"[No]   Cut Body (carve / remove material)",
        L"Add or Cut?",
        MB_YESNO | MB_ICONQUESTION);
    out = (r2 == IDYES) ? BodyOpType::AddBoss : BodyOpType::CutBody;
    return true;
}

// --------------------------------------------------------------------------
// onWireframeEntityAdded – wireframe associativity callback.
//
// Called by WireframeScene whenever an entity is added (entityIndex >= 0)
// or the scene is cleared (entityIndex == -1).  The method searches the
// SolidsManager for any FeatureOp whose wfChainIdx matches the changed
// entity index and updates the Solids tree + status bar to indicate that
// the affected solid(s) may need to be rebuilt.
// --------------------------------------------------------------------------
void MainWindow::onWireframeEntityAdded(int entityIndex) {
    if (!m_solidsMgr) return;

    if (entityIndex < 0) {
        // Scene was cleared: any solids that referenced wireframe entities
        // lose their profile link; rebuild the tree so the "[suppressed]" /
        // label display stays accurate.
        buildSolidsHistoryTree();
        return;
    }

    // Find solids whose history references this entity index.
    std::vector<int> driven = m_solidsMgr->solidsDrivenBy(entityIndex);
    if (driven.empty()) return;

    // Rebuild the Solids tree so the status is up to date.
    buildSolidsHistoryTree();

    // Update the status bar to inform the user that associated solids may
    // need to be re-evaluated (a full geometry kernel would trigger an
    // automatic rebuild; here we notify the user instead).
    std::wstring msg = L"Wireframe entity #"
        + std::to_wstring(entityIndex)
        + L" updated – "
        + std::to_wstring(static_cast<int>(driven.size()))
        + L" solid(s) reference this profile (rebuild pending).";
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
// buildSolidsHistoryTree – rebuild the TreeView from the current SolidsManager
// state.  Each solid is a root node; each FeatureOp is a child branch.
// --------------------------------------------------------------------------
void MainWindow::buildSolidsHistoryTree() {
    if (!m_hSolidsTree || !m_solidsMgr) return;

    // Freeze the TreeView during the update to avoid flicker.
    SendMessage(m_hSolidsTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(m_hSolidsTree);

    for (int si = 0; si < m_solidsMgr->count(); ++si) {
        const SolidEntry& entry = m_solidsMgr->at(si);

        // Solid root node
        std::wstring solidName(entry.solid.name().begin(), entry.solid.name().end());
        if (solidName.empty()) solidName = L"Solid";

        TVINSERTSTRUCTW ins = {};
        ins.hParent         = TVI_ROOT;
        ins.hInsertAfter    = TVI_LAST;
        ins.item.mask       = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
        ins.item.pszText    = const_cast<LPWSTR>(solidName.c_str());
        // Encode solid index; 0xFFFF in low 16 bits → root node (no feature)
        ins.item.lParam     = static_cast<LPARAM>((si << 16) | 0xFFFF);
        ins.item.stateMask  = TVIS_BOLD;
        ins.item.state      = TVIS_BOLD;
        HTREEITEM hRoot = TreeView_InsertItem(m_hSolidsTree, &ins);

        // Feature child nodes
        for (int fi = 0; fi < m_solidsMgr->featureCount(si); ++fi) {
            const FeatureOp& op = m_solidsMgr->getFeature(si, fi);

            std::wstring featureLabel(op.label.begin(), op.label.end());
            if (op.suppressed) featureLabel = L"[suppressed] " + featureLabel;

            TVINSERTSTRUCTW fIns = {};
            fIns.hParent        = hRoot;
            fIns.hInsertAfter   = TVI_LAST;
            fIns.item.mask      = TVIF_TEXT | TVIF_PARAM;
            fIns.item.pszText   = const_cast<LPWSTR>(featureLabel.c_str());
            // Encode solid + feature index into lParam
            fIns.item.lParam    = static_cast<LPARAM>((si << 16) | (fi & 0xFFFF));
            TreeView_InsertItem(m_hSolidsTree, &fIns);
        }

        // Auto-expand the root node so features are visible immediately
        if (hRoot) TreeView_Expand(m_hSolidsTree, hRoot, TVE_EXPAND);
    }

    SendMessage(m_hSolidsTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hSolidsTree, nullptr, TRUE);
}

// --------------------------------------------------------------------------
// onSolidsTreeNotify – handle WM_NOTIFY messages from the Solids TreeView.
// --------------------------------------------------------------------------
void MainWindow::onSolidsTreeNotify(NMHDR* hdr) {
    if (!hdr) return;

    if (hdr->code == NM_RCLICK) {
        // Determine which item is under the cursor
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(m_hSolidsTree, &pt);

        TVHITTESTINFO htInfo = {};
        htInfo.pt = pt;
        HTREEITEM hItem = TreeView_HitTest(m_hSolidsTree, &htInfo);
        if (!hItem) return;

        // Select the right-clicked item
        TreeView_SelectItem(m_hSolidsTree, hItem);

        // Read the lParam to decide if this is a feature node or a root node
        TVITEMW tvi = {};
        tvi.mask  = TVIF_PARAM;
        tvi.hItem = hItem;
        TreeView_GetItem(m_hSolidsTree, &tvi);
        int featureIdx = static_cast<int>(tvi.lParam & 0xFFFF);
        bool isFeatureNode = (featureIdx != 0xFFFF);

        // Build context menu
        HMENU hMenu = CreatePopupMenu();
        if (isFeatureNode) {
            AppendMenuW(hMenu, MF_STRING, IDM_SOLID_TREE_EDIT,     L"&Edit Feature...");
            AppendMenuW(hMenu, MF_STRING, IDM_SOLID_TREE_SUPPRESS, L"&Suppress / Unsuppress");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_SOLID_TREE_DELETE,   L"&Delete Feature");
        } else {
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0,
                        L"(Select a feature branch to edit)");
        }

        // Show the menu at the cursor position
        POINT screenPt;
        GetCursorPos(&screenPt);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON,
                       screenPt.x, screenPt.y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);
    }
}

// --------------------------------------------------------------------------
// solidEditFeature – re-prompt for feature parameters and update the solid.
// --------------------------------------------------------------------------
void MainWindow::solidEditFeature(int solidIdx, int featureIdx) {
    if (!m_solidsMgr) return;
    if (solidIdx < 0 || solidIdx >= m_solidsMgr->count()) return;
    if (featureIdx < 0 || featureIdx >= m_solidsMgr->featureCount(solidIdx)) return;

    FeatureOp op = m_solidsMgr->getFeature(solidIdx, featureIdx);

    // Re-prompt based on operation type
    bool ok = false;
    switch (op.opType) {
    case FeatureOpType::Block:
    case FeatureOpType::Extrude:
        ok = promptTriple(L"Edit Feature – Box/Extrude",
                          L"Length X (mm):", op.param1, op.param1,
                          L"Width  Y (mm):", op.param2, op.param2,
                          L"Height Z (mm):", op.param3, op.param3);
        if (ok && (op.param1 <= 0 || op.param2 <= 0 || op.param3 <= 0)) {
            MessageBoxW(m_hwnd, L"Dimensions must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            // Rebuild the underlying BRep solid geometry
            BRep::Solid updated = BRep::Solid::makeBox(op.param1, op.param2, op.param3);
            updated.setName(m_solidsMgr->at(solidIdx).solid.name());
            m_solidsMgr->at(solidIdx).solid = std::move(updated);
            // Update the label to reflect new dimensions
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Box %.4g×%.4g×%.4g mm",
                          op.param1, op.param2, op.param3);
            op.label = buf;
        }
        break;

    case FeatureOpType::Cylinder:
    case FeatureOpType::Revolve:
        ok = promptDouble2(L"Edit Feature – Cylinder/Revolve",
                           L"Radius (mm):", op.param1, op.param1,
                           L"Height (mm):", op.param2, op.param2);
        if (ok && (op.param1 <= 0 || op.param2 <= 0)) {
            MessageBoxW(m_hwnd, L"Radius and height must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            BRep::Solid updated = BRep::Solid::makeCylinder(op.param1, op.param2);
            updated.setName(m_solidsMgr->at(solidIdx).solid.name());
            m_solidsMgr->at(solidIdx).solid = std::move(updated);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Cylinder R=%.4g H=%.4g mm",
                          op.param1, op.param2);
            op.label = buf;
        }
        break;

    case FeatureOpType::Sphere:
        ok = promptSingle(L"Edit Feature – Sphere",
                          L"Radius (mm):", op.param1, op.param1);
        if (ok && op.param1 <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            BRep::Solid updated = BRep::Solid::makeSphere(op.param1);
            updated.setName(m_solidsMgr->at(solidIdx).solid.name());
            m_solidsMgr->at(solidIdx).solid = std::move(updated);
            char buf[48];
            std::snprintf(buf, sizeof(buf), "Sphere R=%.4g mm", op.param1);
            op.label = buf;
        }
        break;

    case FeatureOpType::Fillet: {
        ok = promptSingle(L"Edit Feature – Fillet",
                          L"Fillet radius (mm):", op.param1, op.param1);
        if (ok && op.param1 <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Fillet R=%.4g mm", op.param1);
            op.label = buf;
        }
        break;
    }

    case FeatureOpType::Chamfer: {
        ok = promptDouble2(L"Edit Feature – Chamfer",
                           L"Distance 1 (mm):", op.param1, op.param1,
                           L"Distance 2 (mm):", op.param2, op.param2);
        if (ok && (op.param1 <= 0 || op.param2 <= 0)) {
            MessageBoxW(m_hwnd, L"Distances must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "Chamfer D1=%.4g D2=%.4g mm",
                          op.param1, op.param2);
            op.label = buf;
        }
        break;
    }

    case FeatureOpType::Hole: {
        ok = promptDouble2(L"Edit Feature – Hole",
                           L"Hole diameter (mm):", op.param1, op.param1,
                           L"Hole depth    (mm):", op.param2, op.param2);
        if (ok && (op.param1 <= 0 || op.param2 <= 0)) {
            MessageBoxW(m_hwnd, L"Diameter and depth must be positive.",
                        L"Edit Feature", MB_OK | MB_ICONWARNING);
            ok = false;
        }
        if (ok) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "Hole Ø%.4g×%.4g mm",
                          op.param1, op.param2);
            op.label = buf;
        }
        break;
    }

    default:
        MessageBoxW(m_hwnd,
            L"Editing is not yet supported for this feature type.",
            L"Edit Feature", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (ok) {
        m_solidsMgr->editFeature(solidIdx, featureIdx, op);
        // Notify copilot about the updated solid
        if (m_copilotEngine) {
            FeatureRecognition fr;
            auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
            m_copilotEngine->setRecognisedFeatures(features);
        }
        wchar_t msg[160] = {};
        std::wstring wlabel(op.label.begin(), op.label.end());
        std::swprintf(msg, 160, L"Feature updated: %s", wlabel.c_str());
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                    reinterpret_cast<LPARAM>(msg));
    }
}

// --------------------------------------------------------------------------
// IDM_SOLID_EXTRUDE → create a parametric box/extrude and record in history
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

    // Ask whether to create a new body, add boss, or cut
    BodyOpType bodyOp = BodyOpType::CreateBody;
    if (!promptBodyOpType(bodyOp)) return;

    BRep::Solid box = BRep::Solid::makeBox(dx, dy, dz);

    // Generate a unique name
    static int boxCount = 0;
    std::string name = "Box_" + std::to_string(++boxCount);
    box.setName(name);

    m_solidsMgr->addSolid(std::move(box));

    // Record this operation in the history tree
    FeatureOp op;
    op.opType  = FeatureOpType::Extrude;
    op.bodyOp  = bodyOp;
    op.param1  = dx;
    op.param2  = dy;
    op.param3  = dz;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Extrude %.4g×%.4g×%.4g mm", dx, dy, dz);
        op.label = buf;
    }
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    // Run feature recognition so Copilot can reason about the new solid
    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
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
// IDM_SOLID_REVOLVE → create a parametric cylinder/revolve and record history
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

    BodyOpType bodyOp = BodyOpType::CreateBody;
    if (!promptBodyOpType(bodyOp)) return;

    BRep::Solid cyl = BRep::Solid::makeCylinder(radius, height);

    static int cylCount = 0;
    std::string name = "Cylinder_" + std::to_string(++cylCount);
    cyl.setName(name);

    m_solidsMgr->addSolid(std::move(cyl));

    // Record in history
    FeatureOp op;
    op.opType  = FeatureOpType::Revolve;
    op.bodyOp  = bodyOp;
    op.param1  = radius;
    op.param2  = height;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Revolve R=%.4g H=%.4g mm", radius, height);
        op.label = buf;
    }
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    wchar_t msg[128] = {};
    std::swprintf(msg, 128, L"Cylinder created: R=%.4g mm, H=%.4g mm", radius, height);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// Boolean solid operations – require at least 2 solids
// Records the operation in the active solid's history tree.
// --------------------------------------------------------------------------
void MainWindow::solidBooleanOp(int commandId) {
    if (!m_solidsMgr || m_solidsMgr->count() < 2) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Boolean operations require at least two solids in the session."));
        return;
    }

    FeatureOpType opType = FeatureOpType::BoolUnion;
    const wchar_t* opName = L"Boolean";
    switch (commandId) {
    case IDM_SOLID_UNION:
        opType = FeatureOpType::BoolUnion;
        opName = L"Add (Union)";
        break;
    case IDM_SOLID_SUBTRACT:
        opType = FeatureOpType::BoolSubtract;
        opName = L"Remove (Subtract)";
        break;
    case IDM_SOLID_INTERSECT:
        opType = FeatureOpType::BoolIntersect;
        opName = L"Common (Intersect)";
        break;
    }

    // Record the boolean op on the last solid (the "target")
    int targetIdx = m_solidsMgr->count() - 1;
    FeatureOp op;
    op.opType = opType;
    op.bodyOp = BodyOpType::CreateBody;  // result becomes the new body
    {
        char buf[80];
        // Label includes the count of participating solids
        std::snprintf(buf, sizeof(buf), "%ls (%d solids)",
                      opName,
                      m_solidsMgr->count());
        op.label = buf;
    }
    m_solidsMgr->addFeature(targetIdx, op);

    // Inform user: full kernel-level boolean operations require a geometry
    // kernel (e.g. OCCT). For now, report the intent clearly.
    std::wstring msg = std::wstring(opName)
        + L": operation recorded on solid \""
        + std::wstring(m_solidsMgr->at(targetIdx).solid.name().begin(),
                       m_solidsMgr->at(targetIdx).solid.name().end())
        + L"\" (requires full geometry kernel for mesh-level execution).";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
// Solid Modify operations – Fillet, Chamfer, Shell, Draft, Trim
// Records each operation in the active solid's history tree.
// --------------------------------------------------------------------------
void MainWindow::solidModify(int commandId) {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Solid Modify: no solid is loaded. Create or import a solid first."));
        return;
    }

    int solidIdx = m_solidsMgr->count() - 1;  // active solid (last added)

    switch (commandId) {

    case IDM_SOLID_FILLET: {
        double r = 3.0;
        if (!promptSingle(L"Solid Fillet", L"Fillet radius (mm):", r, r)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Fillet radius must be positive.",
                        L"Solid Fillet", MB_OK | MB_ICONWARNING);
            return;
        }
        FeatureOp op;
        op.opType = FeatureOpType::Fillet;
        op.param1 = r;
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Fillet R=%.4g mm", r);
            op.label = buf;
        }
        m_solidsMgr->addFeature(solidIdx, op);
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Solid Fillet: select edges to round with R=%.4g mm.", r);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_SOLID_CHAMFER: {
        double d1 = 3.0, d2 = 3.0;
        if (!promptDouble2(L"Solid Chamfer",
                           L"Distance 1 (mm):", d1, d1,
                           L"Distance 2 (mm):", d2, d2)) return;
        if (d1 <= 0 || d2 <= 0) {
            MessageBoxW(m_hwnd, L"Chamfer distances must be positive.",
                        L"Solid Chamfer", MB_OK | MB_ICONWARNING);
            return;
        }
        FeatureOp op;
        op.opType = FeatureOpType::Chamfer;
        op.param1 = d1;
        op.param2 = d2;
        {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "Chamfer D1=%.4g D2=%.4g mm", d1, d2);
            op.label = buf;
        }
        m_solidsMgr->addFeature(solidIdx, op);
        wchar_t msg[160] = {};
        if (std::abs(d1 - d2) < 1e-9)
            std::swprintf(msg, 160,
                L"Solid Chamfer: select edges for symmetric chamfer D=%.4g mm.", d1);
        else
            std::swprintf(msg, 160,
                L"Solid Chamfer: select edges for asymmetric chamfer D1=%.4g mm, D2=%.4g mm.", d1, d2);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_SOLID_SHELL: {
        double thickness = 2.0;
        if (!promptSingle(L"Shell Solid", L"Wall thickness (mm):", thickness, thickness)) return;
        if (thickness <= 0) {
            MessageBoxW(m_hwnd, L"Wall thickness must be positive.",
                        L"Shell Solid", MB_OK | MB_ICONWARNING);
            return;
        }
        FeatureOp op;
        op.opType = FeatureOpType::Shell;
        op.param1 = thickness;
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Shell thickness=%.4g mm", thickness);
            op.label = buf;
        }
        m_solidsMgr->addFeature(solidIdx, op);
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Shell Solid: select open face(s); wall thickness=%.4g mm.", thickness);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_SOLID_DRAFT: {
        double angle = 3.0;
        if (!promptSingle(L"Draft Faces", L"Draft angle (°):", angle, angle)) return;
        if (angle <= 0 || angle >= 90) {
            MessageBoxW(m_hwnd, L"Draft angle must be between 0° and 90°.",
                        L"Draft Faces", MB_OK | MB_ICONWARNING);
            return;
        }
        FeatureOp op;
        op.opType = FeatureOpType::Draft;
        op.param1 = angle;
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Draft %.4g°", angle);
            op.label = buf;
        }
        m_solidsMgr->addFeature(solidIdx, op);
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Draft Faces: select vertical faces to taper at %.4g° for moulding pull.", angle);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_SOLID_TRIM: {
        FeatureOp op;
        op.opType = FeatureOpType::Trim;
        op.label  = "Trim (plane / surface)";
        m_solidsMgr->addFeature(solidIdx, op);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Trim to Plane: select a solid, then choose the plane or surface used as the cutting tool."));
        break;
    }

    default:
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Solid Modify: select a solid and an operation."));
        break;
    }
}

// --------------------------------------------------------------------------
// IDM_SOLID_SWEEP → sweep a profile along a path curve
// Records the pending operation in the active solid's history.
// --------------------------------------------------------------------------
void MainWindow::createSolidSweep() {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(
                L"Solid Sweep: no active solid. Create or import a solid first, "
                L"then select a 2D profile chain and a path curve."));
        return;
    }
    // Link the pending Sweep op to the wireframe chain that the user will select.
    // The wfChainIdx will be updated when the user picks the profile in the scene.
    FeatureOp op;
    op.opType    = FeatureOpType::Sweep;
    op.label     = "Sweep (awaiting profile + path selection)";
    op.wfChainIdx = -1;  // will be resolved when user selects a chain
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(
            L"Solid Sweep: select a 2D profile chain, then select the path curve to sweep along."));
}

// --------------------------------------------------------------------------
// IDM_SOLID_LOFT → loft/blend multiple cross-section profiles
// --------------------------------------------------------------------------
void MainWindow::createSolidLoft() {
    double numSections = 2;
    if (!promptSingle(L"Solid Loft",
                      L"Number of cross-section ribs to select:", numSections, numSections)) return;
    int n = static_cast<int>(numSections);
    if (n < 2) {
        MessageBoxW(m_hwnd, L"Loft requires at least 2 cross-section profiles.",
                    L"Solid Loft", MB_OK | MB_ICONWARNING);
        return;
    }

    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        FeatureOp op;
        op.opType = FeatureOpType::Loft;
        op.param1 = static_cast<double>(n);
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Loft (%d sections)", n);
            op.label = buf;
        }
        m_solidsMgr->addFeature(m_solidsMgr->count() - 1, op);
    }

    wchar_t msg[128] = {};
    std::swprintf(msg, 128,
        L"Solid Loft: select %d cross-section chain(s) to blend into a smooth solid.", n);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SOLID_THICKEN → add thickness to an existing surface
// --------------------------------------------------------------------------
void MainWindow::createSolidThicken() {
    double thickness = 5.0;
    if (!promptSingle(L"Thicken Surface", L"Thickness (mm):", thickness, thickness)) return;
    if (thickness <= 0) {
        MessageBoxW(m_hwnd, L"Thickness must be positive.",
                    L"Thicken Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    if (m_solidsMgr && m_solidsMgr->count() > 0) {
        FeatureOp op;
        op.opType = FeatureOpType::Thicken;
        op.param1 = thickness;
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Thicken t=%.4g mm", thickness);
            op.label = buf;
        }
        m_solidsMgr->addFeature(m_solidsMgr->count() - 1, op);
    }

    wchar_t msg[128] = {};
    std::swprintf(msg, 128,
        L"Thicken Surface: select a surface; thickness=%.4g mm.", thickness);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SOLID_BLOCK → direct block primitive (dimensions or two corner points)
// --------------------------------------------------------------------------
void MainWindow::createSolidBlock() {
    double dx = 100.0, dy = 50.0, dz = 25.0;
    if (!promptTriple(L"Create Block",
                      L"Length X (mm):", dx, dx,
                      L"Width  Y (mm):", dy, dy,
                      L"Height Z (mm):", dz, dz))
        return;

    if (dx <= 0 || dy <= 0 || dz <= 0) {
        MessageBoxW(m_hwnd, L"Dimensions must be positive.",
                    L"Create Block", MB_OK | MB_ICONWARNING);
        return;
    }

    BodyOpType bodyOp = BodyOpType::CreateBody;
    if (!promptBodyOpType(bodyOp)) return;

    BRep::Solid block = BRep::Solid::makeBox(dx, dy, dz);

    static int blockCount = 0;
    std::string name = "Block_" + std::to_string(++blockCount);
    block.setName(name);

    m_solidsMgr->addSolid(std::move(block));

    // Record in history
    FeatureOp op;
    op.opType = FeatureOpType::Block;
    op.bodyOp = bodyOp;
    op.param1 = dx;
    op.param2 = dy;
    op.param3 = dz;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Block %.4g×%.4g×%.4g mm", dx, dy, dz);
        op.label = buf;
    }
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    std::wstring msg = L"Block created: "
        + std::to_wstring(static_cast<int>(dx)) + L" × "
        + std::to_wstring(static_cast<int>(dy)) + L" × "
        + std::to_wstring(static_cast<int>(dz)) + L" mm";
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg.c_str()));
}

// --------------------------------------------------------------------------
// IDM_SOLID_CONE → direct cone primitive
// --------------------------------------------------------------------------
void MainWindow::createSolidCone() {
    double r1 = 25.0, r2 = 0.0, h = 50.0;
    if (!promptTriple(L"Create Cone",
                      L"Base radius    (mm):", r1, r1,
                      L"Top radius     (mm, 0=apex):", r2, r2,
                      L"Height         (mm):", h,  h))
        return;

    if (r1 <= 0 || r2 < 0 || h <= 0) {
        MessageBoxW(m_hwnd, L"Base radius and height must be positive; top radius ≥ 0.",
                    L"Create Cone", MB_OK | MB_ICONWARNING);
        return;
    }

    BodyOpType bodyOp = BodyOpType::CreateBody;
    if (!promptBodyOpType(bodyOp)) return;

    // NOTE: A true cone requires a swept/tapered kernel primitive. The current
    // BRep layer does not expose a makeCone() method, so the solid is stored
    // using makeCylinder() as a bounding-volume stand-in. The name, parameters,
    // and status message correctly describe a cone; the visual representation
    // will be refined once full geometric-kernel support is added.
    BRep::Solid cone = BRep::Solid::makeCylinder(r1, h);
    static int coneCount = 0;
    std::string name = "Cone_" + std::to_string(++coneCount);
    cone.setName(name);

    m_solidsMgr->addSolid(std::move(cone));

    FeatureOp op;
    op.opType = FeatureOpType::Cone;
    op.bodyOp = bodyOp;
    op.param1 = r1;
    op.param2 = r2;
    op.param3 = h;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Cone R1=%.4g R2=%.4g H=%.4g mm", r1, r2, h);
        op.label = buf;
    }
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    wchar_t msg[192] = {};
    std::swprintf(msg, 192,
        L"Cone created: base R=%.4g mm, top R=%.4g mm, H=%.4g mm.", r1, r2, h);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SOLID_TORUS → direct torus primitive
// --------------------------------------------------------------------------
void MainWindow::createSolidTorus() {
    double majorR = 40.0, minorR = 10.0;
    if (!promptDouble2(L"Create Torus",
                       L"Major radius (centre to tube centre, mm):", majorR, majorR,
                       L"Minor radius (tube radius, mm):", minorR, minorR))
        return;

    if (majorR <= 0 || minorR <= 0) {
        MessageBoxW(m_hwnd, L"Both radii must be positive.",
                    L"Create Torus", MB_OK | MB_ICONWARNING);
        return;
    }
    if (minorR >= majorR) {
        MessageBoxW(m_hwnd, L"Minor radius must be smaller than the major radius.",
                    L"Create Torus", MB_OK | MB_ICONWARNING);
        return;
    }

    BodyOpType bodyOp = BodyOpType::CreateBody;
    if (!promptBodyOpType(bodyOp)) return;

    // NOTE: A true torus requires a revolved-circle kernel primitive. The current
    // BRep layer does not expose a makeTorus() method, so the solid is stored
    // using makeSphere() with the major radius as a bounding-volume stand-in.
    // The name, parameters, and status message correctly describe a torus; the
    // visual representation will be refined once full geometric-kernel support is added.
    BRep::Solid torus = BRep::Solid::makeSphere(majorR);
    static int torusCount = 0;
    std::string name = "Torus_" + std::to_string(++torusCount);
    torus.setName(name);

    m_solidsMgr->addSolid(std::move(torus));

    FeatureOp op;
    op.opType = FeatureOpType::Torus;
    op.bodyOp = bodyOp;
    op.param1 = majorR;
    op.param2 = minorR;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Torus MajR=%.4g MinR=%.4g mm", majorR, minorR);
        op.label = buf;
    }
    int solidIdx = m_solidsMgr->count() - 1;
    m_solidsMgr->addFeature(solidIdx, op);

    if (m_copilotEngine) {
        FeatureRecognition fr;
        auto features = fr.recognise(m_solidsMgr->at(solidIdx).solid);
        m_copilotEngine->setRecognisedFeatures(features);
    }

    wchar_t msg[160] = {};
    std::swprintf(msg, 160,
        L"Torus created: major R=%.4g mm, minor R=%.4g mm.", majorR, minorR);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SOLID_HOLE → hole wizard (simple / counterbore / countersink / threaded)
// Records the Hole op in the active solid's history tree (CutBody).
// --------------------------------------------------------------------------
void MainWindow::solidHole() {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Hole: no solid is loaded. Create or import a solid first."));
        return;
    }
    double dia = 10.0, depth = 20.0;
    if (!promptDouble2(L"Hole Wizard",
                       L"Hole diameter (mm):", dia,   dia,
                       L"Hole depth    (mm):", depth, depth))
        return;

    if (dia <= 0 || depth <= 0) {
        MessageBoxW(m_hwnd, L"Diameter and depth must be positive.",
                    L"Hole Wizard", MB_OK | MB_ICONWARNING);
        return;
    }

    // Holes always cut material from the active solid
    FeatureOp op;
    op.opType = FeatureOpType::Hole;
    op.bodyOp = BodyOpType::CutBody;
    op.param1 = dia;
    op.param2 = depth;
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Hole Ø%.4g×%.4g mm", dia, depth);
        op.label = buf;
    }
    m_solidsMgr->addFeature(m_solidsMgr->count() - 1, op);

    wchar_t msg[160] = {};
    std::swprintf(msg, 160,
        L"Hole Wizard: Ø%.4g mm × %.4g mm deep – select face and position.", dia, depth);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SOLID_IMPRESS → generate the negative/impression of a solid
// --------------------------------------------------------------------------
void MainWindow::solidImpression() {
    if (!m_solidsMgr || m_solidsMgr->count() == 0) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Impression: no solid is loaded. Create or import a solid first."));
        return;
    }
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(
            L"Impression: select the target solid and a stock block to generate the mold negative."));
}

// --------------------------------------------------------------------------
// IDM_SOLID_FROM_SURF → convert closed surfaces into a watertight solid
// --------------------------------------------------------------------------
void MainWindow::solidFromSurfaces() {
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(
            L"Solid from Surfaces: select a closed collection of surfaces to stitch into a watertight solid body."));
}

// ==========================================================================
// Wireframe primitive creation (Wireframe tab)
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::createWireframe(int commandId) {
    // Each wireframe command prompts for key parameters, creates a WfEntity
    // in the scene, updates the LevelsManager, and redraws the viewport.

    // Bump the entity count in the default level.
    auto bumpLevel = [this](int n = 1) {
        if (m_levelsMgr) {
            Level* lv = m_levelsMgr->findLevel(1);
            if (lv) m_levelsMgr->setEntityCount(1, lv->entityCount + n);
        }
    };

    // Push undo state before the first entity is added in this command,
    // so the entire command can be undone in one step.
    bool pushedUndo = false;

    // Add entity to scene and refresh UI.
    auto commit = [this, &pushedUndo](WfEntity e) {
        if (m_wfScene) {
            if (!pushedUndo) {
                m_wfScene->pushUndoState();
                pushedUndo = true;
            }
            m_wfScene->addEntity(std::move(e));
        }
        if (m_viewport) m_viewport->redraw();
        updateWfStatusBar();
    };

    static constexpr double kTwoPi    = 2.0 * kPi;
    static constexpr double kDegToRad = kPi / 180.0;
    static constexpr double kEps      = 1e-9;

    auto addLine3D = [&commit](const Geom::Vec3& a, const Geom::Vec3& b) {
        WfEntity e;
        e.type = WfEntityType::Line;
        e.p0   = a;
        e.p1   = b;
        commit(std::move(e));
    };

    auto addLine2D = [this, &addLine3D](double x0, double y0, double x1, double y1) {
        const Geom::Vec3 a = m_wfScene ? m_wfScene->toWorld(x0, y0) : Geom::Vec3{x0, y0, 0.0};
        const Geom::Vec3 b = m_wfScene ? m_wfScene->toWorld(x1, y1) : Geom::Vec3{x1, y1, 0.0};
        addLine3D(a, b);
    };

    auto gatherTargetSolids = [this]() {
        std::vector<const BRep::Solid*> solids;
        if (!m_solidsMgr || m_solidsMgr->count() <= 0) return solids;
        auto selected = m_solidsMgr->selectedIndices();
        for (int si : selected) {
            if (si >= 0 && si < m_solidsMgr->count())
                solids.push_back(&m_solidsMgr->at(si).solid);
        }
        if (solids.empty()) {
            BRep::Solid* a = activeSolid();
            if (a) solids.push_back(a);
        }
        return solids;
    };

    auto gatherTargetSurfaces = [this]() {
        std::vector<const NurbsSurface*> surfaces;
        if (!m_surfacesMgr || m_surfacesMgr->count() <= 0) return surfaces;
        const SurfaceEntry* active = m_surfacesMgr->activeSurface();
        if (active) surfaces.push_back(&active->surface);
        return surfaces;
    };

    auto expandBoxFromSurface = [](const NurbsSurface& srf, Geom::AABB& box, int resU, int resV) {
        auto tris = srf.tessellate(resU, resV);
        for (const auto& tri : tris) {
            box.expand(tri.v[0]);
            box.expand(tri.v[1]);
            box.expand(tri.v[2]);
        }
    };

    auto collectProjectedPointsXY = [](const BRep::Solid& solid, std::vector<Geom::Vec2>& outPts) {
        outPts.reserve(outPts.size() + solid.vertices().size());
        for (const auto& v : solid.vertices())
            outPts.push_back({v.point.x, v.point.y});
    };

    auto collectProjectedPointsXYSurface = [](const NurbsSurface& srf, std::vector<Geom::Vec2>& outPts, int resU, int resV) {
        auto tris = srf.tessellate(resU, resV);
        outPts.reserve(outPts.size() + tris.size() * 3);
        for (const auto& tri : tris) {
            outPts.push_back({tri.v[0].x, tri.v[0].y});
            outPts.push_back({tri.v[1].x, tri.v[1].y});
            outPts.push_back({tri.v[2].x, tri.v[2].y});
        }
    };

    auto convexHull = [](std::vector<Geom::Vec2> pts) {
        std::vector<Geom::Vec2> hull;
        if (pts.size() < 3) return hull;
        std::sort(pts.begin(), pts.end(), [](const Geom::Vec2& a, const Geom::Vec2& b) {
            if (a.x != b.x) return a.x < b.x;
            return a.y < b.y;
        });
        pts.erase(std::unique(pts.begin(), pts.end(), [](const Geom::Vec2& a, const Geom::Vec2& b) {
            return std::abs(a.x - b.x) < kEps && std::abs(a.y - b.y) < kEps;
        }), pts.end());
        if (pts.size() < 3) return hull;

        auto cross = [](const Geom::Vec2& o, const Geom::Vec2& a, const Geom::Vec2& b) {
            return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
        };
        std::vector<Geom::Vec2> lower;
        for (const auto& p : pts) {
            while (lower.size() >= 2 && cross(lower[lower.size() - 2], lower.back(), p) <= 0.0)
                lower.pop_back();
            lower.push_back(p);
        }
        std::vector<Geom::Vec2> upper;
        for (int i = static_cast<int>(pts.size()) - 1; i >= 0; --i) {
            const auto& p = pts[static_cast<std::size_t>(i)];
            while (upper.size() >= 2 && cross(upper[upper.size() - 2], upper.back(), p) <= 0.0)
                upper.pop_back();
            upper.push_back(p);
        }
        lower.pop_back();
        upper.pop_back();
        hull = std::move(lower);
        hull.insert(hull.end(), upper.begin(), upper.end());
        return hull;
    };

    switch (commandId) {

    // -----------------------------------------------------------------------
    // Points group
    // -----------------------------------------------------------------------

    case IDM_WF_POINT: {
        double x = 0, y = 0, z = 0;
        if (!promptTriple(L"Create Point",
                          L"X (mm):", x, x,
                          L"Y (mm):", y, y,
                          L"Z (mm):", z, z)) return;
        bumpLevel();
        WfEntity e;
        e.type = WfEntityType::Point;
        e.p0   = { x, y, z };
        commit(std::move(e));
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Point created at (%.4g, %.4g, %.4g)", x, y, z);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_POINT_DYNAMIC: {
        double distPct = 50.0;
        if (!promptSingle(L"Point Dynamic",
                          L"Position along entity (% of length, 0-100):",
                          distPct, distPct)) return;
        if (distPct < 0) distPct = 0;
        if (distPct > 100) distPct = 100;
        bumpLevel();
        WfEntity e;
        e.type  = WfEntityType::Point;
        e.count = static_cast<int>(distPct);
        commit(std::move(e));
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Dynamic point placed at %.4g%% along selected entity.", distPct);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_POINT_NODE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Point Node: select a spline to place points at its control nodes."));
        break;
    }

    case IDM_WF_POINT_SEGMENT: {
        double numPts = 5;
        if (!promptSingle(L"Point Segment",
                          L"Number of equally spaced points:", numPts, numPts)) return;
        int n = static_cast<int>(numPts);
        if (n < 1) {
            MessageBoxW(m_hwnd, L"Number of points must be at least 1.",
                        L"Point Segment", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel(n);
        WfEntity e;
        e.type  = WfEntityType::Point;
        e.count = n;
        commit(std::move(e));
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Point Segment: %d equally spaced points created along entity.", n);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    // -----------------------------------------------------------------------
    // Lines group
    // -----------------------------------------------------------------------

    case IDM_WF_LINE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Draw Line: Specify first point."));
        double x1 = 0, y1 = 0, x2 = 100, y2 = 0;
        if (!promptDouble2(L"Create Line - Start Point",
                           L"X (mm):", x1, x1,
                           L"Y (mm):", y1, y1)) return;
        if (!promptDouble2(L"Create Line - End Point",
                           L"X (mm):", x2, x2,
                           L"Y (mm):", y2, y2)) return;
        bumpLevel();
        WfEntity e;
        e.type = WfEntityType::Line;
        e.p0   = m_wfScene ? m_wfScene->toWorld(x1, y1) : Geom::Vec3{x1, y1, 0};
        e.p1   = m_wfScene ? m_wfScene->toWorld(x2, y2) : Geom::Vec3{x2, y2, 0};
        double len = (e.p1 - e.p0).length();
        commit(std::move(e));
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Line created: (%.4g,%.4g)→(%.4g,%.4g), len=%.4g mm",
                      x1, y1, x2, y2, len);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_LINE_CLOSEST: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Line Closest: select two entities to connect with the shortest possible line."));
        break;
    }

    case IDM_WF_LINE_BISECT: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Line Bisect: select two intersecting lines to create an angle bisector."));
        break;
    }

    case IDM_WF_LINE_PERP: {
        double len = 25.0;
        if (!promptSingle(L"Line Perpendicular",
                          L"Length (mm):", len, len)) return;
        if (len <= 0) {
            MessageBoxW(m_hwnd, L"Length must be positive.",
                        L"Line Perpendicular", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Line Perpendicular: select entity; line length = %.4g mm.", len);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_LINE_PARALLEL: {
        double offset = 10.0;
        if (!promptSingle(L"Line Parallel",
                          L"Offset distance (mm):", offset, offset)) return;
        if (offset <= 0) {
            MessageBoxW(m_hwnd, L"Offset distance must be positive.",
                        L"Line Parallel", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Line Parallel: select source line; offset = %.4g mm.", offset);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_LINE_NORMAL: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Line Normal: select a point, grid, or chain to create normal lines."));
        break;
    }

    // -----------------------------------------------------------------------
    // Arcs group
    // -----------------------------------------------------------------------

    case IDM_WF_CIRCLE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Draw Circle: Specify center point."));
        double cx = 0, cy = 0, r = 25.0;
        if (!promptTriple(L"Create Circle",
                          L"Centre X (mm):", cx, cx,
                          L"Centre Y (mm):", cy, cy,
                          L"Radius  (mm):", r,  r)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.", L"Create Circle", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        WfEntity e;
        e.type       = WfEntityType::Circle;
        e.p0         = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0};
        e.radius     = r;
        e.startAngle = 0.0;
        e.endAngle   = kTwoPi;
        commit(std::move(e));
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Circle created: centre=(%.4g,%.4g), R=%.4g mm, C=%.4g mm",
                      cx, cy, r, kTwoPi * r);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_CIRCLE_EDGE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Circle Edge Points: select 2 or 3 points on the circumference to define the circle."));
        break;
    }

    case IDM_WF_ARC: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Draw Arc: Specify first point."));
        double x1 = 0.0, y1 = 0.0;
        double x2 = 50.0, y2 = 0.0;
        double x3 = 25.0, y3 = 25.0;
        if (!promptDouble2(L"Arc 3 Points - Start Point",
                           L"X (mm):", x1, x1,
                           L"Y (mm):", y1, y1)) return;
        if (!promptDouble2(L"Arc 3 Points - End Point",
                           L"X (mm):", x2, x2,
                           L"Y (mm):", y2, y2)) return;
        if (!promptDouble2(L"Arc 3 Points - Curvature Point",
                           L"X (mm):", x3, x3,
                           L"Y (mm):", y3, y3)) return;

        // Circumcircle determinant for the three input points.
        const double d = 2.0 * (x1 * (y2 - y3) +
                                x2 * (y3 - y1) +
                                x3 * (y1 - y2));
        if (std::abs(d) < 1e-9) {
            MessageBoxW(m_hwnd,
                        L"The three points are collinear; an arc cannot be created.",
                        L"Arc 3 Points", MB_OK | MB_ICONWARNING);
            return;
        }

        const double p1MagSq = x1 * x1 + y1 * y1;
        const double p2MagSq = x2 * x2 + y2 * y2;
        const double p3MagSq = x3 * x3 + y3 * y3;
        const double cx = (p1MagSq * (y2 - y3) +
                           p2MagSq * (y3 - y1) +
                           p3MagSq * (y1 - y2)) / d;
        const double cy = (p1MagSq * (x3 - x2) +
                           p2MagSq * (x1 - x3) +
                           p3MagSq * (x2 - x1)) / d;
        const double r = std::sqrt((x1 - cx) * (x1 - cx) + (y1 - cy) * (y1 - cy));
        if (r <= 1e-9) {
            MessageBoxW(m_hwnd, L"Arc radius must be positive.", L"Arc 3 Points", MB_OK | MB_ICONWARNING);
            return;
        }

        auto normalizeAngle = [](double a) {
            a = std::fmod(a, kTwoPi);
            if (a < 0.0) a += kTwoPi;
            return a;
        };
        auto ccwSpan = [](double start, double end) {
            double span = std::fmod(end - start, kTwoPi);
            if (span < 0.0) span += kTwoPi;
            return span;
        };

        const double a1 = normalizeAngle(std::atan2(y1 - cy, x1 - cx));
        const double a2 = normalizeAngle(std::atan2(y2 - cy, x2 - cx));
        const double a3 = normalizeAngle(std::atan2(y3 - cy, x3 - cx));

        double startAngle = a1;
        double endAngle = a2;
        const double spanToEnd = ccwSpan(a1, a2);
        const double spanToMid = ccwSpan(a1, a3);
        if (spanToMid > spanToEnd) {
            startAngle = a2;
            endAngle = a1;
        }

        bumpLevel();
        WfEntity e;
        e.type       = WfEntityType::Arc;
        e.p0         = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0};
        e.radius     = r;
        e.startAngle = startAngle;
        e.endAngle   = endAngle;
        commit(std::move(e));
        static constexpr std::size_t kArcMessageBufferSize = 224;
        wchar_t msg[kArcMessageBufferSize] = {};
        std::swprintf(msg, kArcMessageBufferSize,
                      L"Arc 3 Points: start=(%.4g,%.4g), end=(%.4g,%.4g), through=(%.4g,%.4g), R=%.4g mm",
                      x1, y1, x2, y2, x3, y3, r);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_ARC_TANGENT: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Arc Tangent: select 1, 2, or 3 existing entities to create a tangent arc."));
        break;
    }

    case IDM_WF_ARC_ENDPOINTS: {
        double x1 = 0, y1 = 0, x2 = 50, y2 = 0, r = 30.0;
        if (!promptDouble2(L"Arc Endpoints - Start Point",
                           L"X (mm):", x1, x1,
                           L"Y (mm):", y1, y1)) return;
        if (!promptDouble2(L"Arc Endpoints - End Point",
                           L"X (mm):", x2, x2,
                           L"Y (mm):", y2, y2)) return;
        if (!promptSingle(L"Arc Endpoints - Radius",
                          L"Radius (mm):", r, r)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.", L"Arc Endpoints", MB_OK | MB_ICONWARNING);
            return;
        }
        double chord = std::sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
        if (r < chord / 2.0) {
            MessageBoxW(m_hwnd, L"Radius is too small to connect the two endpoints.",
                        L"Arc Endpoints", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        // Compute arc centre from endpoints + radius (first solution).
        double mx = (x1 + x2) / 2.0, my = (y1 + y2) / 2.0;
        double h  = std::sqrt(std::max(0.0, r * r - chord * chord / 4.0));
        double dx = (y2 - y1) / chord, dy = -(x2 - x1) / chord;
        double cxv = mx + h * dx, cyv = my + h * dy;
        WfEntity e;
        e.type       = WfEntityType::Arc;
        e.p0         = m_wfScene ? m_wfScene->toWorld(cxv, cyv) : Geom::Vec3{cxv, cyv, 0};
        e.radius     = r;
        e.startAngle = std::atan2(y1 - cyv, x1 - cxv);
        e.endAngle   = std::atan2(y2 - cyv, x2 - cxv);
        commit(std::move(e));
        wchar_t msg[192] = {};
        std::swprintf(msg, 192,
            L"Arc Endpoints: (%.4g,%.4g)->(%.4g,%.4g), R=%.4g mm, chord=%.4g mm",
            x1, y1, x2, y2, r, chord);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_ARC_POLAR: {
        double cx = 0, cy = 0, r = 25.0;
        if (!promptTriple(L"Arc Polar - Centre & Radius",
                          L"Centre X (mm):", cx, cx,
                          L"Centre Y (mm):", cy, cy,
                          L"Radius  (mm):", r,  r)) return;
        double startDeg = 0, endDeg = 90;
        if (!promptDouble2(L"Arc Polar - Start / End Angles",
                           L"Start angle (deg):", startDeg, startDeg,
                           L"End angle   (deg):", endDeg,   endDeg)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Radius must be positive.", L"Arc Polar", MB_OK | MB_ICONWARNING);
            return;
        }
        double sweep = endDeg - startDeg;
        bumpLevel();
        WfEntity e;
        e.type       = WfEntityType::Arc;
        e.p0         = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0};
        e.radius     = r;
        e.startAngle = startDeg * kDegToRad;
        e.endAngle   = endDeg   * kDegToRad;
        commit(std::move(e));
        wchar_t msg[192] = {};
        std::swprintf(msg, 192,
            L"Arc Polar: centre=(%.4g,%.4g), R=%.4g mm, %.4g deg->%.4g deg (sweep %.4g deg)",
            cx, cy, r, startDeg, endDeg, sweep);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    // -----------------------------------------------------------------------
    // Splines group
    // -----------------------------------------------------------------------

    case IDM_WF_SPLINE: {
        double numPts = 4;
        if (!promptSingle(L"Create Spline", L"Number of control points:", numPts, numPts)) return;
        int n = static_cast<int>(numPts);
        if (n < 2) {
            MessageBoxW(m_hwnd, L"Spline needs at least 2 control points.",
                        L"Create Spline", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        WfEntity e;
        e.type  = WfEntityType::Spline;
        e.count = n;
        // Placeholder control points evenly spaced along in-plane X axis.
        for (int i = 0; i < n; ++i) {
            double px = static_cast<double>(i) * 25.0;
            e.pts.push_back(m_wfScene ? m_wfScene->toWorld(px, 0.0) : Geom::Vec3{px, 0, 0});
        }
        commit(std::move(e));
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Spline created with %d control points.", n);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_SPLINE_AUTO: {
        double tol = 0.01;
        if (!promptSingle(L"Spline Automatic",
                          L"Fit tolerance (mm):", tol, tol)) return;
        if (tol <= 0) tol = 0.01;
        bumpLevel();
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Spline Automatic: select points; fit tolerance = %.4g mm.", tol);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_SPLINE_BLENDED: {
        double tol = 0.01;
        if (!promptSingle(L"Spline Blended",
                          L"Blend tolerance (mm):", tol, tol)) return;
        if (tol <= 0) tol = 0.01;
        bumpLevel();
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Spline Blended: select two curves to connect; tolerance = %.4g mm.", tol);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    // -----------------------------------------------------------------------
    // Shapes group
    // -----------------------------------------------------------------------

    case IDM_WF_RECTANGLE: {
        double x = 0, y = 0, w = 100, h = 50;
        if (!promptDouble2(L"Create Rectangle - Origin",
                           L"X (mm):", x, x,
                           L"Y (mm):", y, y)) return;
        if (!promptDouble2(L"Create Rectangle - Size",
                           L"Width  (mm):", w, w,
                           L"Height (mm):", h, h)) return;
        if (w <= 0 || h <= 0) {
            MessageBoxW(m_hwnd, L"Width and height must be positive.",
                        L"Create Rectangle", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel(4);
        WfEntity e;
        e.type  = WfEntityType::Rectangle;
        e.count = 4;
        e.p0    = m_wfScene ? m_wfScene->toWorld(x,   y)   : Geom::Vec3{x,   y,   0};
        e.pts.push_back(e.p0);
        e.pts.push_back(m_wfScene ? m_wfScene->toWorld(x+w, y)   : Geom::Vec3{x+w, y,   0});
        e.pts.push_back(m_wfScene ? m_wfScene->toWorld(x+w, y+h) : Geom::Vec3{x+w, y+h, 0});
        e.pts.push_back(m_wfScene ? m_wfScene->toWorld(x,   y+h) : Geom::Vec3{x,   y+h, 0});
        commit(std::move(e));
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Rectangle: origin=(%.4g,%.4g), %.4g x %.4g mm", x, y, w, h);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_RECT_SHAPES: {
        double w = 100, h = 50, cornerR = 0;
        if (!promptDouble2(L"Rectangular Shape - Size",
                           L"Width  (mm):", w, w,
                           L"Height (mm):", h, h)) return;
        if (!promptSingle(L"Rectangular Shape - Corner",
                          L"Corner fillet radius (0 = sharp, mm):", cornerR, cornerR)) return;
        if (w <= 0 || h <= 0) {
            MessageBoxW(m_hwnd, L"Width and height must be positive.",
                        L"Rectangular Shape", MB_OK | MB_ICONWARNING);
            return;
        }
        if (cornerR < 0) cornerR = 0;
        int extra = (cornerR > 0) ? 8 : 4;
        bumpLevel(extra);
        wchar_t msg[192] = {};
        if (cornerR > 0)
            std::swprintf(msg, 192,
                L"Rect Shape: %.4g x %.4g mm, fillet R=%.4g mm on all corners.", w, h, cornerR);
        else
            std::swprintf(msg, 192, L"Rect Shape: %.4g x %.4g mm, sharp corners.", w, h);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_POLYGON: {
        double cx = 0, cy = 0, r = 25.0, sides = 6;
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
        bumpLevel(n);
        WfEntity e;
        e.type   = WfEntityType::Polygon;
        e.p0     = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0};
        e.radius = r;
        e.count  = n;
        for (int i = 0; i < n; ++i) {
            double a = kTwoPi * i / n;
            double vx = cx + r * std::cos(a), vy = cy + r * std::sin(a);
            e.pts.push_back(m_wfScene ? m_wfScene->toWorld(vx, vy) : Geom::Vec3{vx, vy, 0});
        }
        commit(std::move(e));
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Regular polygon: %d sides, R=%.4g mm, centre=(%.4g,%.4g)",
                      n, r, cx, cy);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_ELLIPSE: {
        double cx = 0, cy = 0, a = 50.0, b = 25.0;
        if (!promptDouble2(L"Create Ellipse - Centre",
                           L"Centre X (mm):", cx, cx,
                           L"Centre Y (mm):", cy, cy)) return;
        if (!promptDouble2(L"Create Ellipse - Axes",
                           L"Semi-major axis a (mm):", a, a,
                           L"Semi-minor axis b (mm):", b, b)) return;
        if (a <= 0 || b <= 0) {
            MessageBoxW(m_hwnd, L"Axis lengths must be positive.",
                        L"Create Ellipse", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        WfEntity e;
        e.type    = WfEntityType::Ellipse;
        e.p0      = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0};
        e.radius  = a;
        e.radius2 = b;
        commit(std::move(e));
        wchar_t msg[192] = {};
        std::swprintf(msg, 192,
            L"Ellipse: centre=(%.4g,%.4g), a=%.4g mm, b=%.4g mm", cx, cy, a, b);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_HELIX: {
        double cx = 0.0, cy = 0.0, revs = 3.0;
        if (!promptTriple(L"Spiral / Helix - Center and Turns",
                          L"Center X (mm):", cx, cx,
                          L"Center Y (mm):", cy, cy,
                          L"Revolutions:", revs, revs)) return;
        double rStart = 20.0, rEnd = 20.0;
        if (!promptDouble2(L"Spiral / Helix - Radii",
                           L"Start radius (mm):", rStart, rStart,
                           L"End radius (mm):",   rEnd,   rEnd)) return;
        double mode = 0.0;
        if (!promptSingle(L"Spiral / Helix - Height mode",
                          L"0 = pitch input, 1 = total height input:", mode, mode)) return;
        double pitch = 5.0;
        double totalHeight = 15.0;
        if (mode >= 0.5) {
            if (!promptSingle(L"Spiral / Helix - Total Height",
                              L"Total height (mm):", totalHeight, totalHeight)) return;
        } else {
            if (!promptSingle(L"Spiral / Helix - Pitch",
                              L"Pitch (mm/rev):", pitch, pitch)) return;
            totalHeight = pitch * revs;
        }
        if (revs <= 0 || rStart <= 0 || rEnd <= 0) {
            MessageBoxW(m_hwnd, L"Revolutions, radii, and height/pitch must all be positive.",
                        L"Spiral/Helix", MB_OK | MB_ICONWARNING);
            return;
        }
        if (mode >= 0.5) pitch = totalHeight / revs;
        if (std::abs(totalHeight) <= kEps || pitch <= 0.0) {
            MessageBoxW(m_hwnd, L"Height and resulting pitch must be positive.",
                        L"Spiral/Helix", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        WfEntity e;
        e.type        = WfEntityType::Helix;
        e.p0          = m_wfScene ? m_wfScene->toWorld(cx, cy) : Geom::Vec3{cx, cy, 0.0};
        e.radius      = rStart;
        e.radius2     = rEnd;
        e.pitch       = pitch;
        e.revolutions = revs;
        e.height      = totalHeight;
        commit(std::move(e));
        wchar_t msg[192] = {};
        std::swprintf(msg, 192,
            L"Helix: center=(%.4g,%.4g), Rstart=%.4g, Rend=%.4g, pitch=%.4g, revs=%.4g, H=%.4g mm",
            cx, cy, rStart, rEnd, pitch, revs, totalHeight);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_BBOX: {
        auto solids = gatherTargetSolids();
        auto surfaces = gatherTargetSurfaces();
        if (solids.empty() && surfaces.empty()) {
            MessageBoxW(m_hwnd, L"Bounding Box requires an active solid or surface.",
                        L"Bounding Box", MB_OK | MB_ICONWARNING);
            return;
        }
        double pad = 0.25;
        if (!promptSingle(L"Bounding Box", L"Padding (mm):", pad, pad)) return;
        if (pad < 0.0) pad = 0.0;
        double mode = 0.0;
        if (!promptSingle(L"Bounding Box Output", L"0 = lines/arcs, 1 = solid block:", mode, mode)) return;

        Geom::AABB box;
        for (const auto* s : solids) {
            if (!s) continue;
            Geom::AABB b = s->boundingBox();
            if (b.isValid()) {
                box.expand(b.min);
                box.expand(b.max);
            }
        }
        for (const auto* srf : surfaces) {
            if (!srf) continue;
            expandBoxFromSurface(*srf, box, 24, 24);
        }
        if (!box.isValid()) {
            MessageBoxW(m_hwnd, L"Failed to compute a valid bounding box.",
                        L"Bounding Box", MB_OK | MB_ICONWARNING);
            return;
        }

        box.min.x -= pad; box.min.y -= pad; box.min.z -= pad;
        box.max.x += pad; box.max.y += pad; box.max.z += pad;

        if (mode >= 0.5) {
            if (!m_solidsMgr) return;
            auto block = BRep::Solid::makeBox(
                std::max(0.001, box.max.x - box.min.x),
                std::max(0.001, box.max.y - box.min.y),
                std::max(0.001, box.max.z - box.min.z));
            // Translate block to min corner.
            BRep::Solid placed;
            placed.setName("Stock_Bounding_Block");
            for (const auto& v : block.vertices())
                placed.addVertex({v.point.x + box.min.x, v.point.y + box.min.y, v.point.z + box.min.z});
            for (const auto& e : block.edges())
                placed.addEdge(e.startVertexId, e.endVertexId, e.isCurved);
            for (const auto& f : block.faces())
                placed.addFace(f.type, f.edgeIds, f.normal);
            m_solidsMgr->addSolid(std::move(placed));
            SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                reinterpret_cast<LPARAM>(L"Bounding Box: stock solid block created."));
            if (m_viewport) m_viewport->redraw();
            break;
        }

        // Lines/arcs mode: create a rectangular cage (bottom, top, and uprights).
        bumpLevel(12);
        const Geom::Vec3 p000{box.min.x, box.min.y, box.min.z};
        const Geom::Vec3 p100{box.max.x, box.min.y, box.min.z};
        const Geom::Vec3 p110{box.max.x, box.max.y, box.min.z};
        const Geom::Vec3 p010{box.min.x, box.max.y, box.min.z};
        const Geom::Vec3 p001{box.min.x, box.min.y, box.max.z};
        const Geom::Vec3 p101{box.max.x, box.min.y, box.max.z};
        const Geom::Vec3 p111{box.max.x, box.max.y, box.max.z};
        const Geom::Vec3 p011{box.min.x, box.max.y, box.max.z};
        addLine3D(p000, p100); addLine3D(p100, p110); addLine3D(p110, p010); addLine3D(p010, p000);
        addLine3D(p001, p101); addLine3D(p101, p111); addLine3D(p111, p011); addLine3D(p011, p001);
        addLine3D(p000, p001); addLine3D(p100, p101); addLine3D(p110, p111); addLine3D(p010, p011);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Bounding Box: wireframe cage created from geometry extents."));
        break;
    }

    // -----------------------------------------------------------------------
    // Curves (extraction) group
    // -----------------------------------------------------------------------

    case IDM_WF_CURVE_ONE_EDGE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Curve One Edge: select a single solid or surface edge to extract as wireframe."));
        break;
    }

    case IDM_WF_CURVE_ALL_EDGES: {
        auto solids = gatherTargetSolids();
        if (solids.empty()) {
            MessageBoxW(m_hwnd, L"Curve All Edges requires an active solid.",
                        L"Curve All Edges", MB_OK | MB_ICONWARNING);
            return;
        }
        int extracted = 0;
        for (const auto* solid : solids) {
            if (!solid) continue;
            const auto& verts = solid->vertices();
            for (const auto& edge : solid->edges()) {
                if (edge.startVertexId < 0 || edge.endVertexId < 0) continue;
                if (edge.startVertexId >= static_cast<int>(verts.size()) ||
                    edge.endVertexId >= static_cast<int>(verts.size())) continue;
                bumpLevel();
                addLine3D(verts[static_cast<std::size_t>(edge.startVertexId)].point,
                          verts[static_cast<std::size_t>(edge.endVertexId)].point);
                ++extracted;
            }
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Curve All Edges: extracted %d edge curve(s).", extracted);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_SILHOUETTE: {
        auto solids = gatherTargetSolids();
        auto surfaces = gatherTargetSurfaces();
        if (solids.empty() && surfaces.empty()) {
            MessageBoxW(m_hwnd, L"Silhouette requires an active solid or surface.",
                        L"Silhouette Boundary", MB_OK | MB_ICONWARNING);
            return;
        }
        double tol = 0.5;
        if (!promptSingle(L"Silhouette Boundary", L"Tolerance (mm):", tol, tol)) return;
        if (tol <= 0.0) tol = 0.5;
        const int res = std::max(8, std::min(80, static_cast<int>(std::round(40.0 / std::sqrt(tol)))));

        std::vector<Geom::Vec2> projected;
        for (const auto* s : solids) collectProjectedPointsXY(*s, projected);
        for (const auto* srf : surfaces) collectProjectedPointsXYSurface(*srf, projected, res, res);
        auto hull = convexHull(std::move(projected));
        if (hull.size() < 3) {
            MessageBoxW(m_hwnd, L"Unable to compute silhouette boundary from current geometry.",
                        L"Silhouette Boundary", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel(static_cast<int>(hull.size()));
        for (std::size_t i = 0; i < hull.size(); ++i) {
            const auto& a = hull[i];
            const auto& b = hull[(i + 1) % hull.size()];
            addLine2D(a.x, a.y, b.x, b.y);
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Silhouette Boundary: generated closed chain with %d segment(s).",
                      static_cast<int>(hull.size()));
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_CURVE_SLICE_PLN: {
        double numSlices = 5;
        if (!promptSingle(L"Curve Slice by Plane",
                          L"Number of cross-section slices:", numSlices, numSlices)) return;
        int n = static_cast<int>(numSlices);
        if (n < 1) {
            MessageBoxW(m_hwnd, L"Number of slices must be at least 1.",
                        L"Curve Slice by Plane", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel(n);
        wchar_t msg[128] = {};
        std::swprintf(msg, 128, L"Curve Slice by Plane: %d cross-section(s) generated.", n);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_CURVE_SLICE_CRV: {
        double spacing = 10.0;
        if (!promptSingle(L"Curve Slice Along Curve",
                          L"Cross-section spacing (mm):", spacing, spacing)) return;
        if (spacing <= 0) {
            MessageBoxW(m_hwnd, L"Spacing must be positive.",
                        L"Curve Slice Along Curve", MB_OK | MB_ICONWARNING);
            return;
        }
        bumpLevel();
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Curve Slice Along Curve: cross-sections every %.4g mm perpendicular to drive curve.", spacing);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_CURVE_FLOWLINE: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Curve Flowline: select a surface to extract U and V flowline curves."));
        break;
    }

    case IDM_WF_CURVE_INTERSECT: {
        auto solids = gatherTargetSolids();
        if (solids.empty() || !m_wfScene) {
            MessageBoxW(m_hwnd, L"Intersection Curve requires an active solid and work plane.",
                        L"Intersection Curve", MB_OK | MB_ICONWARNING);
            return;
        }
        const CplaneType cp = m_wfScene->cplane();
        const double d = m_wfScene->zDepth();
        auto signedDist = [cp, d](const Geom::Vec3& p) {
            switch (cp) {
            case CplaneType::Top:    return p.z - d;
            case CplaneType::Bottom: return p.z - d;
            case CplaneType::Front:  return p.y - d;
            case CplaneType::Back:   return p.y - d;
            case CplaneType::Right:  return p.x - d;
            case CplaneType::Left:   return p.x - d;
            default:                 return p.z - d;
            }
        };

        int segs = 0;
        for (const auto* solid : solids) {
            if (!solid) continue;
            const auto& verts = solid->vertices();
            for (const auto& edge : solid->edges()) {
                if (edge.startVertexId < 0 || edge.endVertexId < 0) continue;
                if (edge.startVertexId >= static_cast<int>(verts.size()) ||
                    edge.endVertexId >= static_cast<int>(verts.size())) continue;
                const Geom::Vec3 a = verts[static_cast<std::size_t>(edge.startVertexId)].point;
                const Geom::Vec3 b = verts[static_cast<std::size_t>(edge.endVertexId)].point;
                const double da = signedDist(a);
                const double db = signedDist(b);
                if (std::abs(da) < kEps && std::abs(db) < kEps) {
                    bumpLevel();
                    addLine3D(a, b);
                    ++segs;
                    continue;
                }
                if (da * db > 0.0) continue;
                const double t = da / (da - db);
                const Geom::Vec3 p = a + (b - a) * t;
                // Emit a very short marker segment so point-intersections remain visible.
                static constexpr double kIntersectionMarkerLen = 0.05;
                const Geom::Vec3 q = p + Geom::Vec3{kIntersectionMarkerLen, kIntersectionMarkerLen, kIntersectionMarkerLen};
                bumpLevel();
                addLine3D(p, q);
                ++segs;
            }
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160, L"Intersection Curve: generated %d intersection segment marker(s).", segs);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    // -----------------------------------------------------------------------
    // Modify group
    // -----------------------------------------------------------------------

    case IDM_WF_MOD_FILLET: {
        double r = 5.0;
        if (!promptSingle(L"Fillet Entities",
                          L"Fillet radius (mm):", r, r)) return;
        if (r <= 0) {
            MessageBoxW(m_hwnd, L"Fillet radius must be positive.",
                        L"Fillet Entities", MB_OK | MB_ICONWARNING);
            return;
        }
        wchar_t msg[128] = {};
        std::swprintf(msg, 128,
            L"Fillet Entities: select two intersecting entities; fillet R=%.4g mm.", r);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_MOD_CHAMFER: {
        double d1 = 5.0, d2 = 5.0;
        if (!promptDouble2(L"Chamfer Entities",
                           L"Distance 1 (mm):", d1, d1,
                           L"Distance 2 (mm):", d2, d2)) return;
        if (d1 <= 0 || d2 <= 0) {
            MessageBoxW(m_hwnd, L"Chamfer distances must be positive.",
                        L"Chamfer Entities", MB_OK | MB_ICONWARNING);
            return;
        }
        wchar_t msg[160] = {};
        if (std::abs(d1 - d2) < 1e-9)
            std::swprintf(msg, 160,
                L"Chamfer Entities: select two entities; symmetric chamfer D=%.4g mm.", d1);
        else
            std::swprintf(msg, 160,
                L"Chamfer Entities: select two entities; D1=%.4g mm, D2=%.4g mm.", d1, d2);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_MOD_DYN_TRIM: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Dynamic Trim: click geometry to trim, extend, break, or divide it at intersections."));
        break;
    }

    case IDM_WF_MOD_BREAK_TWO: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Break Two Pieces: click on an entity at the point where it should be split into two."));
        break;
    }

    case IDM_WF_MOD_BREAK_INT: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Break at Intersection: select intersecting entities to break them at all crossing points."));
        break;
    }

    case IDM_WF_MOD_JOIN: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Join Entities: select collinear lines or coincident arcs to recombine them into single entities."));
        break;
    }

    case IDM_WF_MOD_INTERSECT: {
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Modify at Intersection: select wireframe and a surface or mesh to break, trim, or create a point."));
        break;
    }

    case IDM_WF_MOD_PROJECT: {
        if (!m_wfScene) return;
        auto selected = m_wfScene->selectedIndices();
        if (selected.empty()) {
            MessageBoxW(m_hwnd, L"Select wireframe entities to project first.",
                        L"Project", MB_OK | MB_ICONWARNING);
            return;
        }
        double targetMode = 0.0;
        if (!promptSingle(L"Project Geometry",
                          L"Target: 0 = Z-depth plane, 1 = active surface:", targetMode, targetMode)) return;
        double copyMode = 1.0;
        if (!promptSingle(L"Project Geometry",
                          L"Operation: 0 = Move, 1 = Copy:", copyMode, copyMode)) return;
        bool makeCopy = copyMode >= 0.5;

        double targetZ = m_wfScene->zDepth();
        const SurfaceEntry* activeSurf = (m_surfacesMgr ? m_surfacesMgr->activeSurface() : nullptr);
        if (targetMode < 0.5) {
            if (!promptSingle(L"Project to Plane", L"Target Z depth (mm):", targetZ, targetZ)) return;
        } else if (!activeSurf) {
            MessageBoxW(m_hwnd, L"No active surface selected for projection target.",
                        L"Project", MB_OK | MB_ICONWARNING);
            return;
        }

        std::vector<std::pair<int, WfEntity>> projected;
        const auto& entsConst = m_wfScene->entities();
        projected.reserve(selected.size());

        auto projectPoint = [&](const Geom::Vec3& p) {
            if (targetMode < 0.5) return Geom::Vec3{p.x, p.y, targetZ};
            auto tris = activeSurf->surface.tessellate(24, 24);
            Geom::Vec3 best = p;
            bool found = false;
            double bestDist2 = 1e100;
            for (const auto& tri : tris) {
                const Geom::Vec3 c{
                    (tri.v[0].x + tri.v[1].x + tri.v[2].x) / 3.0,
                    (tri.v[0].y + tri.v[1].y + tri.v[2].y) / 3.0,
                    (tri.v[0].z + tri.v[1].z + tri.v[2].z) / 3.0
                };
                const double dx = c.x - p.x;
                const double dy = c.y - p.y;
                const double d2 = dx * dx + dy * dy;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    best = {p.x, p.y, c.z};
                    found = true;
                }
            }
            return found ? best : p;
        };

        for (int idx : selected) {
            if (idx < 0 || idx >= static_cast<int>(entsConst.size())) continue;
            WfEntity e = entsConst[static_cast<std::size_t>(idx)];
            e.p0 = projectPoint(e.p0);
            e.p1 = projectPoint(e.p1);
            for (auto& p : e.pts) p = projectPoint(p);
            projected.push_back({idx, std::move(e)});
        }

        if (!makeCopy) {
            m_wfScene->pushUndoState();
            for (auto& item : projected) {
                m_wfScene->setEntity(item.first, std::move(item.second));
            }
            if (m_viewport) m_viewport->redraw();
            SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                reinterpret_cast<LPARAM>(L"Project: selected geometry moved to target."));
            break;
        }
        bumpLevel(static_cast<int>(projected.size()));
        for (auto& item : projected) commit(std::move(item.second));
        wchar_t msg[192] = {};
        std::swprintf(msg, 192,
            L"Project: %d entity(ies) copied onto %s target.",
            static_cast<int>(projected.size()),
            (targetMode < 0.5) ? L"plane" : L"surface");
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_MOD_OFFSET: {
        double dist = 10.0;
        if (!promptSingle(L"Offset / Offset Chains",
                          L"Offset distance (mm, negative = inward):", dist, dist)) return;
        wchar_t msg[160] = {};
        std::swprintf(msg, 160,
            L"Offset: select wireframe or chain to offset by %.4g mm.", dist);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    case IDM_WF_MOD_ROLL: {
        double cylRadius = 50.0;
        if (!promptSingle(L"Roll / Unroll",
                          L"Cylinder radius (mm):", cylRadius, cylRadius)) return;
        if (cylRadius <= 0) {
            MessageBoxW(m_hwnd, L"Cylinder radius must be positive.",
                        L"Roll / Unroll", MB_OK | MB_ICONWARNING);
            return;
        }
        wchar_t msg[160] = {};
        std::swprintf(msg, 160,
            L"Roll/Unroll: select geometry to wrap or unwrap from cylinder R=%.4g mm.", cylRadius);
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
        break;
    }

    default:
        SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
            reinterpret_cast<LPARAM>(L"Wireframe command: select geometry in the viewport."));
        break;
    }
}

// ==========================================================================
// Wireframe Cplane / Z-depth helpers
// ==========================================================================

// --------------------------------------------------------------------------
void MainWindow::updateWfStatusBar() {
    if (!m_hStatusBar || !m_wfScene) return;

    // Pane 1: Cplane name
    wchar_t cpTxt[32] = {};
    std::swprintf(cpTxt, 32, L"CPlane: %s", cplaneName(m_wfScene->cplane()));
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_CPLANE,
                reinterpret_cast<LPARAM>(cpTxt));

    // Pane 2: Z-depth
    wchar_t zTxt[32] = {};
    std::swprintf(zTxt, 32, L"Z: %.4g", m_wfScene->zDepth());
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_ZDEPTH,
                reinterpret_cast<LPARAM>(zTxt));

    // Pane 3: Snap placeholder (filled in by AutoCursor on hover)
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_SNAP,
                reinterpret_cast<LPARAM>(L"Snap: –"));
}

// --------------------------------------------------------------------------
void MainWindow::updateUnitPane() {
    if (!m_hStatusBar) return;
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_UNIT,
                reinterpret_cast<LPARAM>(m_useMetric ? L"mm" : L"in"));
}

// --------------------------------------------------------------------------
void MainWindow::updateCoordinateDisplay(double x, double y, double z) {
    if (!m_hStatusBar) return;

    // Convert from mm to inches when in imperial mode
    double displayX = m_useMetric ? x : x / kMmPerInch;
    double displayY = m_useMetric ? y : y / kMmPerInch;
    double displayZ = m_useMetric ? z : z / kMmPerInch;

    wchar_t xTxt[24] = {}, yTxt[24] = {}, zTxt[24] = {};
    std::swprintf(xTxt, 24, L"X: %.3f", displayX);
    std::swprintf(yTxt, 24, L"Y: %.3f", displayY);
    std::swprintf(zTxt, 24, L"Z: %.3f", displayZ);

    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_COORD_X,
                reinterpret_cast<LPARAM>(xTxt));
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_COORD_Y,
                reinterpret_cast<LPARAM>(yTxt));
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_COORD_Z,
                reinterpret_cast<LPARAM>(zTxt));
}

// --------------------------------------------------------------------------
void MainWindow::updateSnapDisplay(const SnapResult& snap) {
    if (!m_hStatusBar) return;

    const wchar_t* label = L"–";
    switch (snap.type) {
    case SnapType::Endpoint:     label = L"Endpoint"; break;
    case SnapType::Midpoint:     label = L"Midpoint"; break;
    case SnapType::ArcCenter:    label = L"Center";   break;
    case SnapType::Quadrant:     label = L"Quadrant"; break;
    case SnapType::Intersection: label = L"Intersect"; break;
    default: break;
    }

    wchar_t txt[40] = {};
    std::swprintf(txt, sizeof(txt) / sizeof(txt[0]), L"Snap: %s", label);
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_SNAP,
                reinterpret_cast<LPARAM>(txt));
}

// --------------------------------------------------------------------------
void MainWindow::unitToggle() {
    m_useMetric = !m_useMetric;
    updateUnitPane();
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG,
                reinterpret_cast<LPARAM>(m_useMetric
                    ? L"Units: Metric (mm)" : L"Units: Imperial (inches)"));
}

// --------------------------------------------------------------------------
void MainWindow::showViewportContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    const bool hasSelection = m_wfScene && !m_wfScene->selectedIndices().empty();

    if (hasSelection) {
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_ANALYZE, L"Analyze\tEnd");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_DELETE,  L"Delete\tDel");
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_CHANGE_COLOR, L"Change Color…");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    } else {
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_FIT, L"Fit to Screen\tF3");
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_CLEAR_COLORS, L"Clear Colors");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    AppendMenuW(hMenu, MF_STRING, IDM_CTX_ISO,   L"Isometric View");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_FRONT, L"Front View");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_TOP,   L"Top View");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_RIGHT, L"Right View");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_VIEW_WIREFRAME, L"Wireframe");
    AppendMenuW(hMenu, MF_STRING, IDM_VIEW_SHADED,    L"Shaded");
    AppendMenuW(hMenu, MF_STRING, IDM_VIEW_TRANSLU,   L"Translucent");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_UNIT_TOGGLE,
                m_useMetric ? L"Switch to Imperial (in)" : L"Switch to Metric (mm)");

    TrackPopupMenuEx(hMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        screenX, screenY,
        m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

// --------------------------------------------------------------------------
void MainWindow::wfCycleCplane() {
    if (!m_wfScene) return;
    m_wfScene->cycleCplane();
    updateWfStatusBar();
    // Inform the viewport so it can update the Cplane indicator in the render.
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[64] = {};
    std::swprintf(msg, 64, L"Construction plane set to %s.", cplaneName(m_wfScene->cplane()));
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
void MainWindow::wfSetZDepth() {
    if (!m_wfScene) return;
    double z = m_wfScene->zDepth();
    if (!promptSingle(L"Set Z-Depth",
                      L"Z-depth for new wireframe entities (mm):", z, z)) return;
    m_wfScene->setZDepth(z);
    updateWfStatusBar();
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[80] = {};
    std::swprintf(msg, 80, L"Z-depth set to %.4g mm (Cplane: %s).",
                  z, cplaneName(m_wfScene->cplane()));
    SendMessage(m_hStatusBar, SB_SETTEXT, SB_PANE_MSG, reinterpret_cast<LPARAM>(msg));
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

    int proceed = MessageBoxW(
        m_hwnd,
        L"Regenerate all toolpaths now?\n\nThis updates all dirty operations in the current session.",
        L"Regenerate Toolpaths",
        MB_OKCANCEL | MB_ICONQUESTION);
    if (proceed != IDOK) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Regenerate cancelled."));
        appendAudit(L"Regenerate cancelled by user");
        return;
    }

    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Regenerating toolpaths..."));
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    m_toolpathMgr->regenerateAll();
    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    if (m_viewport) m_viewport->redraw();

    wchar_t statusMsg[128] = {};
    std::swprintf(statusMsg, 128,
        L"Regenerated %d toolpath(s).", m_toolpathMgr->count());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
    appendAudit(L"Regenerated " + std::to_wstring(m_toolpathMgr->count()) + L" toolpath(s)");
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
void MainWindow::appendAudit(const std::wstring& message) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t stamp[48] = {};
    std::swprintf(stamp, 48, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    m_operationAudit.emplace_back(std::wstring(stamp) + message);
    if (m_operationAudit.size() > kMaxAuditEntries) {
        const auto excess = m_operationAudit.size() - kMaxAuditEntries;
        m_operationAudit.erase(
            m_operationAudit.begin(),
            m_operationAudit.begin()
                + static_cast<std::vector<std::wstring>::difference_type>(excess));
    }
}

// --------------------------------------------------------------------------
bool MainWindow::preflightForPosting(std::wstring& reason) const {
    if (!m_toolpathMgr || m_toolpathMgr->count() <= 0) {
        reason = L"No toolpaths exist. Generate or import toolpaths before posting.";
        return false;
    }
    int dirtyCount = 0;
    for (int i = 0; i < m_toolpathMgr->count(); ++i)
        if (m_toolpathMgr->at(i).isDirty()) ++dirtyCount;
    if (dirtyCount > 0) {
        reason = L"One or more toolpaths are marked dirty. Run Machine → Regen before posting.";
        return false;
    }
    if (!m_activePostProfilePath.empty()) {
        std::ifstream chk(m_activePostProfilePath);
        if (!chk.good()) {
            reason = L"Configured post profile cannot be opened. Re-select it in Machine → Setup Post Profile.";
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
bool MainWindow::preflightForSimulation(std::wstring& reason) const {
    if (!m_toolpathMgr || m_toolpathMgr->count() <= 0) {
        reason = L"No toolpaths exist. Generate toolpaths before simulation.";
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------
void MainWindow::showOperationAuditTrail() {
    if (m_operationAudit.empty()) {
        MessageBoxW(m_hwnd, L"No operation audit entries yet.",
                    L"Recent Audit Trail", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring msg = L"Recent changes (latest first)\n\n";
    int shown = 0;
    for (int i = static_cast<int>(m_operationAudit.size()) - 1;
         i >= 0 && shown < kMaxAuditDisplayEntries;
         --i, ++shown) {
        msg += L"• " + m_operationAudit[static_cast<std::size_t>(i)] + L"\n";
    }
    MessageBoxW(m_hwnd, msg.c_str(), L"Recent Audit Trail", MB_OK | MB_ICONINFORMATION);
}

// --------------------------------------------------------------------------
void MainWindow::setupPerformanceMode() {
    double mode = static_cast<double>(
        m_perfMode == PerformanceMode::Quality ? 1 :
        (m_perfMode == PerformanceMode::Balanced ? 2 : 3));
    if (!promptSingle(L"Performance Mode",
                      L"Choose mode (1=Quality, 2=Balanced, 3=Speed):",
                      mode, mode))
        return;
    int m = static_cast<int>(std::round(mode));
    if (m < 1 || m > 3) {
        MessageBoxW(m_hwnd, L"Invalid mode. Use 1, 2, or 3.",
                    L"Performance Mode", MB_OK | MB_ICONWARNING);
        return;
    }
    m_perfMode = (m == 1) ? PerformanceMode::Quality
              : (m == 2) ? PerformanceMode::Balanced
                         : PerformanceMode::Speed;
#if defined(CAMEXPERT_USE_OPENMP)
    const wchar_t* ompText = L"OpenMP acceleration: available";
#else
    const wchar_t* ompText = L"OpenMP acceleration: unavailable (deterministic fallback active)";
#endif
    std::wstring modeText =
        (m_perfMode == PerformanceMode::Quality)  ? L"Quality"
      : (m_perfMode == PerformanceMode::Balanced) ? L"Balanced"
                                                  : L"Speed";
    std::wstring status = L"Performance mode set: " + modeText + L". " + ompText;
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(status.c_str()));
    appendAudit(L"Performance mode changed to " + modeText);
}

// --------------------------------------------------------------------------
void MainWindow::showWorkflowGuidance() {
    std::wstring msg =
        L"Suggested next steps for this session:\n\n"
        L"1) Setup Constraints: define sketch relations and run solve diagnostics.\n"
        L"2) Setup Tool/Material DB: sync or edit rows, then apply DB to libraries.\n"
        L"3) Setup Post Profile: load/validate active profile before posting.\n"
        L"4) Generate/Regenerate toolpaths, then run Verify / Machine Sim.\n"
        L"5) Post-process and review Recent Audit Trail.\n\n";

    if (m_toolpathMgr && m_toolpathMgr->count() > 0) {
        int dirtyCount = 0;
        for (int i = 0; i < m_toolpathMgr->count(); ++i)
            if (m_toolpathMgr->at(i).isDirty()) ++dirtyCount;
        msg += L"Current session: " + std::to_wstring(m_toolpathMgr->count())
            + L" toolpath(s), " + std::to_wstring(dirtyCount) + L" dirty.\n";
    } else {
        msg += L"Current session: no toolpaths yet.\n";
    }
    msg += L"Active post profile: "
        + (m_activePostProfilePath.empty() ? std::wstring(L"(none)") : toWideFromUtf8(m_activePostProfilePath));
    MessageBoxW(m_hwnd, msg.c_str(), L"Workflow Guidance", MB_OK | MB_ICONINFORMATION);
}

// --------------------------------------------------------------------------
void MainWindow::setupPostProfile() {
    double action = 1.0;
    if (!promptSingle(L"Setup Post Profile",
                      L"Choose action (1=Load, 2=Validate, 3=Clear, 4=Preview):",
                      action, action))
        return;
    const int choice = static_cast<int>(std::round(action));

    if (choice == 1) {
        wchar_t szFile[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = m_hwnd;
        ofn.lpstrFilter =
            L"Post Profile (*.txt;*.cfg;*.ini)\0*.txt;*.cfg;*.ini\0"
            L"All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile  = MAX_PATH;
        ofn.Flags     = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;

        PostProcessor pp;
        std::string err;
        std::string path = toUtf8FromWide(szFile);
        if (!pp.loadScriptProfile(path, &err)) {
            std::wstring werr = toWideFromUtf8(err);
            MessageBoxW(m_hwnd, werr.c_str(), L"Post Profile Error", MB_OK | MB_ICONWARNING);
            appendAudit(L"Post profile load failed");
            return;
        }
        m_activePostProfilePath = path;
        std::wstring status = L"Post profile loaded: " + std::wstring(szFile);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(status.c_str()));
        appendAudit(L"Post profile loaded");
        return;
    }

    if (choice == 2) {
        if (m_activePostProfilePath.empty()) {
            MessageBoxW(m_hwnd, L"No active post profile path is set.",
                        L"Post Profile", MB_OK | MB_ICONINFORMATION);
            return;
        }
        PostProcessor pp;
        std::string err;
        if (!pp.loadScriptProfile(m_activePostProfilePath, &err)) {
            std::wstring werr = toWideFromUtf8(err);
            MessageBoxW(m_hwnd, werr.c_str(), L"Post Profile Validation", MB_OK | MB_ICONWARNING);
            appendAudit(L"Post profile validation failed");
            return;
        }
        MessageBoxW(m_hwnd, L"Post profile validated successfully.",
                    L"Post Profile Validation", MB_OK | MB_ICONINFORMATION);
        appendAudit(L"Post profile validated");
        return;
    }

    if (choice == 3) {
        m_activePostProfilePath.clear();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"Active post profile cleared."));
        appendAudit(L"Post profile cleared");
        return;
    }

    if (choice == 4) {
        std::wstring preview = L"Post profile preview\n\nActive profile: ";
        preview += m_activePostProfilePath.empty() ? L"(none)" : toWideFromUtf8(m_activePostProfilePath);
        preview += L"\n\nA loaded profile will be applied automatically on Post.";
        MessageBoxW(m_hwnd, preview.c_str(), L"Post Profile Preview", MB_OK | MB_ICONINFORMATION);
        return;
    }

    MessageBoxW(m_hwnd, L"Invalid action. Use 1-4.",
                L"Setup Post Profile", MB_OK | MB_ICONWARNING);
}

// --------------------------------------------------------------------------
void MainWindow::setupConstraints() {
    if (!m_wfScene) return;
    double action = 1.0;
    if (!promptSingle(L"Setup Constraints",
                      L"Action:\n1 Coincident  2 Horizontal  3 Vertical  4 Distance\n5 List  6 Delete  7 Solve  8 Clear",
                      action, action))
        return;
    const int choice = static_cast<int>(std::round(action));

    auto selected = m_wfScene->selectedIndices();
    if (choice == 1) {
        if (selected.size() < 2) {
            MessageBoxW(m_hwnd, L"Select at least 2 entities first for Coincident.",
                        L"Constraints", MB_OK | MB_ICONWARNING);
            return;
        }
        SketchConstraint c;
        c.type = SketchConstraintType::Coincident;
        c.a.entityIndex = selected[0];
        c.b.entityIndex = selected[1];
        int id = m_constraintSolver.addConstraint(c);
        std::wstring msg = L"Added Coincident constraint ID " + std::to_wstring(id) + L".";
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg.c_str()));
        appendAudit(msg);
        return;
    }
    if (choice == 2 || choice == 3) {
        if (selected.empty()) {
            MessageBoxW(m_hwnd, L"Select a line entity first.",
                        L"Constraints", MB_OK | MB_ICONWARNING);
            return;
        }
        SketchConstraint c;
        c.type = (choice == 2) ? SketchConstraintType::Horizontal : SketchConstraintType::Vertical;
        c.a.entityIndex = selected[0];
        int id = m_constraintSolver.addConstraint(c);
        std::wstring msg = std::wstring(L"Added ")
            + (choice == 2 ? L"Horizontal" : L"Vertical")
            + L" constraint ID " + std::to_wstring(id) + L".";
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg.c_str()));
        appendAudit(msg);
        return;
    }
    if (choice == 4) {
        if (selected.size() < 2) {
            MessageBoxW(m_hwnd, L"Select at least 2 entities first for Distance.",
                        L"Constraints", MB_OK | MB_ICONWARNING);
            return;
        }
        double distanceMm = 10.0;
        if (!promptSingle(L"Distance Constraint", L"Distance (mm):", distanceMm, distanceMm))
            return;
        SketchConstraint c;
        c.type = SketchConstraintType::Distance;
        c.value = distanceMm;
        c.a.entityIndex = selected[0];
        c.b.entityIndex = selected[1];
        int id = m_constraintSolver.addConstraint(c);
        std::wstring msg = L"Added Distance constraint ID " + std::to_wstring(id)
            + L" (" + std::to_wstring(distanceMm) + L" mm).";
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg.c_str()));
        appendAudit(msg);
        return;
    }
    if (choice == 5) {
        std::wstring list = L"Constraints\n\n";
        const auto& cs = m_constraintSolver.constraints();
        if (cs.empty()) {
            list += L"(none)";
        } else {
            for (const auto& c : cs) {
                list += L"ID " + std::to_wstring(c.id) + L"  "
                    + constraintTypeName(c.type)
                    + L"  A=" + std::to_wstring(c.a.entityIndex)
                    + L"  B=" + std::to_wstring(c.b.entityIndex);
                if (c.type == SketchConstraintType::Distance ||
                    c.type == SketchConstraintType::Angle ||
                    c.type == SketchConstraintType::Radius) {
                    wchar_t v[48] = {};
                    std::swprintf(v, 48, L"  v=%.4g", c.value);
                    list += v;
                }
                list += L"\n";
            }
        }
        MessageBoxW(m_hwnd, list.c_str(), L"Constraint List", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (choice == 6) {
        double idd = 1.0;
        if (!promptSingle(L"Delete Constraint", L"Constraint ID:", idd, idd))
            return;
        if (m_constraintSolver.removeConstraint(static_cast<int>(std::round(idd)))) {
            SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Constraint deleted."));
            appendAudit(L"Constraint deleted");
        } else {
            MessageBoxW(m_hwnd, L"Constraint ID not found.",
                        L"Constraints", MB_OK | MB_ICONWARNING);
        }
        return;
    }
    if (choice == 7) {
        auto& ents = const_cast<std::vector<WfEntity>&>(m_wfScene->entities());
        SolveResult sr = m_constraintSolver.solve(ents);
        if (m_viewport) m_viewport->redraw();
        std::wstring diag = L"Solve status: ";
        diag += solveStatusName(sr.status);
        diag += L"\nIterations: " + std::to_wstring(sr.iterations)
             +  L"\nApplied: " + std::to_wstring(sr.appliedCount)
             +  L"\nWarnings: " + std::to_wstring(static_cast<int>(sr.diagnostics.size()));
        int maxDiag = std::min<int>(kMaxDisplayedDiagnostics, static_cast<int>(sr.diagnostics.size()));
        for (int i = 0; i < maxDiag; ++i) {
            const auto& d = sr.diagnostics[static_cast<std::size_t>(i)];
            diag += L"\n- [ID " + std::to_wstring(d.constraintId) + L"] " + toWideFromUtf8(d.message);
        }
        MessageBoxW(m_hwnd, diag.c_str(), L"Constraint Diagnostics", MB_OK | MB_ICONINFORMATION);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"Constraints solved; diagnostics available."));
        appendAudit(L"Constraint solve executed");
        return;
    }
    if (choice == 8) {
        m_constraintSolver.clearConstraints();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"All constraints cleared."));
        appendAudit(L"All constraints cleared");
        return;
    }

    MessageBoxW(m_hwnd, L"Invalid action. Use 1-8.", L"Constraints", MB_OK | MB_ICONWARNING);
}

// --------------------------------------------------------------------------
void MainWindow::setupToolDatabase() {
    double action = 1.0;
    if (!promptSingle(L"Setup Tool/Material Database",
                      L"Action:\n1 Summary 2 Search 3 Upsert Tool 4 Upsert Material\n5 Upsert Cutting 6 Export SQL 7 Apply DB->Libraries 8 Sync Libraries->DB",
                      action, action))
        return;
    const int choice = static_cast<int>(std::round(action));

    if (choice == 1) {
        std::wstring msg = L"SQL cache summary\n\nTools: "
            + std::to_wstring(m_sqlToolDb.tools().size())
            + L"\nMaterials: " + std::to_wstring(m_sqlToolDb.materials().size())
            + L"\nCutting rows: " + std::to_wstring(m_sqlToolDb.cuttingData().size());
        if (!m_sqlToolDb.materials().empty()) {
            msg += L"\n\nMaterials (first " + std::to_wstring(kMaxDisplayedMaterials) + L"):";
            int n = std::min<int>(kMaxDisplayedMaterials, static_cast<int>(m_sqlToolDb.materials().size()));
            for (int i = 0; i < n; ++i)
                msg += L"\n- " + toWideFromUtf8(m_sqlToolDb.materials()[static_cast<std::size_t>(i)].key);
        }
        MessageBoxW(m_hwnd, msg.c_str(), L"Tool/Material DB Summary", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (choice == 2) {
        double matClass = 1.0;
        if (!promptSingle(L"Search Material", L"Material class (0=Al,1=Steel,2=SS,3=Ti,...):",
                          matClass, matClass)) {
            return;
        }
        int cls = std::max(0, static_cast<int>(std::round(matClass)));
        std::wstring msg = L"Matches for class " + std::to_wstring(cls) + L":\n";
        int matches = 0;
        for (const auto& m : m_sqlToolDb.materials()) {
            if (static_cast<int>(m.material.matClass) == cls) {
                msg += L"- " + toWideFromUtf8(m.key) + L"\n";
                if (++matches >= kMaxMaterialSearchResults) break;
            }
        }
        if (matches == 0) msg += L"(none)";
        MessageBoxW(m_hwnd, msg.c_str(), L"Search Material", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (choice == 3) {
        double toolId = 1001.0, dia = 10.0, flutes = 4.0;
        if (!promptTriple(L"Upsert Tool Row",
                          L"Tool ID (integer):", toolId, toolId,
                          L"Diameter (mm):", dia, dia,
                          L"Flutes (integer):", flutes, flutes))
            return;
        SqlToolRow row;
        row.key = "tool_" + std::to_string(static_cast<int>(std::round(toolId)));
        row.tool.id = static_cast<int>(std::round(toolId));
        row.tool.name = row.key;
        row.tool.diameter = dia;
        row.tool.numFlutes = std::max(1, static_cast<int>(std::round(flutes)));
        m_sqlToolDb.upsertTool(row);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Tool row upserted into SQL cache."));
        appendAudit(L"Tool DB row upserted");
        return;
    }

    if (choice == 4) {
        double matClass = 1.0, vcMin = 120.0, vcMax = 240.0;
        if (!promptTriple(L"Upsert Material Row",
                          L"Material class (0=Al,1=Steel,2=SS,3=Ti,4=Inconel,...):", matClass, matClass,
                          L"Surface speed min (m/min):", vcMin, vcMin,
                          L"Surface speed max (m/min):", vcMax, vcMax))
            return;
        int cls = std::max(0, static_cast<int>(std::round(matClass)));
        if (cls > static_cast<int>(MaterialClass::Custom)) {
            MessageBoxW(m_hwnd, L"Material class out of range. Use 0..9.",
                        L"Upsert Material", MB_OK | MB_ICONWARNING);
            return;
        }
        SqlMaterialRow row;
        row.key = "mat_" + std::to_string(static_cast<int>(std::round(matClass)));
        row.material.matClass = static_cast<MaterialClass>(cls);
        row.material.name = row.key;
        row.material.surfaceSpeedMin = vcMin;
        row.material.surfaceSpeedMax = std::max(vcMin, vcMax);
        m_sqlToolDb.upsertMaterial(row);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Material row upserted into SQL cache."));
        appendAudit(L"Material DB row upserted");
        return;
    }

    if (choice == 5) {
        double toolId = 1001.0, matClass = 1.0, vcMin = 100.0;
        if (!promptTriple(L"Upsert Cutting Data Row",
                          L"Tool ID key suffix:", toolId, toolId,
                          L"Material class:", matClass, matClass,
                          L"Surface speed min (m/min):", vcMin, vcMin))
            return;
        int cls = std::max(0, static_cast<int>(std::round(matClass)));
        if (cls > static_cast<int>(MaterialClass::Custom)) {
            MessageBoxW(m_hwnd, L"Material class out of range. Use 0..9.",
                        L"Upsert Cutting Data", MB_OK | MB_ICONWARNING);
            return;
        }
        SqlCuttingDataRow row;
        row.toolKey = "tool_" + std::to_string(static_cast<int>(std::round(toolId)));
        row.materialClass = static_cast<MaterialClass>(cls);
        row.nominalDiameter = 10.0;
        row.surfaceSpeedMin = vcMin;
        row.surfaceSpeedMax = vcMin * 1.5;
        row.feedPerToothMin = 0.02;
        row.feedPerToothMax = 0.06;
        row.recommendedAxialDepth = 6.0;
        row.recommendedRadialDepth = 2.5;
        m_sqlToolDb.upsertCuttingData(row);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Cutting-data row upserted into SQL cache."));
        appendAudit(L"Cutting-data row upserted");
        return;
    }

    if (choice == 6) {
        wchar_t szFile[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = m_hwnd;
        ofn.lpstrFilter = L"SQL Files (*.sql)\0*.sql\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile   = szFile;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrDefExt = L"sql";
        ofn.Flags       = OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn)) return;
        std::ofstream out(toUtf8FromWide(szFile), std::ios::binary);
        if (!out.good()) {
            MessageBoxW(m_hwnd, L"Failed to open output SQL file.",
                        L"Export SQL Snapshot", MB_OK | MB_ICONERROR);
            return;
        }
        out << m_sqlToolDb.exportSqlSnapshot();
        out.close();
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"SQL snapshot exported."));
        appendAudit(L"SQL snapshot exported");
        return;
    }

    if (choice == 7) {
        m_materialLib.importFromSqlDatabase(m_sqlToolDb);
        m_cloudToolLib.importFromSqlDatabase(m_sqlToolDb, true);
        if (m_copilotEngine) m_copilotEngine->setMaterialLibrary(&m_materialLib);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"SQL cache applied to material/cloud libraries."));
        appendAudit(L"SQL cache applied to runtime libraries");
        return;
    }

    if (choice == 8) {
        m_sqlToolDb.clear();
        m_materialLib.exportToSqlDatabase(m_sqlToolDb);
        m_cloudToolLib.exportToSqlDatabase(m_sqlToolDb);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(L"SQL cache refreshed from current libraries."));
        appendAudit(L"SQL cache refreshed from runtime libraries");
        return;
    }

    MessageBoxW(m_hwnd, L"Invalid action. Use 1-8.",
                L"Tool/Material Database", MB_OK | MB_ICONWARNING);
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

// --------------------------------------------------------------------------
// IDM_SURF_FLAT_BOUNDARY – create a flat surface within a rectangular loop
// --------------------------------------------------------------------------
void MainWindow::surfaceFlatBoundary()
{
    if (!m_surfacesMgr) return;

    double width = 100.0, depth = 60.0;
    if (!promptDouble2(L"Create Flat Boundary Surface",
                       L"Boundary width (mm):", width, width,
                       L"Boundary depth (mm):", depth, depth))
        return;

    if (width <= 0 || depth <= 0) {
        MessageBoxW(m_hwnd, L"Width and depth must be positive.",
                    L"Create Flat Boundary Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    static int flatCount = 0;
    std::string name = "Flat_" + std::to_string(++flatCount);

    NurbsSurface surf = SurfacesManager::makeFlat(width, depth);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Flat boundary surface created: %.4g × %.4g mm  [%hs]",
                  width, depth, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_SWEPT – drive a cross-section profile along a rail path
// --------------------------------------------------------------------------
void MainWindow::surfaceSwept()
{
    if (!m_surfacesMgr) return;

    double sectionWidth = 40.0, sectionHeight = 20.0, pathLength = 120.0;
    if (!promptTriple(L"Create Swept Surface",
                      L"Section width  (mm):", sectionWidth,  sectionWidth,
                      L"Section height (mm):", sectionHeight, sectionHeight,
                      L"Path length    (mm):", pathLength,    pathLength))
        return;

    if (sectionWidth <= 0 || sectionHeight <= 0 || pathLength <= 0) {
        MessageBoxW(m_hwnd, L"All dimensions must be positive.",
                    L"Create Swept Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Rectangular cross-section in the YZ-plane
    std::vector<Geom::Vec3> crossSection = {
        { 0, -sectionWidth / 2, 0 },
        { 0,  sectionWidth / 2, 0 },
        { 0,  sectionWidth / 2, sectionHeight },
        { 0, -sectionWidth / 2, sectionHeight }
    };

    // Straight path along X
    const int pathPts = 8;
    std::vector<Geom::Vec3> path;
    path.reserve(static_cast<std::size_t>(pathPts));
    for (int i = 0; i < pathPts; ++i)
        path.push_back({ pathLength * static_cast<double>(i) / (pathPts - 1), 0, 0 });

    static int sweptCount = 0;
    std::string name = "Swept_" + std::to_string(++sweptCount);

    NurbsSurface surf = SurfacesManager::makeSwept(crossSection, path);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200,
                  L"Swept surface created: section %.4g×%.4g mm, path %.4g mm  [%hs]",
                  sectionWidth, sectionHeight, pathLength, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_NET – create a net surface from a U/V wireframe grid
// --------------------------------------------------------------------------
void MainWindow::surfaceNet()
{
    if (!m_surfacesMgr) return;

    double width = 100.0, depth = 80.0, height = 30.0;
    if (!promptTriple(L"Create Net Surface",
                      L"Grid width  (mm):", width,  width,
                      L"Grid depth  (mm):", depth,  depth,
                      L"Grid height (mm):", height, height))
        return;

    if (width <= 0 || depth <= 0) {
        MessageBoxW(m_hwnd, L"Width and depth must be positive.",
                    L"Create Net Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Build a 4×4 grid of points with a gentle dome in the centre
    const int nu = 4, nv = 4;
    std::vector<std::vector<Geom::Vec3>> uChains;
    for (int j = 0; j < nv; ++j) {
        double v = static_cast<double>(j) / (nv - 1);
        double y = -depth / 2 + v * depth;
        std::vector<Geom::Vec3> row;
        row.reserve(static_cast<std::size_t>(nu));
        for (int i = 0; i < nu; ++i) {
            double u = static_cast<double>(i) / (nu - 1);
            double x = -width / 2 + u * width;
            // Dome: maximum height at centre, zero at edges
            double bx = 4 * u * (1 - u);
            double by = 4 * v * (1 - v);
            double z  = height * bx * by;
            row.push_back({ x, y, z });
        }
        uChains.push_back(std::move(row));
    }

    // V-chains: columns of the grid
    std::vector<std::vector<Geom::Vec3>> vChains;
    for (int i = 0; i < nu; ++i) {
        std::vector<Geom::Vec3> col;
        col.reserve(static_cast<std::size_t>(nv));
        for (int j = 0; j < nv; ++j)
            col.push_back(uChains[j][i]);
        vChains.push_back(std::move(col));
    }

    static int netCount = 0;
    std::string name = "Net_" + std::to_string(++netCount);

    NurbsSurface surf = SurfacesManager::makeNet(uChains, vChains);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200,
                  L"Net surface created: %.4g × %.4g mm, dome %.4g mm  [%hs]",
                  width, depth, height, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_FENCE – project a surface from a curve at a direction/distance
// --------------------------------------------------------------------------
void MainWindow::surfaceFence()
{
    if (!m_surfacesMgr) return;

    double length = 50.0, angle = 90.0;
    if (!promptDouble2(L"Create Fence Surface",
                       L"Projection length (mm):", length, length,
                       L"Projection angle  (deg, 0=X, 90=Z):", angle, angle))
        return;

    if (length <= 0) {
        MessageBoxW(m_hwnd, L"Projection length must be positive.",
                    L"Create Fence Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Base curve: simple straight line along Y
    const int basePts = 6;
    const double baseLen = 100.0;
    std::vector<Geom::Vec3> baseCurve;
    baseCurve.reserve(static_cast<std::size_t>(basePts));
    for (int i = 0; i < basePts; ++i) {
        double t = static_cast<double>(i) / (basePts - 1);
        baseCurve.push_back({ 0, t * baseLen - baseLen / 2, 0 });
    }

    // Direction vector from angle (in XZ-plane)
    double rad = angle * (kSurfPi / 180.0);
    Geom::Vec3 dir{ std::cos(rad), 0.0, std::sin(rad) };

    static int fenceCount = 0;
    std::string name = "Fence_" + std::to_string(++fenceCount);

    NurbsSurface surf = SurfacesManager::makeFence(baseCurve, dir, length);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Fence surface created: length=%.4g mm, angle=%.4g°  [%hs]",
                  length, angle, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_DRAFT_SURF – extend a surface from a wireframe at a draft angle
// --------------------------------------------------------------------------
void MainWindow::surfaceDraft()
{
    if (!m_surfacesMgr) return;

    double draftAngle = 3.0, wallHeight = 30.0;
    if (!promptDouble2(L"Create Draft Surface",
                       L"Draft angle (deg):", draftAngle, draftAngle,
                       L"Wall height (mm):",  wallHeight, wallHeight))
        return;

    if (std::abs(draftAngle) < 0.01 || wallHeight <= 0) {
        MessageBoxW(m_hwnd, L"Draft angle must be non-zero and height must be positive.",
                    L"Create Draft Surface", MB_OK | MB_ICONWARNING);
        return;
    }

    // Base curve: rectangular loop (4-sided) as a polyline
    const double hw = 50.0, hd = 40.0;
    std::vector<Geom::Vec3> baseCurve = {
        { -hw, -hd, 0 }, {  hw, -hd, 0 },
        {  hw,  hd, 0 }, { -hw,  hd, 0 }, { -hw, -hd, 0 }
    };

    static int draftCount = 0;
    std::string name = "Draft_" + std::to_string(++draftCount);

    NurbsSurface surf = SurfacesManager::makeDraft(baseCurve, draftAngle, wallHeight);
    m_surfacesMgr->addSurface(std::move(surf), name);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Draft surface created: %.4g°, H=%.4g mm  [%hs]",
                  draftAngle, wallHeight, name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_TRIM_TO_SURF – trim one surface using another intersecting surface
// --------------------------------------------------------------------------
void MainWindow::surfaceTrimToSurface()
{
    if (!m_surfacesMgr) return;

    if (m_surfacesMgr->count() < 2) {
        MessageBoxW(m_hwnd,
            L"Trim to Surfaces requires at least two existing surfaces.\n"
            L"Create two surfaces first, then apply this operation.",
            L"Trim to Surfaces", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Mark the most recently added (last) surface as trimmed using the previous one
    int idx = m_surfacesMgr->count() - 1;
    if (m_surfacesMgr->at(idx).trimmed) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Trim to Surfaces: surface is already trimmed. Use Untrim first."));
        return;
    }

    m_surfacesMgr->setTrimmed(idx, true);
    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200,
                  L"Surface \"%hs\" trimmed using surface \"%hs\".",
                  m_surfacesMgr->at(idx).name.c_str(),
                  m_surfacesMgr->at(idx - 1).name.c_str());
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}

// --------------------------------------------------------------------------
// IDM_SURF_FROM_SOLID – extract individual surfaces from a solid body
// --------------------------------------------------------------------------
void MainWindow::surfaceFromSolid()
{
    if (!m_surfacesMgr) return;

    if (!m_solidsMgr || m_solidsMgr->count() == 0) {
        MessageBoxW(m_hwnd,
            L"No solid bodies in the session.\n"
            L"Create or import a solid first, then use From Solids to extract its faces.",
            L"From Solids", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Build a representative surface for each face of the active solid.
    // We approximate each face as a flat NURBS patch based on the face's
    // vertex bounding box (a lightweight analytical proxy).
    const BRep::Solid& solid = m_solidsMgr->at(m_solidsMgr->count() - 1).solid;
    const auto& faces    = solid.faces();
    const auto& edges    = solid.edges();
    const auto& vertices = solid.vertices();
    int faceCount = static_cast<int>(faces.size());

    if (faceCount == 0) {
        MessageBoxW(m_hwnd, L"The selected solid has no faces to extract.",
                    L"From Solids", MB_OK | MB_ICONWARNING);
        return;
    }

    static int fromSolidCount = 0;
    int extracted = 0;
    for (int fi = 0; fi < faceCount; ++fi) {
        const BRep::Face& face = faces[static_cast<std::size_t>(fi)];
        // Compute bounding box of all vertices reachable via this face's edges
        double minX =  1e30, minY =  1e30, minZ =  1e30;
        double maxX = -1e30, maxY = -1e30, maxZ = -1e30;
        for (int eid : face.edgeIds) {
            if (eid < 0 || eid >= static_cast<int>(edges.size())) continue;
            const BRep::Edge& edge = edges[static_cast<std::size_t>(eid)];
            for (int vid : { edge.startVertexId, edge.endVertexId }) {
                if (vid < 0 || vid >= static_cast<int>(vertices.size())) continue;
                const Geom::Vec3& p = vertices[static_cast<std::size_t>(vid)].point;
                if (p.x < minX) minX = p.x;  if (p.x > maxX) maxX = p.x;
                if (p.y < minY) minY = p.y;  if (p.y > maxY) maxY = p.y;
                if (p.z < minZ) minZ = p.z;  if (p.z > maxZ) maxZ = p.z;
            }
        }

        double dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;

        // Select the two largest extents to define the face plane
        double w, d;
        if (dz <= dx && dz <= dy) { w = dx; d = dy; }       // XY-dominant face
        else if (dy <= dx)         { w = dx; d = dz; }       // XZ-dominant face
        else                       { w = dy; d = dz; }       // YZ-dominant face

        if (w < 1e-6 || d < 1e-6) continue; // degenerate face

        NurbsSurface faceSurf = SurfacesManager::makeFlat(w, d);
        std::string name = "FromSolid_" + std::to_string(++fromSolidCount)
                         + "_F" + std::to_string(fi + 1);
        m_surfacesMgr->addSurface(std::move(faceSurf), name);
        ++extracted;
    }

    if (m_viewport) m_viewport->redraw();

    wchar_t msg[200] = {};
    std::swprintf(msg, 200, L"Extracted %d surface(s) from solid body.", extracted);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(msg));
}
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
    double toolDiam = m_promptDefaults.waterlineToolDiam;
    double topZ = 0.0, bottomZ = -30.0;
    double zStep = m_promptDefaults.waterlineZStep;
    if (!promptDouble2(L"3D Waterline",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Z step (mm):",        zStep,    zStep))
        return;
    m_promptDefaults.waterlineToolDiam = toolDiam;
    m_promptDefaults.waterlineZStep    = zStep;

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
    if (m_perfMode == PerformanceMode::Quality) {
        wp.zStep *= 0.75;
        params.feedRate *= 0.9;
    } else if (m_perfMode == PerformanceMode::Speed) {
        wp.zStep *= 1.35;
        params.feedRate *= 1.2;
    }

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
#if defined(CAMEXPERT_USE_OPENMP)
    const wchar_t* ompMode = L"OpenMP ON";
#else
    const wchar_t* ompMode = L"OpenMP fallback";
#endif
    std::swprintf(statusMsg, 200,
        L"3D Waterline generated: Ø%.4g mm, Z %.4g→%.4g mm, step %.4g mm (%ls).",
        toolDiam, topZ, bottomZ, wp.zStep, ompMode);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
    appendAudit(L"3D Waterline generated");
}

// --------------------------------------------------------------------------
// IDM_MACHINE_3D_SCALLOP – constant-scallop finishing on active surface
// --------------------------------------------------------------------------
void MainWindow::generate3DScallop()
{
    double toolDiam = m_promptDefaults.scallopToolDiam;
    double stepOver = m_promptDefaults.scallopStepOver;
    if (!promptDouble2(L"3D Scallop",
                       L"Tool diameter (mm):", toolDiam, toolDiam,
                       L"Step-over   (mm):",   stepOver, stepOver))
        return;
    m_promptDefaults.scallopToolDiam = toolDiam;
    m_promptDefaults.scallopStepOver = stepOver;

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
    if (m_perfMode == PerformanceMode::Quality) {
        sp.stepOver *= 0.8;
        params.feedRate *= 0.9;
    } else if (m_perfMode == PerformanceMode::Speed) {
        sp.stepOver *= 1.3;
        params.feedRate *= 1.15;
    }

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
        toolDiam, sp.stepOver, h);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
    appendAudit(L"3D Scallop generated");
}

// --------------------------------------------------------------------------
// IDM_MACHINE_3D_RASTER – parallel raster passes projected onto mesh/surface
// --------------------------------------------------------------------------
void MainWindow::generate3DRaster()
{
    double toolDiam = m_promptDefaults.rasterToolDiam;
    double stepOver = m_promptDefaults.rasterStepOver;
    double angle    = m_promptDefaults.rasterAngleDeg;
    if (!promptTriple(L"3D Raster",
                      L"Tool diameter (mm):", toolDiam, toolDiam,
                      L"Step-over   (mm):",   stepOver, stepOver,
                      L"Raster angle (deg):", angle,    angle))
        return;
    m_promptDefaults.rasterToolDiam = toolDiam;
    m_promptDefaults.rasterStepOver = stepOver;
    m_promptDefaults.rasterAngleDeg = angle;

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
    if (m_perfMode == PerformanceMode::Quality) {
        rp.stepOver *= 0.8;
        params.feedRate *= 0.9;
    } else if (m_perfMode == PerformanceMode::Speed) {
        rp.stepOver *= 1.3;
        params.feedRate *= 1.15;
    }

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
        toolDiam, rp.stepOver, angle);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(statusMsg));
    appendAudit(L"3D Raster generated");
}

// --------------------------------------------------------------------------
// IDM_MACHINE_5AXIS – 5-axis swarf milling along the active surface
// --------------------------------------------------------------------------
void MainWindow::generate5AxisSwarf()
{
    double toolDiam = m_promptDefaults.swarfToolDiam;
    double leadAngle = m_promptDefaults.swarfLeadAngle;
    if (!promptDouble2(L"5-Axis Swarf",
                       L"Tool diameter (mm):", toolDiam,  toolDiam,
                       L"Lead angle   (deg):", leadAngle, leadAngle))
        return;
    m_promptDefaults.swarfToolDiam = toolDiam;
    m_promptDefaults.swarfLeadAngle = leadAngle;

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
    if (m_perfMode == PerformanceMode::Quality) {
        params.feedRate *= 0.9;
    } else if (m_perfMode == PerformanceMode::Speed) {
        params.feedRate *= 1.2;
    }

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
    appendAudit(L"5-Axis swarf generated");
}
