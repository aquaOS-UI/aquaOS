#pragma once
#include "Common.h"
#include <string>
#include <atomic>
#include <thread>

namespace Aqua {

    class NotificationService {
    public:
        static NotificationService& Instance();

        bool Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Shutdown();

        void PostNotification(const std::wstring& appName, const std::wstring& title, const std::wstring& message, const std::wstring& iconPath = L"");

    private:
        NotificationService();
        ~NotificationService();

        void StartToastListener();
        void StopToastListener();

        HINSTANCE m_hInstance = nullptr;
        std::atomic<bool> m_running{ false };
        std::thread m_listenerThread;
    };

}
