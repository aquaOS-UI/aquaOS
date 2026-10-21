#include "ControlCenter.h"
#include "SystemInfo.h"
#include <algorithm>

namespace Aqua {

    const wchar_t CC_CLASS_NAME[] = L"AquaControlCenterWindow";

    ControlCenter::ControlCenter() {
    }

    ControlCenter::~ControlCenter() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool ControlCenter::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        m_width = 320;
        m_height = 340;

        int posX = m_screenWidth - m_width - 12;
        int posY = 38;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = ControlCenter::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CC_CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            CC_CLASS_NAME,
            L"AquaOS Control Center",
            WS_POPUP,
            posX, posY, m_width, m_height,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_width = static_cast<int>(320 * m_dpiScale);
        m_height = static_cast<int>(340 * m_dpiScale);

        if (!m_surface.Create(m_hwnd, m_width, m_height)) {
            return false;
        }

        // Query current system volume
        m_soundVolume = SystemInfo::GetMasterVolume();

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // DirectWrite Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.5f * m_dpiScale,
            L"en-US",
            &m_formatTitle
        );
        if (!m_formatTitle) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.5f * m_dpiScale,
                L"en-US",
                &m_formatTitle
            );
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.5f * m_dpiScale,
            L"en-US",
            &m_formatSubtitle
        );
        if (!m_formatSubtitle) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                10.5f * m_dpiScale,
                L"en-US",
                &m_formatSubtitle
            );
        }

        if (m_formatTitle) {
            m_formatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_formatSubtitle) {
            m_formatSubtitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::GlassBackgroundDark, &m_brushBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &m_brushCardBg);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushActive);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f), &m_brushInactive);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_brushSliderFill);

        // Precompute Layout
        float pad = 12.0f * m_dpiScale;
        float gap = 8.0f * m_dpiScale;
        float wHalf = (m_width - (pad * 2.0f) - gap) * 0.5f;
        float hCard = 64.0f * m_dpiScale;

        // Top Row: Wi-Fi & Bluetooth
        m_wifiRect = D2D1::RectF(pad, pad, pad + wHalf, pad + hCard);
        m_btRect = D2D1::RectF(pad + wHalf + gap, pad, m_width - pad, pad + hCard);

        // Middle Sliders
        float sliderY1 = pad + hCard + gap + 4.0f;
        float sliderH = 46.0f * m_dpiScale;
        m_sliderBrightnessRect = D2D1::RectF(pad, sliderY1, m_width - pad, sliderY1 + sliderH);

        float sliderY2 = sliderY1 + sliderH + gap;
        m_sliderVolumeRect = D2D1::RectF(pad, sliderY2, m_width - pad, sliderY2 + sliderH);

        // Bottom Row: Dark Mode & Focus
        float botY = sliderY2 + sliderH + gap + 4.0f;
        m_darkRect = D2D1::RectF(pad, botY, pad + wHalf, botY + hCard);
        m_focusRect = D2D1::RectF(pad + wHalf + gap, botY, m_width - pad, botY + hCard);

        return true;
    }

    void ControlCenter::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    void ControlCenter::Show() {
        m_visible = true;
        m_soundVolume = SystemInfo::GetMasterVolume();

        int posX = m_screenWidth - m_width - 12;
        int posY = 38;
        SetWindowPos(m_hwnd, HWND_TOPMOST, posX, posY, m_width, m_height, SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        Render();
    }

    void ControlCenter::Hide() {
        m_visible = false;
        ShowWindow(m_hwnd, SW_HIDE);
    }

    void ControlCenter::Render() {
        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        float w = static_cast<float>(m_width);
        float h = static_cast<float>(m_height);
        float r = 18.0f * m_dpiScale;

        // 1. Frosted Glass Container
        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), r, r);
        ctx->FillRoundedRectangle(box, m_brushBg.Get());
        ctx->DrawRoundedRectangle(box, m_brushBorder.Get(), 1.2f);

        // Helper Lambda to Draw Quick Toggle Card
        auto drawToggleCard = [&](const D2D1_RECT_F& rect, bool active, const std::wstring& title, const std::wstring& sub, bool isWifi) {
            D2D1_ROUNDED_RECT card = D2D1::RoundedRect(rect, 12.0f * m_dpiScale, 12.0f * m_dpiScale);
            ctx->FillRoundedRectangle(card, m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(card, m_brushBorder.Get(), 1.0f);

            // Icon Circle
            float iconSize = 28.0f * m_dpiScale;
            float iconX = rect.left + 10.0f * m_dpiScale;
            float iconY = rect.top + (rect.bottom - rect.top - iconSize) * 0.5f;

            D2D1_ELLIPSE iconCirc = D2D1::Ellipse(
                D2D1::Point2F(iconX + iconSize * 0.5f, iconY + iconSize * 0.5f),
                iconSize * 0.5f, iconSize * 0.5f
            );
            ctx->FillEllipse(iconCirc, active ? m_brushActive.Get() : m_brushInactive.Get());

            // Label Text
            D2D1_RECT_F titleRect = D2D1::RectF(iconX + iconSize + 8.0f, rect.top + 12.0f, rect.right - 6.0f, rect.top + 30.0f);
            ctx->DrawTextW(title.c_str(), static_cast<UINT32>(title.length()), m_formatTitle.Get(), titleRect, m_brushTextPrimary.Get());

            D2D1_RECT_F subRect = D2D1::RectF(iconX + iconSize + 8.0f, rect.top + 30.0f, rect.right - 6.0f, rect.bottom - 8.0f);
            ctx->DrawTextW(sub.c_str(), static_cast<UINT32>(sub.length()), m_formatSubtitle.Get(), subRect, m_brushTextSecondary.Get());
        };

        // Draw Wi-Fi & Bluetooth
        drawToggleCard(m_wifiRect, m_wifiEnabled, L"Wi-Fi", m_wifiEnabled ? L"Connected" : L"Off", true);
        drawToggleCard(m_btRect, m_bluetoothEnabled, L"Bluetooth", m_bluetoothEnabled ? L"On" : L"Off", false);

        // Helper Lambda to Draw Slider Bar
        auto drawSlider = [&](const D2D1_RECT_F& rect, float value, const std::wstring& label) {
            D2D1_ROUNDED_RECT sliderBox = D2D1::RoundedRect(rect, 14.0f * m_dpiScale, 14.0f * m_dpiScale);
            // Background
            ctx->FillRoundedRectangle(sliderBox, m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(sliderBox, m_brushBorder.Get(), 1.0f);

            // Filled progress portion
            float fillWidth = (rect.right - rect.left) * value;
            if (fillWidth > 6.0f) {
                D2D1_ROUNDED_RECT fillBox = D2D1::RoundedRect(
                    D2D1::RectF(rect.left, rect.top, rect.left + fillWidth, rect.bottom),
                    14.0f * m_dpiScale, 14.0f * m_dpiScale
                );
                ctx->FillRoundedRectangle(fillBox, m_brushSliderFill.Get());
            }

            // Slider Label
            D2D1_RECT_F lblRect = D2D1::RectF(rect.left + 16.0f * m_dpiScale, rect.top, rect.right - 16.0f * m_dpiScale, rect.bottom);
            ComPtr<ID2D1SolidColorBrush> textBrush;
            ctx->CreateSolidColorBrush((value > 0.45f) ? D2D1::ColorF(0.1f, 0.1f, 0.1f, 1.0f) : Colors::TextPrimary, &textBrush);
            ctx->DrawTextW(label.c_str(), static_cast<UINT32>(label.length()), m_formatTitle.Get(), lblRect, textBrush.Get());
        };

        // Draw Sliders
        wchar_t bStr[32];
        swprintf_s(bStr, L"Display Brightness (%d%%)", static_cast<int>(m_displayBrightness * 100.0f));
        drawSlider(m_sliderBrightnessRect, m_displayBrightness, bStr);

        wchar_t vStr[32];
        swprintf_s(vStr, L"Sound Volume (%d%%)", static_cast<int>(m_soundVolume * 100.0f));
        drawSlider(m_sliderVolumeRect, m_soundVolume, vStr);

        // Draw Dark Mode & Focus
        drawToggleCard(m_darkRect, m_darkModeEnabled, L"Dark Mode", m_darkModeEnabled ? L"On" : L"Off", false);
        drawToggleCard(m_focusRect, m_focusEnabled, L"Focus", m_focusEnabled ? L"Do Not Disturb" : L"Off", false);

        m_surface.EndDraw();
    }

    LRESULT CALLBACK ControlCenter::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ControlCenter* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<ControlCenter*>(cs->lpCreateParams);
            if (pThis) {
                pThis->m_hwnd = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        } else {
            pThis = reinterpret_cast<ControlCenter*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT ControlCenter::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                Hide();
            }
            return 0;

        case WM_LBUTTONDOWN: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            SetCapture(m_hwnd);

            // Wi-Fi click
            if (mx >= m_wifiRect.left && mx <= m_wifiRect.right && my >= m_wifiRect.top && my <= m_wifiRect.bottom) {
                m_wifiEnabled = !m_wifiEnabled;
                Render();
                return 0;
            }

            // Bluetooth click
            if (mx >= m_btRect.left && mx <= m_btRect.right && my >= m_btRect.top && my <= m_btRect.bottom) {
                m_bluetoothEnabled = !m_bluetoothEnabled;
                Render();
                return 0;
            }

            // Dark mode click
            if (mx >= m_darkRect.left && mx <= m_darkRect.right && my >= m_darkRect.top && my <= m_darkRect.bottom) {
                m_darkModeEnabled = !m_darkModeEnabled;
                Render();
                return 0;
            }

            // Focus click
            if (mx >= m_focusRect.left && mx <= m_focusRect.right && my >= m_focusRect.top && my <= m_focusRect.bottom) {
                m_focusEnabled = !m_focusEnabled;
                Render();
                return 0;
            }

            // Brightness slider drag start
            if (mx >= m_sliderBrightnessRect.left && mx <= m_sliderBrightnessRect.right &&
                my >= m_sliderBrightnessRect.top && my <= m_sliderBrightnessRect.bottom) {
                m_draggingBrightness = true;
                float val = (mx - m_sliderBrightnessRect.left) / (m_sliderBrightnessRect.right - m_sliderBrightnessRect.left);
                m_displayBrightness = std::clamp(val, 0.05f, 1.0f);
                Render();
                return 0;
            }

            // Volume slider drag start
            if (mx >= m_sliderVolumeRect.left && mx <= m_sliderVolumeRect.right &&
                my >= m_sliderVolumeRect.top && my <= m_sliderVolumeRect.bottom) {
                m_draggingVolume = true;
                float val = (mx - m_sliderVolumeRect.left) / (m_sliderVolumeRect.right - m_sliderVolumeRect.left);
                m_soundVolume = std::clamp(val, 0.0f, 1.0f);
                SystemInfo::SetMasterVolume(m_soundVolume);
                Render();
                return 0;
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (wParam & MK_LBUTTON) {
                float mx = static_cast<float>(GET_X_LPARAM(lParam));

                if (m_draggingBrightness) {
                    float val = (mx - m_sliderBrightnessRect.left) / (m_sliderBrightnessRect.right - m_sliderBrightnessRect.left);
                    m_displayBrightness = std::clamp(val, 0.05f, 1.0f);
                    Render();
                } else if (m_draggingVolume) {
                    float val = (mx - m_sliderVolumeRect.left) / (m_sliderVolumeRect.right - m_sliderVolumeRect.left);
                    m_soundVolume = std::clamp(val, 0.0f, 1.0f);
                    SystemInfo::SetMasterVolume(m_soundVolume);
                    Render();
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
            m_draggingBrightness = false;
            m_draggingVolume = false;
            ReleaseCapture();
            return 0;

        case WM_DESTROY:
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

