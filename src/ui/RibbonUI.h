#pragma once
#ifndef RIBBON_UI_H
#define RIBBON_UI_H

#include <windows.h>
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

    HWND hwnd() const { return m_hwnd; }

private:
    void build();
    void buildHomeTab();
    void buildWireframeTab();
    void buildSurfacesTab();
    void buildSolidsTab();
    void buildModelPrepTab();
    void buildMachineTab();
    void buildViewTab();

    void addTab(RibbonTab tab);
    void renderTab(int tabIndex);

    HWND                     m_hwnd    = nullptr;
    HWND                     m_tabCtrl = nullptr;
    HWND                     m_toolbar = nullptr;
    HINSTANCE                m_hInst   = nullptr;
    int                      m_activeTab = 0;
    std::vector<RibbonTab>   m_tabs;
};

#endif // RIBBON_UI_H
