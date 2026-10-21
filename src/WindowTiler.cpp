#include "WindowTiler.h"
#include <algorithm>
#include <cmath>

namespace Aqua {

    const wchar_t PREVIEW_CLASS[] = L"AquaSnapPreviewWindow";

    WindowTiler& WindowTiler::Instance() {
        static WindowTiler s_instance;
        return s_instance;
    }

    WindowTiler::WindowTiler() = default;

    WindowTiler::~WindowTiler() {
        Shutdown();
    }

    bool WindowTiler::Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight, int topBarHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_topBarHeight = topBarHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WindowTiler::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = PREVIEW_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        // WS_EX_TRANSPARENT allows all mouse events to pass straight through
        m_previewHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_NOREDIRECTIONBITMAP,
            PREVIEW_CLASS,
            L"AquaOS Snap Preview",
            WS_POPUP,
            0, 0, m_screenWidth, m_screenHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_previewHwnd) return false;

        m_dpiScale = GetDpiScale(m_previewHwnd);

        if (!m_surface.Create(m_previewHwnd, m_screenWidth, m_screenHeight)) {
            return false;
        }

        auto ctx = DCompEngine::Instance().GetD2DContext();
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 0.22f), &m_brushPreviewFill);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 0.85f), &m_brushPreviewBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.35f), &m_brushGlow);

        // Install WinEventHook for window move/size events
        m_hWinEventHook = SetWinEventHook(
            EVENT_SYSTEM_MOVESIZESTART,
            EVENT_SYSTEM_MOVESIZEEND,
            nullptr,
            WindowTiler::WinEventProc,
            0, 0,
            WINEVENT_OUTOFPROCESS | WINEVENT_SKIPOWNPROCESS
        );

        return true;
    }

    void WindowTiler::Shutdown() {
        if (m_hWinEventHook) {
            UnhookWinEvent(m_hWinEventHook);
            m_hWinEventHook = nullptr;
        }
        if (m_previewHwnd) {
            DestroyWindow(m_previewHwnd);
            m_previewHwnd = nullptr;
        }
    }

    RECT WindowTiler::GetZoneRect(SnapZone zone) const {
        int margin = static_cast<int>(6.0f * m_dpiScale);
        int topY = m_topBarHeight + margin;
        int bottomMargin = static_cast<int>(10.0f * m_dpiScale);
        int availW = m_screenWidth - (margin * 2);
        int availH = m_screenHeight - topY - bottomMargin;

        int halfW = (availW - margin) / 2;
        int halfH = (availH - margin) / 2;

        RECT r = {};

        switch (zone) {
        case SnapZone::LeftHalf:
            r = { margin, topY, margin + halfW, topY + availH };
            break;
        case SnapZone::RightHalf:
            r = { margin + halfW + margin, topY, margin + halfW + margin + halfW, topY + availH };
            break;
        case SnapZone::TopHalf:
            r = { margin, topY, margin + availW, topY + halfH };
            break;
        case SnapZone::BottomHalf:
            r = { margin, topY + halfH + margin, margin + availW, topY + halfH + margin + halfH };
            break;
        case SnapZone::TopLeftQuarter:
            r = { margin, topY, margin + halfW, topY + halfH };
            break;
        case SnapZone::TopRightQuarter:
            r = { margin + halfW + margin, topY, margin + halfW + margin + halfW, topY + halfH };
            break;
        case SnapZone::BottomLeftQuarter:
            r = { margin, topY + halfH + margin, margin + halfW, topY + halfH + margin + halfH };
            break;
        case SnapZone::BottomRightQuarter:
            r = { margin + halfW + margin, topY + halfH + margin, margin + halfW + margin + halfW, topY + halfH + margin + halfH };
            break;
        case SnapZone::Maximize:
            r = { margin, topY, margin + availW, topY + availH };
            break;
        case SnapZone::Center: {
            int w = static_cast<int>(availW * 0.72f);
            int h = static_cast<int>(availH * 0.76f);
            int x = margin + (availW - w) / 2;
            int y = topY + (availH - h) / 2;
            r = { x, y, x + w, y + h };
            break;
        }
        default:
            break;
        }

        return r;
    }

    void WindowTiler::SnapForegroundWindow(SnapZone zone) {
        HWND fg = GetForegroundWindow();
        if (!fg || !IsWindow(fg)) return;

        // Verify window is not desktop, taskbar, or AquaShell
        wchar_t className[256] = {};
        GetClassNameW(fg, className, 256);
        std::wstring cls(className);

        if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" ||
            cls.find(L"Aqua") != std::wstring::npos) {
            return;
        }

        SnapWindow(fg, zone);
    }

    void WindowTiler::SnapWindow(HWND hwnd, SnapZone zone) {
        if (!hwnd || !IsWindow(hwnd) || zone == SnapZone::None) return;

        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }

        if (IsZoomed(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }

        RECT r = GetZoneRect(zone);
        if (r.right <= r.left || r.bottom <= r.top) return;

        SetWindowPos(
            hwnd,
            HWND_TOP,
            r.left, r.top,
            r.right - r.left, r.bottom - r.top,
            SWP_SHOWWINDOW | SWP_FRAMECHANGED
        );
        SetForegroundWindow(hwnd);
    }

    void CALLBACK WindowTiler::WinEventProc(HWINEVENTHOOK hHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
        if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
        if (!hwnd || !IsWindow(hwnd)) return;

        wchar_t className[256] = {};
        GetClassNameW(hwnd, className, 256);
        std::wstring cls(className);

        // Filter out AquaShell windows and Windows shell surfaces
        if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" ||
            cls == L"Shell_SecondaryTrayWnd" || cls.find(L"Aqua") != std::wstring::npos) {
            return;
        }

        if (event == EVENT_SYSTEM_MOVESIZESTART) {
            Instance().StartTracking(hwnd);
        } else if (event == EVENT_SYSTEM_MOVESIZEEND) {
            Instance().StopTracking();
        }
    }

    void WindowTiler::StartTracking(HWND draggedHwnd) {
        m_draggedHwnd = draggedHwnd;
        m_isDragging = true;
        m_currentZone = SnapZone::None;
        m_targetZone = SnapZone::None;

        SetTimer(m_previewHwnd, TIMER_DRAG, 16, nullptr);
    }

    void WindowTiler::StopTracking() {
        KillTimer(m_previewHwnd, TIMER_DRAG);
        m_isDragging = false;

        SnapZone finalZone = m_targetZone;
        HWND finalHwnd = m_draggedHwnd;

        HidePreview();

        if (finalZone != SnapZone::None && finalHwnd && IsWindow(finalHwnd)) {
            SnapWindow(finalHwnd, finalZone);
        }

        m_draggedHwnd = nullptr;
        m_targetZone = SnapZone::None;
        m_currentZone = SnapZone::None;
    }

    void WindowTiler::UpdateDrag(POINT pt) {
        float edgeThreshold = 18.0f * m_dpiScale;
        float cornerThreshold = 45.0f * m_dpiScale;

        SnapZone zone = SnapZone::None;

        float topEdge = static_cast<float>(m_topBarHeight) + edgeThreshold;
        float botEdge = static_cast<float>(m_screenHeight) - edgeThreshold;
        float leftEdge = edgeThreshold;
        float rightEdge = static_cast<float>(m_screenWidth) - edgeThreshold;

        // Top Edge & Corners
        if (pt.y <= topEdge) {
            if (pt.x <= cornerThreshold) {
                zone = SnapZone::TopLeftQuarter;
            } else if (pt.x >= m_screenWidth - cornerThreshold) {
                zone = SnapZone::TopRightQuarter;
            } else {
                zone = SnapZone::Maximize;
            }
        }
        // Bottom Edge & Corners
        else if (pt.y >= botEdge) {
            if (pt.x <= cornerThreshold) {
                zone = SnapZone::BottomLeftQuarter;
            } else if (pt.x >= m_screenWidth - cornerThreshold) {
                zone = SnapZone::BottomRightQuarter;
            } else {
                zone = SnapZone::BottomHalf;
            }
        }
        // Left Edge & Corners
        else if (pt.x <= leftEdge) {
            if (pt.y <= topEdge + cornerThreshold) {
                zone = SnapZone::TopLeftQuarter;
            } else if (pt.y >= botEdge - cornerThreshold) {
                zone = SnapZone::BottomLeftQuarter;
            } else {
                zone = SnapZone::LeftHalf;
            }
        }
        // Right Edge & Corners
        else if (pt.x >= rightEdge) {
            if (pt.y <= topEdge + cornerThreshold) {
                zone = SnapZone::TopRightQuarter;
            } else if (pt.y >= botEdge - cornerThreshold) {
                zone = SnapZone::BottomRightQuarter;
            } else {
                zone = SnapZone::RightHalf;
            }
        }

        if (zone != m_targetZone) {
            m_targetZone = zone;
            if (zone == SnapZone::None) {
                HidePreview();
            } else {
                ShowPreview(zone);
            }
        }
    }

    void WindowTiler::ShowPreview(SnapZone zone) {
        RECT r = GetZoneRect(zone);
        m_destRect = D2D1::RectF(
            static_cast<float>(r.left),
            static_cast<float>(r.top),
            static_cast<float>(r.right),
            static_cast<float>(r.bottom)
        );

        if (!m_previewVisible) {
            // Start from slightly inset rect for spring scale-in effect
            float inset = 20.0f * m_dpiScale;
            m_animRect = D2D1::RectF(
                m_destRect.left + inset,
                m_destRect.top + inset,
                m_destRect.right - inset,
                m_destRect.bottom - inset
            );
            m_previewVisible = true;
            ShowWindow(m_previewHwnd, SW_SHOWNOACTIVATE);
            SetTimer(m_previewHwnd, TIMER_ANIM, 16, nullptr);
        }

        RenderPreview();
    }

    void WindowTiler::HidePreview() {
        if (m_previewVisible) {
            m_previewVisible = false;
            KillTimer(m_previewHwnd, TIMER_ANIM);
            ShowWindow(m_previewHwnd, SW_HIDE);
        }
    }

    void WindowTiler::RenderPreview() {
        if (!m_previewHwnd || !m_previewVisible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        float radius = 12.0f * m_dpiScale;
        D2D1_ROUNDED_RECT round = D2D1::RoundedRect(m_animRect, radius, radius);

        // Translucent macOS Accent Blue Fill
        ctx->FillRoundedRectangle(round, m_brushPreviewFill.Get());

        // Outer Glow & Border
        ctx->DrawRoundedRectangle(round, m_brushPreviewBorder.Get(), 2.0f * m_dpiScale);

        m_surface.EndDraw();
    }

    LRESULT CALLBACK WindowTiler::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowTiler* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<WindowTiler*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<WindowTiler*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT WindowTiler::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_DRAG) {
                if (m_isDragging) {
                    POINT pt = {};
                    GetCursorPos(&pt);
                    UpdateDrag(pt);
                }
                return 0;
            }
            if (wParam == TIMER_ANIM) {
                if (m_previewVisible) {
                    // Smooth cubic interpolation towards destRect
                    float f = 0.28f;
                    m_animRect.left += (m_destRect.left - m_animRect.left) * f;
                    m_animRect.top += (m_destRect.top - m_animRect.top) * f;
                    m_animRect.right += (m_destRect.right - m_animRect.right) * f;
                    m_animRect.bottom += (m_destRect.bottom - m_animRect.bottom) * f;

                    RenderPreview();

                    if (std::abs(m_destRect.left - m_animRect.left) < 1.0f &&
                        std::abs(m_destRect.top - m_animRect.top) < 1.0f) {
                        m_animRect = m_destRect;
                        KillTimer(m_previewHwnd, TIMER_ANIM);
                    }
                }
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            RenderPreview();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_DRAG);
            KillTimer(hwnd, TIMER_ANIM);
            return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

}

