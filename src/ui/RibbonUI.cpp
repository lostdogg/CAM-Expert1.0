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
    RibbonTab tab; tab.name = "Wireframe";
    addTab(tab);
}

void RibbonUI::buildSurfacesTab() {
    RibbonTab tab; tab.name = "Surfaces";
    addTab(tab);
}

void RibbonUI::buildSolidsTab() {
    RibbonTab tab; tab.name = "Solids";
    addTab(tab);
}

void RibbonUI::buildModelPrepTab() {
    RibbonTab tab; tab.name = "Model Prep";
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
        {IDM_VIEW_ISOMETRIC, "ISO",   "Isometric view"},
        {IDM_VIEW_TOP,       "Top",   "Top view"},
        {IDM_VIEW_FRONT,     "Front", "Front view"},
        {IDM_VIEW_RIGHT,     "Right", "Right view"}
    }};
    tab.groups.push_back(mode);
    tab.groups.push_back(view);
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
