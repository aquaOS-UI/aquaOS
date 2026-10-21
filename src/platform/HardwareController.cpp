#define NOMINMAX
#include "HardwareController.h"
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Radios.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <thread>
#include <algorithm>
#include <vector>

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "windowsapp.lib")

namespace Aqua {

    HardwareController& HardwareController::Instance() {
        static HardwareController s_instance;
        return s_instance;
    }

    HardwareController::HardwareController() {
        // Query initial live radio states
        try {
            winrt::init_apartment();
            auto radios = winrt::Windows::Devices::Radios::Radio::GetRadiosAsync().get();
            for (auto radio : radios) {
                if (radio.Kind() == winrt::Windows::Devices::Radios::RadioKind::WiFi) {
                    m_wifiState = (radio.State() == winrt::Windows::Devices::Radios::RadioState::On);
                } else if (radio.Kind() == winrt::Windows::Devices::Radios::RadioKind::Bluetooth) {
                    m_bluetoothState = (radio.State() == winrt::Windows::Devices::Radios::RadioState::On);
                }
            }
        } catch (...) {}
    }

    HardwareController::~HardwareController() {
        RestoreBrightness();
    }

    float HardwareController::GetBrightness() {
        return m_currentBrightness;
    }

    void HardwareController::SetBrightness(float level) {
        m_currentBrightness = std::clamp(level, 0.05f, 1.0f);
        DWORD ddcValue = static_cast<DWORD>(m_currentBrightness * 100.0f);

        // 1. Hardware DDC/CI for physical external displays
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
            DWORD numMonitors = 0;
            if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMon, &numMonitors) && numMonitors > 0) {
                std::vector<PHYSICAL_MONITOR> phys(numMonitors);
                if (GetPhysicalMonitorsFromHMONITOR(hMon, numMonitors, phys.data())) {
                    for (DWORD i = 0; i < numMonitors; ++i) {
                        SetMonitorBrightness(phys[i].hPhysicalMonitor, static_cast<DWORD>(lParam));
                    }
                    DestroyPhysicalMonitors(numMonitors, phys.data());
                }
            }
            return TRUE;
        }, (LPARAM)ddcValue);

        // 2. Hardware Gamma LUT Ramp (works universally across laptops, internal panels, and virtual displays)
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            WORD ramp[3][256];
            for (int i = 0; i < 256; ++i) {
                DWORD val = static_cast<DWORD>(i * 256 * m_currentBrightness);
                if (val > 65535) val = 65535;
                ramp[0][i] = ramp[1][i] = ramp[2][i] = static_cast<WORD>(val);
            }
            SetDeviceGammaRamp(hdc, ramp);
            ReleaseDC(nullptr, hdc);
        }
    }

    void HardwareController::RestoreBrightness() {
        // Reset hardware gamma ramp to linear 1.0f standard
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            WORD ramp[3][256];
            for (int i = 0; i < 256; ++i) {
                DWORD val = static_cast<DWORD>(i * 256);
                if (val > 65535) val = 65535;
                ramp[0][i] = ramp[1][i] = ramp[2][i] = static_cast<WORD>(val);
            }
            SetDeviceGammaRamp(hdc, ramp);
            ReleaseDC(nullptr, hdc);
        }
    }

    bool HardwareController::IsWifiEnabled() {
        return m_wifiState;
    }

    void HardwareController::SetWifiEnabled(bool enable) {
        m_wifiState = enable;

        std::thread([enable]() {
            try {
                winrt::init_apartment();
                auto radios = winrt::Windows::Devices::Radios::Radio::GetRadiosAsync().get();
                bool handled = false;
                for (auto radio : radios) {
                    if (radio.Kind() == winrt::Windows::Devices::Radios::RadioKind::WiFi) {
                        auto access = radio.RequestAccessAsync().get();
                        if (access == winrt::Windows::Devices::Radios::RadioAccessStatus::Allowed) {
                            radio.SetStateAsync(enable ? winrt::Windows::Devices::Radios::RadioState::On : winrt::Windows::Devices::Radios::RadioState::Off).get();
                            handled = true;
                        }
                    }
                }
                if (!handled) {
                    // Fallback to netsh
                    std::wstring cmd = enable ? L"netsh interface set interface \"Wi-Fi\" admin=enable" : L"netsh interface set interface \"Wi-Fi\" admin=disable";
                    _wsystem(cmd.c_str());
                }
            } catch (...) {}
        }).detach();
    }

    bool HardwareController::IsBluetoothEnabled() {
        return m_bluetoothState;
    }

    void HardwareController::SetBluetoothEnabled(bool enable) {
        m_bluetoothState = enable;

        std::thread([enable]() {
            try {
                winrt::init_apartment();
                auto radios = winrt::Windows::Devices::Radios::Radio::GetRadiosAsync().get();
                for (auto radio : radios) {
                    if (radio.Kind() == winrt::Windows::Devices::Radios::RadioKind::Bluetooth) {
                        auto access = radio.RequestAccessAsync().get();
                        if (access == winrt::Windows::Devices::Radios::RadioAccessStatus::Allowed) {
                            radio.SetStateAsync(enable ? winrt::Windows::Devices::Radios::RadioState::On : winrt::Windows::Devices::Radios::RadioState::Off).get();
                        }
                    }
                }
            } catch (...) {}
        }).detach();
    }

    bool HardwareController::IsStartupEnabled() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD type = 0;
            DWORD size = 0;
            LONG res = RegQueryValueExW(hKey, L"AquaOS", nullptr, &type, nullptr, &size);
            RegCloseKey(hKey);
            return (res == ERROR_SUCCESS);
        }
        return false;
    }

    bool HardwareController::SetStartupEnabled(bool enable) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (enable) {
                wchar_t exePath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
                DWORD bytes = static_cast<DWORD>((cmd.length() + 1) * sizeof(wchar_t));
                RegSetValueExW(hKey, L"AquaOS", 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()), bytes);
            } else {
                RegDeleteValueW(hKey, L"AquaOS");
            }
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

}
