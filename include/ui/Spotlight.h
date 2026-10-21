#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include "AppCatalogService.h"
#include <string>
#include <vector>

namespace Aqua {

    class Spotlight {
    public:
        Spotlight();
        ~Spotlight();

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

        void UpdateSearchResults();
        void ExecuteSelected(bool revealInExplorer = false);
        void CopyToClipboard(const std::wstring& text);

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_width = 680;
        int m_height = 56;
        int m_baseHeight = 56;
        int m_maxHeight = 440;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        std::wstring m_query;
        int m_selectedIndex = 0;
        std::vector<CatalogItem> m_results;

        // Cursor blink
        bool m_showCursor = true;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_queryFormat;
        ComPtr<IDWriteTextFormat> m_resultTitleFormat;
        ComPtr<IDWriteTextFormat> m_resultSubFormat;
        ComPtr<IDWriteTextFormat> m_badgeFormat;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushBg;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushSelection;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPlaceholder;
        ComPtr<ID2D1SolidColorBrush> m_brushSeparator;
        ComPtr<ID2D1SolidColorBrush> m_brushChip;
        ComPtr<ID2D1SolidColorBrush> m_brushBadgeBg;
    };

}
