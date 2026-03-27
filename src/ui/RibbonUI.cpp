#include "RibbonUI.h"
#include "../MainWindow.h"
#include <commctrl.h>

// --------------------------------------------------------------------------
RibbonUI::RibbonUI(HWND parent, HINSTANCE hInstance)
    : m_hInst(hInstance) {

    // Container window for the ribbon area (filled with a tab control)
    m_hwnd = CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 100, 100,
        parent, nullptr, hInstance, nullptr);

    // Tab control inside the ribbon container
    m_tabCtrl = CreateWindowExW(0, WC_TABCONTROL, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 100, 100,
        m_hwnd, nullptr, hInstance, nullptr);

    build();
}

// --------------------------------------------------------------------------
RibbonUI::~RibbonUI() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// --------------------------------------------------------------------------
void RibbonUI::resize(int x, int y, int width, int height) {
    SetWindowPos(m_hwnd,    nullptr, x, y, width, height, SWP_NOZORDER);
    SetWindowPos(m_tabCtrl, nullptr, 0, 0, width, height, SWP_NOZORDER);
}

// --------------------------------------------------------------------------
void RibbonUI::build() {
    buildHomeTab();
    buildWireframeTab();
    buildSurfacesTab();
    buildSolidsTab();
    buildModelPrepTab();
    buildMachineTab();
    buildViewTab();
    buildCopilotTab();
}

// --------------------------------------------------------------------------
void RibbonUI::addTab(RibbonTab tab) {
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    auto wname = std::wstring(tab.name.begin(), tab.name.end());
    ti.pszText = const_cast<wchar_t*>(wname.c_str());
    TabCtrl_InsertItem(m_tabCtrl, static_cast<int>(m_tabs.size()), &ti);
    m_tabs.push_back(std::move(tab));
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
    tab.name = "Wireframe";

    // Points group
    RibbonGroup points{"Points", {
        {IDM_WF_POINT,          "Point Pos",    "Create a point at exact coordinates [P]"},
        {IDM_WF_POINT_DYNAMIC,  "Point Dyn",    "Create a point dynamically along a curve, surface, or mesh"},
        {IDM_WF_POINT_NODE,     "Point Node",   "Place points at spline control node locations"},
        {IDM_WF_POINT_SEGMENT,  "Point Seg",    "Create evenly spaced points along a selected entity"}
    }};

    // Lines group
    RibbonGroup lines{"Lines", {
        {IDM_WF_LINE,           "Line Endpts",  "Create a line between two endpoints [L]"},
        {IDM_WF_LINE_CLOSEST,   "Line Closest", "Create the shortest line between two selected entities"},
        {IDM_WF_LINE_BISECT,    "Line Bisect",  "Create a line bisecting the angle between two lines"},
        {IDM_WF_LINE_PERP,      "Line Perp",    "Create a line perpendicular to a selected entity"},
        {IDM_WF_LINE_PARALLEL,  "Line Para",    "Create a line parallel to an existing line at an offset"},
        {IDM_WF_LINE_NORMAL,    "Line Normal",  "Create normal lines to a point, grid, or along a chain"}
    }};

    // Arcs group
    RibbonGroup arcs{"Arcs", {
        {IDM_WF_CIRCLE,         "Circle Ctr",   "Create a circle from centre point and radius [C]"},
        {IDM_WF_CIRCLE_EDGE,    "Circle Edge",  "Create a circle through 2 or 3 circumference points"},
        {IDM_WF_ARC,            "Arc 3 Pts",    "Create an arc passing through three specified points [A]"},
        {IDM_WF_ARC_TANGENT,    "Arc Tangent",  "Create an arc tangent to 1, 2, or 3 existing entities"},
        {IDM_WF_ARC_ENDPOINTS,  "Arc Endpts",   "Create an arc from two endpoints and a radius"},
        {IDM_WF_ARC_POLAR,      "Arc Polar",    "Create an arc using centre, radius, and start/end angles"}
    }};

    // Splines group
    RibbonGroup splines{"Splines", {
        {IDM_WF_SPLINE,         "Spline Man",   "Create a spline by placing control points manually"},
        {IDM_WF_SPLINE_AUTO,    "Spline Auto",  "Auto-fit a smooth spline through selected points"},
        {IDM_WF_SPLINE_BLENDED, "Spline Blend", "Create a smooth blended spline connecting two curves"}
    }};

    // Shapes group
    RibbonGroup shapes{"Shapes", {
        {IDM_WF_RECTANGLE,      "Rectangle",    "Create a rectangle from corners or centre point"},
        {IDM_WF_RECT_SHAPES,    "Rect Shapes",  "Create a rectangular profile with fillets or chamfers"},
        {IDM_WF_POLYGON,        "Polygon",      "Create a regular polygon (hexagon, octagon, etc.)"},
        {IDM_WF_ELLIPSE,        "Ellipse",      "Create an ellipse from centre and major/minor axes"},
        {IDM_WF_HELIX,          "Spiral/Helix", "Create a 2D spiral or 3D helix"},
        {IDM_WF_BBOX,           "Bounding Box", "Generate a 2D/3D bounding box around selected geometry"}
    }};

    // Curves (extraction) group
    RibbonGroup curves{"Curves", {
        {IDM_WF_CURVE_ONE_EDGE,  "Curve 1 Edge", "Extract wireframe from a single solid/surface edge"},
        {IDM_WF_CURVE_ALL_EDGES, "Curve All",    "Extract wireframe from all edges of a solid or surface"},
        {IDM_WF_CURVE_SLICE_PLN, "Slice/Plane",  "Create wireframe cross-sections by slicing with a plane"},
        {IDM_WF_CURVE_SLICE_CRV, "Slice/Curve",  "Create cross-sections perpendicular to a drive curve"},
        {IDM_WF_CURVE_FLOWLINE,  "Flowline",     "Create curves along the U and V flowlines of a surface"},
        {IDM_WF_CURVE_INTERSECT, "At Intersect", "Generate curves where two surfaces or solids intersect"}
    }};

    // Modify group
    RibbonGroup modify{"Modify", {
        {IDM_WF_MOD_FILLET,    "Fillet",      "Apply a rounded radius between two intersecting entities"},
        {IDM_WF_MOD_CHAMFER,   "Chamfer",     "Apply a flat angled break between two intersecting entities"},
        {IDM_WF_MOD_DYN_TRIM,  "Dyn Trim",   "Trim, divide, or lengthen entities dynamically"},
        {IDM_WF_MOD_BREAK_TWO, "Break 2",    "Split an entity into two pieces at a click point"},
        {IDM_WF_MOD_BREAK_INT, "Break Int",  "Break intersecting entities at their crossing points"},
        {IDM_WF_MOD_JOIN,      "Join",        "Recombine collinear lines or coincident arcs"},
        {IDM_WF_MOD_INTERSECT, "Mod Intersect", "Break, trim, or create a point at wireframe/surface intersections"}
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

void RibbonUI::buildSurfacesTab() {
    RibbonTab tab;
    tab.name = "Surfaces";
    RibbonGroup create{"Create", {
        {IDM_SURF_LOFT,    "Loft",    "Loft surface through cross-sections"},
        {IDM_SURF_REVOLVE, "Revolve", "Revolve a curve about an axis"},
        {IDM_SURF_EXTEND,  "Extend",  "Extend a surface to meet another"}
    }};
    RibbonGroup edit{"Edit", {
        {IDM_SURF_FILLET,  "Fillet",  "Fillet between two surfaces"},
        {IDM_SURF_OFFSET,  "Offset",  "Offset a surface by a distance"},
        {IDM_SURF_TRIM,    "Trim",    "Trim surface with a cutting curve"},
        {IDM_SURF_UNTRIM,  "Untrim",  "Remove trim boundaries from surface"}
    }};
    tab.groups.push_back(create);
    tab.groups.push_back(edit);
    addTab(tab);
}

void RibbonUI::buildSolidsTab() {
    RibbonTab tab;
    tab.name = "Solids";

    // Create group – history-based operations from wireframe chains
    RibbonGroup create{"Create", {
        {IDM_SOLID_EXTRUDE,  "Extrude",   "Push a 2D profile along a linear path (Add Boss, Cut Body, or New)"},
        {IDM_SOLID_REVOLVE,  "Revolve",   "Rotate a 2D profile around an axis to create cylindrical shapes"},
        {IDM_SOLID_SWEEP,    "Sweep",     "Move a profile along a path curve to create complex tubular shapes"},
        {IDM_SOLID_LOFT,     "Loft",      "Blend multiple cross-section ribs into a smooth organic solid"},
        {IDM_SOLID_THICKEN,  "Thicken",   "Add thickness to an existing surface to produce a solid body"}
    }};

    // Primitives group – drop standard shapes without needing wireframe
    RibbonGroup prims{"Primitives", {
        {IDM_SOLID_BLOCK,    "Block",     "Create a rectangular solid from dimensions or two corner points"},
        {IDM_SOLID_CYLINDER, "Cylinder",  "Create a cylindrical solid with a defined radius and height"},
        {IDM_SOLID_SPHERE,   "Sphere",    "Create a solid ball from a centre point and radius"},
        {IDM_SOLID_CONE,     "Cone",      "Create a tapered conical solid"},
        {IDM_SOLID_TORUS,    "Torus",     "Create a donut-shaped solid from a major and minor radius"}
    }};

    // Modify group – refine features on an existing solid
    RibbonGroup modify{"Modify", {
        {IDM_SOLID_FILLET,   "Fillet",    "Round off sharp edges with a constant or variable radius"},
        {IDM_SOLID_CHAMFER,  "Chamfer",   "Apply a flat angled break to edges (symmetrical or asymmetrical)"},
        {IDM_SOLID_SHELL,    "Shell",     "Hollow out a solid to a specified wall thickness"},
        {IDM_SOLID_DRAFT,    "Draft",     "Taper vertical faces for moulding or casting pull"},
        {IDM_SOLID_TRIM,     "Trim",      "Cut a solid body using a plane, surface, or another solid"}
    }};

    // Boolean group – combine or subtract solid bodies
    RibbonGroup boolean_{"Boolean", {
        {IDM_SOLID_UNION,    "Add",       "Merge two or more solid bodies into a single entity"},
        {IDM_SOLID_SUBTRACT, "Remove",    "Use one solid to cut a shape out of another"},
        {IDM_SOLID_INTERSECT,"Common",    "Keep only the volume where two solids overlap"}
    }};

    // Advanced / Specialized group
    RibbonGroup advanced{"Advanced", {
        {IDM_SOLID_HOLE,     "Hole",      "Create complex holes: simple, counterbore, countersink, taper, or threaded"},
        {IDM_SOLID_IMPRESS,  "Impression","Generate the negative of a solid (useful for molds and electrodes)"},
        {IDM_SOLID_FROM_SURF,"From Surfs","Convert a collection of closed surfaces into a watertight solid body"}
    }};

    tab.groups.push_back(create);
    tab.groups.push_back(prims);
    tab.groups.push_back(modify);
    tab.groups.push_back(boolean_);
    tab.groups.push_back(advanced);
    addTab(tab);
}

void RibbonUI::buildModelPrepTab() {
    RibbonTab tab;
    tab.name = "Model Prep";
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

void RibbonUI::buildMachineTab() {
    RibbonTab tab;
    tab.name = "Machine";
    RibbonGroup gen2D{"2D", {
        {IDM_MACHINE_GEN_POCKET,  "2D Pocket",  "Generate a dynamic 2D pocket toolpath"},
        {IDM_MACHINE_GEN_CONTOUR, "2D Contour", "Generate a dynamic 2D contour toolpath"},
        {IDM_MACHINE_CHAMFER,     "Chamfer",    "Generate a chamfer toolpath"},
        {IDM_MACHINE_THREAD,      "Thread Mill","Generate a thread milling toolpath"}
    }};
    RibbonGroup gen3D{"3D", {
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
    RibbonGroup ops{"Simulate", {
        {IDM_MACHINE_BACKPLOT, "Backplot",    "Run backplot animation"},
        {IDM_MACHINE_VERIFY,   "Verify",      "Run solid verify"},
        {IDM_MACHINE_SIM,      "Machine Sim", "Run machine simulation"}
    }};
    RibbonGroup post{"Output", {
        {IDM_MACHINE_POST,    "Post",    "Generate G-code (post-process) [Ctrl+P]"},
        {IDM_MACHINE_REGEN,   "Regen",   "Regenerate all toolpaths"},
        {IDM_MACHINE_SUMMARY, "Summary", "Show machining time / length summary"}
    }};
    RibbonGroup toolpathMgr{"Manager", {
        {IDM_TOOLPATH_MGR_TOGGLE,  "Manager [T]",     "Open Toolpath Manager [T]"},
        {IDM_TOOLPATH_TOGGLE_DISP, "Show/Hide [C+S+T]","Toggle toolpath display [Ctrl+Shift+T]"},
        {IDM_TOOLPATH_COPY_PARAMS, "Copy Params [C+S+C]","Copy toolpath parameters [Ctrl+Shift+C]"}
    }};
    tab.groups.push_back(gen2D);
    tab.groups.push_back(gen3D);
    tab.groups.push_back(probe);
    tab.groups.push_back(ops);
    tab.groups.push_back(post);
    tab.groups.push_back(toolpathMgr);
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
}

// --------------------------------------------------------------------------
void RibbonUI::enableButton(int commandId, bool enabled) {
    // Walk all tabs and all groups to find the button with matching commandId
    for (auto& tab : m_tabs) {
        for (auto& group : tab.groups) {
            for (auto& btn : group.buttons) {
                if (btn.commandId == commandId) {
                    btn.enabled = enabled;
                    // If the tab with this button is currently active, update
                    // the toolbar button state via TB_ENABLEBUTTON.
                    if (m_toolbar) {
                        SendMessageW(m_toolbar, TB_ENABLEBUTTON,
                                     static_cast<WPARAM>(commandId),
                                     MAKELPARAM(enabled ? TRUE : FALSE, 0));
                    }
                    return;
                }
            }
        }
    }
}

void RibbonUI::checkButton(int commandId, bool checked) {
    // Walk all tabs and all groups to find the button with matching commandId
    for (auto& tab : m_tabs) {
        for (auto& group : tab.groups) {
            for (auto& btn : group.buttons) {
                if (btn.commandId == commandId) {
                    btn.checked = checked;
                    // Update the toolbar button pressed/checked state
                    if (m_toolbar) {
                        SendMessageW(m_toolbar, TB_CHECKBUTTON,
                                     static_cast<WPARAM>(commandId),
                                     MAKELPARAM(checked ? TRUE : FALSE, 0));
                    }
                    return;
                }
            }
        }
    }
}

bool RibbonUI::handleCommand(int /*commandId*/) {
    return false; // parent handles commands
}
