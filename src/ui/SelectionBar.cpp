#include "SelectionBar.h"

SelectionBar::SelectionBar(HWND parent, HINSTANCE hInstance) {
    m_hwnd = CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 30, 100,
        parent, nullptr, hInstance, nullptr);
}

SelectionBar::~SelectionBar() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void SelectionBar::resize(int x, int y, int width, int height) {
    SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
}
