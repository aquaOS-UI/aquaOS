#define NOMINMAX
#include "ControlCenter.h"
#include "SystemInfo.h"
#include "ThemeManager.h"
#include "MediaControllerService.h"
#include <algorithm>

namespace Aqua {

    const wchar_t CC_CLASS_NAME[] = L"AquaControlCenterWindow";

    ControlCenter::ControlCenter() = default;

    ControlCenter::~ControlCenter() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool ControlCenter::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        m_width = 330;
        m_height = 368;

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
        m_width = static_cast<int>(330 * m_dpiScale);
        m_height = static_cast<int>(368 * m_dpiScale);

        if (!m_surface.Create(m_hwnd, m_width, m_height)) {
            return false;
        }

        // Query current system volume
        m_soundVolume = SystemInfo::GetMasterVolume();

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // 1. DirectWrite Formats
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

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatMediaTitle
        );
        if (m_formatMediaTitle) {
            m_formatMediaTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatMediaTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.5f * m_dpiScale,
            L"en-US",
            &m_formatMediaArtist
        );
        if (m_formatMediaArtist) {
            m_formatMediaArtist->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatMediaArtist->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f * m_dpiScale,
            L"en-US",
            &m_formatMediaButton
        );
        if (m_formatMediaButton) {
            m_formatMediaButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatMediaButton->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::GlassBackgroundDark, &m_brushBg);
        ctx->CreateSolidColorBrush(Colors::GlassCard, &m_brushCardBg);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushActive);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f), &m_brushInactive);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_brushSliderFill);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f, 1.0f), &m_brushSliderTextDark);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f), &m_brushButtonHover);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.22f, 0.35f, 1.0f), &m_brushMediaAccent); // Apple Music pink/red accent

        CalculateLayout();

        // 400ms update timer for live media & volume sync
        SetTimer(m_hwnd, 4001, 400, nullptr);

        return true;
    }

    void ControlCenter::CalculateLayout() {
        float pad = 12.0f * m_dpiScale;
        float gap = 8.0f * m_dpiScale;
        float wHalf = (m_width - (pad * 2.0f) - gap) * 0.5f;

        // Row 1: Wi-Fi & Bluetooth
        float hCard = 58.0f * m_dpiScale;
        m_wifiRect = D2D1::RectF(pad, pad, pad + wHalf, pad + hCard);
        m_btRect = D2D1::RectF(pad + wHalf + gap, pad, m_width - pad, pad + hCard);

        // Row 2: Sliders
        float sliderY1 = pad + hCard + gap;
        float sliderH = 42.0f * m_dpiScale;
        m_sliderBrightnessRect = D2D1::RectF(pad, sliderY1, m_width - pad, sliderY1 + sliderH);

        float sliderY2 = sliderY1 + sliderH + gap;
        m_sliderVolumeRect = D2D1::RectF(pad, sliderY2, m_width - pad, sliderY2 + sliderH);

        // Row 3: Now Playing Media Card
        float mediaY = sliderY2 + sliderH + gap;
        float mediaH = 74.0f * m_dpiScale;
        m_mediaCardRect = D2D1::RectF(pad, mediaY, m_width - pad, mediaY + mediaH);

        float artSize = 52.0f * m_dpiScale;
        m_mediaArtRect = D2D1::RectF(pad + 10.0f * m_dpiScale, mediaY + 11.0f * m_dpiScale, pad + 10.0f * m_dpiScale + artSize, mediaY + 11.0f * m_dpiScale + artSize);

        // Transport Buttons
        float btnSize = 26.0f * m_dpiScale;
        float btnY = mediaY + (mediaH - btnSize) * 0.5f;
        float rX = m_width - pad - 10.0f * m_dpiScale;

        m_mediaNextRect = D2D1::RectF(rX - btnSize, btnY, rX, btnY + btnSize);
        rX -= (btnSize + 5.0f * m_dpiScale);

        m_mediaPlayPauseRect = D2D1::RectF(rX - btnSize, btnY, rX, btnY + btnSize);
        rX -= (btnSize + 5.0f * m_dpiScale);

        m_mediaPrevRect = D2D1::RectF(rX - btnSize, btnY, rX, btnY + btnSize);

        // Row 4: Dark Mode & Focus
        float botY = mediaY + mediaH + gap;
        float botH = 56.0f * m_dpiScale;
        m_darkRect = D2D1::RectF(pad, botY, pad + wHalf, botY + botH);
        m_focusRect = D2D1::RectF(pad + wHalf + gap, botY, m_width - pad, botY + botH);

        int targetHeight = static_cast<int>(botY + botH + pad);
        if (targetHeight != m_height) {
            m_height = targetHeight;
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, m_width, m_height, SWP_NOMOVE | SWP_NOACTIVATE);
            m_surface.Resize(m_width, m_height);
        }
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

        POINT pt;
        GetCursorPos(&pt);
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
            mi.rcMonitor = { 0, 0, m_screenWidth, m_screenHeight };
        }

        int posX = mi.rcMonitor.right - m_width - 12;
        int posY = mi.rcMonitor.top + 38;
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
        auto drawToggleCard = [&](const D2D1_RECT_F& rect, bool active, const std::wstring& title, const std::wstring& sub) {
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
            D2D1_RECT_F titleRect = D2D1::RectF(iconX + iconSize + 8.0f, rect.top + 10.0f, rect.right - 6.0f, rect.top + 28.0f);
            ctx->DrawTextW(title.c_str(), static_cast<UINT32>(title.length()), m_formatTitle.Get(), titleRect, m_brushTextPrimary.Get());

            D2D1_RECT_F subRect = D2D1::RectF(iconX + iconSize + 8.0f, rect.top + 28.0f, rect.right - 6.0f, rect.bottom - 6.0f);
            ctx->DrawTextW(sub.c_str(), static_cast<UINT32>(sub.length()), m_formatSubtitle.Get(), subRect, m_brushTextSecondary.Get());
        };

        // Draw Row 1: Wi-Fi & Bluetooth
        drawToggleCard(m_wifiRect, m_wifiEnabled, L"Wi-Fi", m_wifiEnabled ? L"Connected" : L"Off");
        drawToggleCard(m_btRect, m_bluetoothEnabled, L"Bluetooth", m_bluetoothEnabled ? L"On" : L"Off");

        // Helper Lambda to Draw Slider Card
        auto drawSlider = [&](const D2D1_RECT_F& rect, float val, const wchar_t* label) {
            D2D1_ROUNDED_RECT card = D2D1::RoundedRect(rect, 14.0f * m_dpiScale, 14.0f * m_dpiScale);
            ctx->FillRoundedRectangle(card, m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(card, m_brushBorder.Get(), 1.0f);

            // Fill active track proportion
            float trackW = (rect.right - rect.left) * val;
            D2D1_ROUNDED_RECT fillRect = D2D1::RoundedRect(
                D2D1::RectF(rect.left, rect.top, rect.left + trackW, rect.bottom),
                14.0f * m_dpiScale, 14.0f * m_dpiScale
            );
            ctx->FillRoundedRectangle(fillRect, m_brushSliderFill.Get());

            // Text Label
            D2D1_RECT_F labelRect = D2D1::RectF(rect.left + 16.0f * m_dpiScale, rect.top + 10.0f, rect.right - 16.0f, rect.bottom - 10.0f);
            ctx->DrawTextW(
                label,
                static_cast<UINT32>(wcslen(label)),
                m_formatTitle.Get(),
                labelRect,
                (val > 0.45f) ? m_brushSliderTextDark.Get() : m_brushTextPrimary.Get()
            );
        };

        // Draw Row 2: Sliders
        wchar_t bStr[32];
        swprintf_s(bStr, L"Display Brightness (%d%%)", static_cast<int>(m_displayBrightness * 100.0f));
        drawSlider(m_sliderBrightnessRect, m_displayBrightness, bStr);

        wchar_t vStr[32];
        swprintf_s(vStr, L"Sound Volume (%d%%)", static_cast<int>(m_soundVolume * 100.0f));
        drawSlider(m_sliderVolumeRect, m_soundVolume, vStr);

        // Draw Row 3: Now Playing Media Card
        D2D1_ROUNDED_RECT mediaCardBox = D2D1::RoundedRect(m_mediaCardRect, 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(mediaCardBox, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(mediaCardBox, m_brushBorder.Get(), 1.0f);

        auto track = MediaControllerService::Instance().GetCurrentTrack();
        auto thumbBitmap = MediaControllerService::Instance().GetThumbnailBitmap(ctx);

        // Album Artwork Box
        D2D1_ROUNDED_RECT artBox = D2D1::RoundedRect(m_mediaArtRect, 8.0f * m_dpiScale, 8.0f * m_dpiScale);
        if (thumbBitmap) {
            ctx->DrawBitmap(thumbBitmap.Get(), m_mediaArtRect);
            ctx->DrawRoundedRectangle(artBox, m_brushBorder.Get(), 0.8f);
        } else {
            // Gradient / Accent Placeholder
            ctx->FillRoundedRectangle(artBox, m_brushMediaAccent.Get());
            ctx->DrawTextW(L"♫", 1, m_formatMediaButton.Get(), m_mediaArtRect, m_brushTextPrimary.Get());
        }

        // Track Details (Middle)
        float textX = m_mediaArtRect.right + 10.0f * m_dpiScale;
        float textRight = m_mediaPrevRect.left - 6.0f * m_dpiScale;

        D2D1_RECT_F mediaTitleRect = D2D1::RectF(textX, m_mediaCardRect.top + 15.0f * m_dpiScale, textRight, m_mediaCardRect.top + 34.0f * m_dpiScale);
        ctx->DrawTextW(
            track.title.c_str(),
            static_cast<UINT32>(track.title.length()),
            m_formatMediaTitle.Get(),
            mediaTitleRect,
            m_brushTextPrimary.Get()
        );

        std::wstring subDetails = track.artist;
        if (!track.sourceApp.empty()) {
            subDetails = subDetails.empty() ? track.sourceApp : (subDetails + L" • " + track.sourceApp);
        }
        if (subDetails.empty()) subDetails = L"Audio Controller";

        D2D1_RECT_F mediaSubRect = D2D1::RectF(textX, m_mediaCardRect.top + 34.0f * m_dpiScale, textRight, m_mediaCardRect.top + 55.0f * m_dpiScale);
        ctx->DrawTextW(
            subDetails.c_str(),
            static_cast<UINT32>(subDetails.length()),
            m_formatMediaArtist.Get(),
            mediaSubRect,
            m_brushTextSecondary.Get()
        );

        // Transport Buttons (Previous, Play/Pause, Next)
        auto drawTransportBtn = [&](const D2D1_RECT_F& btnRect, const wchar_t* symbol, bool isHovered) {
            D2D1_ROUNDED_RECT btnBox = D2D1::RoundedRect(btnRect, (btnRect.bottom - btnRect.top) * 0.5f, (btnRect.bottom - btnRect.top) * 0.5f);
            if (isHovered) {
                ctx->FillRoundedRectangle(btnBox, m_brushButtonHover.Get());
            }
            ctx->DrawTextW(symbol, static_cast<UINT32>(wcslen(symbol)), m_formatMediaButton.Get(), btnRect, m_brushTextPrimary.Get());
        };

        drawTransportBtn(m_mediaPrevRect, L"⏮", m_mediaPrevHover);
        drawTransportBtn(m_mediaPlayPauseRect, track.isPlaying ? L"⏸" : L"▶", m_mediaPlayHover);
        drawTransportBtn(m_mediaNextRect, L"⏭", m_mediaNextHover);

        // Draw Row 4: Dark Mode & Focus
        drawToggleCard(m_darkRect, m_darkModeEnabled, L"Dark Mode", m_darkModeEnabled ? L"On" : L"Off");
        drawToggleCard(m_focusRect, m_focusEnabled, L"Focus", m_focusEnabled ? L"Do Not Disturb" : L"Off");

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
        case WM_TIMER:
            if (m_visible) {
                Render();
            }
            return 0;

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
                ThemeManager::Instance().ToggleTheme();
                m_darkModeEnabled = ThemeManager::Instance().IsDarkMode();
                Render();
                return 0;
            }

            // Focus click
            if (mx >= m_focusRect.left && mx <= m_focusRect.right && my >= m_focusRect.top && my <= m_focusRect.bottom) {
                m_focusEnabled = !m_focusEnabled;
                Render();
                return 0;
            }

            // Media Transport Controls
            if (mx >= m_mediaPrevRect.left && mx <= m_mediaPrevRect.right && my >= m_mediaPrevRect.top && my <= m_mediaPrevRect.bottom) {
                MediaControllerService::Instance().PreviousTrack();
                Render();
                return 0;
            }
            if (mx >= m_mediaPlayPauseRect.left && mx <= m_mediaPlayPauseRect.right && my >= m_mediaPlayPauseRect.top && my <= m_mediaPlayPauseRect.bottom) {
                MediaControllerService::Instance().TogglePlayPause();
                Render();
                return 0;
            }
            if (mx >= m_mediaNextRect.left && mx <= m_mediaNextRect.right && my >= m_mediaNextRect.top && my <= m_mediaNextRect.bottom) {
                MediaControllerService::Instance().NextTrack();
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
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            bool prevH = (mx >= m_mediaPrevRect.left && mx <= m_mediaPrevRect.right && my >= m_mediaPrevRect.top && my <= m_mediaPrevRect.bottom);
            bool playH = (mx >= m_mediaPlayPauseRect.left && mx <= m_mediaPlayPauseRect.right && my >= m_mediaPlayPauseRect.top && my <= m_mediaPlayPauseRect.bottom);
            bool nextH = (mx >= m_mediaNextRect.left && mx <= m_mediaNextRect.right && my >= m_mediaNextRect.top && my <= m_mediaNextRect.bottom);

            bool needRepaint = (prevH != m_mediaPrevHover || playH != m_mediaPlayHover || nextH != m_mediaNextHover);
            m_mediaPrevHover = prevH;
            m_mediaPlayHover = playH;
            m_mediaNextHover = nextH;

            if (wParam & MK_LBUTTON) {
                if (m_draggingBrightness) {
                    float val = (mx - m_sliderBrightnessRect.left) / (m_sliderBrightnessRect.right - m_sliderBrightnessRect.left);
                    m_displayBrightness = std::clamp(val, 0.05f, 1.0f);
                    needRepaint = true;
                } else if (m_draggingVolume) {
                    float val = (mx - m_sliderVolumeRect.left) / (m_sliderVolumeRect.right - m_sliderVolumeRect.left);
                    m_soundVolume = std::clamp(val, 0.0f, 1.0f);
                    SystemInfo::SetMasterVolume(m_soundVolume);
                    needRepaint = true;
                }
            }

            if (needRepaint) {
                Render();
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
