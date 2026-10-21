#include "Common.h"
#include "DCompRenderer.h"
#include "TopBar.h"
#include "Dock.h"
#include "Spotlight.h"
#include "ControlCenter.h"
#include "TaskbarController.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Single Instance Guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AquaOS_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"AquaOS is already running.", L"AquaOS", MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 2. Safety: ensure taskbar is restored if process exits unexpectedly or crashes
    atexit([]() {
        Aqua::TaskbarController::Instance().RestoreTaskbar();
    });

    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        Aqua::TaskbarController::Instance().RestoreTaskbar();
        return EXCEPTION_CONTINUE_SEARCH;
    });

    SetConsoleCtrlHandler([](DWORD ctrlType) -> BOOL {
        Aqua::TaskbarController::Instance().RestoreTaskbar();
        return FALSE;
    }, TRUE);
    // 1. Enable Per-Monitor DPI Awareness v2 for ultra-crisp high-DPI scaling
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 2. Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    // 3. Screen Dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 4. Initialize Shared DirectComposition & Direct2D Engine
    if (!Aqua::DCompEngine::Instance().Initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize DirectComposition & Direct2D engine.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // 5. Instantiate UI Components
    Aqua::TopBar topBar;
    Aqua::Dock dock;
    Aqua::Spotlight spotlight;
    Aqua::ControlCenter controlCenter;

    if (!topBar.Create(hInstance, screenWidth, 32)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS TopBar.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!dock.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Dock.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!spotlight.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Spotlight.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!controlCenter.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Control Center.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 6. Connect Inter-component Callbacks
    topBar.SetSpotlightCallback([&]() {
        controlCenter.Hide();
        spotlight.Toggle();
    });

    topBar.SetControlCenterCallback([&]() {
        spotlight.Hide();
        controlCenter.Toggle();
    });

    dock.SetSpotlightCallback([&]() {
        controlCenter.Hide();
        spotlight.Toggle();
    });

    // 7. Register Global Hotkeys:
    // Alt + Space for Spotlight Search
    RegisterHotKey(nullptr, 100, MOD_ALT | MOD_NOREPEAT, VK_SPACE);
    // Ctrl + Alt + T to toggle Windows Taskbar visibility anytime
    RegisterHotKey(nullptr, 101, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');

    // 8. Suppress Windows Taskbar & Adjust Work Area for macOS Shell Experience
    Aqua::TaskbarController::Instance().SuppressTaskbar();
    Aqua::TaskbarController::Instance().AdjustWorkArea(32);

    // 9. Present the UI Layer
    topBar.Show();
    dock.Show();

    // 10. High-performance Win32 Message Loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 100) {
                spotlight.Toggle();
                continue;
            } else if (msg.wParam == 101) {
                Aqua::TaskbarController::Instance().ToggleTaskbar();
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 11. Clean Shutdown & Restore System State
    UnregisterHotKey(nullptr, 100);
    UnregisterHotKey(nullptr, 101);
    Aqua::TaskbarController::Instance().RestoreTaskbar();
    Aqua::DCompEngine::Instance().Shutdown();
    CoUninitialize();

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return static_cast<int>(msg.wParam);
}

