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
        {IDM_FILE_NEW,    "New",    "Create a new file"},
        {IDM_FILE_OPEN,   "Open",   "Open an existing file"},
        {IDM_FILE_SAVE,   "Save",   "Save the current file"},
        {IDM_FILE_IMPORT, "Import", "Import a CAD file"}
    }};
    tab.groups.push_back(file);
    addTab(tab);
}

void RibbonUI::buildWireframeTab() {
    RibbonTab tab;
    tab.name = "Wireframe";
    RibbonGroup draw{"Draw", {
        {IDM_WF_POINT,     "Point",     "Create a point"},
        {IDM_WF_LINE,      "Line",      "Create a line segment"},
        {IDM_WF_ARC,       "Arc",       "Create an arc (3-point)"},
        {IDM_WF_CIRCLE,    "Circle",    "Create a circle"},
        {IDM_WF_RECTANGLE, "Rectangle", "Create a rectangle"},
        {IDM_WF_POLYGON,   "Polygon",   "Create a regular polygon"},
        {IDM_WF_SPLINE,    "Spline",    "Create a NURBS spline"}
    }};
    tab.groups.push_back(draw);
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
        {IDM_SOLID_EXTRUDE,  "Extrude",   "Extrude a profile into a solid"},
        {IDM_SOLID_REVOLVE,  "Revolve",   "Revolve a profile into a solid"}
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
    RibbonGroup ops{"Operations", {
        {IDM_MACHINE_BACKPLOT, "Backplot",   "Run backplot animation"},
        {IDM_MACHINE_VERIFY,   "Verify",     "Run solid verify"},
        {IDM_MACHINE_SIM,      "Machine Sim","Run machine simulation"},
        {IDM_MACHINE_POST,     "Post",       "Generate G-code"}
    }};
    tab.groups.push_back(ops);
    addTab(tab);
}

void RibbonUI::buildViewTab() {
    RibbonTab tab;
    tab.name = "View";
    RibbonGroup mode{"Display", {
        {IDM_VIEW_WIREFRAME, "Wireframe", "Wireframe display"},
        {IDM_VIEW_SHADED,    "Shaded",    "Shaded display"},
        {IDM_VIEW_TRANSLU,   "Translucent","Translucent display"}
    }};
    RibbonGroup view{"View Direction", {
        {IDM_VIEW_ISOMETRIC, "ISO",    "Isometric view"},
        {IDM_VIEW_TOP,       "Top",    "Top view"},
        {IDM_VIEW_FRONT,     "Front",  "Front view"},
        {IDM_VIEW_RIGHT,     "Right",  "Right view"},
        {IDM_VIEW_BACK,      "Back",   "Back view"},
        {IDM_VIEW_BOTTOM,    "Bottom", "Bottom view"},
        {IDM_VIEW_LEFT,      "Left",   "Left view"}
    }};
    RibbonGroup fit{"Zoom", {
        {IDM_VIEW_FIT, "Fit All", "Fit all geometry in view"}
    }};
    tab.groups.push_back(mode);
    tab.groups.push_back(view);
    tab.groups.push_back(fit);
    addTab(tab);
}

// --------------------------------------------------------------------------
void RibbonUI::setActiveTab(int tabIndex) {
    m_activeTab = tabIndex;
    TabCtrl_SetCurSel(m_tabCtrl, tabIndex);
}

// --------------------------------------------------------------------------
void RibbonUI::enableButton(int /*commandId*/, bool /*enabled*/) {
    // In a production implementation this would walk m_tabs and update the
    // corresponding toolbar button state via TB_ENABLEBUTTON.
}

void RibbonUI::checkButton(int /*commandId*/, bool /*checked*/) {}

bool RibbonUI::handleCommand(int /*commandId*/) {
    return false; // parent handles commands
}
