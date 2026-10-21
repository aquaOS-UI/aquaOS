#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <vector>
#include <string>
#include <functional>

namespace Aqua {

    enum class DockAppType {
        Finder,
        Launchpad,
        Browser,
        Terminal,
        CodeEditor,
        Explorer,
        Calculator,
        ActivityMonitor,
        Settings,
        CustomApp,
        Separator,
        Trash
    };

    struct DockItem {
        DockAppType type;
        std::wstring name;
        std::wstring executePath;
        std::wstring executeArgs;
        std::wstring iconPath;
        bool isRunning = false;

        // Dynamic layout & physics
        float targetScale = 1.0f;
        float currentScale = 1.0f;
        float currentX = 0.0f;
        float currentY = 0.0f;
        float currentSize = 48.0f;
        D2D1_RECT_F bounds = {};

        // Click bounce animation
        float bounceTime = 0.0f;
        bool isBouncing = false;
    };

    class Dock {
    public:
        Dock();
        ~Dock();

        bool Create(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Show();
        void Hide();
        HWND GetHwnd() const { return m_hwnd; }

        void SetSpotlightCallback(std::function<void()> callback) { m_onSpotlight = callback; }

        void UpdatePhysics();
        void Render();

        void PinApplication(const std::wstring& filePath);
        void RemoveItem(int index);
        void LoadConfiguration();
        void SaveConfiguration();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void CalculateLayout();
        void DrawIcon(ID2D1DeviceContext* ctx, const DockItem& item, float x, float y, float size);
        void DrawTooltip(ID2D1DeviceContext* ctx, const std::wstring& text, float centerX, float topY);
        void ShowContextMenu(int itemIndex, int screenX, int screenY);

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_windowWidth = 0;
        int m_windowHeight = 110;
        int m_windowX = 0;
        int m_windowY = 0;
        float m_dpiScale = 1.0f;

        float m_baseIconSize = 48.0f;
        float m_maxScale = 1.45f;
        float m_magnificationRadius = 110.0f;
        float m_iconSpacing = 10.0f;

        bool m_isHovered = false;
        float m_mouseWindowX = -999.0f;
        float m_mouseWindowY = -999.0f;
        int m_hoveredIndex = -1;

        std::vector<DockItem> m_items;
        std::function<void()> m_onSpotlight;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_tooltipFormat;

        // Direct2D Resources
        ComPtr<ID2D1SolidColorBrush> m_brushGlassBg;
        ComPtr<ID2D1SolidColorBrush> m_brushGlassBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushTooltipBg;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushDot;
        ComPtr<ID2D1SolidColorBrush> m_brushSeparator;
    };

}

