#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <string>
#include <vector>
#include <functional>

namespace Aqua {

    class LockScreen {
    public:
        static LockScreen& Instance();

        bool Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Shutdown();

        void Show();
        void Hide();
        void Toggle();
        bool IsVisible() const { return m_visible; }
        HWND GetHwnd() const { return m_hwnd; }

        void TriggerShakeAnimation();
        void Unlock();

    private:
        LockScreen();
        ~LockScreen();

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void UpdateAnimation();
        void Render();

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_vx = 0;
        int m_vy = 0;
        int m_screenWidth = 0;
        int m_screenHeight = 0;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        // User info
        std::wstring m_userName;
        std::wstring m_passwordInput;

        // Animation State
        float m_animTime = 0.0f;        // Background particle/wave phase
        float m_fadeProgress = 0.0f;    // Fade-in / Fade-out (0.0 to 1.0)
        bool m_isFading = false;
        bool m_targetVisible = false;

        float m_shakeProgress = 1.0f;   // 0.0 to 1.0 during error shake
        bool m_isShaking = false;
        float m_shakeOffset = 0.0f;

        // Interactive Rectangles
        D2D1_RECT_F m_passwordBoxRect = {};
        D2D1_RECT_F m_submitButtonRect = {};
        D2D1_RECT_F m_sleepButtonRect = {};
        D2D1_RECT_F m_restartButtonRect = {};
        D2D1_RECT_F m_shutdownButtonRect = {};
        D2D1_RECT_F m_switchUserButtonRect = {};

        int m_hoveredAction = -1; // 0=Sleep, 1=Restart, 2=ShutDown, 3=SwitchUser, 4=Submit

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatClock;
        ComPtr<IDWriteTextFormat> m_formatDate;
        ComPtr<IDWriteTextFormat> m_formatUserName;
        ComPtr<IDWriteTextFormat> m_formatPassword;
        ComPtr<IDWriteTextFormat> m_formatAction;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushBg;
        ComPtr<ID2D1SolidColorBrush> m_brushWhite;
        ComPtr<ID2D1SolidColorBrush> m_brushWhiteTrans;
        ComPtr<ID2D1SolidColorBrush> m_brushInputBg;
        ComPtr<ID2D1SolidColorBrush> m_brushInputBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushAccent;
        ComPtr<ID2D1SolidColorBrush> m_brushAvatarPlate;
        ComPtr<ID2D1SolidColorBrush> m_brushNebula;
        ComPtr<ID2D1SolidColorBrush> m_brushHorizon;

        static constexpr UINT_PTR TIMER_ANIM = 401;
    };

}

