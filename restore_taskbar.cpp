#include <windows.h>
#include <iostream>

int main() {
    HDESK hDesk = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hDesk) {
        SetThreadDesktop(hDesk);
    }

    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) {
        // Fallback: enum windows on default desktop
        EnumDesktopWindows(hDesk, [](HWND hwnd, LPARAM lParam) -> BOOL {
            wchar_t cls[64] = {};
            GetClassNameW(hwnd, cls, 64);
            if (wcscmp(cls, L"Shell_TrayWnd") == 0) {
                *(HWND*)lParam = hwnd;
                return FALSE;
            }
            return TRUE;
        }, (LPARAM)&hTray);
    }

    if (hTray) {
        std::cout << "Found Shell_TrayWnd: " << (void*)hTray << std::endl;
        ShowWindow(hTray, SW_SHOW);
        EnableWindow(hTray, TRUE);
        std::cout << "ShowWindow SW_SHOW called. IsVisible: " << IsWindowVisible(hTray) << std::endl;
    } else {
        std::cout << "Shell_TrayWnd still not found!" << std::endl;
    }

    // Secondary taskbars
    HWND hSec = nullptr;
    while ((hSec = FindWindowExW(nullptr, hSec, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        ShowWindow(hSec, SW_SHOW);
    }

    // Windows 11 Start button
    HWND hStart = FindWindowExW(nullptr, nullptr, L"Button", L"Start");
    if (hStart) {
        ShowWindow(hStart, SW_SHOW);
    }

    // Reset work area to full monitor dimensions minus default taskbar height (or query monitor info)
    RECT rcScreen = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (hTray) {
        RECT rcTray = {};
        GetWindowRect(hTray, &rcTray);
        if (rcTray.bottom - rcTray.top > 0) {
            // Taskbar is typically at bottom
            rcScreen.bottom = rcTray.top;
        }
    }
    SystemParametersInfoW(SPI_SETWORKAREA, 0, &rcScreen, SPIF_SENDCHANGE);
    std::cout << "Work area restored to: [" << rcScreen.left << ", " << rcScreen.top << ", " << rcScreen.right << ", " << rcScreen.bottom << "]" << std::endl;

    if (hDesk) {
        CloseDesktop(hDesk);
    }
    return 0;
}
