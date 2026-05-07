#pragma once
#ifndef RIBBON_UI_H
#define RIBBON_UI_H

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <functional>

// --------------------------------------------------------------------------
// RibbonUI
//
// A ribbon-style toolbar across the top of the application window.
// Organised into tabs mirroring the workflow stages:
//   Home | Wireframe | Surfaces | Solids | Model Prep | Machine | View
//
// Each tab contains groups of buttons/controls. Clicking a tab activates its
// group; buttons dispatch commands via callbacks.
// --------------------------------------------------------------------------

struct RibbonButton {
    int         commandId = 0;
    std::string label;
    std::string tooltip;
    bool        enabled   = true;
    bool        checked   = false;
};

struct RibbonGroup {
    std::string                label;
    std::vector<RibbonButton>  buttons;
};

struct RibbonTab {
    std::string              name;
    std::vector<RibbonGroup> groups;
};

class RibbonUI {
public:
    RibbonUI(HWND parent, HINSTANCE hInstance);
    ~RibbonUI();

    void resize(int x, int y, int width, int height);
    void setActiveTab(int tabIndex);
    int  activeTab() const { return m_activeTab; }

    void enableButton(int commandId, bool enabled);
    void checkButton(int commandId, bool checked);

    // Notification: parent calls this when receiving WM_COMMAND
    bool handleCommand(int commandId);

    // Called from the main window's WM_NOTIFY handler
    void onNotify(LPARAM lParam);

    HWND hwnd() const { return m_hwnd; }

    // Icon size used for all toolbar buttons
    static constexpr int ICON_SIZE = 24;

private:
    void build();
    void buildHomeTab();
    void buildWireframeTab();       // "Geometry & Wireframe"
    void buildSurfacesSolidsTab();  // "Surfaces & Solids" (merged)
    void buildModelPrepTab();       // "Preparation"
    void buildToolpathTab();        // "Toolpath Generation"
    void buildMachineControlTab();  // "Machine Control"
    void buildViewTab();
    void buildCopilotTab();

    void addTab(RibbonTab tab);
    void renderTab(int tabIndex);      // create a TOOLBAR + HIMAGELIST for one tab
    void repositionToolbars();         // fit all toolbars inside the tab display area

    // Programmatic icon generation
    static HBITMAP makeToolIcon(int commandId);
    static void    drawIconShape(HDC dc, const RECT& r, int shapeId,
                                 COLORREF bg, COLORREF fg);

    // Subclass of the ribbon container HWND to intercept TCN_SELCHANGE
    static LRESULT CALLBACK RibbonContainerProc(HWND hwnd, UINT msg,
                                                 WPARAM wp, LPARAM lp);
    WNDPROC m_oldContainerProc = nullptr;

    HWND                     m_hwnd    = nullptr;
    HWND                     m_tabCtrl = nullptr;
    HINSTANCE                m_hInst   = nullptr;
    int                      m_activeTab = 0;
    std::vector<RibbonTab>   m_tabs;

    // One toolbar + image list per tab (parallel to m_tabs)
    std::vector<HWND>       m_toolbars;
    std::vector<HIMAGELIST> m_imageLists;
};

#endif // RIBBON_UI_H
