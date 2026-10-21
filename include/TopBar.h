#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <functional>

namespace Aqua {

    struct TopMenuItem {
        std::wstring text;
        D2D1_RECT_F bounds;
        bool isHovered = false;
        std::function<void()> onClick;
    };

    class TopBar {
    public:
        TopBar();
        ~TopBar();

        bool Create(HINSTANCE hInstance, int screenWidth, int barHeight = 32);
        void Show();
        void Hide();
        HWND GetHwnd() const { return m_hwnd; }

        void SetSpotlightCallback(std::function<void()> callback) { m_onSpotlight = callback; }
        void SetControlCenterCallback(std::function<void()> callback) { m_onControlCenter = callback; }
        void SetMissionControlCallback(std::function<void()> callback) { m_onMissionControl = callback; }

        void Render();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void UpdateLayout();
        void DrawLogo(ID2D1DeviceContext* ctx, float x, float y, float size);
        void DrawWifiIcon(ID2D1DeviceContext* ctx, float x, float y, float size);
        void DrawBatteryIcon(ID2D1DeviceContext* ctx, float x, float y, float width, float height, int percent, bool charging);
        void DrawSearchIcon(ID2D1DeviceContext* ctx, float x, float y, float size);
        void DrawControlCenterIcon(ID2D1DeviceContext* ctx, float x, float y, float size);

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;
        int m_width = 0;
        int m_height = 32;
        float m_dpiScale = 1.0f;

        // Interactive Menu Items (Left)
        std::vector<TopMenuItem> m_leftMenuItems;

        // Interactive Status Buttons (Right)
        D2D1_RECT_F m_logoRect = {};
        D2D1_RECT_F m_searchRect = {};
        D2D1_RECT_F m_controlCenterRect = {};
        D2D1_RECT_F m_batteryRect = {};
        D2D1_RECT_F m_wifiRect = {};
        D2D1_RECT_F m_clockRect = {};

        bool m_logoHovered = false;
        bool m_searchHovered = false;
        bool m_controlCenterHovered = false;
        bool m_wifiHovered = false;
        bool m_batteryHovered = false;
        bool m_clockHovered = false;

        // Callbacks
        std::function<void()> m_onSpotlight;
        std::function<void()> m_onControlCenter;
        std::function<void()> m_onMissionControl;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_textFormatRegular;
        ComPtr<IDWriteTextFormat> m_textFormatBold;
        ComPtr<IDWriteTextFormat> m_textFormatClock;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushHover;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
    };

}

