#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <vector>
#include <string>
#include <functional>

namespace Aqua {

    struct LaunchpadApp {
        std::wstring name;
        std::wstring path;
        ComPtr<ID2D1Bitmap> icon;
        D2D1_RECT_F tileRect = {};
        D2D1_RECT_F iconRect = {};
        D2D1_RECT_F labelRect = {};
        bool isHovered = false;
        float hoverScale = 1.0f;
    };

    class Launchpad {
    public:
        Launchpad();
        ~Launchpad();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Show();
        void Hide();
        void Toggle();
        bool IsVisible() const { return m_visible; }

        HWND GetHwnd() const { return m_hwnd; }

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void ScanInstalledApplications();
        void UpdateFilter();
        void CalculateLayout();
        void LaunchApp(const LaunchpadApp& app);
        void Render();

        HWND m_hwnd = nullptr;
        int m_screenWidth = 0;
        int m_screenHeight = 0;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        DCompSurface m_surface;

        std::vector<LaunchpadApp> m_allApps;
        std::vector<size_t> m_filteredIndices; // Indices into m_allApps

        std::wstring m_searchQuery;
        int m_currentPage = 0;
        int m_itemsPerPage = 35; // 7 columns x 5 rows
        int m_totalPages = 1;
        int m_hoveredIndex = -1;

        // Search bar rect
        D2D1_RECT_F m_searchBarRect = {};

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatLabel;
        ComPtr<IDWriteTextFormat> m_formatSearch;
        ComPtr<IDWriteTextFormat> m_formatSearchPlaceholder;
        ComPtr<IDWriteTextFormat> m_formatPageHint;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushGlassBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushSearchBarBg;
        ComPtr<ID2D1SolidColorBrush> m_brushTileHover;
        ComPtr<ID2D1SolidColorBrush> m_brushDotActive;
        ComPtr<ID2D1SolidColorBrush> m_brushDotInactive;
    };

}

