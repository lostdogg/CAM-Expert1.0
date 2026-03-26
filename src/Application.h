#pragma once
#ifndef APPLICATION_H
#define APPLICATION_H

#include <windows.h>
#include <string>
#include <memory>

class MainWindow;

// Application singleton – manages Win32 lifecycle, config, and top-level state.
class Application {
public:
    static Application& instance();

    // Non-copyable / non-movable singleton
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Entry point called from WinMain
    int run(HINSTANCE hInstance, int nCmdShow);

    // Accessors
    HINSTANCE hInstance() const { return m_hInstance; }
    MainWindow* mainWindow() const { return m_mainWindow.get(); }

    // Application-wide strings
    static constexpr const wchar_t* APP_NAME    = L"CAM-Expert 1.0";
    static constexpr const wchar_t* APP_VERSION = L"1.1.0";

private:
    Application() = default;

    bool initialize(HINSTANCE hInstance);
    void shutdown();
    void registerWindowClasses();

    HINSTANCE                  m_hInstance = nullptr;
    std::unique_ptr<MainWindow> m_mainWindow;
};

#endif // APPLICATION_H
