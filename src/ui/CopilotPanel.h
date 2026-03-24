#pragma once
#ifndef COPILOT_PANEL_H
#define COPILOT_PANEL_H

#include <windows.h>
#include <string>
#include <memory>
#include <functional>
#include "../copilot/CopilotEngine.h"

// --------------------------------------------------------------------------
// CopilotPanel – Chat-Style Side Panel
//
// A Windows child window that provides the machinist with a natural-language
// interface to the CAM Copilot.  Layout (top to bottom):
//
//   ┌────────────────────────────────────────┐
//   │  "CAM Copilot"          [?] help icon  │  ← title bar
//   ├────────────────────────────────────────┤
//   │  [Conversation / suggestion display]   │  ← read-only rich-text area
//   │                                        │
//   │                                        │
//   ├────────────────────────────────────────┤
//   │  Type a command…          [Send] [↺]  │  ← input row
//   ├────────────────────────────────────────┤
//   │  [Apply]      [Dismiss]   [Opt] [Vrfy] │  ← action row
//   └────────────────────────────────────────┘
//
// The panel holds a CopilotEngine internally and wires itself to the live
// ToolpathManager through setCopilotEngine().
// --------------------------------------------------------------------------

// Control IDs used inside the panel
constexpr int IDC_COPILOT_OUTPUT  = 5001;
constexpr int IDC_COPILOT_INPUT   = 5002;
constexpr int IDC_COPILOT_SEND    = 5003;
constexpr int IDC_COPILOT_APPLY   = 5004;
constexpr int IDC_COPILOT_DISMISS = 5005;
constexpr int IDC_COPILOT_OPTIMIZE= 5006;
constexpr int IDC_COPILOT_VERIFY  = 5007;
constexpr int IDC_COPILOT_CLEAR   = 5008;

class CopilotPanel {
public:
    CopilotPanel(HWND parent, HINSTANCE hInstance);
    ~CopilotPanel();

    // Position / size the panel within the parent window
    void resize(int x, int y, int width, int height);

    // Wire the engine to the live CAM managers.
    // Must be called before the panel becomes visible.
    void setCopilotEngine(CopilotEngine* engine);

    // Called from the parent WM_COMMAND handler
    bool handleCommand(int commandId);

    HWND hwnd() const { return m_hwnd; }

private:
    // Win32 window procedure
    static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Internal actions
    void onSend();
    void onApply();
    void onDismiss();
    void onOptimize();
    void onVerify();
    void onClear();

    // Append text to the output area
    void appendOutput(const std::string& text, bool isError = false);

    // Create all child controls
    void createControls();

    HWND           m_hwnd      = nullptr;
    HWND           m_hOutput   = nullptr;   // EDIT (multi-line, read-only)
    HWND           m_hInput    = nullptr;   // EDIT (single-line)
    HWND           m_hSend     = nullptr;
    HWND           m_hApply    = nullptr;
    HWND           m_hDismiss  = nullptr;
    HWND           m_hOptimize = nullptr;
    HWND           m_hVerify   = nullptr;
    HWND           m_hClear    = nullptr;
    HINSTANCE      m_hInst     = nullptr;
    HBRUSH         m_hBgBrush  = nullptr;   // background brush (owned)

    CopilotEngine* m_engine    = nullptr;   // not owned
    bool           m_hasPendingSuggestion = false;

    static constexpr int INPUT_HEIGHT   = 24;
    static constexpr int BUTTON_HEIGHT  = 26;
    static constexpr int TITLE_HEIGHT   = 22;
    static constexpr const wchar_t* PANEL_CLASS = L"CAMCopilotPanel";
};

#endif // COPILOT_PANEL_H
