#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <string>
#include <functional>
#include <mutex>
#include <queue>

namespace Aqua {

    struct BannerData {
        std::wstring appName;
        std::wstring title;
        std::wstring message;
        std::wstring iconPath;
    };

    class NotificationBanner {
    public:
        static NotificationBanner& Instance();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Shutdown();

        void ShowBanner(const std::wstring& appName, const std::wstring& title, const std::wstring& message, const std::wstring& iconPath = L"");
        void Dismiss();

        HWND GetHwnd() const { return m_hwnd; }
        bool IsVisible() const { return m_visible; }

    private:
        NotificationBanner();
        ~NotificationBanner();

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void Render();
        void ProcessQueue();

        HWND m_hwnd = nullptr;
        HINSTANCE m_hInstance = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 1920;
        int m_screenHeight = 1080;
        float m_dpiScale = 1.0f;

        float m_width = 360.0f;
        float m_height = 76.0f;
        float m_targetX = 0.0f;
        float m_currentX = 0.0f;
        float m_y = 44.0f;

        bool m_visible = false;
        bool m_isHovered = false;
        bool m_isCloseHovered = false;
        bool m_isDismissing = false;
        float m_animProgress = 0.0f;
        DWORD m_displayStartTime = 0;

        std::mutex m_queueMutex;
        std::queue<BannerData> m_bannerQueue;

        std::wstring m_appName;
        std::wstring m_title;
        std::wstring m_message;
        ComPtr<ID2D1Bitmap> m_appIcon;

        D2D1_RECT_F m_closeBtnRect = {};

        // DirectWrite text formats
        ComPtr<IDWriteTextFormat> m_formatHeader;
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatBody;

        // Direct2D brushes
        ComPtr<ID2D1SolidColorBrush> m_brushBg;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushAccent;
        ComPtr<ID2D1SolidColorBrush> m_brushCloseBtn;
    };

}

