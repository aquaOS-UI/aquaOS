#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <dwmapi.h>
#include <vector>
#include <string>
#include <functional>

namespace Aqua {

    // Represents a grouped stage in the left Stage Manager strip
    struct StageGroup {
        DWORD pid = 0;
        std::wstring processPath;
        std::wstring appName;
        std::wstring title;
        HWND primaryHwnd = nullptr;
        std::vector<HWND> hwnds;
        ComPtr<ID2D1Bitmap> icon;
        HTHUMBNAIL hThumbnail = nullptr;
        D2D1_RECT_F cardRect = {};
        D2D1_RECT_F thumbRect = {};
        float hoverAnim = 0.0f; // 0.0f to 1.0f smooth hover slide
    };

    class StageManager {
    public:
        StageManager();
        ~StageManager();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Toggle();
        void Show();
        void Hide();
        bool IsVisible() const { return m_enabled; }
        HWND GetHwnd() const { return m_hwnd; }

        void RefreshStages();
        void Render();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void ClearThumbnails();
        void UpdateThumbnails();
        void CalculateLayout();
        void ActivateStage(size_t index);
        void PeekDesktop();

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_topBarHeight = 32;
        int m_stripWidth = 175;
        int m_stripHeight = 0;
        float m_dpiScale = 1.0f;

        bool m_enabled = false;
        float m_slideProgress = 0.0f; // 0.0 to 1.0 animation for sliding in/out
        bool m_animating = false;

        std::vector<StageGroup> m_stages;
        int m_hoveredIndex = -1;
        HWND m_currentForeground = nullptr;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatBadge;
        ComPtr<IDWriteTextFormat> m_formatHeader;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushStripBg;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBg;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBgHover;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushBorderHover;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushBadgeBg;
        ComPtr<ID2D1SolidColorBrush> m_brushStackCard;
        ComPtr<ID2D1SolidColorBrush> m_brushIconPlate;

        static constexpr UINT_PTR TIMER_ANIM = 101;
        static constexpr UINT_PTR TIMER_TRACK = 102;
    };

}

