#include "Application.h"
#include <windows.h>

// --------------------------------------------------------------------------
// WinMain – standard Windows entry point.
// --------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE /*hPrevInstance*/,
                    LPWSTR    /*lpCmdLine*/,
                    int        nCmdShow) {
    return Application::instance().run(hInstance, nCmdShow);
}
