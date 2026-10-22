#include "NotificationService.h"
#include "NotificationBanner.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <chrono>

namespace Aqua {

    NotificationService& NotificationService::Instance() {
        static NotificationService s_instance;
        return s_instance;
    }

    NotificationService::NotificationService() = default;

    NotificationService::~NotificationService() {
        Shutdown();
    }

    bool NotificationService::Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_hInstance = hInstance;

        // 1. Create macOS Notification Banner Overlay Window
        if (!NotificationBanner::Instance().Create(hInstance, screenWidth, screenHeight)) {
            return false;
        }

        // 2. Start Windows Toast Listener thread
        StartToastListener();

        return true;
    }

    void NotificationService::Shutdown() {
        StopToastListener();
        NotificationBanner::Instance().Shutdown();
    }

    void NotificationService::PostNotification(const std::wstring& appName, const std::wstring& title, const std::wstring& message, const std::wstring& iconPath) {
        NotificationBanner::Instance().ShowBanner(appName, title, message, iconPath);
    }

    void NotificationService::StartToastListener() {
        if (m_running.load()) return;
        m_running.store(true);

        m_listenerThread = std::thread([this]() {
            try {
                winrt::init_apartment();
                auto listener = winrt::Windows::UI::Notifications::Management::UserNotificationListener::Current();
                auto status = listener.RequestAccessAsync().get();

                if (status == winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus::Allowed) {
                    auto token = listener.NotificationChanged([this](auto const& sender, auto const& args) {
                        try {
                            if (args.ChangeKind() == winrt::Windows::UI::Notifications::UserNotificationChangedKind::Added) {
                                auto notif = sender.GetNotification(args.UserNotificationId());
                                if (notif) {
                                    std::wstring appName = L"System";
                                    auto appInfo = notif.AppInfo();
                                    if (appInfo) {
                                        appName = appInfo.DisplayInfo().DisplayName().c_str();
                                    }

                                    auto toast = notif.Notification();
                                    auto visual = toast.Visual();
                                    auto bindings = visual.Bindings();

                                    std::wstring title = L"Notification";
                                    std::wstring body = L"";

                                    for (auto const& b : bindings) {
                                        auto textEls = b.GetTextElements();
                                        int idx = 0;
                                        for (auto const& t : textEls) {
                                            if (idx == 0) title = t.Text().c_str();
                                            else if (idx == 1) body = t.Text().c_str();
                                            idx++;
                                        }
                                    }

                                    if (!title.empty()) {
                                        PostNotification(appName, title, body);
                                    }
                                }
                            }
                        } catch (...) {}
                    });

                    while (m_running.load()) {
                        MSG msg = {};
                        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                            TranslateMessage(&msg);
                            DispatchMessageW(&msg);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    }

                    try { listener.NotificationChanged(token); } catch (...) {}
                }
            } catch (...) {
                // UserNotificationListener unavailable or access restricted; continue silently
            }
        });
    }

    void NotificationService::StopToastListener() {
        if (!m_running.load()) return;
        m_running.store(false);

        if (m_listenerThread.joinable()) {
            m_listenerThread.join();
        }
    }

}
