#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <memory>
#include "simulation/Verify.h"
#include "cad/BRep.h"
#include "cad/WireframeScene.h"
#include "managers/SolidsManager.h"

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

// IDs for child panels / controls
constexpr int IDC_RIBBON          = 100;
constexpr int IDC_VIEWPORT        = 101;
constexpr int IDC_MANAGERS_PANEL  = 102;
constexpr int IDC_STATUS_BAR      = 103;
constexpr int IDC_SELECTION_BAR   = 104;
constexpr int IDC_COPILOT_PANEL   = 105;
constexpr int IDC_SOLIDS_TREE     = 106;  // TreeView inside the Solids manager tab

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
constexpr int IDM_MACHINE_CHAMFER     = 2009; // Generate a chamfer toolpath
constexpr int IDM_MACHINE_THREAD      = 2010; // Generate a thread mill toolpath
constexpr int IDM_MACHINE_PROBE_Z     = 2011; // Probe Z surface
constexpr int IDM_MACHINE_PROBE_BORE  = 2012; // Probe bore / boss center
constexpr int IDM_MACHINE_PROBE_CORNER= 2013; // Probe corner
constexpr int IDM_MACHINE_3D_WATERLINE= 2014; // Generate 3D waterline (Z-level) toolpath
constexpr int IDM_MACHINE_3D_SCALLOP  = 2015; // Generate 3D scallop toolpath
constexpr int IDM_MACHINE_3D_RASTER   = 2016; // Generate 3D raster toolpath
constexpr int IDM_MACHINE_5AXIS       = 2017; // Generate 5-axis swarf toolpath

constexpr int IDM_VIEW_WIREFRAME       = 3001;
constexpr int IDM_VIEW_SHADED          = 3002;
constexpr int IDM_VIEW_TRANSLU         = 3003;
constexpr int IDM_VIEW_ISOMETRIC       = 3004;
constexpr int IDM_VIEW_FRONT           = 3005;
constexpr int IDM_VIEW_TOP             = 3006;
constexpr int IDM_VIEW_RIGHT           = 3007;
constexpr int IDM_VIEW_BACK            = 3008;
constexpr int IDM_VIEW_BOTTOM          = 3009;
constexpr int IDM_VIEW_LEFT            = 3010;
constexpr int IDM_VIEW_FIT             = 3011;  // F3 – Zoom to fit all entities
constexpr int IDM_VIEW_ZOOM_SELECTED   = 3012;  // F2 – Zoom to selected entities
constexpr int IDM_VIEW_TOGGLE_GRID     = 3013;  // F4 – Toggle grid display
constexpr int IDM_VIEW_TOGGLE_GNOMON   = 3014;  // F5 – Toggle dynamic gnomon

// Wireframe tab commands – Points group
constexpr int IDM_WF_POINT           = 4001;  // Point Position (coordinates) [P]
constexpr int IDM_WF_POINT_DYNAMIC   = 4008;  // Point Dynamic (along curve/surface/mesh)
constexpr int IDM_WF_POINT_NODE      = 4009;  // Point Node Points (spline control nodes)
constexpr int IDM_WF_POINT_SEGMENT   = 4010;  // Point Segment (evenly spaced points)

// Wireframe tab commands – Lines group
constexpr int IDM_WF_LINE            = 4002;  // Line Endpoints [L]
constexpr int IDM_WF_LINE_CLOSEST    = 4011;  // Line Closest (shortest between two entities)
constexpr int IDM_WF_LINE_BISECT     = 4012;  // Line Bisect (bisects angle between two lines)
constexpr int IDM_WF_LINE_PERP       = 4013;  // Line Perpendicular
constexpr int IDM_WF_LINE_PARALLEL   = 4014;  // Line Parallel (offset distance)
constexpr int IDM_WF_LINE_NORMAL     = 4015;  // Line Normal (to point/grid/chain)

// Wireframe tab commands – Arcs group
constexpr int IDM_WF_ARC             = 4003;  // Arc 3 Points [A]
constexpr int IDM_WF_CIRCLE          = 4005;  // Circle Center Point [C]
constexpr int IDM_WF_CIRCLE_EDGE     = 4016;  // Circle Edge Points (2- or 3-point)
constexpr int IDM_WF_ARC_TANGENT     = 4017;  // Arc Tangent (1/2/3 entities)
constexpr int IDM_WF_ARC_ENDPOINTS   = 4018;  // Arc Endpoints (two pts + radius)
constexpr int IDM_WF_ARC_POLAR       = 4019;  // Arc Polar (centre + radius + angles)

// Wireframe tab commands – Splines group
constexpr int IDM_WF_SPLINE          = 4004;  // Spline Manual
constexpr int IDM_WF_SPLINE_AUTO     = 4020;  // Spline Automatic (fit through points)
constexpr int IDM_WF_SPLINE_BLENDED  = 4021;  // Spline Blended (connect two curves)

// Wireframe tab commands – Shapes group
constexpr int IDM_WF_RECTANGLE       = 4006;  // Rectangle
constexpr int IDM_WF_RECT_SHAPES     = 4022;  // Rectangular Shapes (rounded corners / chamfers)
constexpr int IDM_WF_POLYGON         = 4007;  // Polygon
constexpr int IDM_WF_ELLIPSE         = 4023;  // Ellipse (centre + major/minor axes)
constexpr int IDM_WF_HELIX           = 4024;  // Spiral / Helix
constexpr int IDM_WF_BBOX            = 4025;  // Bounding Box (2D/3D)

// Wireframe tab commands – Curves (extraction) group
constexpr int IDM_WF_CURVE_ONE_EDGE  = 4026;  // Curve One Edge (from solid/surface edge)
constexpr int IDM_WF_CURVE_ALL_EDGES = 4027;  // Curve All Edges
constexpr int IDM_WF_CURVE_SLICE_PLN = 4028;  // Curve Slice by Plane
constexpr int IDM_WF_CURVE_SLICE_CRV = 4029;  // Curve Slice Along Curve
constexpr int IDM_WF_CURVE_FLOWLINE  = 4030;  // Curve Flowline (U/V)
constexpr int IDM_WF_CURVE_INTERSECT = 4031;  // Curve at Intersection

// Wireframe tab commands – Modify group
constexpr int IDM_WF_MOD_FILLET      = 4032;  // Fillet Entities
constexpr int IDM_WF_MOD_CHAMFER     = 4033;  // Chamfer Entities
constexpr int IDM_WF_MOD_DYN_TRIM    = 4034;  // Dynamic Trim
constexpr int IDM_WF_MOD_BREAK_TWO   = 4035;  // Break Two Pieces
constexpr int IDM_WF_MOD_BREAK_INT   = 4036;  // Break at Intersection
constexpr int IDM_WF_MOD_JOIN        = 4037;  // Join Entities
constexpr int IDM_WF_MOD_INTERSECT   = 4038;  // Modify at Intersection
constexpr int IDM_WF_MOD_PROJECT     = 4039;  // Project geometry onto a plane or surface
constexpr int IDM_WF_MOD_OFFSET      = 4040;  // Offset / Offset Chains
constexpr int IDM_WF_MOD_ROLL        = 4041;  // Roll / Unroll around a cylinder

// Wireframe construction-plane and Z-depth commands
constexpr int IDM_WF_SET_CPLANE      = 4042;  // Cycle to next Cplane [F8]
constexpr int IDM_WF_SET_ZDEPTH      = 4043;  // Prompt for Z-depth value [F9]

// Surfaces tab commands
constexpr int IDM_SURF_LOFT       = 4101;
constexpr int IDM_SURF_REVOLVE    = 4102;
constexpr int IDM_SURF_FILLET     = 4103;
constexpr int IDM_SURF_OFFSET     = 4104;
constexpr int IDM_SURF_TRIM       = 4105;
constexpr int IDM_SURF_UNTRIM     = 4106;
constexpr int IDM_SURF_EXTEND     = 4107;

// Solids tab commands – Create group (history-based, from wireframe chains)
constexpr int IDM_SOLID_EXTRUDE   = 4201;  // Extrude a 2D profile into a solid
constexpr int IDM_SOLID_REVOLVE   = 4202;  // Revolve a profile around an axis
constexpr int IDM_SOLID_SWEEP     = 4209;  // Sweep a profile along a path curve
constexpr int IDM_SOLID_LOFT      = 4210;  // Loft / blend multiple cross-sections
constexpr int IDM_SOLID_THICKEN   = 4211;  // Add thickness to a surface to make a solid

// Solids tab commands – Primitives group (direct geometric shapes, no wireframe needed)
constexpr int IDM_SOLID_BLOCK     = 4212;  // Rectangular solid from dimensions or 2 points
constexpr int IDM_SOLID_CYLINDER  = 4213;  // Cylindrical solid (radius + height)
constexpr int IDM_SOLID_SPHERE    = 4208;  // Solid sphere (centre + radius)
constexpr int IDM_SOLID_CONE      = 4214;  // Tapered conical solid
constexpr int IDM_SOLID_TORUS     = 4215;  // Donut-shaped solid

// Solids tab commands – Modify group (refine an existing solid)
constexpr int IDM_SOLID_FILLET    = 4207;  // Round off edges with constant/variable radius
constexpr int IDM_SOLID_CHAMFER   = 4216;  // Flat angled break on edges
constexpr int IDM_SOLID_SHELL     = 4206;  // Hollow out a solid to a wall thickness
constexpr int IDM_SOLID_DRAFT     = 4217;  // Taper vertical faces for moulding/casting
constexpr int IDM_SOLID_TRIM      = 4218;  // Cut solid with a plane, surface, or solid

// Solids tab commands – Boolean group (combine/subtract solid bodies)
constexpr int IDM_SOLID_UNION     = 4203;  // Add: merge two or more solids
constexpr int IDM_SOLID_SUBTRACT  = 4204;  // Remove: cut one solid from another
constexpr int IDM_SOLID_INTERSECT = 4205;  // Common: keep only the overlapping volume

// Solids tab commands – Advanced / Specialized
constexpr int IDM_SOLID_HOLE      = 4219;  // Hole wizard (simple/counterbore/countersink/threaded)
constexpr int IDM_SOLID_IMPRESS   = 4220;  // Impression: negative of a solid (mold / electrode)
constexpr int IDM_SOLID_FROM_SURF = 4221;  // Convert closed surfaces to a watertight solid

// Solids history-tree context-menu commands
constexpr int IDM_SOLID_TREE_EDIT     = 4230; // Edit the selected feature's parameters
constexpr int IDM_SOLID_TREE_SUPPRESS = 4231; // Toggle suppression of the selected feature
constexpr int IDM_SOLID_TREE_DELETE   = 4232; // Remove the selected feature from the tree

// Model Prep tab commands
constexpr int IDM_PREP_HEAL       = 4301;
constexpr int IDM_PREP_REM_FILLET = 4302;
constexpr int IDM_PREP_BOUNDS     = 4303;
constexpr int IDM_PREP_CLASSIFY   = 4304;
constexpr int IDM_PREP_DRAFT      = 4305;
constexpr int IDM_PREP_SPLIT      = 4306;

constexpr int IDM_HELP_ABOUT      = 9001;
constexpr int IDM_COPILOT_TOGGLE  = 9002;
constexpr int IDM_HELP_TOPICS     = 9003;  // F1 – Open help topics

// Edit menu command IDs
constexpr int IDM_EDIT_UNDO            = 5001;  // Ctrl+Z
constexpr int IDM_EDIT_REDO            = 5002;  // Ctrl+Y
constexpr int IDM_EDIT_COPY            = 5003;  // Ctrl+C
constexpr int IDM_EDIT_PASTE           = 5004;  // Ctrl+V
constexpr int IDM_EDIT_DELETE          = 5005;  // Delete – remove selected entities
constexpr int IDM_EDIT_ANALYZE         = 5006;  // End – display Analyze dialog
constexpr int IDM_TOGGLE_SELECT_MODE   = 5007;  // Spacebar – toggle selection mode

// Geometry transform command IDs
constexpr int IDM_GEOM_MOVE            = 6001;  // M – move selected geometry
constexpr int IDM_GEOM_ROTATE          = 6002;  // R – rotate selected geometry
constexpr int IDM_GEOM_SCALE           = 6003;  // S – scale selected geometry

// Toolpath manager / display command IDs
constexpr int IDM_TOOLPATH_MGR_TOGGLE  = 6101;  // T – open/focus Toolpath Manager
constexpr int IDM_TOOLPATH_TOGGLE_DISP = 6102;  // Ctrl+Shift+T – toggle toolpath display
constexpr int IDM_TOOLPATH_COPY_PARAMS = 6103;  // Ctrl+Shift+C – copy toolpath parameters

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
    void surfaceLoft();           // IDM_SURF_LOFT:    loft through cross-sections
    void surfaceRevolve();        // IDM_SURF_REVOLVE: surface of revolution
    void surfaceFillet();         // IDM_SURF_FILLET:  fillet blend between surfaces
    void surfaceOffset();         // IDM_SURF_OFFSET:  offset active surface
    void surfaceTrim();           // IDM_SURF_TRIM:    trim active surface
    void surfaceUntrim();         // IDM_SURF_UNTRIM:  remove trim from active surface
    void surfaceExtend();         // IDM_SURF_EXTEND:  extend active surface

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
    HWND                              m_hSolidsTree   = nullptr;  // TreeView in the Solids manager tab
    HACCEL                            m_hAccel        = nullptr;  // keyboard accelerator table
    HBRUSH                            m_backgroundBrush = nullptr;

    std::wstring                      m_currentFile;  // path of the currently open project (empty = untitled)
    bool                              m_selectionMode = true;     // true=select, false=deselect (Spacebar toggle)
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
    static constexpr COLORREF FRAME_BACKGROUND_COLOR   = RGB(0x17, 0x1B, 0x22);

    // Status bar pane indices
    static constexpr int SB_PANE_MSG      = 0;   // main message (auto-width)
    static constexpr int SB_PANE_CPLANE   = 1;   // Cplane name  (fixed 90 px)
    static constexpr int SB_PANE_ZDEPTH   = 2;   // Z-depth value (fixed 110 px)
    static constexpr int SB_PANE_SNAP     = 3;   // AutoCursor snap type (fixed 100 px)
};

#endif // MAINWINDOW_H
