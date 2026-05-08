#include "RibbonUI.h"
#include "CommandIds.h"
#include "MenuCommands.h"
#include <commctrl.h>
#include <cmath>

// --------------------------------------------------------------------------
RibbonUI::RibbonUI(HWND parent, HINSTANCE hInstance)
    : m_hInst(hInstance) {

    // Container window for the ribbon area (filled with a tab control)
    m_hwnd = CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 100, 100,
        parent, nullptr, hInstance, nullptr);

    // Subclass the container so WM_NOTIFY from the tab control
    // (and TTN_GETDISPINFOW from toolbars) are intercepted here.
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this));
    m_oldContainerProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(RibbonContainerProc)));

    // Tab control inside the ribbon container
    m_tabCtrl = CreateWindowExW(0, WC_TABCONTROL, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 100, 100,
        m_hwnd, nullptr, hInstance, nullptr);

    build();
}

// --------------------------------------------------------------------------
RibbonUI::~RibbonUI() {
    // Destroy the window first (which destroys all toolbar child windows).
    if (m_hwnd) DestroyWindow(m_hwnd);
    // Image lists are GDI objects independent of windows — destroy them after.
    for (HIMAGELIST hIml : m_imageLists) {
        if (hIml) ImageList_Destroy(hIml);
    }
}

// --------------------------------------------------------------------------
void RibbonUI::resize(int x, int y, int width, int height) {
    SetWindowPos(m_hwnd,    nullptr, x, y, width, height, SWP_NOZORDER);
    SetWindowPos(m_tabCtrl, nullptr, 0, 0, width, height, SWP_NOZORDER);
    repositionToolbars();
}

// --------------------------------------------------------------------------
void RibbonUI::build() {
    buildHomeTab();
    buildWireframeTab();
    buildSurfacesSolidsTab();
    buildModelPrepTab();
    buildToolpathTab();
    buildMachineControlTab();
    buildViewTab();
    buildCopilotTab();
    repositionToolbars();
}

// --------------------------------------------------------------------------
void RibbonUI::addTab(RibbonTab tab) {
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    auto wname = std::wstring(tab.name.begin(), tab.name.end());
    ti.pszText = const_cast<wchar_t*>(wname.c_str());
    TabCtrl_InsertItem(m_tabCtrl, static_cast<int>(m_tabs.size()), &ti);
    m_tabs.push_back(std::move(tab));
    renderTab(static_cast<int>(m_tabs.size()) - 1);
}

// --------------------------------------------------------------------------
void RibbonUI::buildHomeTab() {
    RibbonTab tab;
    tab.name = "Home";
    RibbonGroup file{"File", {
        {IDM_FILE_NEW,    "New",    "Create a new file [Ctrl+N]"},
        {IDM_FILE_OPEN,   "Open",   "Open an existing file [Ctrl+O]"},
        {IDM_FILE_SAVE,   "Save",   "Save the current file [Ctrl+S]"},
        {IDM_FILE_IMPORT, "Import", "Import a CAD file [Ctrl+I]"}
    }};
    RibbonGroup edit{"Edit", {
        {IDM_EDIT_UNDO,          "Undo",    "Undo last action [Ctrl+Z]"},
        {IDM_EDIT_REDO,          "Redo",    "Redo last undone action [Ctrl+Y]"},
        {IDM_EDIT_COPY,          "Copy",    "Copy selected entities [Ctrl+C]"},
        {IDM_EDIT_PASTE,         "Paste",   "Paste entities [Ctrl+V]"},
        {IDM_EDIT_DELETE,        "Delete",  "Remove selected entities [Del]"},
        {IDM_TOGGLE_SELECT_MODE, "Sel Mode","Toggle selection/deselection mode [Space]"}
    }};
    tab.groups.push_back(file);
    tab.groups.push_back(edit);
    addTab(tab);
}

void RibbonUI::buildWireframeTab() {
    RibbonTab tab;
    tab.name = "Geometry & Wireframe";

    // Points group
    RibbonGroup points{"Points", {
        {IDM_WF_POINT,          "Point Pos",    "Place a coordinate node in 3D space by clicking in the graphics area or typing exact X/Y/Z values [P]"},
        {IDM_WF_POINT_DYNAMIC,  "Point Dyn",    "Create a point dynamically along a curve, surface, or mesh"},
        {IDM_WF_POINT_NODE,     "Point Node",   "Place points at spline control node locations"},
        {IDM_WF_POINT_SEGMENT,  "Point Seg",    "Create evenly spaced points along a selected entity"}
    }};

    // Lines group
    RibbonGroup lines{"Lines", {
        {IDM_WF_LINE,           "Line Endpts",  "Create a straight segment by picking a start point and an end point; length or angle can be locked [L]"},
        {IDM_WF_LINE_CLOSEST,   "Line Closest", "Create the shortest line between two selected entities"},
        {IDM_WF_LINE_BISECT,    "Line Bisect",  "Create a line bisecting the angle between two lines"},
        {IDM_WF_LINE_PERP,      "Line Perp",    "Create a line at 90 degrees to a selected entity by choosing the base line and the start or end point"},
        {IDM_WF_LINE_PARALLEL,  "Line Para",    "Create a line parallel to an existing line by choosing the side and entering an offset distance"},
        {IDM_WF_LINE_NORMAL,    "Line Normal",  "Create normal lines to a point, grid, or along a chain"}
    }};

    // Arcs group
    RibbonGroup arcs{"Arcs", {
        {IDM_WF_CIRCLE,         "Circle Ctr",   "Create a circle by picking the centre point and then defining the radius or diameter [C]"},
        {IDM_WF_CIRCLE_EDGE,    "Circle Edge",  "Create a circle through 2 or 3 circumference points"},
        {IDM_WF_ARC,            "Arc 3 Pts",    "Create an arc from start, end, and third-point picks that define the curvature [A]"},
        {IDM_WF_ARC_TANGENT,    "Arc Tangent",  "Create an arc that blends smoothly from existing geometry by maintaining tangency"},
        {IDM_WF_ARC_ENDPOINTS,  "Arc Endpts",   "Create an arc from two endpoints and a radius"},
        {IDM_WF_ARC_POLAR,      "Arc Polar",    "Create an arc using centre, radius, and start/end angles"}
    }};

    // Splines group
    RibbonGroup splines{"Splines", {
        {IDM_WF_SPLINE,         "Spline Man",   "Create a smooth manual spline by defining a series of control points"},
        {IDM_WF_SPLINE_AUTO,    "Spline Auto",  "Auto-fit a smooth spline through selected points"},
        {IDM_WF_SPLINE_BLENDED, "Spline Blend", "Create a smooth blended spline connecting two curves"}
    }};

    // Shapes group
    RibbonGroup shapes{"Shapes", {
        {IDM_WF_RECTANGLE,      "Rectangle",    "Create a rectangle by choosing the first corner and opposite corner, with an option to anchor from centre"},
        {IDM_WF_RECT_SHAPES,    "Rect Shapes",  "Create obrounds, D-shapes, or pre-filleted/chamfered rectangles from entered dimensions"},
        {IDM_WF_POLYGON,        "Polygon",      "Create a regular polygon by entering the number of sides and the across-flats or across-corners size"},
        {IDM_WF_ELLIPSE,        "Ellipse",      "Create an ellipse from centre and major/minor axes"},
        {IDM_WF_HELIX,          "Spiral/Helix", "Create a 2D spiral or 3D helix"},
        {IDM_WF_BBOX,           "Bounding Box", "Generate a 2D/3D bounding box around selected geometry"}
    }};

    // Curves (extraction) group
    RibbonGroup curves{"Curves", {
        {IDM_WF_CURVE_ONE_EDGE,  "Curve Edge",   "Convert a selected solid or surface edge into matching wireframe geometry"},
        {IDM_WF_CURVE_ALL_EDGES, "Curve All",    "Extract wireframe from all edges of a solid or surface"},
        {IDM_WF_SILHOUETTE,      "Silhouette",   "Project the outer silhouette boundary of a solid/surface onto the active work plane"},
        {IDM_WF_CURVE_SLICE_PLN, "Slice/Plane",  "Create wireframe cross-sections by slicing with a plane"},
        {IDM_WF_CURVE_SLICE_CRV, "Slice/Curve",  "Create cross-sections perpendicular to a drive curve"},
        {IDM_WF_CURVE_FLOWLINE,  "Flowline",     "Create curves along the U and V flowlines of a surface"},
        {IDM_WF_CURVE_INTERSECT, "At Intersect", "Generate curves where two surfaces or solids intersect"}
    }};

    // Modify group
    RibbonGroup modify{"Modify", {
        {IDM_WF_MOD_FILLET,    "Fillet",      "Round a sharp corner by entering a radius and selecting the two intersecting entities"},
        {IDM_WF_MOD_CHAMFER,   "Chamfer",     "Create an angled flat by entering the chamfer size and selecting the two intersecting entities"},
        {IDM_WF_MOD_DYN_TRIM,  "Dyn Trim",    "Trim, break, or extend wireframe by clicking the segment to keep or modify near an intersection"},
        {IDM_WF_MOD_BREAK_TWO, "Break 2",    "Split an entity into two pieces at a click point"},
        {IDM_WF_MOD_BREAK_INT, "Break Int",  "Break intersecting entities at their crossing points"},
        {IDM_WF_MOD_JOIN,      "Join",        "Recombine collinear lines or coincident arcs"},
        {IDM_WF_MOD_INTERSECT, "Mod Intersect", "Break, trim, or create a point at wireframe/surface intersections"},
        {IDM_WF_MOD_PROJECT,   "Project",     "Flatten 3D geometry onto a plane or wrap it onto a surface"},
        {IDM_WF_MOD_OFFSET,    "Offset",      "Duplicate wireframe at a specified distance by choosing the entity, direction, and offset value"},
        {IDM_WF_MOD_ROLL,      "Roll/Unroll", "Wrap or flatten geometry around a cylinder for rotary work"}
    }};

    // Transform group
    RibbonGroup xform{"Transform", {
        {IDM_GEOM_MOVE,    "Move (M)",   "Move selected geometry [M]"},
        {IDM_GEOM_ROTATE,  "Rotate (R)", "Rotate selected geometry [R]"},
        {IDM_GEOM_SCALE,   "Scale (S)",  "Scale selected geometry [S]"}
    }};

    tab.groups.push_back(points);
    tab.groups.push_back(lines);
    tab.groups.push_back(arcs);
    tab.groups.push_back(splines);
    tab.groups.push_back(shapes);
    tab.groups.push_back(curves);
    tab.groups.push_back(modify);
    tab.groups.push_back(xform);
    addTab(tab);
}

void RibbonUI::buildSurfacesSolidsTab() {
    RibbonTab tab;
    tab.name = "Surfaces & Solids";

    // ── Basic Surface Creation ─────────────────────────────────────────────
    RibbonGroup surfBasic{"Basic Creation", {
        {IDM_SURF_FLAT_BOUNDARY, "Flat",     "Flat Boundary: fill a closed wireframe loop with a perfectly flat surface"},
        {IDM_SURF_LOFT,          "Ruled/Loft","Ruled/Lofted: stretch a surface between two or more separate wireframe chains"},
        {IDM_SURF_REVOLVE,       "Revolve",  "Revolved: spin a profile around a centre axis (partial or full 360°)"},
        {IDM_SURF_SWEPT,         "Swept",    "Swept: drive a cross-section profile along one or more rail paths"}
    }};

    // ── Advanced Shape Generation ──────────────────────────────────────────
    RibbonGroup surfAdvanced{"Advanced", {
        {IDM_SURF_NET,        "Net",   "Net Surface: create a surface from a grid of intersecting U and V wireframe chains"},
        {IDM_SURF_FENCE,      "Fence", "Fence Surface: project a surface from a curve at a specific angle and distance"},
        {IDM_SURF_DRAFT_SURF, "Draft", "Draft Surface: extend a surface from a curve at a specified draft angle for mold pull"}
    }};

    // ── Surface Modification and Cleanup ──────────────────────────────────
    RibbonGroup surfModify{"Surf Modify", {
        {IDM_SURF_TRIM,         "Trim/Crv", "Trim to Curves: use a wireframe line to cut a hole or trim the edge of a surface"},
        {IDM_SURF_TRIM_TO_SURF, "Trim/Srf", "Trim to Surfaces: use one surface to cut another where they intersect"},
        {IDM_SURF_UNTRIM,       "Untrim",   "Untrim: restore a trimmed surface to its original un-cut mathematical boundaries"},
        {IDM_SURF_FILLET,       "Fillet",   "Fillet Surfaces: create a smooth radiused transition where two surfaces meet"}
    }};

    // ── Utility ────────────────────────────────────────────────────────────
    RibbonGroup surfUtil{"Surf Utility", {
        {IDM_SURF_FROM_SOLID, "From Solid", "From Solids: convert the faces of a 3D solid model into individual surfaces"},
        {IDM_SURF_OFFSET,     "Offset",     "Offset: create a duplicate surface at a specific distance from the original"},
        {IDM_SURF_EXTEND,     "Extend",     "Extend a surface edge by a specified distance"}
    }};

    // ── Solids – Create ───────────────────────────────────────────────────
    RibbonGroup solCreate{"Solid Create", {
        {IDM_SOLID_EXTRUDE,  "Extrude",   "Push a closed wireframe chain into 3D by setting distance, direction, and body mode (Create Body or Cut Body)"},
        {IDM_SOLID_REVOLVE,  "Revolve",   "Spin a wireframe profile around a selected axis and angle (for example 360° for a full body)"},
        {IDM_SOLID_SWEEP,    "Sweep",     "Move a cross-section profile along a selected wireframe path"},
        {IDM_SOLID_LOFT,     "Loft",      "Blend two or more wireframe chains by skinning the volume between them"},
        {IDM_SOLID_THICKEN,  "Thicken",   "Turn a thin surface into a solid by applying thickness in a chosen direction"}
    }};

    // ── Solids – Primitives ───────────────────────────────────────────────
    RibbonGroup solPrims{"Primitives", {
        {IDM_SOLID_BLOCK,    "Block",     "Create a rectangular solid from dimensions or two corner points"},
        {IDM_SOLID_CYLINDER, "Cylinder",  "Create a cylindrical solid with a defined radius and height"},
        {IDM_SOLID_SPHERE,   "Sphere",    "Create a solid ball from a centre point and radius"},
        {IDM_SOLID_CONE,     "Cone",      "Create a tapered conical solid"},
        {IDM_SOLID_TORUS,    "Torus",     "Create a donut-shaped solid from a major and minor radius"}
    }};

    // ── Solids – Modify ───────────────────────────────────────────────────
    RibbonGroup solModify{"Solid Modify", {
        {IDM_SOLID_FILLET,   "Fillet",    "Round selected solid edges or faces with a constant fillet radius"},
        {IDM_SOLID_CHAMFER,  "Chamfer",   "Apply a flat angled cut to selected edges using distance or distance-angle values"},
        {IDM_SOLID_SHELL,    "Shell",     "Hollow a solid by selecting an opening face and setting wall thickness"},
        {IDM_SOLID_DRAFT,    "Draft",     "Taper vertical faces for moulding or casting pull"},
        {IDM_SOLID_TRIM,     "Trim",      "Trim a solid to a plane or surface and keep the side you need"}
    }};

    // ── Solids – Boolean ──────────────────────────────────────────────────
    RibbonGroup solBoolean{"Boolean", {
        {IDM_SOLID_UNION,    "Add",       "Merge target and tool solids into a single body (Boolean Add)"},
        {IDM_SOLID_SUBTRACT, "Remove",    "Subtract tool solids from the target body (Boolean Remove)"},
        {IDM_SOLID_INTERSECT,"Common",    "Keep only the overlapping volume shared by selected solids (Boolean Common)"}
    }};

    // ── Solids – Advanced ─────────────────────────────────────────────────
    RibbonGroup solAdvanced{"Advanced", {
        {IDM_SOLID_HOLE,     "Hole",      "Create complex holes: simple, counterbore, countersink, taper, or threaded"},
        {IDM_SOLID_IMPRESS,  "Impression","Generate the negative of a solid (useful for molds and electrodes)"},
        {IDM_SOLID_FROM_SURF,"From Surfs","Convert a collection of closed surfaces into a watertight solid body"}
    }};

    tab.groups.push_back(surfBasic);
    tab.groups.push_back(surfAdvanced);
    tab.groups.push_back(surfModify);
    tab.groups.push_back(surfUtil);
    tab.groups.push_back(solCreate);
    tab.groups.push_back(solPrims);
    tab.groups.push_back(solModify);
    tab.groups.push_back(solBoolean);
    tab.groups.push_back(solAdvanced);
    addTab(tab);
}

void RibbonUI::buildModelPrepTab() {
    RibbonTab tab;
    tab.name = "Preparation";
    RibbonGroup repair{"Repair", {
        {IDM_PREP_HEAL,      "Heal",       "Heal gaps and surface inconsistencies"},
        {IDM_PREP_REM_FILLET,"Rem. Fillet","Remove small fillet faces"},
        {IDM_PREP_SPLIT,     "Split",      "Split solid at a plane"}
    }};
    RibbonGroup analyse{"Analyse", {
        {IDM_PREP_BOUNDS,    "Boundaries", "Extract boundary curves"},
        {IDM_PREP_CLASSIFY,  "Classify",   "Classify features (holes, pockets, bosses)"},
        {IDM_PREP_DRAFT,     "Draft",      "Check draft angle for moulding"}
    }};
    tab.groups.push_back(repair);
    tab.groups.push_back(analyse);
    addTab(tab);
}

void RibbonUI::buildToolpathTab() {
    RibbonTab tab;
    tab.name = "Toolpath Generation";

    RibbonGroup gen2D{"2D Strategies", {
        {IDM_MACHINE_GEN_POCKET,  "2D Pocket",  "Generate a dynamic 2D pocket toolpath"},
        {IDM_MACHINE_GEN_CONTOUR, "2D Contour", "Generate a dynamic 2D contour toolpath"},
        {IDM_MACHINE_CHAMFER,     "Chamfer",    "Generate a chamfer toolpath"},
        {IDM_MACHINE_THREAD,      "Thread Mill","Generate a thread milling toolpath"}
    }};
    RibbonGroup gen3D{"3D Strategies", {
        {IDM_MACHINE_3D_WATERLINE, "Waterline",  "Generate 3D Z-level waterline toolpath (F6)"},
        {IDM_MACHINE_3D_SCALLOP,   "Scallop",    "Generate 3D constant-scallop toolpath (F7)"},
        {IDM_MACHINE_3D_RASTER,    "Raster",     "Generate 3D parallel raster toolpath"},
        {IDM_MACHINE_5AXIS,        "5-Axis",     "Generate 5-axis swarf toolpath"}
    }};
    RibbonGroup probe{"Probing", {
        {IDM_MACHINE_PROBE_Z,     "Probe Z",    "Probe Z surface height"},
        {IDM_MACHINE_PROBE_BORE,  "Probe Bore", "Probe bore / boss centre"},
        {IDM_MACHINE_PROBE_CORNER,"Probe Corner","Probe a rectangular corner"}
    }};
    RibbonGroup simulate{"Simulate", {
        {IDM_MACHINE_BACKPLOT, "Backplot",    "Run backplot animation"},
        {IDM_MACHINE_VERIFY,   "Verify",      "Run solid verify"},
        {IDM_MACHINE_SIM,      "Machine Sim", "Run machine simulation"}
    }};
    RibbonGroup toolpathMgr{"Manager", {
        {IDM_TOOLPATH_MGR_TOGGLE,  "Manager [T]",        "Open Toolpath Manager [T]"},
        {IDM_TOOLPATH_TOGGLE_DISP, "Show/Hide [C+S+T]",  "Toggle toolpath display [Ctrl+Shift+T]"},
        {IDM_TOOLPATH_COPY_PARAMS, "Copy Params [C+S+C]","Copy toolpath parameters [Ctrl+Shift+C]"}
    }};
    tab.groups.push_back(gen2D);
    tab.groups.push_back(gen3D);
    tab.groups.push_back(probe);
    tab.groups.push_back(simulate);
    tab.groups.push_back(toolpathMgr);
    addTab(tab);
}

void RibbonUI::buildMachineControlTab() {
    RibbonTab tab;
    tab.name = "Machine Control";

    RibbonGroup output{"Output", {
        {IDM_MACHINE_POST,    "Post",    "Generate G-code via post-processor [Ctrl+P]"},
        {IDM_MACHINE_REGEN,   "Regen",   "Regenerate all dirty toolpaths"},
        {IDM_MACHINE_SUMMARY, "Summary", "Show machining time / length summary"}
    }};
    RibbonGroup setup{"Setup", {
        {IDM_SETUP_CONSTRAINTS,  "Constraints", "Create/list/delete/solve sketch constraints with diagnostics"},
        {IDM_SETUP_POST_PROFILE, "Post Profile", "Load/validate/clear scriptable post profiles"},
        {IDM_SETUP_TOOL_DB,      "Tool DB", "Browse/search/edit/export/import SQL tool/material/cutting data"},
        {IDM_SETUP_PERF_MODE,    "Performance", "Select Quality/Balanced/Speed mode and OpenMP visibility"},
        {IDM_SETUP_GUIDANCE,     "Guidance", "Show context-sensitive workflow guidance"},
        {IDM_SETUP_AUDIT_LOG,    "Audit", "Show recent operation-level audit trail"}
    }};
    tab.groups.push_back(output);
    tab.groups.push_back(setup);
    addTab(tab);
}

void RibbonUI::buildViewTab() {
    RibbonTab tab;
    tab.name = "View";
    RibbonGroup mode{"Display", {
        {IDM_VIEW_WIREFRAME, "Wireframe",  "Wireframe display"},
        {IDM_VIEW_SHADED,    "Shaded",     "Shaded display"},
        {IDM_VIEW_TRANSLU,   "Translucent","Translucent display"}
    }};
    RibbonGroup view{"Viewpoint", {
        {IDM_VIEW_ISOMETRIC, "ISO",    "Isometric view"},
        {IDM_VIEW_TOP,       "Top",    "Top view"},
        {IDM_VIEW_FRONT,     "Front",  "Front view"},
        {IDM_VIEW_RIGHT,     "Right",  "Right view"},
        {IDM_VIEW_BACK,      "Back",   "Back view"},
        {IDM_VIEW_BOTTOM,    "Bottom", "Bottom view"},
        {IDM_VIEW_LEFT,      "Left",   "Left view"}
    }};
    RibbonGroup fit{"Zoom", {
        {IDM_VIEW_ZOOM_SELECTED, "Zoom Sel", "Zoom to selected entities [F2]"},
        {IDM_VIEW_FIT,           "Fit All",  "Fit all geometry in view [F3]"}
    }};
    RibbonGroup toggles{"Display Toggles", {
        {IDM_VIEW_TOGGLE_GRID,   "Grid [F4]",   "Toggle grid display [F4]"},
        {IDM_VIEW_TOGGLE_GNOMON, "Gnomon [F5]", "Toggle dynamic gnomon [F5]"}
    }};
    tab.groups.push_back(mode);
    tab.groups.push_back(view);
    tab.groups.push_back(fit);
    tab.groups.push_back(toggles);
    addTab(tab);
}

// --------------------------------------------------------------------------
void RibbonUI::buildCopilotTab() {
    RibbonTab tab;
    tab.name = "Copilot";
    RibbonGroup panel{"Panel", {
        {IDM_COPILOT_TOGGLE, "Toggle Copilot", "Show or hide the AI Copilot panel"}
    }};
    tab.groups.push_back(panel);
    addTab(tab);
}

// --------------------------------------------------------------------------
void RibbonUI::setActiveTab(int tabIndex) {
    m_activeTab = tabIndex;
    TabCtrl_SetCurSel(m_tabCtrl, tabIndex);
    for (int i = 0; i < static_cast<int>(m_toolbars.size()); ++i) {
        ShowWindow(m_toolbars[i], (i == tabIndex) ? SW_SHOW : SW_HIDE);
    }
}

// --------------------------------------------------------------------------
void RibbonUI::enableButton(int commandId, bool enabled) {
    for (auto& tab : m_tabs) {
        for (auto& group : tab.groups) {
            for (auto& btn : group.buttons) {
                if (btn.commandId == commandId)
                    btn.enabled = enabled;
            }
        }
    }
    // The button may live in any tab's toolbar — update them all.
    for (HWND hTb : m_toolbars) {
        if (hTb) {
            SendMessageW(hTb, TB_ENABLEBUTTON,
                         static_cast<WPARAM>(commandId),
                         MAKELPARAM(enabled ? TRUE : FALSE, 0));
        }
    }
}

void RibbonUI::checkButton(int commandId, bool checked) {
    for (auto& tab : m_tabs) {
        for (auto& group : tab.groups) {
            for (auto& btn : group.buttons) {
                if (btn.commandId == commandId)
                    btn.checked = checked;
            }
        }
    }
    for (HWND hTb : m_toolbars) {
        if (hTb) {
            SendMessageW(hTb, TB_CHECKBUTTON,
                         static_cast<WPARAM>(commandId),
                         MAKELPARAM(checked ? TRUE : FALSE, 0));
        }
    }
}

bool RibbonUI::handleCommand(int /*commandId*/) {
    return false; // parent handles commands
}

// ─────────────────────────────────────────────────────────────────────────────
// Icon-shape mapping helpers
// ─────────────────────────────────────────────────────────────────────────────

// Named constants used in renderTab / RibbonContainerProc
static constexpr int TOOLBAR_ID_OFFSET = 10; // IDC_RIBBON + this = first toolbar child ID
static constexpr int SEPARATOR_PX      =  8; // group separator width in pixels
static constexpr double kPi = 3.14159265358979323846;

// Shape IDs (used by drawIconShape)
enum {
    ISHAPE_POINT    =  0,
    ISHAPE_LINE     =  1,
    ISHAPE_ARC      =  2,
    ISHAPE_CIRCLE   =  3,
    ISHAPE_RECT     =  4,
    ISHAPE_SPLINE   =  5,
    ISHAPE_HELIX    =  6,
    ISHAPE_CUBE     =  7,
    ISHAPE_CYLINDER =  8,
    ISHAPE_SPHERE   =  9,
    ISHAPE_EXTRUDE  = 10,
    ISHAPE_REVOLVE  = 11,
    ISHAPE_FILLET   = 12,
    ISHAPE_BOOLEAN  = 13,
    ISHAPE_FILE     = 14,
    ISHAPE_GEAR     = 15,
    ISHAPE_EYE      = 16,
    ISHAPE_WRENCH   = 17,
    ISHAPE_MOVE     = 18,
    ISHAPE_UNDO     = 19,
    ISHAPE_POLYGON  = 20,
    ISHAPE_COPILOT  = 21,
};

static int iconShapeForId(int cmdId) {
    switch (cmdId) {
    // Points
    case IDM_WF_POINT: case IDM_WF_POINT_DYNAMIC:
    case IDM_WF_POINT_NODE: case IDM_WF_POINT_SEGMENT:
        return ISHAPE_POINT;
    // Lines
    case IDM_WF_LINE: case IDM_WF_LINE_CLOSEST: case IDM_WF_LINE_BISECT:
    case IDM_WF_LINE_PERP: case IDM_WF_LINE_PARALLEL: case IDM_WF_LINE_NORMAL:
        return ISHAPE_LINE;
    // Arcs
    case IDM_WF_ARC: case IDM_WF_ARC_TANGENT:
    case IDM_WF_ARC_ENDPOINTS: case IDM_WF_ARC_POLAR:
        return ISHAPE_ARC;
    // Circles / ellipses
    case IDM_WF_CIRCLE: case IDM_WF_CIRCLE_EDGE: case IDM_WF_ELLIPSE:
    case IDM_SOLID_TORUS:
        return ISHAPE_CIRCLE;
    // Splines / curves
    case IDM_WF_SPLINE: case IDM_WF_SPLINE_AUTO: case IDM_WF_SPLINE_BLENDED:
    case IDM_WF_CURVE_ONE_EDGE: case IDM_WF_CURVE_ALL_EDGES:
    case IDM_WF_SILHOUETTE:
    case IDM_WF_CURVE_SLICE_PLN: case IDM_WF_CURVE_SLICE_CRV:
    case IDM_WF_CURVE_FLOWLINE: case IDM_WF_CURVE_INTERSECT:
    case IDM_SURF_LOFT: case IDM_SURF_REVOLVE: case IDM_SURF_EXTEND:
    case IDM_SURF_FILLET: case IDM_SURF_OFFSET:
    case IDM_SURF_TRIM: case IDM_SURF_UNTRIM:
    case IDM_SURF_SWEPT: case IDM_SURF_NET: case IDM_SURF_FENCE:
    case IDM_SURF_DRAFT_SURF: case IDM_SURF_TRIM_TO_SURF:
        return ISHAPE_SPLINE;
    // Flat / rectangular surfaces
    case IDM_SURF_FLAT_BOUNDARY: case IDM_SURF_FROM_SOLID:
        return ISHAPE_RECT;
    // Rectangles / bounding box
    case IDM_WF_RECTANGLE: case IDM_WF_RECT_SHAPES: case IDM_WF_BBOX:
        return ISHAPE_RECT;
    // Polygon
    case IDM_WF_POLYGON:
        return ISHAPE_POLYGON;
    // Helix
    case IDM_WF_HELIX:
        return ISHAPE_HELIX;
    // Fillet / chamfer / trim type modifiers
    case IDM_WF_MOD_FILLET: case IDM_WF_MOD_CHAMFER: case IDM_WF_MOD_DYN_TRIM:
    case IDM_WF_MOD_BREAK_TWO: case IDM_WF_MOD_BREAK_INT:
    case IDM_WF_MOD_JOIN: case IDM_WF_MOD_INTERSECT:
    case IDM_WF_MOD_PROJECT: case IDM_WF_MOD_OFFSET: case IDM_WF_MOD_ROLL:
    case IDM_SOLID_FILLET: case IDM_SOLID_CHAMFER:
    case IDM_SOLID_TRIM: case IDM_SOLID_DRAFT:
        return ISHAPE_FILLET;
    // Extrude-family solids
    case IDM_SOLID_EXTRUDE: case IDM_SOLID_SWEEP:
    case IDM_SOLID_LOFT: case IDM_SOLID_THICKEN:
        return ISHAPE_EXTRUDE;
    case IDM_SOLID_REVOLVE:
        return ISHAPE_REVOLVE;
    // Block-like solids
    case IDM_SOLID_BLOCK: case IDM_SOLID_SHELL:
    case IDM_SOLID_IMPRESS: case IDM_SOLID_FROM_SURF:
        return ISHAPE_CUBE;
    case IDM_SOLID_CYLINDER: case IDM_SOLID_CONE:
        return ISHAPE_CYLINDER;
    case IDM_SOLID_SPHERE: case IDM_SOLID_HOLE:
        return ISHAPE_SPHERE;
    // Boolean ops
    case IDM_SOLID_UNION: case IDM_SOLID_SUBTRACT: case IDM_SOLID_INTERSECT:
        return ISHAPE_BOOLEAN;
    // File / edit
    case IDM_FILE_NEW: case IDM_FILE_OPEN: case IDM_FILE_SAVE:
    case IDM_FILE_IMPORT: case IDM_EDIT_COPY: case IDM_EDIT_PASTE:
    case IDM_EDIT_DELETE: case IDM_TOGGLE_SELECT_MODE:
        return ISHAPE_FILE;
    case IDM_EDIT_UNDO: case IDM_EDIT_REDO:
        return ISHAPE_UNDO;
    // Transform
    case IDM_GEOM_MOVE: case IDM_GEOM_ROTATE: case IDM_GEOM_SCALE:
        return ISHAPE_MOVE;
    // View
    case IDM_VIEW_WIREFRAME: case IDM_VIEW_SHADED: case IDM_VIEW_TRANSLU:
    case IDM_VIEW_ISOMETRIC: case IDM_VIEW_TOP: case IDM_VIEW_FRONT:
    case IDM_VIEW_RIGHT: case IDM_VIEW_BACK: case IDM_VIEW_BOTTOM:
    case IDM_VIEW_LEFT: case IDM_VIEW_FIT: case IDM_VIEW_ZOOM_SELECTED:
    case IDM_VIEW_TOGGLE_GRID: case IDM_VIEW_TOGGLE_GNOMON:
        return ISHAPE_EYE;
    // Machine / toolpath
    case IDM_MACHINE_GEN_POCKET: case IDM_MACHINE_GEN_CONTOUR:
    case IDM_MACHINE_CHAMFER: case IDM_MACHINE_THREAD:
    case IDM_MACHINE_3D_WATERLINE: case IDM_MACHINE_3D_SCALLOP:
    case IDM_MACHINE_3D_RASTER: case IDM_MACHINE_5AXIS:
    case IDM_MACHINE_PROBE_Z: case IDM_MACHINE_PROBE_BORE:
    case IDM_MACHINE_PROBE_CORNER: case IDM_MACHINE_BACKPLOT:
    case IDM_MACHINE_VERIFY: case IDM_MACHINE_SIM: case IDM_MACHINE_POST:
    case IDM_MACHINE_REGEN: case IDM_MACHINE_SUMMARY:
    case IDM_SETUP_CONSTRAINTS: case IDM_SETUP_POST_PROFILE:
    case IDM_SETUP_TOOL_DB: case IDM_SETUP_PERF_MODE:
    case IDM_SETUP_GUIDANCE: case IDM_SETUP_AUDIT_LOG:
    case IDM_TOOLPATH_MGR_TOGGLE: case IDM_TOOLPATH_TOGGLE_DISP:
    case IDM_TOOLPATH_COPY_PARAMS:
        return ISHAPE_GEAR;
    // Model Prep
    case IDM_PREP_HEAL: case IDM_PREP_REM_FILLET: case IDM_PREP_BOUNDS:
    case IDM_PREP_CLASSIFY: case IDM_PREP_DRAFT: case IDM_PREP_SPLIT:
        return ISHAPE_WRENCH;
    // Copilot
    case IDM_COPILOT_TOGGLE:
        return ISHAPE_COPILOT;
    default:
        return ISHAPE_RECT;
    }
}

static COLORREF iconBgForId(int cmdId) {
    if (cmdId >= 1000 && cmdId < 2000) return RGB(0x1A, 0x42, 0x80); // File/Edit – cobalt
    if (cmdId >= 2000 && cmdId < 3000) return RGB(0x72, 0x38, 0x00); // Machine  – amber
    if (cmdId >= 3000 && cmdId < 4000) return RGB(0x22, 0x28, 0x45); // View     – slate
    if (cmdId >= 4000 && cmdId < 4100) return RGB(0x0E, 0x52, 0x1C); // Wireframe– forest
    if (cmdId >= 4100 && cmdId < 4200) return RGB(0x00, 0x42, 0x58); // Surfaces – teal
    if (cmdId >= 4200 && cmdId < 4300) return RGB(0x3C, 0x0C, 0x7A); // Solids   – violet
    if (cmdId >= 4300 && cmdId < 4400) return RGB(0x58, 0x2A, 0x0C); // Prep     – brown
    if (cmdId >= 5000 && cmdId < 6000) return RGB(0x16, 0x26, 0x72); // Edit     – navy
    if (cmdId >= 6000 && cmdId < 7000) return RGB(0x62, 0x16, 0x28); // Transform– crimson
    if (cmdId >= 9000 && cmdId < 9100) return RGB(0x08, 0x0A, 0x3E); // Copilot  – indigo
    return RGB(0x30, 0x30, 0x30);
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::drawIconShape
// ─────────────────────────────────────────────────────────────────────────────
void RibbonUI::drawIconShape(HDC dc, const RECT& r, int shapeId,
                              COLORREF bg, COLORREF fg) {
    const int w  = r.right  - r.left;
    const int h  = r.bottom - r.top;
    const int cx = r.left + w / 2;
    const int cy = r.top  + h / 2;
    const int m  = 3;

    HBRUSH bgBrush = CreateSolidBrush(bg);
    FillRect(dc, &r, bgBrush);
    DeleteObject(bgBrush);

    HPEN   pen     = CreatePen(PS_SOLID, 2, fg);
    HBRUSH fgBrush = CreateSolidBrush(fg);
    HPEN   oldPen  = static_cast<HPEN>  (SelectObject(dc, pen));
    HBRUSH oldBrush= static_cast<HBRUSH>(SelectObject(dc, fgBrush));
    SetBkMode(dc, TRANSPARENT);

    switch (shapeId) {

    case ISHAPE_POINT:
        Ellipse(dc, cx - 4, cy - 4, cx + 4, cy + 4);
        break;

    case ISHAPE_LINE:
        MoveToEx(dc, r.left + m, r.bottom - m, nullptr);
        LineTo  (dc, r.right - m, r.top + m);
        break;

    case ISHAPE_ARC:
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, r.left + m, r.top + m + 2, r.right - m, r.bottom - m + 2,
            r.right - m, cy, r.left + m, cy);
        break;

    case ISHAPE_CIRCLE:
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, r.left + m, r.top + m, r.right - m, r.bottom - m);
        break;

    case ISHAPE_RECT:
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, r.left + m, r.top + m, r.right - m, r.bottom - m);
        break;

    case ISHAPE_SPLINE: {
        POINT pts[4] = {
            {r.left + m,  r.bottom - m},
            {cx - 2,      r.top + m},
            {cx + 2,      r.bottom - m},
            {r.right - m, r.top + m}
        };
        PolyBezier(dc, pts, 4);
        break;
    }

    case ISHAPE_HELIX: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, cx - 9, cy - 9, cx + 9, cy + 9, cx,      cy - 9, cx + 9, cy);
        Arc(dc, cx - 6, cy - 6, cx + 6, cy + 6, cx + 6,  cy,     cx,     cy - 6);
        Arc(dc, cx - 3, cy - 3, cx + 3, cy + 3, cx,      cy + 3, cx - 3, cy);
        break;
    }

    case ISHAPE_CUBE: {
        // Isometric diamond outline + top face lines
        const int s = 7;
        POINT diamond[5] = {
            {cx,     cy - s}, {cx + s, cy},
            {cx,     cy + s}, {cx - s, cy}, {cx, cy - s}
        };
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Polyline(dc, diamond, 5);
        POINT top[4] = {
            {cx - s - 3, cy - 3}, {cx - 3, cy - s - 3},
            {cx + s - 3, cy - s}, {cx + s, cy}
        };
        Polyline(dc, top, 4);
        MoveToEx(dc, cx,     cy - s, nullptr); LineTo(dc, cx - 3,     cy - s - 3);
        MoveToEx(dc, cx - s, cy,     nullptr); LineTo(dc, cx - s - 3, cy - 3);
        break;
    }

    case ISHAPE_CYLINDER: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int cw = w / 2 - m, ch = 4;
        Ellipse(dc, cx - cw, r.top + m, cx + cw, r.top + m + ch * 2);
        MoveToEx(dc, cx - cw, r.top + m + ch, nullptr);
        LineTo  (dc, cx - cw, r.bottom - m);
        MoveToEx(dc, cx + cw, r.top + m + ch, nullptr);
        LineTo  (dc, cx + cw, r.bottom - m);
        Arc(dc, cx - cw, r.bottom - m - ch * 2, cx + cw, r.bottom - m,
            cx + cw, r.bottom - m - ch, cx - cw, r.bottom - m - ch);
        break;
    }

    case ISHAPE_SPHERE: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int sr = h / 2 - m;
        Ellipse(dc, cx - sr, cy - sr, cx + sr, cy + sr);
        Arc(dc, cx - sr, cy - sr / 2, cx + sr, cy + sr / 2,
            cx + sr, cy, cx - sr, cy);
        break;
    }

    case ISHAPE_EXTRUDE: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, cx - 5, cy, cx + 5, r.bottom - m);
        POINT arrow[3] = {{cx, r.top + m}, {cx - 5, cy - 2}, {cx + 5, cy - 2}};
        SelectObject(dc, fgBrush);
        Polygon(dc, arrow, 3);
        break;
    }

    case ISHAPE_REVOLVE: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int rr = h / 2 - m - 1;
        Arc(dc, cx - rr, cy - rr, cx + rr, cy + rr, cx + rr, cy, cx, cy - rr);
        POINT ah[3] = {{cx, cy - rr}, {cx - 3, cy - rr + 4}, {cx + 3, cy - rr + 4}};
        SelectObject(dc, fgBrush);
        Polygon(dc, ah, 3);
        break;
    }

    case ISHAPE_FILLET:
        MoveToEx(dc, r.left + m,     r.bottom - m,     nullptr);
        LineTo  (dc, r.left + m,     r.top + m + 6);
        MoveToEx(dc, r.left + m + 6, r.top + m,        nullptr);
        LineTo  (dc, r.right - m,    r.top + m);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, r.left + m, r.top + m, r.left + m + 12, r.top + m + 12,
            r.left + m + 6, r.top + m, r.left + m, r.top + m + 6);
        break;

    case ISHAPE_BOOLEAN: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - 9, cy - 7, cx + 5, cy + 7);
        Ellipse(dc, cx - 5, cy - 7, cx + 9, cy + 7);
        break;
    }

    case ISHAPE_FILE: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int fw = w / 2, fh = h - m * 2, fold = 5;
        POINT doc[6] = {
            {cx - fw / 2,        cy - fh / 2},
            {cx + fw / 2 - fold, cy - fh / 2},
            {cx + fw / 2,        cy - fh / 2 + fold},
            {cx + fw / 2,        cy + fh / 2},
            {cx - fw / 2,        cy + fh / 2},
            {cx - fw / 2,        cy - fh / 2}
        };
        Polyline(dc, doc, 6);
        POINT corner[3] = {
            {cx + fw / 2 - fold, cy - fh / 2},
            {cx + fw / 2 - fold, cy - fh / 2 + fold},
            {cx + fw / 2,        cy - fh / 2 + fold}
        };
        Polyline(dc, corner, 3);
        break;
    }

    case ISHAPE_GEAR: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - 6, cy - 6, cx + 6, cy + 6);
        for (int i = 0; i < 8; ++i) {
            double angle = i * kPi / 4.0;
            int x1 = cx + static_cast<int>(6.0  * std::cos(angle));
            int y1 = cy + static_cast<int>(6.0  * std::sin(angle));
            int x2 = cx + static_cast<int>(10.0 * std::cos(angle));
            int y2 = cy + static_cast<int>(10.0 * std::sin(angle));
            MoveToEx(dc, x1, y1, nullptr);
            LineTo  (dc, x2, y2);
        }
        break;
    }

    case ISHAPE_EYE: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, r.left + m, cy - 5, r.right - m, cy + 7,
            r.left + m, cy + 1, r.right - m, cy + 1);
        Arc(dc, r.left + m, cy - 7, r.right - m, cy + 5,
            r.right - m, cy - 1, r.left + m, cy - 1);
        Ellipse(dc, cx - 3, cy - 3, cx + 3, cy + 3);
        break;
    }

    case ISHAPE_WRENCH: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, r.left + m, r.top + m, r.left + m + 9, r.top + m + 9);
        MoveToEx(dc, r.left + m + 6, r.top + m + 6, nullptr);
        LineTo  (dc, r.right - m, r.bottom - m);
        break;
    }

    case ISHAPE_MOVE: {
        MoveToEx(dc, r.left + m, cy, nullptr); LineTo(dc, r.right - m, cy);
        MoveToEx(dc, cx, r.top + m, nullptr);  LineTo(dc, cx, r.bottom - m);
        SelectObject(dc, fgBrush);
        POINT up[3]   = {{cx, r.top + m},    {cx - 3, r.top + m + 4},    {cx + 3, r.top + m + 4}};
        POINT dn[3]   = {{cx, r.bottom - m}, {cx - 3, r.bottom - m - 4}, {cx + 3, r.bottom - m - 4}};
        POINT lf[3]   = {{r.left + m, cy},   {r.left + m + 4, cy - 3},   {r.left + m + 4, cy + 3}};
        POINT rt[3]   = {{r.right - m, cy},  {r.right - m - 4, cy - 3},  {r.right - m - 4, cy + 3}};
        Polygon(dc, up, 3); Polygon(dc, dn, 3);
        Polygon(dc, lf, 3); Polygon(dc, rt, 3);
        break;
    }

    case ISHAPE_UNDO: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, cx - 7, cy - 7, cx + 7, cy + 7, cx + 7, cy, cx, cy - 7);
        POINT ah[3] = {{cx, cy - 7}, {cx - 4, cy - 9}, {cx - 4, cy - 5}};
        SelectObject(dc, fgBrush);
        Polygon(dc, ah, 3);
        break;
    }

    case ISHAPE_POLYGON: {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int pr = h / 2 - m;
        POINT pts[6];
        for (int i = 0; i < 6; ++i) {
            double angle = i * kPi / 3.0 - kPi / 6.0;
            pts[i] = {cx + static_cast<int>(pr * std::cos(angle)),
                      cy + static_cast<int>(pr * std::sin(angle))};
        }
        Polygon(dc, pts, 6);
        break;
    }

    case ISHAPE_COPILOT: {
        // Four-pointed star
        POINT star[8] = {
            {cx,      cy - 10}, {cx + 2,  cy - 2},
            {cx + 10, cy},      {cx + 2,  cy + 2},
            {cx,      cy + 10}, {cx - 2,  cy + 2},
            {cx - 10, cy},      {cx - 2,  cy - 2}
        };
        SelectObject(dc, fgBrush);
        Polygon(dc, star, 8);
        break;
    }

    default:
        Rectangle(dc, r.left + m + 2, r.top + m + 2,
                  r.right - m - 2, r.bottom - m - 2);
        break;
    }

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(fgBrush);
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::makeToolIcon
// ─────────────────────────────────────────────────────────────────────────────
HBITMAP RibbonUI::makeToolIcon(int commandId) {
    const int sz = ICON_SIZE;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = sz;
    bi.bmiHeader.biHeight      = -sz; // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDC = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(screenDC, &bi, DIB_RGB_COLORS,
                                    &bits, nullptr, 0);
    HDC dc = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(dc, hBmp));

    RECT rc = {0, 0, sz, sz};
    drawIconShape(dc, rc,
                  iconShapeForId(commandId),
                  iconBgForId(commandId),
                  RGB(0xFF, 0xFF, 0xFF));

    SelectObject(dc, oldBmp);
    DeleteDC(dc);
    return hBmp;
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::renderTab  –  create one flat toolbar + image list for a tab
// ─────────────────────────────────────────────────────────────────────────────
void RibbonUI::renderTab(int tabIndex) {
    const RibbonTab& tab = m_tabs[tabIndex];

    HIMAGELIST hIml = ImageList_Create(ICON_SIZE, ICON_SIZE,
                                       ILC_COLOR32, 32, 8);

    DWORD tbStyle = WS_CHILD | WS_CLIPSIBLINGS
                  | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS
                  | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN;
    HWND hTb = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
        tbStyle, 0, 0, 100, ICON_SIZE + 12,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RIBBON + TOOLBAR_ID_OFFSET + tabIndex)),
        m_hInst, nullptr);

    SendMessageW(hTb, TB_BUTTONSTRUCTSIZE,
                 static_cast<WPARAM>(sizeof(TBBUTTON)), 0);
    SendMessageW(hTb, TB_SETIMAGELIST, 0,
                 reinterpret_cast<LPARAM>(hIml));
    SendMessageW(hTb, TB_SETBITMAPSIZE, 0,
                 MAKELPARAM(ICON_SIZE, ICON_SIZE));
    SendMessageW(hTb, TB_SETBUTTONSIZE, 0,
                 MAKELPARAM(ICON_SIZE + 8, ICON_SIZE + 6));

    std::vector<TBBUTTON> buttons;
    int imageIdx = 0;
    bool firstGroup = true;

    for (const auto& group : tab.groups) {
        if (!firstGroup) {
            TBBUTTON sep{};
            sep.iBitmap = SEPARATOR_PX;
            sep.fsStyle = TBSTYLE_SEP;
            buttons.push_back(sep);
        }
        firstGroup = false;

        for (const auto& btn : group.buttons) {
            HBITMAP hbm = makeToolIcon(btn.commandId);
            ImageList_Add(hIml, hbm, nullptr);
            DeleteObject(hbm);

            TBBUTTON tb{};
            tb.iBitmap   = imageIdx++;
            tb.idCommand = btn.commandId;
            tb.fsState   = btn.enabled ? TBSTATE_ENABLED : 0;
            tb.fsStyle   = BTNS_BUTTON;
            tb.iString   = static_cast<INT_PTR>(-1); // no text label on buttons
            buttons.push_back(tb);
        }
    }

    if (!buttons.empty()) {
        SendMessageW(hTb, TB_ADDBUTTONS,
                     static_cast<WPARAM>(buttons.size()),
                     reinterpret_cast<LPARAM>(buttons.data()));
    }
    SendMessageW(hTb, TB_AUTOSIZE, 0, 0);

    ShowWindow(hTb, (tabIndex == m_activeTab) ? SW_SHOW : SW_HIDE);

    m_toolbars.push_back(hTb);
    m_imageLists.push_back(hIml);
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::repositionToolbars
// ─────────────────────────────────────────────────────────────────────────────
void RibbonUI::repositionToolbars() {
    if (m_toolbars.empty()) return;

    // Calculate the content rect inside the tab control's tab strip.
    RECT contentRect;
    GetClientRect(m_hwnd, &contentRect);
    TabCtrl_AdjustRect(m_tabCtrl, FALSE, &contentRect);

    const int cw = contentRect.right  - contentRect.left;
    const int ch = contentRect.bottom - contentRect.top;

    for (int i = 0; i < static_cast<int>(m_toolbars.size()); ++i) {
        SetWindowPos(m_toolbars[i], HWND_TOP,
                     contentRect.left, contentRect.top, cw, ch,
                     SWP_NOZORDER);
        ShowWindow(m_toolbars[i], (i == m_activeTab) ? SW_SHOW : SW_HIDE);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::onNotify  –  called by the main window's WM_NOTIFY handler
// ─────────────────────────────────────────────────────────────────────────────
void RibbonUI::onNotify(LPARAM lParam) {
    NMHDR* nm = reinterpret_cast<NMHDR*>(lParam);
    if (!nm) return;
    if (nm->hwndFrom == m_tabCtrl && nm->code == TCN_SELCHANGE) {
        int idx = TabCtrl_GetCurSel(m_tabCtrl);
        if (idx >= 0 && idx < static_cast<int>(m_toolbars.size())) {
            m_activeTab = idx;
            for (int i = 0; i < static_cast<int>(m_toolbars.size()); ++i)
                ShowWindow(m_toolbars[i], (i == idx) ? SW_SHOW : SW_HIDE);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RibbonUI::RibbonContainerProc  –  subclass proc on m_hwnd
// Catches WM_NOTIFY from the tab control (TCN_SELCHANGE) and from
// toolbar tooltip controls (TTN_GETDISPINFOW).
// ─────────────────────────────────────────────────────────────────────────────
LRESULT CALLBACK RibbonUI::RibbonContainerProc(HWND hwnd, UINT msg,
                                                WPARAM wp, LPARAM lp) {
    RibbonUI* self = reinterpret_cast<RibbonUI*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (self && msg == WM_NOTIFY) {
        NMHDR* nm = reinterpret_cast<NMHDR*>(lp);

        // Tab selection changed – show the newly selected toolbar
        if (nm->hwndFrom == self->m_tabCtrl && nm->code == TCN_SELCHANGE) {
            int idx = TabCtrl_GetCurSel(self->m_tabCtrl);
            if (idx >= 0 && idx < static_cast<int>(self->m_toolbars.size())) {
                self->m_activeTab = idx;
                for (int i = 0; i < static_cast<int>(self->m_toolbars.size()); ++i)
                    ShowWindow(self->m_toolbars[i], (i == idx) ? SW_SHOW : SW_HIDE);
            }
        }

        // Tooltip text request from any toolbar button
        if (nm->code == TTN_GETDISPINFOW) {
            NMTTDISPINFOW* tt = reinterpret_cast<NMTTDISPINFOW*>(lp);
            int cmdId = static_cast<int>(tt->hdr.idFrom);
            for (const auto& tab : self->m_tabs) {
                for (const auto& group : tab.groups) {
                    for (const auto& btn : group.buttons) {
                        if (btn.commandId == cmdId) {
                            MultiByteToWideChar(CP_UTF8, 0,
                                btn.tooltip.c_str(), -1,
                                tt->szText,
                                static_cast<int>(ARRAYSIZE(tt->szText)));
                            tt->szText[ARRAYSIZE(tt->szText) - 1] = L'\0';
                            return 0;
                        }
                    }
                }
            }
        }
    }

    if (self && self->m_oldContainerProc)
        return CallWindowProcW(self->m_oldContainerProc, hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}
