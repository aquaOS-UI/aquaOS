#include "LockScreen.h"
#include "SystemInfo.h"
#include <algorithm>
#include <cmath>

namespace Aqua {

    const wchar_t LOCK_SCREEN_CLASS[] = L"AquaLockScreenWindow";

    LockScreen& LockScreen::Instance() {
        static LockScreen s_instance;
        return s_instance;
    }

    LockScreen::LockScreen() = default;

    LockScreen::~LockScreen() {
        Shutdown();
    }

    bool LockScreen::Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = LockScreen::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = LOCK_SCREEN_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            LOCK_SCREEN_CLASS,
            L"AquaOS Lock Screen",
            WS_POPUP,
            0, 0, m_screenWidth, m_screenHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);

        if (!m_surface.Create(m_hwnd, m_screenWidth, m_screenHeight)) {
            return false;
        }

        // Query logged in user name
        wchar_t userBuf[256] = {};
        DWORD size = 256;
        if (GetUserNameW(userBuf, &size) && wcslen(userBuf) > 0) {
            m_userName = userBuf;
            if (m_userName[0] >= L'a' && m_userName[0] <= L'z') {
                m_userName[0] = m_userName[0] - L'a' + L'A';
            }
        } else {
            m_userName = L"macOS User";
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // 1. DirectWrite Formats
        // Clock Format (Large Bold numerals)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            76.0f * m_dpiScale,
            L"en-US",
            &m_formatClock
        );
        if (!m_formatClock) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                76.0f * m_dpiScale,
                L"en-US",
                &m_formatClock
            );
        }
        if (m_formatClock) {
            m_formatClock->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatClock->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Date Format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f * m_dpiScale,
            L"en-US",
            &m_formatDate
        );
        if (m_formatDate) {
            m_formatDate->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatDate->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // User Name Format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.0f * m_dpiScale,
            L"en-US",
            &m_formatUserName
        );
        if (m_formatUserName) {
            m_formatUserName->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatUserName->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Password Format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f * m_dpiScale,
            L"en-US",
            &m_formatPassword
        );
        if (m_formatPassword) {
            m_formatPassword->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatPassword->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Action Format (Bottom buttons)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.5f * m_dpiScale,
            L"en-US",
            &m_formatAction
        );
        if (m_formatAction) {
            m_formatAction->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatAction->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Direct2D Brushes
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.08f, 0.12f, 1.0f), &m_brushBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_brushWhite);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.70f), &m_brushWhiteTrans);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f), &m_brushInputBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.35f), &m_brushInputBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 1.0f), &m_brushAccent);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.22f), &m_brushAvatarPlate);

        return true;
    }

    void LockScreen::Shutdown() {
        if (m_hwnd) {
            KillTimer(m_hwnd, TIMER_ANIM);
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    void LockScreen::Show() {
        m_passwordInput.clear();
        m_targetVisible = true;
        m_isFading = true;
        m_fadeProgress = 0.0f;
        m_visible = true;

        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        SetFocus(m_hwnd);

        SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
    }

    void LockScreen::Hide() {
        m_targetVisible = false;
        m_isFading = true;
        SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
    }

    void LockScreen::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    void LockScreen::TriggerShakeAnimation() {
        m_isShaking = true;
        m_shakeProgress = 0.0f;
    }

    void LockScreen::Unlock() {
        Hide();
    }

    void LockScreen::UpdateAnimation() {
        m_animTime += 0.016f;

        // Fade In / Out
        if (m_isFading) {
            if (m_targetVisible) {
                m_fadeProgress += 0.10f;
                if (m_fadeProgress >= 1.0f) {
                    m_fadeProgress = 1.0f;
                    m_isFading = false;
                }
            } else {
                m_fadeProgress -= 0.10f;
                if (m_fadeProgress <= 0.0f) {
                    m_fadeProgress = 0.0f;
                    m_isFading = false;
                    m_visible = false;
                    ShowWindow(m_hwnd, SW_HIDE);
                    KillTimer(m_hwnd, TIMER_ANIM);
                    return;
                }
            }
        }

        // Error Shake horizontal spring
        if (m_isShaking) {
            m_shakeProgress += 0.045f;
            if (m_shakeProgress >= 1.0f) {
                m_shakeProgress = 1.0f;
                m_isShaking = false;
                m_shakeOffset = 0.0f;
                m_passwordInput.clear();
            } else {
                m_shakeOffset = std::sin(m_shakeProgress * 6.0f * 3.14159f) * (1.0f - m_shakeProgress) * (14.0f * m_dpiScale);
            }
        }
    }

    void LockScreen::Render() {
        if (!m_hwnd || !m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        float w = static_cast<float>(m_screenWidth);
        float h = static_cast<float>(m_screenHeight);
        float cx = w * 0.5f;

        // 1. Dynamic Atmospheric Background (Sonoma Aerial Night Mesh)
        ctx->Clear(D2D1::ColorF(0.05f, 0.07f, 0.11f, m_fadeProgress));

        // Atmospheric glowing gradient nebulae
        float wave1 = std::sin(m_animTime * 0.4f) * 40.0f;
        float wave2 = std::cos(m_animTime * 0.3f) * 30.0f;

        D2D1_ELLIPSE glow1 = D2D1::Ellipse(D2D1::Point2F(cx + wave1, h * 0.40f + wave2), w * 0.45f, h * 0.35f);
        ComPtr<ID2D1SolidColorBrush> nebulaBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.24f, 0.10f, 0.40f, 0.30f * m_fadeProgress), &nebulaBrush);
        ctx->FillEllipse(glow1, nebulaBrush.Get());

        D2D1_ELLIPSE glow2 = D2D1::Ellipse(D2D1::Point2F(cx - wave2, h * 0.65f), w * 0.50f, h * 0.28f);
        ComPtr<ID2D1SolidColorBrush> horizonBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.22f, 0.45f, 0.25f * m_fadeProgress), &horizonBrush);
        ctx->FillEllipse(glow2, horizonBrush.Get());

        // 2. Giant Centered Clock & Date
        SYSTEMTIME st = {};
        GetLocalTime(&st);

        // Date string: e.g. "Wednesday, October 21"
        wchar_t dateBuf[128] = {};
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, dateBuf, 128);

        // Time string: e.g. "17:30"
        wchar_t timeBuf[32] = {};
        swprintf_s(timeBuf, L"%02d:%02d", st.wHour, st.wMinute);

        float clockY = h * 0.16f;
        D2D1_RECT_F dateRect = D2D1::RectF(0, clockY, w, clockY + 28.0f * m_dpiScale);
        if (m_formatDate) {
            ctx->DrawTextW(dateBuf, static_cast<UINT32>(wcslen(dateBuf)), m_formatDate.Get(), dateRect, m_brushWhiteTrans.Get());
        }

        D2D1_RECT_F clockRect = D2D1::RectF(0, clockY + 30.0f * m_dpiScale, w, clockY + 115.0f * m_dpiScale);
        if (m_formatClock) {
            ctx->DrawTextW(timeBuf, static_cast<UINT32>(wcslen(timeBuf)), m_formatClock.Get(), clockRect, m_brushWhite.Get());
        }

        // 3. User Avatar & Profile Card
        float userCenterY = h * 0.58f;
        float avatarR = 36.0f * m_dpiScale;
        float avatarX = cx + m_shakeOffset;
        float avatarY = userCenterY - 45.0f * m_dpiScale;

        // Circular Avatar Plate
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(avatarX, avatarY), avatarR, avatarR), m_brushAvatarPlate.Get());
        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(avatarX, avatarY), avatarR, avatarR), m_brushInputBorder.Get(), 1.0f);

        // Initial Letter inside Avatar
        wchar_t initial[2] = { m_userName.empty() ? L'A' : m_userName[0], 0 };
        D2D1_RECT_F initialRect = D2D1::RectF(avatarX - avatarR, avatarY - avatarR, avatarX + avatarR, avatarY + avatarR);
        if (m_formatDate) {
            ctx->DrawTextW(initial, 1, m_formatDate.Get(), initialRect, m_brushWhite.Get());
        }

        // User Name Label
        D2D1_RECT_F userNameRect = D2D1::RectF(cx - 200.0f + m_shakeOffset, userCenterY, cx + 200.0f + m_shakeOffset, userCenterY + 24.0f * m_dpiScale);
        if (m_formatUserName) {
            ctx->DrawTextW(m_userName.c_str(), static_cast<UINT32>(m_userName.size()), m_formatUserName.Get(), userNameRect, m_brushWhite.Get());
        }

        // 4. Frosted Password / PIN Input Box
        float boxW = 210.0f * m_dpiScale;
        float boxH = 34.0f * m_dpiScale;
        float boxX = cx - boxW * 0.5f + m_shakeOffset;
        float boxY = userCenterY + 30.0f * m_dpiScale;
        m_passwordBoxRect = D2D1::RectF(boxX, boxY, boxX + boxW, boxY + boxH);

        D2D1_ROUNDED_RECT inputPill = D2D1::RoundedRect(m_passwordBoxRect, boxH * 0.5f, boxH * 0.5f);
        ctx->FillRoundedRectangle(inputPill, m_brushInputBg.Get());
        ctx->DrawRoundedRectangle(inputPill, m_brushInputBorder.Get(), 1.0f);

        // Submit Arrow Button at right end
        float submitR = 12.0f * m_dpiScale;
        float submitCx = m_passwordBoxRect.right - 18.0f * m_dpiScale;
        float submitCy = m_passwordBoxRect.top + boxH * 0.5f;
        m_submitButtonRect = D2D1::RectF(submitCx - submitR, submitCy - submitR, submitCx + submitR, submitCy + submitR);

        if (!m_passwordInput.empty()) {
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(submitCx, submitCy), submitR, submitR), m_brushWhite.Get());
            D2D1_RECT_F arrowRect = D2D1::RectF(submitCx - submitR, submitCy - submitR, submitCx + submitR, submitCy + submitR);
            ComPtr<ID2D1SolidColorBrush> blackBrush;
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f, 1.0f), &blackBrush);
            if (m_formatAction) {
                ctx->DrawTextW(L"➔", 1, m_formatAction.Get(), arrowRect, blackBrush.Get());
            }
        }

        // Masked Password Dots or Placeholder
        D2D1_RECT_F textInner = D2D1::RectF(boxX + 16.0f * m_dpiScale, boxY, boxX + boxW - 32.0f * m_dpiScale, boxY + boxH);
        if (m_passwordInput.empty()) {
            if (m_formatPassword) {
                ctx->DrawTextW(L"Enter Password", 14, m_formatPassword.Get(), textInner, m_brushWhiteTrans.Get());
            }
        } else {
            std::wstring dots(m_passwordInput.size(), L'●');
            if (m_formatPassword) {
                ctx->DrawTextW(dots.c_str(), static_cast<UINT32>(dots.size()), m_formatPassword.Get(), textInner, m_brushWhite.Get());
            }
        }

        // Caps Lock Indicator if active
        if (GetKeyState(VK_CAPITAL) & 0x0001) {
            float capsY = boxY + boxH + 8.0f * m_dpiScale;
            D2D1_RECT_F capsRect = D2D1::RectF(0, capsY, w, capsY + 18.0f * m_dpiScale);
            if (m_formatAction) {
                ctx->DrawTextW(L"⇪ Caps Lock is On", 17, m_formatAction.Get(), capsRect, m_brushWhiteTrans.Get());
            }
        }

        // 5. Bottom System Action Buttons (Sleep, Restart, Shut Down, Switch User)
        float botY = h - 68.0f * m_dpiScale;
        float btnSpacing = 90.0f * m_dpiScale;
        float btnStartX = cx - (btnSpacing * 1.5f);

        auto drawActionBtn = [&](int idx, float x, const wchar_t* glyph, const wchar_t* label, D2D1_RECT_F& outRect) {
            float r = 18.0f * m_dpiScale;
            float cyBtn = botY;
            outRect = D2D1::RectF(x - r, cyBtn - r, x + r, cyBtn + r);

            bool hovered = (m_hoveredAction == idx);
            ComPtr<ID2D1SolidColorBrush> btnBg;
            ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, hovered ? 0.35f : 0.16f), &btnBg);

            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, cyBtn), r, r), btnBg.Get());
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, cyBtn), r, r), m_brushInputBorder.Get(), 1.0f);

            D2D1_RECT_F glyphRect = D2D1::RectF(x - r, cyBtn - r, x + r, cyBtn + r);
            if (m_formatAction) {
                ctx->DrawTextW(glyph, static_cast<UINT32>(wcslen(glyph)), m_formatAction.Get(), glyphRect, m_brushWhite.Get());
            }

            D2D1_RECT_F labelRect = D2D1::RectF(x - 45.0f * m_dpiScale, cyBtn + r + 4.0f * m_dpiScale, x + 45.0f * m_dpiScale, cyBtn + r + 20.0f * m_dpiScale);
            if (m_formatAction) {
                ctx->DrawTextW(label, static_cast<UINT32>(wcslen(label)), m_formatAction.Get(), labelRect, m_brushWhiteTrans.Get());
            }
        };

        drawActionBtn(0, btnStartX + btnSpacing * 0.0f, L"☾", L"Sleep", m_sleepButtonRect);
        drawActionBtn(1, btnStartX + btnSpacing * 1.0f, L"↻", L"Restart", m_restartButtonRect);
        drawActionBtn(2, btnStartX + btnSpacing * 2.0f, L"⏻", L"Shut Down", m_shutdownButtonRect);
        drawActionBtn(3, btnStartX + btnSpacing * 3.0f, L"👤", L"Switch User", m_switchUserButtonRect);

        m_surface.EndDraw();
    }

    LRESULT CALLBACK LockScreen::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        LockScreen* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<LockScreen*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<LockScreen*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT LockScreen::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_ANIM) {
                UpdateAnimation();
                Render();
                return 0;
            }
            break;

        case WM_CHAR: {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == VK_BACK) {
                if (!m_passwordInput.empty()) {
                    m_passwordInput.pop_back();
                    Render();
                }
                return 0;
            }
            if (ch == VK_RETURN) {
                Unlock();
                return 0;
            }
            if (ch == VK_ESCAPE) {
                Unlock();
                return 0;
            }
            if (ch >= 32) {
                m_passwordInput.push_back(ch);
                Render();
                return 0;
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Unlock();
                return 0;
            }
            if (wParam == VK_CAPITAL) {
                Render();
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int newHover = -1;

            auto inRect = [&](const D2D1_RECT_F& r) {
                return (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom);
            };

            if (inRect(m_sleepButtonRect)) newHover = 0;
            else if (inRect(m_restartButtonRect)) newHover = 1;
            else if (inRect(m_shutdownButtonRect)) newHover = 2;
            else if (inRect(m_switchUserButtonRect)) newHover = 3;
            else if (inRect(m_submitButtonRect)) newHover = 4;

            if (newHover != m_hoveredAction) {
                m_hoveredAction = newHover;
                Render();
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (m_hoveredAction != -1) {
                m_hoveredAction = -1;
                Render();
            }
            return 0;

        case WM_LBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            auto inRect = [&](const D2D1_RECT_F& r) {
                return (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom);
            };

            if (inRect(m_submitButtonRect)) {
                Unlock();
                return 0;
            }
            if (inRect(m_sleepButtonRect)) {
                SetSuspendState(FALSE, FALSE, FALSE);
                return 0;
            }
            if (inRect(m_restartButtonRect)) {
                ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE);
                return 0;
            }
            if (inRect(m_shutdownButtonRect)) {
                ExitWindowsEx(EWX_SHUTDOWN, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE);
                return 0;
            }
            if (inRect(m_switchUserButtonRect)) {
                LockWorkStation();
                return 0;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ANIM);
            return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

}

