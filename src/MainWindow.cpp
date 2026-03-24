#include "MainWindow.h"
#include "Application.h"
#include "ui/RibbonUI.h"
#include "ui/Viewport3D.h"
#include "ui/SelectionBar.h"
#include "ui/CopilotPanel.h"
#include "managers/ToolpathManager.h"
#include "managers/SolidsManager.h"
#include "managers/LevelsManager.h"
#include "managers/PlanesManager.h"
#include "simulation/Backplot.h"
#include "simulation/Verify.h"
#include "simulation/MachineSimulation.h"
#include "cam/PostProcessor.h"
#include "copilot/CopilotEngine.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <sstream>
#include <cstdio>

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

    case WM_PAINT:
        onPaint();
        return 0;

    case WM_DESTROY:
        onDestroy();
        return 0;

    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

// --------------------------------------------------------------------------
void MainWindow::onCreate() {
    buildMenu();

    HINSTANCE hInst = Application::instance().hInstance();

    // --- Status bar (bottom) ---
    m_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd,
        reinterpret_cast<HMENU>(IDC_STATUS_BAR), hInst, nullptr);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0,
        reinterpret_cast<LPARAM>(L"Ready"));

    // --- Managers panel (left side – tab control) ---
    m_hManagersPanel = CreateWindowExW(
        0, WC_TABCONTROL, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
        0, RIBBON_HEIGHT, MANAGERS_PANEL_WIDTH, 600,
        m_hwnd, reinterpret_cast<HMENU>(IDC_MANAGERS_PANEL), hInst, nullptr);

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
    m_toolpathMgr = std::make_unique<ToolpathManager>();
    m_solidsMgr   = std::make_unique<SolidsManager>();
    m_levelsMgr   = std::make_unique<LevelsManager>();
    m_planesMgr   = std::make_unique<PlanesManager>();

    // Connect toolpath manager to the viewport so toolpaths are rendered live
    m_viewport->setToolpathManager(m_toolpathMgr.get());

    // Connect the toolpath manager change callback to trigger a viewport redraw
    m_toolpathMgr->setOnChange([this]() {
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

    m_copilotPanel = std::make_unique<CopilotPanel>(m_hwnd, hInst);
    m_copilotPanel->setCopilotEngine(m_copilotEngine.get());
    ShowWindow(m_copilotPanel->hwnd(), SW_HIDE);  // hidden by default
}

// --------------------------------------------------------------------------
void MainWindow::buildMenu() {
    HMENU hMenu     = CreateMenu();
    HMENU hFile     = CreatePopupMenu();
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

    // Machine menu
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_BACKPLOT, L"&Backplot");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_VERIFY,   L"&Verify");
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_SIM,      L"Machine &Simulation");
    AppendMenuW(hMachine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMachine, MF_STRING, IDM_MACHINE_POST,     L"&Post Process…");

    // View menu
    AppendMenuW(hView, MF_STRING, IDM_VIEW_WIREFRAME, L"&Wireframe");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_SHADED,    L"&Shaded");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TRANSLU,   L"&Translucent");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ISOMETRIC, L"&Isometric");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FRONT,     L"&Front");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TOP,       L"&Top");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_RIGHT,     L"&Right");

    // Help menu
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT,      L"&About CAM-Expert…");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelp, MF_STRING, IDM_COPILOT_TOGGLE,  L"Toggle &Copilot Panel\tF1");

    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile),    L"&File");
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
    case IDM_SURF_LOFT:
    case IDM_SURF_REVOLVE:
    case IDM_SURF_EXTEND:
    case IDM_SURF_FILLET:
    case IDM_SURF_OFFSET:
    case IDM_SURF_TRIM:
    case IDM_SURF_UNTRIM:
    case IDM_SOLID_EXTRUDE:
    case IDM_SOLID_REVOLVE:
    case IDM_SOLID_UNION:
    case IDM_SOLID_SUBTRACT:
    case IDM_SOLID_INTERSECT:
    case IDM_SOLID_FILLET:
    case IDM_SOLID_SHELL:
    case IDM_PREP_HEAL:
    case IDM_PREP_REM_FILLET:
    case IDM_PREP_SPLIT:
    case IDM_PREP_BOUNDS:
    case IDM_PREP_CLASSIFY:
    case IDM_PREP_DRAFT:
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Command received (geometry creation not yet implemented)."));
        break;

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
    // Background is filled by child windows; nothing extra needed here.
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
        L"\u00A9 2024 CAM-Expert Project";
    MessageBoxW(m_hwnd, msg.c_str(), L"About CAM-Expert", MB_OK | MB_ICONINFORMATION);
}

// --------------------------------------------------------------------------
void MainWindow::fileNew() {
    // Reset managers and viewport for a fresh session
    if (m_toolpathMgr) m_toolpathMgr->clear();
    if (m_solidsMgr)   m_solidsMgr->clear();
    if (m_levelsMgr)   m_levelsMgr->clear();
    if (m_viewport)    m_viewport->reset();
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
        std::wstring msg = std::wstring(L"Opened: ") + szFile;
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(msg.c_str()));
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
        std::wstring msg = std::wstring(L"Saved: ") + szFile;
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(msg.c_str()));
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

    if (GetOpenFileNameW(&ofn)) {
        std::wstring msg = std::wstring(L"Importing: ") + szFile;
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(msg.c_str()));
        // FileImporter::import(szFile) would be invoked here to load geometry
    }
}

// --------------------------------------------------------------------------
void MainWindow::postProcess() {
    PostProcessor pp;
    const CoordPlane* wcsPlane = m_planesMgr ? m_planesMgr->wcsPlane() : nullptr;
    auto gcode = pp.generate(m_toolpathMgr.get(), wcsPlane);
    if (!gcode.empty()) {
        MessageBoxW(m_hwnd,
            L"Post-processing complete. NC file generated.",
            L"Post-Processor", MB_OK | MB_ICONINFORMATION);
    } else {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"Post-Processor: no toolpaths to process."));
    }
}

// --------------------------------------------------------------------------
void MainWindow::runVerify() {
    Verify v;
    VerifyResult result = v.run(m_toolpathMgr.get());
    std::wstring msg = L"Verify complete.";
    if (result.hasGouge) {
        // Format max gouge depth to 2 decimal places.
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
