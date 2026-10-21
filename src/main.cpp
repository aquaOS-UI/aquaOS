#include "Common.h"
#include "SystemInfo.h"
#include "DCompRenderer.h"
#include "TopBar.h"
#include "Dock.h"
#include "Spotlight.h"
#include "ControlCenter.h"
#include "TaskbarController.h"
#include "MissionControl.h"
#include "Launchpad.h"
#include "NotificationCenter.h"
#include "StageManager.h"
#include "WindowTiler.h"
#include "QuickLook.h"
#include "ThemeManager.h"
#include "LockScreen.h"

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
    std::vector<std::unique_ptr<Aqua::TopBar>> topBars;
    Aqua::Dock dock;
    Aqua::Spotlight spotlight;
    Aqua::ControlCenter controlCenter;
    Aqua::MissionControl missionControl;
    Aqua::Launchpad launchpad;
    Aqua::NotificationCenter notificationCenter;
    Aqua::StageManager stageManager;

    auto monitors = Aqua::MonitorManager::GetMonitors();
    if (monitors.empty()) {
        Aqua::MonitorData defMon;
        defMon.rcMonitor = { 0, 0, screenWidth, screenHeight };
        monitors.push_back(defMon);
    }

    for (const auto& mon : monitors) {
        auto bar = std::make_unique<Aqua::TopBar>();
        int monW = mon.rcMonitor.right - mon.rcMonitor.left;
        if (bar->Create(hInstance, mon.rcMonitor.left, mon.rcMonitor.top, monW, 32)) {
            bar->SetSpotlightCallback([&]() {
                controlCenter.Hide();
                missionControl.Hide();
                launchpad.Hide();
                notificationCenter.Hide();
                spotlight.Toggle();
            });

            bar->SetControlCenterCallback([&]() {
                spotlight.Hide();
                missionControl.Hide();
                launchpad.Hide();
                notificationCenter.Hide();
                controlCenter.Toggle();
            });

            bar->SetMissionControlCallback([&]() {
                spotlight.Hide();
                controlCenter.Hide();
                launchpad.Hide();
                notificationCenter.Hide();
                missionControl.Toggle();
            });

            bar->SetLaunchpadCallback([&]() {
                spotlight.Hide();
                controlCenter.Hide();
                missionControl.Hide();
                notificationCenter.Hide();
                launchpad.Toggle();
            });

            bar->SetNotificationCenterCallback([&]() {
                spotlight.Hide();
                controlCenter.Hide();
                missionControl.Hide();
                launchpad.Hide();
                notificationCenter.Toggle();
            });

            bar->SetStageManagerCallback([&]() {
                stageManager.Toggle();
            });

            topBars.push_back(std::move(bar));
        }
    }

    if (topBars.empty()) {
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

    if (!missionControl.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Mission Control.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!launchpad.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Launchpad.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!notificationCenter.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Notification Center.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!stageManager.Create(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to create AquaOS Stage Manager.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!Aqua::WindowTiler::Instance().Initialize(hInstance, screenWidth, screenHeight, 32)) {
        MessageBoxW(nullptr, L"Failed to initialize AquaOS Window Tiler.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!Aqua::QuickLook::Instance().Initialize(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to initialize AquaOS Quick Look.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!Aqua::ThemeManager::Instance().Initialize(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to initialize AquaOS Theme Manager.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!Aqua::LockScreen::Instance().Initialize(hInstance, screenWidth, screenHeight)) {
        MessageBoxW(nullptr, L"Failed to initialize AquaOS Lock Screen.", L"AquaOS Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 6. Connect Inter-component Callbacks
    dock.SetSpotlightCallback([&]() {
        controlCenter.Hide();
        missionControl.Hide();
        launchpad.Hide();
        notificationCenter.Hide();
        spotlight.Toggle();
    });

    dock.SetMissionControlCallback([&]() {
        spotlight.Hide();
        controlCenter.Hide();
        launchpad.Hide();
        notificationCenter.Hide();
        missionControl.Toggle();
    });

    dock.SetLaunchpadCallback([&]() {
        spotlight.Hide();
        controlCenter.Hide();
        missionControl.Hide();
        notificationCenter.Hide();
        launchpad.Toggle();
    });

    // 7. Register Global Hotkeys:
    // Alt + Space for Spotlight Search
    RegisterHotKey(nullptr, 100, MOD_ALT | MOD_NOREPEAT, VK_SPACE);
    // Ctrl + Alt + T to toggle Windows Taskbar visibility anytime
    RegisterHotKey(nullptr, 101, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');
    // F3 for macOS Mission Control Window Grid
    RegisterHotKey(nullptr, 102, 0, VK_F3);
    // F4 for macOS Launchpad Fullscreen App Grid
    RegisterHotKey(nullptr, 103, 0, VK_F4);
    // Ctrl + Alt + N for macOS Notification Center & Widgets
    RegisterHotKey(nullptr, 104, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
    // Ctrl + Alt + S for macOS Stage Manager Window Strip
    RegisterHotKey(nullptr, 105, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S');

    // macOS Sequoia Window Tiling Shortcuts:
    RegisterHotKey(nullptr, 110, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_LEFT);  // Left Half
    RegisterHotKey(nullptr, 111, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_RIGHT); // Right Half
    RegisterHotKey(nullptr, 112, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_UP);    // Maximize / Fill
    RegisterHotKey(nullptr, 113, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_DOWN);  // Center / Restore
    RegisterHotKey(nullptr, 114, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '1');      // Top-Left Quarter
    RegisterHotKey(nullptr, 115, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '2');      // Top-Right Quarter
    RegisterHotKey(nullptr, 116, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '3');      // Bottom-Left Quarter
    RegisterHotKey(nullptr, 117, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '4');      // Bottom-Right Quarter

    // macOS Theme & Wallpaper Shortcuts:
    RegisterHotKey(nullptr, 120, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'D');      // Toggle Light/Dark Mode
    RegisterHotKey(nullptr, 121, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W');      // Cycle Wallpaper Style
    RegisterHotKey(nullptr, 125, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'L');      // Lock Screen

    // 8. Suppress Windows Taskbar & Adjust Work Area for macOS Shell Experience
    Aqua::TaskbarController::Instance().SuppressTaskbar();
    Aqua::TaskbarController::Instance().AdjustWorkArea(32);

    // 9. Present the UI Layer
    for (auto& bar : topBars) {
        bar->Show();
    }
    dock.Show();

    // 10. High-performance Win32 Message Loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 100) {
                controlCenter.Hide();
                missionControl.Hide();
                launchpad.Hide();
                notificationCenter.Hide();
                spotlight.Toggle();
                continue;
            } else if (msg.wParam == 101) {
                Aqua::TaskbarController::Instance().ToggleTaskbar();
                continue;
            } else if (msg.wParam == 102) {
                controlCenter.Hide();
                spotlight.Hide();
                launchpad.Hide();
                notificationCenter.Hide();
                missionControl.Toggle();
                continue;
            } else if (msg.wParam == 103) {
                controlCenter.Hide();
                spotlight.Hide();
                missionControl.Hide();
                notificationCenter.Hide();
                launchpad.Toggle();
                continue;
            } else if (msg.wParam == 104) {
                controlCenter.Hide();
                spotlight.Hide();
                missionControl.Hide();
                launchpad.Hide();
                notificationCenter.Toggle();
                continue;
            } else if (msg.wParam == 105) {
                stageManager.Toggle();
                continue;
            } else if (msg.wParam == 110) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::LeftHalf);
                continue;
            } else if (msg.wParam == 111) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::RightHalf);
                continue;
            } else if (msg.wParam == 112) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::Maximize);
                continue;
            } else if (msg.wParam == 113) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::Center);
                continue;
            } else if (msg.wParam == 114) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::TopLeftQuarter);
                continue;
            } else if (msg.wParam == 115) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::TopRightQuarter);
                continue;
            } else if (msg.wParam == 116) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::BottomLeftQuarter);
                continue;
            } else if (msg.wParam == 117) {
                Aqua::WindowTiler::Instance().SnapForegroundWindow(Aqua::SnapZone::BottomRightQuarter);
                continue;
            } else if (msg.wParam == 120) {
                Aqua::ThemeManager::Instance().ToggleTheme();
                continue;
            } else if (msg.wParam == 121) {
                Aqua::ThemeManager::Instance().CycleWallpaper();
                continue;
            } else if (msg.wParam == 125) {
                Aqua::LockScreen::Instance().Toggle();
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 11. Clean Shutdown & Restore System State
    UnregisterHotKey(nullptr, 100);
    UnregisterHotKey(nullptr, 101);
    UnregisterHotKey(nullptr, 102);
    UnregisterHotKey(nullptr, 103);
    UnregisterHotKey(nullptr, 104);
    UnregisterHotKey(nullptr, 105);
    for (int k = 110; k <= 117; ++k) {
        UnregisterHotKey(nullptr, k);
    }
    UnregisterHotKey(nullptr, 120);
    UnregisterHotKey(nullptr, 121);
    UnregisterHotKey(nullptr, 125);
    Aqua::LockScreen::Instance().Shutdown();
    Aqua::ThemeManager::Instance().Shutdown();
    Aqua::WindowTiler::Instance().Shutdown();
    Aqua::QuickLook::Instance().Shutdown();
    Aqua::TaskbarController::Instance().RestoreTaskbar();
    Aqua::DCompEngine::Instance().Shutdown();
    CoUninitialize();

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return static_cast<int>(msg.wParam);
}

