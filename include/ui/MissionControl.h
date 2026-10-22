#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <dwmapi.h>
#include <vector>
#include <string>
#include <functional>

namespace Aqua {

    struct WindowCard {
        HWND hwnd = nullptr;
        std::wstring title;
        std::wstring processPath;
        ComPtr<ID2D1Bitmap> icon;
        HTHUMBNAIL hThumbnail = nullptr;
        D2D1_RECT_F cardRect = {};
        D2D1_RECT_F thumbRect = {};
        D2D1_RECT_F closeButtonRect = {};
        bool isHovered = false;
        bool isCloseHovered = false;
        float hoverAnim = 0.0f;
    };

    struct SpaceItem {
        int id = 1;
        std::wstring name;
        D2D1_RECT_F rect = {};
        D2D1_RECT_F thumbRect = {};
        bool isHovered = false;
        bool isActive = false;
    };

    class MissionControl {
    public:
        MissionControl();
        ~MissionControl();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Show();
        void Hide();
        void Toggle();
        bool IsVisible() const { return m_visible; }

        HWND GetHwnd() const { return m_hwnd; }

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void CollectWindows();
        void CalculateLayout();
        void ClearThumbnails();
        void ActivateWindow(HWND targetHwnd);
        void CloseTargetWindow(HWND targetHwnd);
        void SwitchToSpace(int spaceIndex);
        void AddNewSpace();
        void Render();

        HWND m_hwnd = nullptr;
        int m_vx = 0;
        int m_vy = 0;
        int m_screenWidth = 0;
        int m_screenHeight = 0;
        float m_dpiScale = 1.0f;
        bool m_visible = false;
        int m_hoveredIndex = -1;

        int m_currentSpace = 0;
        int m_spaceCount = 2;
        std::vector<SpaceItem> m_spaces;
        D2D1_RECT_F m_addSpaceRect = {};
        bool m_isAddSpaceHovered = false;

        DCompSurface m_surface;

        std::vector<WindowCard> m_cards;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatHeader;
        ComPtr<IDWriteTextFormat> m_formatHint;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBg;
        ComPtr<ID2D1SolidColorBrush> m_brushCardBgHover;
        ComPtr<ID2D1SolidColorBrush> m_brushAccent;
        ComPtr<ID2D1SolidColorBrush> m_brushCloseBtn;
        ComPtr<ID2D1SolidColorBrush> m_brushBtnIdle;
    };

}

