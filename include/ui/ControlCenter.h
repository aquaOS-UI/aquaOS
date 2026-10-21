#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include "MediaControllerService.h"

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

        void CalculateLayout();

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_width = 330;
        int m_height = 360;
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

        // Now Playing Media Widget
        D2D1_RECT_F m_mediaCardRect = {};
        D2D1_RECT_F m_mediaArtRect = {};
        D2D1_RECT_F m_mediaPrevRect = {};
        D2D1_RECT_F m_mediaPlayPauseRect = {};
        D2D1_RECT_F m_mediaNextRect = {};

        bool m_mediaPrevHover = false;
        bool m_mediaPlayHover = false;
        bool m_mediaNextHover = false;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatSubtitle;
        ComPtr<IDWriteTextFormat> m_formatMediaTitle;
        ComPtr<IDWriteTextFormat> m_formatMediaArtist;
        ComPtr<IDWriteTextFormat> m_formatMediaButton;

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
        ComPtr<ID2D1SolidColorBrush> m_brushButtonHover;
        ComPtr<ID2D1SolidColorBrush> m_brushMediaAccent;
    };

}
