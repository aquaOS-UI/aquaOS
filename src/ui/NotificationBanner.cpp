#include "NotificationBanner.h"
#include "IconManager.h"
#include "SystemInfo.h"
#include <windowsx.h>
#include <cmath>
#include <algorithm>

namespace Aqua {

    const wchar_t NOTIFICATION_BANNER_CLASS[] = L"AquaNotificationBannerWindow";
    constexpr UINT WM_PROCESS_QUEUE = WM_USER + 101;

    NotificationBanner& NotificationBanner::Instance() {
        static NotificationBanner s_instance;
        return s_instance;
    }

    NotificationBanner::NotificationBanner() = default;

    NotificationBanner::~NotificationBanner() {
        Shutdown();
    }

    bool NotificationBanner::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_hInstance = hInstance;
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = NotificationBanner::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = NOTIFICATION_BANNER_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        UINT dpi = GetDpiForWindow(GetDesktopWindow());
        m_dpiScale = static_cast<float>(dpi) / 96.0f;
        if (m_dpiScale <= 0.0f) m_dpiScale = 1.0f;

        m_width = 360.0f * m_dpiScale;
        m_height = 76.0f * m_dpiScale;
        m_y = 44.0f * m_dpiScale;
        m_targetX = static_cast<float>(m_screenWidth) - m_width - (16.0f * m_dpiScale);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE,
            NOTIFICATION_BANNER_CLASS,
            L"AquaOS Notification Banner",
            WS_POPUP,
            static_cast<int>(m_screenWidth + 20),
            static_cast<int>(m_y),
            static_cast<int>(m_width),
            static_cast<int>(m_height),
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_width = 360.0f * m_dpiScale;
        m_height = 76.0f * m_dpiScale;
        m_targetX = static_cast<float>(m_screenWidth) - m_width - (16.0f * m_dpiScale);

        if (!m_surface.Create(m_hwnd, static_cast<int>(m_width), static_cast<int>(m_height))) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // DirectWrite Text Formats
        // 1. Header (App name + timestamp)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
            L"en-US",
            &m_formatHeader
        );
        if (!m_formatHeader) {
            dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f * m_dpiScale, L"en-US", &m_formatHeader);
        }
        if (m_formatHeader) {
            m_formatHeader->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatHeader->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Title (Notification headline)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f * m_dpiScale,
            L"en-US",
            &m_formatTitle
        );
        if (!m_formatTitle) {
            dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f * m_dpiScale, L"en-US", &m_formatTitle);
        }
        if (m_formatTitle) {
            m_formatTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 3. Body text
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatBody
        );
        if (!m_formatBody) {
            dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f * m_dpiScale, L"en-US", &m_formatBody);
        }
        if (m_formatBody) {
            m_formatBody->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatBody->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Direct2D Brushes
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.90f), &m_brushBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f), &m_brushBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f), &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.68f, 0.68f, 0.72f, 1.0f), &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.48f, 1.00f, 1.0f), &m_brushAccent);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.40f, 0.45f, 0.85f), &m_brushCloseBtn);

        // Close button geometry: 16x16 at top-right
        float btnSize = 16.0f * m_dpiScale;
        float btnX = m_width - btnSize - (10.0f * m_dpiScale);
        float btnY = 10.0f * m_dpiScale;
        m_closeBtnRect = D2D1::RectF(btnX, btnY, btnX + btnSize, btnY + btnSize);

        return true;
    }

    void NotificationBanner::Shutdown() {
        if (m_hwnd) {
            KillTimer(m_hwnd, 2001);
            m_surface.Destroy();
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    void NotificationBanner::ShowBanner(const std::wstring& appName, const std::wstring& title, const std::wstring& message, const std::wstring& iconPath) {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_bannerQueue.push({ appName, title, message, iconPath });
        }

        if (m_hwnd) {
            PostMessageW(m_hwnd, WM_PROCESS_QUEUE, 0, 0);
        }
    }

    void NotificationBanner::ProcessQueue() {
        if (m_visible && !m_isDismissing) {
            // Already showing a banner; will show next one upon dismiss
            return;
        }

        BannerData item;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_bannerQueue.empty()) {
                return;
            }
            item = m_bannerQueue.front();
            m_bannerQueue.pop();
        }

        m_appName = item.appName.empty() ? L"AquaOS" : item.appName;
        m_title = item.title;
        m_message = item.message;

        // Load Icon
        auto& engine = DCompEngine::Instance();
        auto ctx = engine.GetD2DContext();
        if (!item.iconPath.empty()) {
            m_appIcon = IconManager::Instance().GetIconForPath(ctx, item.iconPath, 36);
        } else {
            m_appIcon = nullptr;
        }

        m_visible = true;
        m_isDismissing = false;
        m_animProgress = 0.0f;
        m_displayStartTime = GetTickCount();

        float offscreenX = static_cast<float>(m_screenWidth) + 20.0f;
        SetWindowPos(m_hwnd, HWND_TOPMOST, static_cast<int>(offscreenX), static_cast<int>(m_y), static_cast<int>(m_width), static_cast<int>(m_height), SWP_NOACTIVATE | SWP_SHOWWINDOW);

        SetTimer(m_hwnd, 2001, 16, nullptr);
        Render();
    }

    void NotificationBanner::Dismiss() {
        if (!m_visible || m_isDismissing) return;
        m_isDismissing = true;
    }

    void NotificationBanner::Render() {
        if (!m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        ctx->Clear(D2D1::ColorF(0, 0, 0, 0));

        // 1. Frosted Glass Banner Background
        D2D1_ROUNDED_RECT cardBox = D2D1::RoundedRect(
            D2D1::RectF(0.0f, 0.0f, m_width, m_height),
            16.0f * m_dpiScale, 16.0f * m_dpiScale
        );
        ctx->FillRoundedRectangle(cardBox, m_brushBg.Get());
        ctx->DrawRoundedRectangle(cardBox, m_brushBorder.Get(), 1.0f);

        // Specular top light edge
        ctx->DrawLine(
            D2D1::Point2F(16.0f * m_dpiScale, 1.0f),
            D2D1::Point2F(m_width - 16.0f * m_dpiScale, 1.0f),
            m_brushBorder.Get(), 1.0f
        );

        // 2. App Icon (36x36)
        float iconSize = 36.0f * m_dpiScale;
        float iconX = 14.0f * m_dpiScale;
        float iconY = (m_height - iconSize) * 0.5f;

        if (m_appIcon) {
            ctx->DrawBitmap(m_appIcon.Get(), D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize));
        } else {
            // macOS Style circular accent badge
            D2D1_ROUNDED_RECT iconBadge = D2D1::RoundedRect(
                D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize),
                8.0f * m_dpiScale, 8.0f * m_dpiScale
            );
            ctx->FillRoundedRectangle(iconBadge, m_brushAccent.Get());
            ctx->DrawRoundedRectangle(iconBadge, m_brushBorder.Get(), 1.0f);

            // Inner bell / dot
            ctx->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(iconX + iconSize * 0.5f, iconY + iconSize * 0.5f), 5.0f * m_dpiScale, 5.0f * m_dpiScale),
                m_brushTextPrimary.Get()
            );
        }

        // 3. Header row: App Name • now
        float textLeft = iconX + iconSize + (12.0f * m_dpiScale);
        float textRight = m_closeBtnRect.left - (8.0f * m_dpiScale);

        if (m_formatHeader && textRight > textLeft) {
            std::wstring headerStr = m_appName + L"  •  now";
            D2D1_RECT_F headerBox = D2D1::RectF(textLeft, 10.0f * m_dpiScale, textRight, 26.0f * m_dpiScale);
            ctx->DrawTextW(headerStr.c_str(), static_cast<UINT32>(headerStr.length()), m_formatHeader.Get(), headerBox, m_brushTextSecondary.Get());
        }

        // 4. Notification Title (bold)
        if (m_formatTitle && textRight > textLeft) {
            D2D1_RECT_F titleBox = D2D1::RectF(textLeft, 27.0f * m_dpiScale, textRight, 47.0f * m_dpiScale);
            ctx->DrawTextW(m_title.c_str(), static_cast<UINT32>(m_title.length()), m_formatTitle.Get(), titleBox, m_brushTextPrimary.Get());
        }

        // 5. Notification Message Body
        if (m_formatBody && textRight > textLeft) {
            D2D1_RECT_F bodyBox = D2D1::RectF(textLeft, 47.0f * m_dpiScale, textRight, 67.0f * m_dpiScale);
            ctx->DrawTextW(m_message.c_str(), static_cast<UINT32>(m_message.length()), m_formatBody.Get(), bodyBox, m_brushTextSecondary.Get());
        }

        // 6. Close 'X' Button on hover
        if (m_isHovered) {
            float btnR = (m_closeBtnRect.right - m_closeBtnRect.left) * 0.5f;
            D2D1_POINT_2F btnCenter = D2D1::Point2F(m_closeBtnRect.left + btnR, m_closeBtnRect.top + btnR);

            ctx->FillEllipse(D2D1::Ellipse(btnCenter, btnR, btnR), m_brushCloseBtn.Get());
            float xCross = btnR * 0.42f;
            ctx->DrawLine(
                D2D1::Point2F(btnCenter.x - xCross, btnCenter.y - xCross),
                D2D1::Point2F(btnCenter.x + xCross, btnCenter.y + xCross),
                m_brushTextPrimary.Get(), 1.4f * m_dpiScale
            );
            ctx->DrawLine(
                D2D1::Point2F(btnCenter.x + xCross, btnCenter.y - xCross),
                D2D1::Point2F(btnCenter.x - xCross, btnCenter.y + xCross),
                m_brushTextPrimary.Get(), 1.4f * m_dpiScale
            );
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK NotificationBanner::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        NotificationBanner* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<NotificationBanner*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            pThis->m_hwnd = hwnd;
        } else {
            pThis = reinterpret_cast<NotificationBanner*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT NotificationBanner::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_PROCESS_QUEUE: {
            ProcessQueue();
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 2001) {
                if (!m_isDismissing) {
                    m_animProgress = std::min(1.0f, m_animProgress + 0.08f);

                    // Auto dismiss after 5 seconds if not hovered
                    if (m_animProgress >= 1.0f && !m_isHovered) {
                        if (GetTickCount() - m_displayStartTime > 5000) {
                            Dismiss();
                        }
                    }
                } else {
                    m_animProgress = std::max(0.0f, m_animProgress - 0.10f);
                    if (m_animProgress <= 0.0f) {
                        KillTimer(m_hwnd, 2001);
                        m_visible = false;
                        ShowWindow(m_hwnd, SW_HIDE);
                        ProcessQueue();
                        return 0;
                    }
                }

                // Cubic ease-out sliding animation
                float t = m_animProgress;
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                float offscreenX = static_cast<float>(m_screenWidth) + 20.0f;
                m_currentX = offscreenX + (m_targetX - offscreenX) * ease;

                SetWindowPos(m_hwnd, HWND_TOPMOST, static_cast<int>(m_currentX), static_cast<int>(m_y), 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
                Render();
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            bool needRender = false;
            if (!m_isHovered) {
                m_isHovered = true;
                needRender = true;
            }

            bool closeHov = (mx >= m_closeBtnRect.left && mx <= m_closeBtnRect.right &&
                             my >= m_closeBtnRect.top && my <= m_closeBtnRect.bottom);
            if (m_isCloseHovered != closeHov) {
                m_isCloseHovered = closeHov;
                needRender = true;
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);

            if (needRender) {
                Render();
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_isHovered = false;
            m_isCloseHovered = false;
            m_displayStartTime = GetTickCount(); // reset 5s countdown on mouse leave
            Render();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            Dismiss();
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(m_hwnd, 2001);
            m_surface.Destroy();
            return 0;
        }
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}
