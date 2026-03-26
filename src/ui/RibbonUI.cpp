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
    RibbonGroup draw{"Draw", {
        {IDM_WF_POINT,     "Point (P)",     "Create a point [P]"},
        {IDM_WF_LINE,      "Line (L)",      "Create a line segment [L]"},
        {IDM_WF_ARC,       "Arc (A)",       "Create an arc (3-point) [A]"},
        {IDM_WF_CIRCLE,    "Circle (C)",    "Create a circle [C]"},
        {IDM_WF_RECTANGLE, "Rectangle",     "Create a rectangle"},
        {IDM_WF_POLYGON,   "Polygon",       "Create a regular polygon"},
        {IDM_WF_SPLINE,    "Spline",        "Create a NURBS spline"}
    }};
    RibbonGroup xform{"Transform", {
        {IDM_GEOM_MOVE,    "Move (M)",      "Move selected geometry [M]"},
        {IDM_GEOM_ROTATE,  "Rotate (R)",    "Rotate selected geometry [R]"},
        {IDM_GEOM_SCALE,   "Scale (S)",     "Scale selected geometry [S]"}
    }};
    tab.groups.push_back(draw);
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
    RibbonGroup create{"Create", {
        {IDM_SOLID_EXTRUDE,  "Extrude",   "Extrude a profile into a solid (Box)"},
        {IDM_SOLID_REVOLVE,  "Revolve",   "Revolve a profile into a solid (Cylinder)"},
        {IDM_SOLID_SPHERE,   "Sphere",    "Create a parametric sphere"}
    }};
    RibbonGroup boolean_{"Boolean", {
        {IDM_SOLID_UNION,    "Union",     "Boolean union of two solids"},
        {IDM_SOLID_SUBTRACT, "Subtract",  "Boolean subtract from a solid"},
        {IDM_SOLID_INTERSECT,"Intersect", "Boolean intersection of two solids"}
    }};
    RibbonGroup modify{"Modify", {
        {IDM_SOLID_FILLET,   "Fillet",    "Add a fillet edge to a solid"},
        {IDM_SOLID_SHELL,    "Shell",     "Shell/hollow a solid body"}
    }};
    tab.groups.push_back(create);
    tab.groups.push_back(boolean_);
    tab.groups.push_back(modify);
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
