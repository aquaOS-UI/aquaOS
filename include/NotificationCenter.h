#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <string>
#include <vector>

namespace Aqua {

    class NotificationCenter {
    public:
        NotificationCenter();
        ~NotificationCenter();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Show();
        void Hide();
        void Toggle();
        bool IsVisible() const { return m_visible; }

        HWND GetHwnd() const { return m_hwnd; }

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void UpdateMetrics();
        void Render();

        void DrawProgressRing(ID2D1DeviceContext* ctx, float cx, float cy, float radius, float thickness, float percent, D2D1_COLOR_F color);

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_panelWidth = 380;
        int m_panelHeight = 0;
        int m_windowX = 0;
        int m_windowY = 32;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        // Slide Animation
        float m_currentOffset = 400.0f;
        float m_targetOffset = 400.0f;
        bool m_isAnimating = false;

        // Real-time System Metrics
        int m_cpuPercent = 0;
        int m_ramPercent = 0;
        double m_ramUsedGB = 0.0;
        double m_ramTotalGB = 0.0;
        uint64_t m_prevIdleTime = 0;
        uint64_t m_prevKernelTime = 0;
        uint64_t m_prevUserTime = 0;

        // Quick Note content
        std::wstring m_quickNote = L"Design macOS UI layer with pure DirectComposition.\nSmooth 120 FPS animations.";

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatHeaderDate;
        ComPtr<IDWriteTextFormat> m_formatHeaderDay;
        ComPtr<IDWriteTextFormat> m_formatCardTitle;
        ComPtr<IDWriteTextFormat> m_formatMetricValue;
        ComPtr<IDWriteTextFormat> m_formatMetricLabel;
        ComPtr<IDWriteTextFormat> m_formatCalendarHeader;
        ComPtr<IDWriteTextFormat> m_formatCalendarDay;
        ComPtr<IDWriteTextFormat> m_formatNoteText;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBg;
        ComPtr<ID2D1SolidColorBrush> m_brushCardHighlight;
        ComPtr<ID2D1SolidColorBrush> m_brushAccentBlue;
        ComPtr<ID2D1SolidColorBrush> m_brushAccentPurple;
        ComPtr<ID2D1SolidColorBrush> m_brushAccentOrange;
        ComPtr<ID2D1SolidColorBrush> m_brushTrack;
    };

}

