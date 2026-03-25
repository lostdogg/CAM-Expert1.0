#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <memory>
#include "simulation/Verify.h"
#include "cad/BRep.h"

// Forward declarations
class RibbonUI;
class Viewport3D;
class SelectionBar;
class ToolpathManager;
class SolidsManager;
class LevelsManager;
class PlanesManager;
class CopilotPanel;
class CopilotEngine;

// IDs for child panels / controls
constexpr int IDC_RIBBON          = 100;
constexpr int IDC_VIEWPORT        = 101;
constexpr int IDC_MANAGERS_PANEL  = 102;
constexpr int IDC_STATUS_BAR      = 103;
constexpr int IDC_SELECTION_BAR   = 104;
constexpr int IDC_COPILOT_PANEL   = 105;

// Menu command IDs
constexpr int IDM_FILE_NEW        = 1001;
constexpr int IDM_FILE_OPEN       = 1002;
constexpr int IDM_FILE_SAVE       = 1003;
constexpr int IDM_FILE_SAVEAS     = 1004;
constexpr int IDM_FILE_IMPORT     = 1005;
constexpr int IDM_FILE_EXIT       = 1099;

constexpr int IDM_MACHINE_POST    = 2001;
constexpr int IDM_MACHINE_VERIFY  = 2002;
constexpr int IDM_MACHINE_BACKPLOT= 2003;
constexpr int IDM_MACHINE_SIM     = 2004;
constexpr int IDM_MACHINE_GEN_POCKET  = 2005; // Generate a 2D dynamic pocket toolpath
constexpr int IDM_MACHINE_GEN_CONTOUR = 2006; // Generate a 2D dynamic contour toolpath
constexpr int IDM_MACHINE_REGEN       = 2007; // Regenerate all toolpaths
constexpr int IDM_MACHINE_SUMMARY     = 2008; // Show machining summary

constexpr int IDM_VIEW_WIREFRAME  = 3001;
constexpr int IDM_VIEW_SHADED     = 3002;
constexpr int IDM_VIEW_TRANSLU    = 3003;
constexpr int IDM_VIEW_ISOMETRIC  = 3004;
constexpr int IDM_VIEW_FRONT      = 3005;
constexpr int IDM_VIEW_TOP        = 3006;
constexpr int IDM_VIEW_RIGHT      = 3007;
constexpr int IDM_VIEW_BACK       = 3008;
constexpr int IDM_VIEW_BOTTOM     = 3009;
constexpr int IDM_VIEW_LEFT       = 3010;
constexpr int IDM_VIEW_FIT        = 3011;  // Fit-to-screen

// Wireframe tab commands
constexpr int IDM_WF_POINT        = 4001;
constexpr int IDM_WF_LINE         = 4002;
constexpr int IDM_WF_ARC          = 4003;
constexpr int IDM_WF_SPLINE       = 4004;
constexpr int IDM_WF_CIRCLE       = 4005;
constexpr int IDM_WF_RECTANGLE    = 4006;
constexpr int IDM_WF_POLYGON      = 4007;

// Surfaces tab commands
constexpr int IDM_SURF_LOFT       = 4101;
constexpr int IDM_SURF_REVOLVE    = 4102;
constexpr int IDM_SURF_FILLET     = 4103;
constexpr int IDM_SURF_OFFSET     = 4104;
constexpr int IDM_SURF_TRIM       = 4105;
constexpr int IDM_SURF_UNTRIM     = 4106;
constexpr int IDM_SURF_EXTEND     = 4107;

// Solids tab commands
constexpr int IDM_SOLID_EXTRUDE   = 4201;
constexpr int IDM_SOLID_REVOLVE   = 4202;
constexpr int IDM_SOLID_UNION     = 4203;
constexpr int IDM_SOLID_SUBTRACT  = 4204;
constexpr int IDM_SOLID_INTERSECT = 4205;
constexpr int IDM_SOLID_SHELL     = 4206;
constexpr int IDM_SOLID_FILLET    = 4207;
constexpr int IDM_SOLID_SPHERE    = 4208;  // Create a parametric sphere

// Model Prep tab commands
constexpr int IDM_PREP_HEAL       = 4301;
constexpr int IDM_PREP_REM_FILLET = 4302;
constexpr int IDM_PREP_BOUNDS     = 4303;
constexpr int IDM_PREP_CLASSIFY   = 4304;
constexpr int IDM_PREP_DRAFT      = 4305;
constexpr int IDM_PREP_SPLIT      = 4306;

constexpr int IDM_HELP_ABOUT      = 9001;
constexpr int IDM_COPILOT_TOGGLE  = 9002;

// --------------------------------------------------------------------------
// MainWindow – the top-level application frame.
//   Layout (from left to right / top to bottom):
//     [Ribbon bar across the full width at the top]
//     [Managers panel (left) | 3-D Viewport (centre/right)]
//     [Status bar at the bottom]
// --------------------------------------------------------------------------
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool create(HINSTANCE hInstance);
    void show(int nCmdShow);

    HWND hwnd() const { return m_hwnd; }

private:
    // Win32 window procedure (static trampoline)
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Message handlers
    void onCreate();
    void onSize(int cx, int cy);
    void onCommand(int id);
    void onDestroy();
    void onPaint();
    void updateLayout(int cx, int cy);

    // Menu helpers
    void buildMenu();
    void showAboutDialog();
    void fileNew();
    void fileOpen();
    void fileSave();
    void fileImport();
    void postProcess();
    void runVerify();
    void runBackplot();
    void runMachineSim();
    void toggleCopilotPanel();

    // --- Solid creation (Solids tab) ---
    void createSolidBox();        // IDM_SOLID_EXTRUDE: box via parameter dialog
    void createSolidCylinder();   // IDM_SOLID_REVOLVE: cylinder via parameter dialog
    void createSolidSphere();     // IDM_SOLID_SPHERE: sphere via parameter dialog
    void solidBooleanOp(int commandId); // Union / Subtract / Intersect

    // --- Wireframe primitive creation (Wireframe tab) ---
    void createWireframe(int commandId);

    // --- Model Prep commands (Model Prep tab) ---
    void prepHeal();          // Heal gaps on active solid
    void prepRemoveFillet();  // Remove small fillets from active solid
    void prepBoundaries();    // Extract boundary curves from active solid
    void prepClassify();      // Classify features on active solid
    void prepAnalyse();       // Run model analysis / draft check
    void prepSplit();         // Split solid at active work plane

    // --- CAM toolpath generation (Machine tab) ---
    void generateToolpathPocket();   // 2D dynamic pocket roughing
    void generateToolpathContour();  // 2D dynamic contour following
    void regenerateAllToolpaths();   // Regen all dirty toolpaths
    void showMachiningSummary();     // Stats dialog (time, length, operations)

    // --- Project serialisation (CAMX format) ---
    void loadProjectCamx(const std::wstring& path);
    void saveProjectCamx(const std::wstring& path);

    // --- Utility ---
    void updateWindowTitle();   // Reflect m_currentFile in title bar

    // --- Input dialog helpers ---
    // Show a one-value prompt; returns true if the user clicked OK.
    bool promptSingle(const wchar_t* title,
                      const wchar_t* label,
                      double defaultVal, double& outVal);
    // Show a two-value prompt (e.g. radius + height).
    bool promptDouble2(const wchar_t* title,
                       const wchar_t* label1, double defVal1, double& out1,
                       const wchar_t* label2, double defVal2, double& out2);
    // Show a three-value prompt (e.g. dx, dy, dz for a box).
    bool promptTriple(const wchar_t* title,
                      const wchar_t* label1, double defVal1, double& out1,
                      const wchar_t* label2, double defVal2, double& out2,
                      const wchar_t* label3, double defVal3, double& out3);

    // Return a pointer to the most recently added (active) solid, or nullptr.
    BRep::Solid* activeSolid();

    // Window/controls
    HWND                              m_hwnd          = nullptr;
    HWND                              m_hStatusBar    = nullptr;
    HWND                              m_hManagersPanel= nullptr;

    std::wstring                      m_currentFile;  // path of the currently open project (empty = untitled)

    std::unique_ptr<RibbonUI>         m_ribbon;
    std::unique_ptr<Viewport3D>       m_viewport;
    std::unique_ptr<SelectionBar>     m_selectionBar;

    // Managers (left panel tabs)
    std::unique_ptr<ToolpathManager>  m_toolpathMgr;
    std::unique_ptr<SolidsManager>    m_solidsMgr;
    std::unique_ptr<LevelsManager>    m_levelsMgr;
    std::unique_ptr<PlanesManager>    m_planesMgr;

    // Copilot
    std::unique_ptr<CopilotPanel>     m_copilotPanel;
    std::unique_ptr<CopilotEngine>    m_copilotEngine;
    bool                              m_copilotVisible = false;
    VerifyResult                      m_lastVerifyResult; // persists for Copilot engine

    static constexpr const wchar_t* CLASS_NAME        = L"CAMExpertMainWnd";
    static constexpr int MANAGERS_PANEL_WIDTH          = 280;
    static constexpr int RIBBON_HEIGHT                 = 100;
    static constexpr int STATUS_BAR_HEIGHT             = 22;
    static constexpr int SELECTION_BAR_WIDTH           = 40;  // wider for buttons
    static constexpr int COPILOT_PANEL_WIDTH           = 320; // collapsible Copilot panel
};

#endif // MAINWINDOW_H
