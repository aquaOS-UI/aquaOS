#pragma once
#include "Common.h"
#include <string>

namespace Aqua {

    class HardwareController {
    public:
        static HardwareController& Instance();

        // 1. Physical Display Brightness (DDC/CI + Hardware Gamma LUT Ramp)
        float GetBrightness();
        void SetBrightness(float level);
        void RestoreBrightness();

        // 2. Wireless Radio Controls (Wi-Fi & Bluetooth)
        bool IsWifiEnabled();
        void SetWifiEnabled(bool enable);

        bool IsBluetoothEnabled();
        void SetBluetoothEnabled(bool enable);

        // 3. Windows Startup & Auto-Launch Integration
        bool IsStartupEnabled();
        bool SetStartupEnabled(bool enable);

    private:
        HardwareController();
        ~HardwareController();

        float m_currentBrightness = 0.85f;
        bool m_wifiState = true;
        bool m_bluetoothState = true;
    };

}
