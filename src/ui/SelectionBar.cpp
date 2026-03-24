#include "SelectionBar.h"
#include <commctrl.h>

// --------------------------------------------------------------------------
// Button definitions – label (narrow), tooltip text, associated mask
static const struct {
    const wchar_t* label;
    const wchar_t* tooltip;
    SelectMask     mask;
} kButtonDefs[] = {
    { L"All",  L"Select All",           SelectMask::All        },
    { L"Pts",  L"Select Points",        SelectMask::Points     },
    { L"Lns",  L"Select Lines",         SelectMask::Lines      },
    { L"Arc",  L"Select Arcs",          SelectMask::Arcs       },
    { L"Spl",  L"Select Splines",       SelectMask::Splines    },
    { L"Srf",  L"Select Surfaces",      SelectMask::Surfaces   },
    { L"Sol",  L"Select Solids",        SelectMask::Solids     },
    { L"Hol",  L"Select Holes",         SelectMask::Holes      },
    { L"Pln",  L"Select Planar Faces",  SelectMask::PlanarFaces},
    { L"Non",  L"Deselect All",         SelectMask::None       },
};
static constexpr int kButtonCount =
    static_cast<int>(sizeof(kButtonDefs) / sizeof(kButtonDefs[0]));

// --------------------------------------------------------------------------
SelectionBar::SelectionBar(HWND parent, HINSTANCE hInstance)
    : m_hInst(hInstance), m_hwndParent(parent) {

    // Register the host window class if necessary
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = BarProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);  // OK if already registered

    m_hwnd = CreateWindowExW(0, CLASS_NAME, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, BTN_HEIGHT, 100,
        parent, nullptr, hInstance, this);

    createButtons();
}

// --------------------------------------------------------------------------
SelectionBar::~SelectionBar() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// --------------------------------------------------------------------------
void SelectionBar::resize(int x, int y, int width, int height) {
    SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
    layoutButtons(width, height);
}

// --------------------------------------------------------------------------
void SelectionBar::setActiveMask(SelectMask mask) {
    m_currentMask = mask;
    // Visually distinguish the active button using checked state
    for (auto& btn : m_buttons) {
        LONG_PTR style = GetWindowLongPtrW(btn.hwnd, GWL_STYLE);
        if (btn.mask == mask)
            style |=  BS_DEFPUSHBUTTON;
        else
            style &= ~BS_DEFPUSHBUTTON;
        SetWindowLongPtrW(btn.hwnd, GWL_STYLE, style);
        InvalidateRect(btn.hwnd, nullptr, TRUE);
    }
}

// --------------------------------------------------------------------------
void SelectionBar::createButtons() {
    for (int i = 0; i < kButtonCount; ++i) {
        MaskButton btn;
        btn.mask    = kButtonDefs[i].mask;
        btn.label   = kButtonDefs[i].label;
        btn.tooltip = kButtonDefs[i].tooltip;  // stored in btn for safe pointer lifetime
        btn.cmdId   = BTN_BASE_ID + i;

        btn.hwnd = CreateWindowExW(0, L"BUTTON", kButtonDefs[i].label,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER,
            0, i * BTN_HEIGHT, BTN_HEIGHT, BTN_HEIGHT,
            m_hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(btn.cmdId)),
            m_hInst, nullptr);

        m_buttons.push_back(std::move(btn));
    }

    // Attach tooltips after all buttons are pushed (so pointers are stable)
    for (auto& b : m_buttons) {
        if (!b.hwnd) continue;
        HWND hTip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            m_hwnd, nullptr, m_hInst, nullptr);
        if (hTip) {
            TOOLINFOW ti{};
            ti.cbSize   = sizeof(ti);
            ti.hwnd     = m_hwnd;
            ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
            ti.uId      = reinterpret_cast<UINT_PTR>(b.hwnd);
            // Use the mutable copy stored in btn.tooltip to avoid const_cast
            ti.lpszText = b.tooltip.data();
            SendMessageW(hTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
        }
    }

    // Highlight the initial "All" button
    setActiveMask(m_currentMask);
}

// --------------------------------------------------------------------------
void SelectionBar::layoutButtons(int width, int height) {
    // Buttons fill the full width, stacked vertically
    for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
        auto idx = static_cast<std::size_t>(i);
        if (!m_buttons[idx].hwnd) continue;
        SetWindowPos(m_buttons[idx].hwnd, nullptr,
            0, i * BTN_HEIGHT, width, BTN_HEIGHT, SWP_NOZORDER);
    }
    (void)height;
}

// --------------------------------------------------------------------------
// Static WndProc
LRESULT CALLBACK SelectionBar::BarProc(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam) {
    SelectionBar* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<SelectionBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<SelectionBar*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SelectionBar::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND) {
        int cmdId = LOWORD(wParam);
        int idx   = cmdId - BTN_BASE_ID;
        if (idx >= 0 && idx < kButtonCount) {
            SelectMask chosen = kButtonDefs[idx].mask;
            setActiveMask(chosen);
            if (m_callback) m_callback(chosen);
            return 0;
        }
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}
