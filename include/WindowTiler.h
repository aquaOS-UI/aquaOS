#pragma once
#include "Common.h"
#include "DCompRenderer.h"

namespace Aqua {

    enum class SnapZone {
        None,
        LeftHalf,
        RightHalf,
        TopHalf,
        BottomHalf,
        TopLeftQuarter,
        TopRightQuarter,
        BottomLeftQuarter,
        BottomRightQuarter,
        Maximize,
        Center
    };

    class WindowTiler {
    public:
        static WindowTiler& Instance();

        bool Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight, int topBarHeight = 32);
        void Shutdown();

        // Snap active foreground window to a zone
        void SnapForegroundWindow(SnapZone zone);
        void SnapWindow(HWND hwnd, SnapZone zone);

        // Compute target bounding box for a snap zone
        RECT GetZoneRect(SnapZone zone, HMONITOR hMon = nullptr) const;

    private:
        WindowTiler();
        ~WindowTiler();

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        static void CALLBACK WinEventProc(HWINEVENTHOOK hHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);

        void StartTracking(HWND draggedHwnd);
        void StopTracking();
        void UpdateDrag(POINT pt);
        void ShowPreview(SnapZone zone);
        void HidePreview();
        void RenderPreview();

        HWND m_previewHwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_topBarHeight = 32;
        float m_dpiScale = 1.0f;

        HWINEVENTHOOK m_hWinEventHook = nullptr;
        HWND m_draggedHwnd = nullptr;
        bool m_isDragging = false;
        SnapZone m_currentZone = SnapZone::None;
        SnapZone m_targetZone = SnapZone::None;

        D2D1_RECT_F m_animRect = {};
        D2D1_RECT_F m_destRect = {};
        bool m_previewVisible = false;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushPreviewFill;
        ComPtr<ID2D1SolidColorBrush> m_brushPreviewBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushGlow;

        static constexpr UINT_PTR TIMER_DRAG = 201;
        static constexpr UINT_PTR TIMER_ANIM = 202;
    };

}

