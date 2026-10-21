#pragma once
#include "Common.h"
#include "DCompRenderer.h"

namespace Aqua {

    class ControlCenter {
    public:
        ControlCenter();
        ~ControlCenter();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Toggle();
        void Show();
        void Hide();
        bool IsVisible() const { return m_visible; }
        HWND GetHwnd() const { return m_hwnd; }

        void Render();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_width = 320;
        int m_height = 340;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        // State
        bool m_wifiEnabled = true;
        bool m_bluetoothEnabled = true;
        bool m_darkModeEnabled = true;
        bool m_focusEnabled = false;

        float m_displayBrightness = 0.85f;
        float m_soundVolume = 0.70f;
        bool m_draggingVolume = false;
        bool m_draggingBrightness = false;

        // Interactive Rectangles
        D2D1_RECT_F m_wifiRect = {};
        D2D1_RECT_F m_btRect = {};
        D2D1_RECT_F m_darkRect = {};
        D2D1_RECT_F m_focusRect = {};
        D2D1_RECT_F m_sliderBrightnessRect = {};
        D2D1_RECT_F m_sliderVolumeRect = {};

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatSubtitle;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushBg;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBg;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushActive;
        ComPtr<ID2D1SolidColorBrush> m_brushInactive;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushSliderFill;
        ComPtr<ID2D1SolidColorBrush> m_brushSliderTextDark;
    };

}

