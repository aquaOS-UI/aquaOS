#pragma once
#include "Common.h"

namespace Aqua {

    class SystemInfo {
    public:
        // Returns formatted string e.g. "Wed Oct 21  1:55 PM"
        static std::wstring GetFormattedClock() {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &t);

            wchar_t buffer[64];
            wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"%a %b %d  %I:%M %p", &tm);
            return std::wstring(buffer);
        }

        // Returns battery percentage (0-100), or -1 if no battery / desktop
        static int GetBatteryPercent(bool& isCharging) {
            SYSTEM_POWER_STATUS sps;
            if (GetSystemPowerStatus(&sps)) {
                isCharging = (sps.BatteryFlag & 8) != 0 || sps.ACLineStatus == 1;
                if (sps.BatteryLifePercent != 255) {
                    return static_cast<int>(sps.BatteryLifePercent);
                }
            }
            isCharging = true;
            return -1;
        }

        // Returns active application / foreground window title
        static std::wstring GetActiveAppTitle() {
            HWND fg = GetForegroundWindow();
            if (!fg) return L"AquaOS";

            wchar_t title[128];
            int len = GetWindowTextW(fg, title, 128);
            if (len > 0) {
                // If title is long, extract the app name after last '-' or '|'
                std::wstring s(title);
                size_t pos = s.rfind(L" - ");
                if (pos != std::wstring::npos && pos + 3 < s.length()) {
                    return s.substr(pos + 3);
                }
                if (s.length() > 24) {
                    return s.substr(0, 24) + L"...";
                }
                return s;
            }
            return L"Finder";
        }

        // Master Audio Volume Control (0.0f - 1.0f)
        static float GetMasterVolume() {
            ComPtr<IMMDeviceEnumerator> enumerator;
            HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
            if (FAILED(hr)) return 0.7f;

            ComPtr<IMMDevice> defaultDevice;
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
            if (FAILED(hr)) return 0.7f;

            ComPtr<IAudioEndpointVolume> endpointVolume;
            hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(endpointVolume.GetAddressOf()));
            if (FAILED(hr)) return 0.7f;

            float currentVolume = 0.0f;
            endpointVolume->GetMasterVolumeLevelScalar(&currentVolume);
            return currentVolume;
        }

        static void SetMasterVolume(float level) {
            level = std::clamp(level, 0.0f, 1.0f);
            ComPtr<IMMDeviceEnumerator> enumerator;
            HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
            if (FAILED(hr)) return;

            ComPtr<IMMDevice> defaultDevice;
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
            if (FAILED(hr)) return;

            ComPtr<IAudioEndpointVolume> endpointVolume;
            hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(endpointVolume.GetAddressOf()));
            if (FAILED(hr)) return;

            endpointVolume->SetMasterVolumeLevelScalar(level, nullptr);
        }
    };

    struct MonitorData {
        HMONITOR hMonitor = nullptr;
        RECT rcMonitor = {};
        RECT rcWork = {};
        bool isPrimary = false;
        float dpiScale = 1.0f;
        std::wstring deviceName;
    };

    class MonitorManager {
    public:
        static std::vector<MonitorData> GetMonitors() {
            std::vector<MonitorData> list;
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* pList = reinterpret_cast<std::vector<MonitorData>*>(lParam);
                MONITORINFOEXW mi = {};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(hMon, &mi)) {
                    MonitorData md;
                    md.hMonitor = hMon;
                    md.rcMonitor = mi.rcMonitor;
                    md.rcWork = mi.rcWork;
                    md.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
                    md.deviceName = mi.szDevice;
                    md.dpiScale = 1.0f;

                    typedef HRESULT(WINAPI* GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
                    static auto pfnGetDpi = (GetDpiForMonitorFn)GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor");
                    UINT dpiX = 96, dpiY = 96;
                    if (pfnGetDpi && SUCCEEDED(pfnGetDpi(hMon, 0, &dpiX, &dpiY))) {
                        md.dpiScale = static_cast<float>(dpiX) / 96.0f;
                    }
                    pList->push_back(md);
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&list));
            return list;
        }

        static MonitorData GetPrimaryMonitor() {
            auto monitors = GetMonitors();
            for (const auto& m : monitors) {
                if (m.isPrimary) return m;
            }
            if (!monitors.empty()) return monitors.front();
            MonitorData fallback;
            fallback.rcMonitor = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            fallback.rcWork = fallback.rcMonitor;
            fallback.isPrimary = true;
            return fallback;
        }

        static MonitorData GetMonitorForPoint(POINT pt) {
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEXW mi = {};
            mi.cbSize = sizeof(mi);
            if (hMon && GetMonitorInfoW(hMon, &mi)) {
                MonitorData md;
                md.hMonitor = hMon;
                md.rcMonitor = mi.rcMonitor;
                md.rcWork = mi.rcWork;
                md.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
                md.deviceName = mi.szDevice;
                md.dpiScale = 1.0f;
                typedef HRESULT(WINAPI* GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
                static auto pfnGetDpi = (GetDpiForMonitorFn)GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor");
                UINT dpiX = 96, dpiY = 96;
                if (pfnGetDpi && SUCCEEDED(pfnGetDpi(hMon, 0, &dpiX, &dpiY))) {
                    md.dpiScale = static_cast<float>(dpiX) / 96.0f;
                }
                return md;
            }
            return GetPrimaryMonitor();
        }

        static MonitorData GetMonitorForWindow(HWND hwnd) {
            if (!hwnd || !IsWindow(hwnd)) return GetPrimaryMonitor();
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEXW mi = {};
            mi.cbSize = sizeof(mi);
            if (hMon && GetMonitorInfoW(hMon, &mi)) {
                MonitorData md;
                md.hMonitor = hMon;
                md.rcMonitor = mi.rcMonitor;
                md.rcWork = mi.rcWork;
                md.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
                md.deviceName = mi.szDevice;
                md.dpiScale = 1.0f;
                typedef HRESULT(WINAPI* GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
                static auto pfnGetDpi = (GetDpiForMonitorFn)GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor");
                UINT dpiX = 96, dpiY = 96;
                if (pfnGetDpi && SUCCEEDED(pfnGetDpi(hMon, 0, &dpiX, &dpiY))) {
                    md.dpiScale = static_cast<float>(dpiX) / 96.0f;
                }
                return md;
            }
            return GetPrimaryMonitor();
        }

        static RECT GetVirtualScreenRect() {
            return RECT{
                GetSystemMetrics(SM_XVIRTUALSCREEN),
                GetSystemMetrics(SM_YVIRTUALSCREEN),
                GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
            };
        }
    };

}

