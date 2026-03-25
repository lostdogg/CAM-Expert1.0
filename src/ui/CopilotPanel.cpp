#include "CopilotPanel.h"
#include "../Application.h"
#include <commctrl.h>
#include <string>
#include <sstream>

// --------------------------------------------------------------------------
// Ctor / Dtor
// --------------------------------------------------------------------------
CopilotPanel::CopilotPanel(HWND parent, HINSTANCE hInstance)
    : m_hInst(hInstance)
    , m_hBgBrush(CreateSolidBrush(RGB(248, 248, 252)))
{
    // Register the panel window class (once per process)
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PanelProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = PANEL_CLASS;
    RegisterClassExW(&wc);   // ok if already registered

    m_hwnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        parent, nullptr, hInstance, this);

    createControls();

    // Announce ourselves to the user
    appendOutput("CAM Copilot ready.\n"
                 "Type a natural-language command, e.g.:\n"
                 "  \"Rough out this pocket in titanium\"\n"
                 "  \"Optimize cycle time\"\n"
                 "  \"Verify for gouges\"\n"
                 "  \"Explain why G41 is in the output\"\n");
}

CopilotPanel::~CopilotPanel() {
    if (m_hwnd)    DestroyWindow(m_hwnd);
    if (m_hBgBrush) DeleteObject(m_hBgBrush);
}

// --------------------------------------------------------------------------
void CopilotPanel::createControls() {
    if (!m_hwnd) return;

    // Output area (multi-line read-only edit)
    m_hOutput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
        ES_READONLY | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_OUTPUT), m_hInst, nullptr);
    SendMessage(m_hOutput, EM_SETLIMITTEXT, 0xFFFF, 0);

    // Input field
    m_hInput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_INPUT), m_hInst, nullptr);
    SetWindowTextW(m_hInput, L"Type a command…");

    // Send button
    m_hSend = CreateWindowExW(0, L"BUTTON", L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_SEND), m_hInst, nullptr);

    // Apply button
    m_hApply = CreateWindowExW(0, L"BUTTON", L"Apply",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_APPLY), m_hInst, nullptr);

    // Dismiss button
    m_hDismiss = CreateWindowExW(0, L"BUTTON", L"Dismiss",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_DISMISS), m_hInst, nullptr);

    // Optimize button
    m_hOptimize = CreateWindowExW(0, L"BUTTON", L"Optimize",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_OPTIMIZE), m_hInst, nullptr);

    // Verify button
    m_hVerify = CreateWindowExW(0, L"BUTTON", L"Verify",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_VERIFY), m_hInst, nullptr);

    // Clear button
    m_hClear = CreateWindowExW(0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        m_hwnd, reinterpret_cast<HMENU>(IDC_COPILOT_CLEAR), m_hInst, nullptr);
}

// --------------------------------------------------------------------------
// resize
// --------------------------------------------------------------------------
void CopilotPanel::resize(int x, int y, int width, int height) {
    if (!m_hwnd) return;

    MoveWindow(m_hwnd, x, y, width, height, TRUE);

    // Layout constants
    const int pad   = 4;
    const int bh    = BUTTON_HEIGHT;
    const int ih    = INPUT_HEIGHT;
    const int bw    = (width - pad * 2) / 3;   // three buttons per row

    // Output area: fills height minus two bottom rows
    int outH = height - ih - bh - pad * 5;
    if (outH < 10) outH = 10;

    MoveWindow(m_hOutput, pad, pad, width - pad * 2, outH, TRUE);

    // Input row
    int iy  = pad + outH + pad;
    int sendW = 60;
    MoveWindow(m_hInput, pad, iy, width - sendW - pad * 3, ih, TRUE);
    MoveWindow(m_hSend,  width - sendW - pad, iy, sendW, ih, TRUE);

    // Action row: Apply | Dismiss | Optimize | Verify | Clear
    int ay  = iy + ih + pad;
    int aw  = (width - pad * 2) / 5;
    for (int i = 0; i < 5; ++i) {
        HWND hBtn = nullptr;
        switch (i) {
            case 0: hBtn = m_hApply;   break;
            case 1: hBtn = m_hDismiss; break;
            case 2: hBtn = m_hOptimize;break;
            case 3: hBtn = m_hVerify;  break;
            case 4: hBtn = m_hClear;   break;
        }
        if (hBtn)
            MoveWindow(hBtn, pad + i * aw, ay, aw - 2, bh, TRUE);
    }
}

// --------------------------------------------------------------------------
// setCopilotEngine
// --------------------------------------------------------------------------
void CopilotPanel::setCopilotEngine(CopilotEngine* engine) {
    m_engine = engine;
    if (!m_engine) return;

    // Register a callback so the engine can push proactive suggestions
    m_engine->setSuggestionCallback([this](const CopilotResponse& resp) {
        if (!resp.suggestion.empty()) {
            appendOutput("\n" + resp.suggestion + "\n");
            // Enable Apply / Dismiss buttons
            m_hasPendingSuggestion = resp.success;
            EnableWindow(m_hApply,   m_hasPendingSuggestion ? TRUE : FALSE);
            EnableWindow(m_hDismiss, m_hasPendingSuggestion ? TRUE : FALSE);
        }
    });
}

// --------------------------------------------------------------------------
// appendOutput
// --------------------------------------------------------------------------
void CopilotPanel::appendOutput(const std::string& text, bool /*isError*/) {
    if (!m_hOutput) return;

    // Convert to wide string for Win32 EDIT control
    // MultiByteToWideChar returns the required size including null terminator;
    // initialise wstring with (wlen-1) chars so the buffer is exact.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 1) return;   // nothing or only a null terminator
    std::wstring wide(static_cast<std::size_t>(wlen - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wlen);

    // Win32 EDIT uses \r\n for newlines; replace bare \n
    std::wstring crlf;
    crlf.reserve(wide.size());
    for (wchar_t c : wide) {
        if (c == L'\n') crlf += L"\r\n";
        else if (c != L'\r') crlf += c;
    }

    // Append by moving caret to end then replacing selection (empty) with text
    int len = static_cast<int>(GetWindowTextLengthW(m_hOutput));
    SendMessage(m_hOutput, EM_SETSEL, static_cast<WPARAM>(len),
                                      static_cast<LPARAM>(len));
    SendMessage(m_hOutput, EM_REPLACESEL, 0,
                reinterpret_cast<LPARAM>(crlf.c_str()));
    SendMessage(m_hOutput, EM_SCROLLCARET, 0, 0);
}

// --------------------------------------------------------------------------
// handleCommand – called from parent WM_COMMAND
// --------------------------------------------------------------------------
bool CopilotPanel::handleCommand(int commandId) {
    switch (commandId) {
        case IDC_COPILOT_SEND:    onSend();    return true;
        case IDC_COPILOT_APPLY:   onApply();   return true;
        case IDC_COPILOT_DISMISS: onDismiss(); return true;
        case IDC_COPILOT_OPTIMIZE:onOptimize();return true;
        case IDC_COPILOT_VERIFY:  onVerify();  return true;
        case IDC_COPILOT_CLEAR:   onClear();   return true;
        default:                              return false;
    }
}

// --------------------------------------------------------------------------
// Actions
// --------------------------------------------------------------------------
void CopilotPanel::onSend() {
    if (!m_hInput || !m_engine) return;

    int len = GetWindowTextLengthW(m_hInput);
    if (len <= 0) return;

    std::wstring wCmd(static_cast<std::size_t>(len + 1), L'\0');
    GetWindowTextW(m_hInput, wCmd.data(), len + 1);
    SetWindowTextW(m_hInput, L"");   // clear input

    // Convert to UTF-8
    int nbytes = WideCharToMultiByte(CP_UTF8, 0, wCmd.c_str(), -1,
                                     nullptr, 0, nullptr, nullptr);
    if (nbytes <= 0) return;
    std::string cmd(static_cast<std::size_t>(nbytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wCmd.c_str(), -1,
                        cmd.data(), nbytes, nullptr, nullptr);
    // Trim null terminator
    if (!cmd.empty() && cmd.back() == '\0') cmd.pop_back();

    appendOutput("\n> " + cmd + "\n");

    // Process command (callback will append the suggestion)
    m_engine->processCommand(cmd);
}

void CopilotPanel::onApply() {
    if (!m_engine) return;
    bool ok = m_engine->applyLastSuggestion();
    if (ok) {
        appendOutput("[Applied] Toolpath added to the operation list.\n");
    } else {
        appendOutput("[Error] Could not apply the suggestion. "
                     "Ensure a part is loaded and a material is selected.\n");
    }
    m_hasPendingSuggestion = false;
    EnableWindow(m_hApply,   FALSE);
    EnableWindow(m_hDismiss, FALSE);
}

void CopilotPanel::onDismiss() {
    if (!m_engine) return;
    m_engine->rejectLastSuggestion();
    appendOutput("[Dismissed] Suggestion discarded.\n");
    m_hasPendingSuggestion = false;
    EnableWindow(m_hApply,   FALSE);
    EnableWindow(m_hDismiss, FALSE);
}

void CopilotPanel::onOptimize() {
    if (!m_engine) return;
    appendOutput("\n[Copilot] Analysing cycle time…\n");
    CopilotResponse resp = m_engine->analyseCycleTime();
    // The suggestion callback (registered in setCopilotEngine) already displays
    // the result via appendOutput, so we only need to handle the case where
    // the callback was not set.
    if (resp.success && !resp.suggestion.empty() && !m_engine) {
        appendOutput(resp.suggestion + "\n");
    }
}

void CopilotPanel::onVerify() {
    if (!m_engine) return;
    appendOutput("\n[Copilot] Running simulation analysis…\n");
    // Use the engine's stored verify result (set after the last Verify run)
    // rather than passing an empty result.  The engine selects the best
    // available source internally.
    CopilotResponse resp = m_engine->analyseVerifyResult(VerifyResult{});
    if (resp.success && !resp.suggestion.empty() && !m_engine) {
        appendOutput(resp.suggestion + "\n");
    }
}

void CopilotPanel::onClear() {
    if (m_hOutput) SetWindowTextW(m_hOutput, L"");
}

// --------------------------------------------------------------------------
// Window procedure
// --------------------------------------------------------------------------
LRESULT CALLBACK CopilotPanel::PanelProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
    CopilotPanel* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<CopilotPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<CopilotPanel*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->handleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CopilotPanel::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        // Handle button clicks from child controls
        if (HIWORD(wParam) == BN_CLICKED)
            handleCommand(LOWORD(wParam));
        return 0;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        // Give the panel a slightly off-white background (brush is owned by this object)
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, RGB(248, 248, 252));
        return reinterpret_cast<LRESULT>(m_hBgBrush);
    }

    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}
