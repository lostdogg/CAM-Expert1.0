#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <memory>
#include "simulation/Verify.h"
#include "cad/BRep.h"
#include "cad/WireframeScene.h"
#include "cad/ConstraintSolver.h"
#include "cam/MaterialLibrary.h"
#include "cam/CloudToolLibrary.h"
#include "cam/SqlToolDatabase.h"
#include "managers/SolidsManager.h"
#include "ui/CommandIds.h"
#include "ui/MenuCommands.h"

// Forward declarations
class RibbonUI;
class Viewport3D;
class SelectionBar;
class ToolpathManager;
class SurfacesManager;
class LevelsManager;
class PlanesManager;
class CopilotPanel;
class CopilotEngine;

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

    HWND   hwnd()   const { return m_hwnd; }
    HACCEL haccel() const { return m_hAccel; }

private:
    // Win32 window procedure (static trampoline)
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Message handlers
    void onCreate();
    void onSize(int cx, int cy);
    void onCommand(int id);
    bool handleFileCommand(int id);
    bool handleWireframeCommand(int id);
    bool handleSurfaceCommand(int id);
    bool handleSolidCommand(int id);
    bool handleCamCommand(int id);
    bool handleSetupWorkflowCommand(int id);
    void onDestroy();
    void onPaint();
    void updateLayout(int cx, int cy);

    // Menu helpers
    void buildMenu();
    void buildAcceleratorTable();     // create HACCEL for keyboard shortcuts
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

    // --- Edit commands ---
    void editUndo();                  // Ctrl+Z
    void editRedo();                  // Ctrl+Y
    void editCopy();                  // Ctrl+C
    void editPaste();                 // Ctrl+V
    void editDelete();                // Delete
    void editAnalyze();               // End
    void toggleSelectionMode();       // Spacebar

    // --- View commands ---
    void viewZoomSelected();          // F2
    void viewToggleGrid();            // F4
    void viewToggleGnomon();          // F5

    // --- Geometry transform commands ---
    void geomMove();                  // M
    void geomRotate();                // R
    void geomScale();                 // S

    // --- Toolpath manager commands ---
    void toolpathMgrToggle();         // T
    void toolpathToggleDisplay();     // Ctrl+Shift+T
    void toolpathCopyParams();        // Ctrl+Shift+C

    // --- Help ---
    void showHelpTopics();            // F1

    // --- Solid creation (Solids tab) ---
    void createSolidBox();        // IDM_SOLID_EXTRUDE: extrude/box via parameter dialog
    void createSolidCylinder();   // IDM_SOLID_REVOLVE: revolve/cylinder via parameter dialog
    void createSolidSphere();     // IDM_SOLID_SPHERE: sphere via parameter dialog
    void createSolidSweep();      // IDM_SOLID_SWEEP: sweep profile along a path
    void createSolidLoft();       // IDM_SOLID_LOFT: loft/blend multiple cross-sections
    void createSolidThicken();    // IDM_SOLID_THICKEN: add thickness to a surface
    void createSolidBlock();      // IDM_SOLID_BLOCK: direct block primitive
    void createSolidCone();       // IDM_SOLID_CONE: direct cone primitive
    void createSolidTorus();      // IDM_SOLID_TORUS: direct torus primitive
    void solidModify(int commandId);    // Fillet / Chamfer / Shell / Draft / Trim
    void solidBooleanOp(int commandId); // Union / Subtract / Intersect
    void solidHole();             // IDM_SOLID_HOLE: hole wizard
    void solidImpression();       // IDM_SOLID_IMPRESS: impression/negative of solid
    void solidFromSurfaces();     // IDM_SOLID_FROM_SURF: closed surfaces → solid

    // Solids history-tree panel (embedded in the Managers panel "Solids" tab)
    void buildSolidsHistoryTree();        // populate the TreeView from SolidsManager
    void onSolidsTreeNotify(NMHDR* hdr); // WM_NOTIFY dispatcher for the TreeView
    void solidEditFeature(int solidIdx, int featureIdx); // re-prompt and update

    // Prompt for a body-operation mode (Create Body / Add Boss / Cut Body).
    // Returns true if the user confirmed a choice; fills *out* with the result.
    bool promptBodyOpType(BodyOpType& out);

    // Wireframe associativity: called by the WireframeScene change callback.
    // entityIndex is the index of the newly added entity (-1 on clear).
    // Finds any solids whose FeatureOp references that entity and marks them
    // in the Solids tree / status bar so the user knows a rebuild is pending.
    void onWireframeEntityAdded(int entityIndex);

    // --- Wireframe primitive creation (Wireframe tab) ---
    void createWireframe(int commandId);
    void wfCycleCplane();      // IDM_WF_SET_CPLANE – advance to the next Cplane
    void wfSetZDepth();        // IDM_WF_SET_ZDEPTH – prompt for a new Z-depth value
    void updateWfStatusBar();  // refresh the Cplane and Z-depth panels in the status bar

    // --- Surface creation (Surfaces tab) ---
    void surfaceLoft();           // IDM_SURF_LOFT:          ruled/loft surface through cross-sections
    void surfaceRevolve();        // IDM_SURF_REVOLVE:       surface of revolution
    void surfaceFillet();         // IDM_SURF_FILLET:        fillet blend between surfaces
    void surfaceOffset();         // IDM_SURF_OFFSET:        offset active surface
    void surfaceTrim();           // IDM_SURF_TRIM:          trim surface with a wireframe curve
    void surfaceUntrim();         // IDM_SURF_UNTRIM:        remove trim from active surface
    void surfaceExtend();         // IDM_SURF_EXTEND:        extend active surface
    void surfaceFlatBoundary();   // IDM_SURF_FLAT_BOUNDARY: flat surface from a rectangular loop
    void surfaceSwept();          // IDM_SURF_SWEPT:         sweep cross-section along a path
    void surfaceNet();            // IDM_SURF_NET:           net surface from U/V wireframe grid
    void surfaceFence();          // IDM_SURF_FENCE:         fence surface from curve + vector
    void surfaceDraft();          // IDM_SURF_DRAFT_SURF:    draft surface at a specified angle
    void surfaceTrimToSurface();  // IDM_SURF_TRIM_TO_SURF:  trim surface against another surface
    void surfaceFromSolid();      // IDM_SURF_FROM_SOLID:    extract surfaces from a solid body

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
    void generateToolpathChamfer();  // Chamfer milling along active solid boundary
    void generateToolpathThread();   // Thread milling at a user-specified position
    void probeZSurface();            // Generate a Z-surface probing cycle
    void probeBoreCenter();          // Generate a bore/boss center-finding cycle
    void probeCorner();              // Generate a corner-finder probing cycle
    void generate3DWaterline();      // 3D waterline (Z-level) finishing on active surface
    void generate3DScallop();        // 3D scallop (constant step-over) on active surface
    void generate3DRaster();         // 3D raster (parallel passes) on active mesh/surface
    void generate5AxisSwarf();       // 5-axis swarf milling along active surface
    void regenerateAllToolpaths();   // Regen all dirty toolpaths
    void showMachiningSummary();     // Stats dialog (time, length, operations)
    void setupConstraints();         // Constraint setup and diagnostics workflow
    void setupPostProfile();         // Script profile select/validate/clear workflow
    void setupToolDatabase();        // SQL data browse/search/edit/import/export
    void setupPerformanceMode();     // Quality/Balanced/Speed tuning
    void showWorkflowGuidance();     // Context-sensitive next-step guidance
    void showOperationAuditTrail();  // Recent operation-level audit entries

    // --- Project serialisation (CAMX format) ---
    void loadProjectCamx(const std::wstring& path);
    void saveProjectCamx(const std::wstring& path);

    // --- Utility ---
    void updateWindowTitle();   // Reflect m_currentFile in title bar
    void appendAudit(const std::wstring& message);
    bool preflightForPosting(std::wstring& reason) const;
    bool preflightForSimulation(std::wstring& reason) const;

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

    // --- Unit / coordinate display ---
    void unitToggle();                                         // IDM_UNIT_TOGGLE
    void updateCoordinateDisplay(double x, double y, double z);// update X/Y/Z status bar panes
    void updateSnapDisplay(const SnapResult& snap);            // update snap-pane text
    void updateUnitPane();                                     // refresh the unit pane text

    // --- Right-click context menu ---
    void showViewportContextMenu(int screenX, int screenY);    // spawn context popup

    // Return a pointer to the most recently added (active) solid, or nullptr.
    BRep::Solid* activeSolid();

    // Window/controls
    HWND                              m_hwnd          = nullptr;
    HWND                              m_hStatusBar    = nullptr;
    HWND                              m_hManagersPanel= nullptr;
    HWND                              m_hSolidsTree   = nullptr;  // TreeView in the Solids manager tab
    HACCEL                            m_hAccel        = nullptr;  // keyboard accelerator table
    HBRUSH                            m_backgroundBrush = nullptr;

    std::wstring                      m_currentFile;  // path of the currently open project (empty = untitled)
    bool                              m_selectionMode = true;     // true=select, false=deselect (Spacebar toggle)
    bool                              m_useMetric     = true;     // true = mm, false = inches
    std::vector<WfEntity>             m_wfClipboard;  // internal clipboard for copy/paste of wireframe entities

    std::unique_ptr<RibbonUI>         m_ribbon;
    std::unique_ptr<Viewport3D>       m_viewport;
    std::unique_ptr<SelectionBar>     m_selectionBar;

    // Managers (left panel tabs)
    std::unique_ptr<ToolpathManager>  m_toolpathMgr;
    std::unique_ptr<SolidsManager>    m_solidsMgr;
    std::unique_ptr<SurfacesManager>  m_surfacesMgr;
    std::unique_ptr<LevelsManager>    m_levelsMgr;
    std::unique_ptr<PlanesManager>    m_planesMgr;

    // Wireframe entity scene (geometry store + Cplane + Z-depth state)
    std::unique_ptr<WireframeScene>   m_wfScene;
    ConstraintSolver                  m_constraintSolver;
    MaterialLibrary                   m_materialLib;
    CloudToolLibrary                  m_cloudToolLib;
    SqlToolDatabase                   m_sqlToolDb;
    std::string                       m_activePostProfilePath;
    enum class PerformanceMode { Quality, Balanced, Speed };
    PerformanceMode                   m_perfMode = PerformanceMode::Balanced;
    struct PromptDefaults {
        double waterlineToolDiam = 12.0;
        double waterlineZStep    = 1.0;
        double scallopToolDiam   = 8.0;
        double scallopStepOver   = 0.5;
        double rasterToolDiam    = 10.0;
        double rasterStepOver    = 0.5;
        double rasterAngleDeg    = 0.0;
        double swarfToolDiam     = 16.0;
        double swarfLeadAngle    = 5.0;
    } m_promptDefaults;
    std::vector<std::wstring>         m_operationAudit;

    // Copilot
    std::unique_ptr<CopilotPanel>     m_copilotPanel;
    std::unique_ptr<CopilotEngine>    m_copilotEngine;
    bool                              m_copilotVisible = false;
    VerifyResult                      m_lastVerifyResult; // persists for Copilot engine

    static constexpr const wchar_t* CLASS_NAME        = L"CAMExpertMainWnd";
    static constexpr int MANAGERS_PANEL_WIDTH          = 280;
    static constexpr int RIBBON_HEIGHT                 = 100;
    static constexpr int STATUS_BAR_HEIGHT             = 22;
    static constexpr int SELECTION_BAR_HEIGHT          = 28;  // horizontal bar above viewport
    static constexpr int COPILOT_PANEL_WIDTH           = 320; // collapsible Copilot panel
    static constexpr COLORREF FRAME_BACKGROUND_COLOR   = RGB(0x17, 0x1B, 0x22);

    // Status bar pane indices
    static constexpr int SB_PANE_MSG      = 0;   // main message (auto-width)
    static constexpr int SB_PANE_CPLANE   = 1;   // Cplane name  (fixed 90 px)
    static constexpr int SB_PANE_ZDEPTH   = 2;   // Z-depth value (fixed 80 px)
    static constexpr int SB_PANE_SNAP     = 3;   // AutoCursor snap type (fixed 80 px)
    static constexpr int SB_PANE_COORD_X  = 4;   // Cursor world X (fixed 80 px)
    static constexpr int SB_PANE_COORD_Y  = 5;   // Cursor world Y (fixed 80 px)
    static constexpr int SB_PANE_COORD_Z  = 6;   // Cursor world Z (fixed 80 px)
    static constexpr int SB_PANE_UNIT     = 7;   // Unit toggle: "mm" or "in" (fixed 50 px)
};

#endif // MAINWINDOW_H
