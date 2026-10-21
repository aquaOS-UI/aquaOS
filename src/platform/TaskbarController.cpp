#include "TaskbarController.h"

namespace Aqua {

    TaskbarController& TaskbarController::Instance() {
        static TaskbarController instance;
        return instance;
    }

    TaskbarController::TaskbarController() {
        // Record original Windows Work Area
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &m_originalWorkArea, 0)) {
            m_hasOriginalWorkArea = true;
        }
    }

    TaskbarController::~TaskbarController() {
        RestoreTaskbar();
    }

    void TaskbarController::AdjustWorkArea(int topBarHeight) {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        RECT newWorkArea;
        newWorkArea.left = 0;
        newWorkArea.top = topBarHeight;
        newWorkArea.right = screenWidth;
        newWorkArea.bottom = screenHeight;

        // Apply new desktop work area so maximized windows respect the macOS TopBar
        SystemParametersInfoW(SPI_SETWORKAREA, 0, &newWorkArea, SPIF_SENDCHANGE);
    }

    void TaskbarController::RestoreWorkArea() {
        if (m_hasOriginalWorkArea) {
            SystemParametersInfoW(SPI_SETWORKAREA, 0, &m_originalWorkArea, SPIF_SENDCHANGE);
        }
    }

    void TaskbarController::SuppressTaskbar() {
        // Primary Taskbar
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hTray) {
            ShowWindow(hTray, SW_HIDE);
        }

        // Secondary Taskbars on Multi-monitor setups
        HWND hSec = nullptr;
        while ((hSec = FindWindowExW(nullptr, hSec, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
            ShowWindow(hSec, SW_HIDE);
        }

        // Windows 11 Start Button
        HWND hStart = FindWindowExW(nullptr, nullptr, L"Button", L"Start");
        if (hStart) {
            ShowWindow(hStart, SW_HIDE);
        }

        m_suppressed = true;
    }

    void TaskbarController::RestoreTaskbar() {
        if (!m_suppressed) return;

        // Primary Taskbar
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hTray) {
            ShowWindow(hTray, SW_SHOW);
        }

        // Secondary Taskbars
        HWND hSec = nullptr;
        while ((hSec = FindWindowExW(nullptr, hSec, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
            ShowWindow(hSec, SW_SHOW);
        }

        // Windows 11 Start Button
        HWND hStart = FindWindowExW(nullptr, nullptr, L"Button", L"Start");
        if (hStart) {
            ShowWindow(hStart, SW_SHOW);
        }

        RestoreWorkArea();
        m_suppressed = false;
    }

    void TaskbarController::ToggleTaskbar() {
        if (m_suppressed) {
            RestoreTaskbar();
        } else {
            SuppressTaskbar();
            AdjustWorkArea(32);
        }
    }

}

