#include "Application.h"
#include "MainWindow.h"
#include <commctrl.h>
#include <stdexcept>

// --------------------------------------------------------------------------
Application& Application::instance() {
    static Application app;
    return app;
}

// --------------------------------------------------------------------------
int Application::run(HINSTANCE hInstance, int nCmdShow) {
    if (!initialize(hInstance))
        return -1;

    m_mainWindow->show(nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    shutdown();
    return static_cast<int>(msg.wParam);
}

// --------------------------------------------------------------------------
bool Application::initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Initialise Common Controls v6 (required for ribbon/toolbar visuals)
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES |
                 ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_COOL_CLASSES |
                 ICC_UPDOWN_CLASS | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    registerWindowClasses();

    m_mainWindow = std::make_unique<MainWindow>();
    if (!m_mainWindow->create(hInstance))
        return false;

    return true;
}

// --------------------------------------------------------------------------
void Application::shutdown() {
    m_mainWindow.reset();
}

// --------------------------------------------------------------------------
void Application::registerWindowClasses() {
    // Additional classes can be registered here; MainWindow registers its own.
}
