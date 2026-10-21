#include "TopBar.h"
#include "SystemInfo.h"
#include "TaskbarController.h"

namespace Aqua {

    const wchar_t TOPBAR_CLASS_NAME[] = L"AquaTopBarWindow";

    TopBar::TopBar() {
        m_leftMenuItems = {
            { L"AquaOS", {}, false, nullptr },
            { L"File",   {}, false, nullptr },
            { L"Edit",   {}, false, nullptr },
            { L"View",   {}, false, nullptr },
            { L"Window", {}, false, nullptr },
            { L"Help",   {}, false, nullptr }
        };
    }

    TopBar::~TopBar() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool TopBar::Create(HINSTANCE hInstance, int screenWidth, int barHeight) {
        m_width = screenWidth;
        m_height = barHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = TopBar::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = TOPBAR_CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            TOPBAR_CLASS_NAME,
            L"AquaOS TopBar",
            WS_POPUP,
            0, 0, m_width, m_height,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);

        if (!m_surface.Create(m_hwnd, m_width, m_height)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // Create Text Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.5f * m_dpiScale,
            L"en-US",
            &m_textFormatRegular
        );
        if (!m_textFormatRegular) {
            // Fallback font
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.5f * m_dpiScale,
                L"en-US",
                &m_textFormatRegular
            );
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.5f * m_dpiScale,
            L"en-US",
            &m_textFormatBold
        );
        if (!m_textFormatBold) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.5f * m_dpiScale,
                L"en-US",
                &m_textFormatBold
            );
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_textFormatClock
        );
        if (!m_textFormatClock) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_MEDIUM,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f * m_dpiScale,
                L"en-US",
                &m_textFormatClock
            );
        }

        if (m_textFormatRegular) {
            m_textFormatRegular->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_textFormatBold) {
            m_textFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_textFormatClock) {
            m_textFormatClock->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            m_textFormatClock->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        }

        // Create Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::HoverItem, &m_brushHover);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);

        UpdateLayout();

        // 1-second timer for clock and battery updates
        SetTimer(m_hwnd, 1001, 1000, nullptr);

        return true;
    }

    void TopBar::Show() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
            Render();
        }
    }

    void TopBar::Hide() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    void TopBar::UpdateLayout() {
        float x = 12.0f * m_dpiScale;
        float h = static_cast<float>(m_height);

        // Logo
        m_logoRect = D2D1::RectF(x, 4.0f, x + 24.0f * m_dpiScale, h - 4.0f);
        x += 30.0f * m_dpiScale;

        // Left Menu Items
        for (size_t i = 0; i < m_leftMenuItems.size(); ++i) {
            float itemWidth = (i == 0) ? (58.0f * m_dpiScale) : (static_cast<float>(m_leftMenuItems[i].text.length()) * 8.5f * m_dpiScale + 14.0f);
            m_leftMenuItems[i].bounds = D2D1::RectF(x, 3.0f, x + itemWidth, h - 3.0f);
            x += itemWidth + 2.0f;
        }

        // Right Items (Right to Left)
        float rx = static_cast<float>(m_width) - (14.0f * m_dpiScale);

        // Clock
        float clockWidth = 160.0f * m_dpiScale;
        m_clockRect = D2D1::RectF(rx - clockWidth, 3.0f, rx, h - 3.0f);
        rx -= (clockWidth + 10.0f * m_dpiScale);

        // Control Center
        float ccWidth = 28.0f * m_dpiScale;
        m_controlCenterRect = D2D1::RectF(rx - ccWidth, 4.0f, rx, h - 4.0f);
        rx -= (ccWidth + 6.0f * m_dpiScale);

        // Search / Spotlight
        float searchWidth = 28.0f * m_dpiScale;
        m_searchRect = D2D1::RectF(rx - searchWidth, 4.0f, rx, h - 4.0f);
        rx -= (searchWidth + 8.0f * m_dpiScale);

        // Battery
        float batteryWidth = 44.0f * m_dpiScale;
        m_batteryRect = D2D1::RectF(rx - batteryWidth, 4.0f, rx, h - 4.0f);
        rx -= (batteryWidth + 8.0f * m_dpiScale);

        // Wi-Fi
        float wifiWidth = 26.0f * m_dpiScale;
        m_wifiRect = D2D1::RectF(rx - wifiWidth, 4.0f, rx, h - 4.0f);
    }

    void TopBar::DrawLogo(ID2D1DeviceContext* ctx, float x, float y, float size) {
        // Draw elegant minimalist OS Apple-like glyph
        ComPtr<ID2D1SolidColorBrush> brush;
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);

        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        // Top Leaf
        D2D1_ELLIPSE leaf = D2D1::Ellipse(D2D1::Point2F(cx + 1.5f, cy - size * 0.38f), size * 0.16f, size * 0.10f);
        ctx->FillEllipse(leaf, brush.Get());

        // Body
        D2D1_ELLIPSE bodyLeft = D2D1::Ellipse(D2D1::Point2F(cx - size * 0.15f, cy + size * 0.05f), size * 0.28f, size * 0.32f);
        D2D1_ELLIPSE bodyRight = D2D1::Ellipse(D2D1::Point2F(cx + size * 0.15f, cy + size * 0.05f), size * 0.28f, size * 0.32f);
        ctx->FillEllipse(bodyLeft, brush.Get());
        ctx->FillEllipse(bodyRight, brush.Get());
    }

    void TopBar::DrawWifiIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        ComPtr<ID2D1SolidColorBrush> brush;
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);

        float cx = x + size * 0.5f;
        float cy = y + size * 0.65f;

        // Center dot
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 1.8f, 1.8f), brush.Get());

        // Arcs (1 and 2)
        D2D1_ELLIPSE arc1 = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.32f, size * 0.32f);
        ctx->DrawEllipse(arc1, brush.Get(), 1.5f);

        D2D1_ELLIPSE arc2 = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.52f, size * 0.52f);
        ctx->DrawEllipse(arc2, brush.Get(), 1.5f);
    }

    void TopBar::DrawBatteryIcon(ID2D1DeviceContext* ctx, float x, float y, float width, float height, int percent, bool charging) {
        ComPtr<ID2D1SolidColorBrush> borderBrush;
        ComPtr<ID2D1SolidColorBrush> fillBrush;
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &borderBrush);
        ctx->CreateSolidColorBrush(charging ? Colors::AccentGreen : Colors::TextPrimary, &fillBrush);

        float bodyW = width - 5.0f;
        float bodyH = height - 8.0f;
        float by = y + 4.0f;

        // Battery outer shell
        D2D1_ROUNDED_RECT shell = D2D1::RoundedRect(D2D1::RectF(x, by, x + bodyW, by + bodyH), 3.0f, 3.0f);
        ctx->DrawRoundedRectangle(shell, borderBrush.Get(), 1.2f);

        // Terminal tip
        D2D1_ROUNDED_RECT tip = D2D1::RoundedRect(D2D1::RectF(x + bodyW + 1.0f, by + bodyH * 0.25f, x + bodyW + 3.0f, by + bodyH * 0.75f), 1.0f, 1.0f);
        ctx->FillRoundedRectangle(tip, borderBrush.Get());

        // Fill capacity
        if (percent > 0) {
            float pct = static_cast<float>(percent) / 100.0f;
            float fillW = (bodyW - 4.0f) * pct;
            D2D1_ROUNDED_RECT fill = D2D1::RoundedRect(D2D1::RectF(x + 2.0f, by + 2.0f, x + 2.0f + fillW, by + bodyH - 2.0f), 1.5f, 1.5f);
            ctx->FillRoundedRectangle(fill, fillBrush.Get());
        }
    }

    void TopBar::DrawSearchIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        ComPtr<ID2D1SolidColorBrush> brush;
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);

        float cx = x + size * 0.42f;
        float cy = y + size * 0.42f;
        float r = size * 0.24f;

        // Glass circle
        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush.Get(), 1.5f);

        // Handle
        float hx1 = cx + r * 0.7f;
        float hy1 = cy + r * 0.7f;
        float hx2 = cx + r * 1.55f;
        float hy2 = cy + r * 1.55f;
        ctx->DrawLine(D2D1::Point2F(hx1, hy1), D2D1::Point2F(hx2, hy2), brush.Get(), 1.8f);
    }

    void TopBar::DrawControlCenterIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        ComPtr<ID2D1SolidColorBrush> brush;
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);

        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        // Two horizontal sliders
        float barW = size * 0.55f;
        float barH = 3.2f;

        // Top toggle
        ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - barW * 0.5f, cy - 5.0f, cx + barW * 0.5f, cy - 5.0f + barH), 1.6f, 1.6f), brush.Get());
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + barW * 0.2f, cy - 3.4f), 3.2f, 3.2f), brush.Get());

        // Bottom toggle
        ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - barW * 0.5f, cy + 2.5f, cx + barW * 0.5f, cy + 2.5f + barH), 1.6f, 1.6f), brush.Get());
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - barW * 0.2f, cy + 4.1f), 3.2f, 3.2f), brush.Get());
    }

    void TopBar::Render() {
        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Draw Frosted Glass TopBar Background
        ComPtr<ID2D1SolidColorBrush> bgBrush;
        ctx->CreateSolidColorBrush(Colors::GlassBackground, &bgBrush);
        ctx->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(m_width), static_cast<float>(m_height)), bgBrush.Get());

        // 2. Draw Bottom Specular Separator Line
        ctx->DrawLine(
            D2D1::Point2F(0, static_cast<float>(m_height) - 0.5f),
            D2D1::Point2F(static_cast<float>(m_width), static_cast<float>(m_height) - 0.5f),
            m_brushBorder.Get(),
            1.0f
        );

        // 3. Draw Logo
        if (m_logoHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_logoRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawLogo(ctx, m_logoRect.left, m_logoRect.top + 2.0f, 20.0f * m_dpiScale);

        // 4. Draw Left Menu Items
        for (size_t i = 0; i < m_leftMenuItems.size(); ++i) {
            auto& item = m_leftMenuItems[i];
            if (item.isHovered) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(item.bounds, 4.0f, 4.0f), m_brushHover.Get());
            }

            IDWriteTextFormat* format = (i == 0) ? m_textFormatBold.Get() : m_textFormatRegular.Get();
            D2D1_RECT_F textRect = D2D1::RectF(item.bounds.left + 6.0f, item.bounds.top, item.bounds.right - 6.0f, item.bounds.bottom);

            ctx->DrawTextW(
                item.text.c_str(),
                static_cast<UINT32>(item.text.length()),
                format,
                textRect,
                m_brushTextPrimary.Get()
            );
        }

        // 5. Draw Right Items (Wi-Fi, Battery, Search, Control Center, Clock)
        // Wi-Fi
        if (m_wifiHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_wifiRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawWifiIcon(ctx, m_wifiRect.left + 3.0f, m_wifiRect.top + 3.0f, 20.0f * m_dpiScale);

        // Battery
        bool charging = true;
        int batteryPct = SystemInfo::GetBatteryPercent(charging);
        if (m_batteryHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_batteryRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawBatteryIcon(ctx, m_batteryRect.left + 4.0f, m_batteryRect.top + 4.0f, 28.0f * m_dpiScale, 16.0f * m_dpiScale, batteryPct, charging);

        // Search / Spotlight Button
        if (m_searchHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_searchRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawSearchIcon(ctx, m_searchRect.left + 4.0f, m_searchRect.top + 3.0f, 20.0f * m_dpiScale);

        // Control Center Button
        if (m_controlCenterHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_controlCenterRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawControlCenterIcon(ctx, m_controlCenterRect.left + 4.0f, m_controlCenterRect.top + 3.0f, 20.0f * m_dpiScale);

        // Real-Time Clock
        if (m_clockHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_clockRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        std::wstring clockStr = SystemInfo::GetFormattedClock();
        ctx->DrawTextW(
            clockStr.c_str(),
            static_cast<UINT32>(clockStr.length()),
            m_textFormatClock.Get(),
            m_clockRect,
            m_brushTextPrimary.Get()
        );

        m_surface.EndDraw();
    }

    LRESULT CALLBACK TopBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        TopBar* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<TopBar*>(cs->lpCreateParams);
            if (pThis) {
                pThis->m_hwnd = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        } else {
            pThis = reinterpret_cast<TopBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT TopBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            Render();
            return 0;

        case WM_MOUSEMOVE: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            bool needRepaint = false;

            // Track mouse leaving window
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);

            // Check Logo
            bool logoH = (mx >= m_logoRect.left && mx <= m_logoRect.right && my >= m_logoRect.top && my <= m_logoRect.bottom);
            if (logoH != m_logoHovered) { m_logoHovered = logoH; needRepaint = true; }

            // Check Menu items
            for (auto& item : m_leftMenuItems) {
                bool h = (mx >= item.bounds.left && mx <= item.bounds.right && my >= item.bounds.top && my <= item.bounds.bottom);
                if (h != item.isHovered) {
                    item.isHovered = h;
                    needRepaint = true;
                }
            }

            // Check Right items
            bool sH = (mx >= m_searchRect.left && mx <= m_searchRect.right);
            if (sH != m_searchHovered) { m_searchHovered = sH; needRepaint = true; }

            bool ccH = (mx >= m_controlCenterRect.left && mx <= m_controlCenterRect.right);
            if (ccH != m_controlCenterHovered) { m_controlCenterHovered = ccH; needRepaint = true; }

            bool wH = (mx >= m_wifiRect.left && mx <= m_wifiRect.right);
            if (wH != m_wifiHovered) { m_wifiHovered = wH; needRepaint = true; }

            bool bH = (mx >= m_batteryRect.left && mx <= m_batteryRect.right);
            if (bH != m_batteryHovered) { m_batteryHovered = bH; needRepaint = true; }

            bool clkH = (mx >= m_clockRect.left && mx <= m_clockRect.right);
            if (clkH != m_clockHovered) { m_clockHovered = clkH; needRepaint = true; }

            if (needRepaint) {
                Render();
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_logoHovered = false;
            m_searchHovered = false;
            m_controlCenterHovered = false;
            m_wifiHovered = false;
            m_batteryHovered = false;
            m_clockHovered = false;
            for (auto& item : m_leftMenuItems) {
                item.isHovered = false;
            }
            Render();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            // Apple Logo -> System Menu
            if (mx >= m_logoRect.left && mx <= m_logoRect.right) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, 2001, L"About AquaOS");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2002, L"System Settings...");
                AppendMenuW(hMenu, MF_STRING, 2003, L"Task Manager (Activity Monitor)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2004, L"Lock Screen\tWin+L");
                AppendMenuW(hMenu, MF_STRING, 2005, L"Sleep");
                AppendMenuW(hMenu, MF_STRING, 2006, L"Restart...");
                AppendMenuW(hMenu, MF_STRING, 2007, L"Shut Down...");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2010, TaskbarController::Instance().IsSuppressed() ? L"Show Windows Taskbar\tCtrl+Alt+T" : L"Hide Windows Taskbar\tCtrl+Alt+T");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2099, L"Exit AquaOS");

                POINT pt = { static_cast<LONG>(m_logoRect.left), static_cast<LONG>(m_height) };
                ClientToScreen(m_hwnd, &pt);

                int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, nullptr);
                DestroyMenu(hMenu);

                if (cmd == 2001) {
                    MessageBoxW(m_hwnd, L"AquaOS Desktop Shell v1.0\nBuilt with C++20 & DirectComposition\nUltra-fast 120 FPS Native Architecture", L"About AquaOS", MB_OK | MB_ICONINFORMATION);
                } else if (cmd == 2002) {
                    ShellExecuteW(nullptr, L"open", L"ms-settings:", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (cmd == 2003) {
                    ShellExecuteW(nullptr, L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (cmd == 2004) {
                    LockWorkStation();
                } else if (cmd == 2005) {
                    SetSuspendState(FALSE, FALSE, FALSE);
                } else if (cmd == 2010) {
                    TaskbarController::Instance().ToggleTaskbar();
                } else if (cmd == 2099) {
                    TaskbarController::Instance().RestoreTaskbar();
                    PostQuitMessage(0);
                }
                return 0;
            }

            // Search Icon
            if (mx >= m_searchRect.left && mx <= m_searchRect.right) {
                if (m_onSpotlight) m_onSpotlight();
                return 0;
            }

            // Control Center Icon
            if (mx >= m_controlCenterRect.left && mx <= m_controlCenterRect.right) {
                if (m_onControlCenter) m_onControlCenter();
                return 0;
            }

            // Menu item clicks
            for (size_t i = 1; i < m_leftMenuItems.size(); ++i) {
                auto& item = m_leftMenuItems[i];
                if (mx >= item.bounds.left && mx <= item.bounds.right) {
                    HMENU hMenu = CreatePopupMenu();
                    if (item.text == L"File") {
                        AppendMenuW(hMenu, MF_STRING, 3001, L"New Window\tCtrl+N");
                        AppendMenuW(hMenu, MF_STRING, 3002, L"Open...\tCtrl+O");
                        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(hMenu, MF_STRING, 3003, L"Close Window\tCtrl+W");
                    } else if (item.text == L"Edit") {
                        AppendMenuW(hMenu, MF_STRING, 3011, L"Undo\tCtrl+Z");
                        AppendMenuW(hMenu, MF_STRING, 3012, L"Redo\tCtrl+Y");
                        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(hMenu, MF_STRING, 3013, L"Cut\tCtrl+X");
                        AppendMenuW(hMenu, MF_STRING, 3014, L"Copy\tCtrl+C");
                        AppendMenuW(hMenu, MF_STRING, 3015, L"Paste\tCtrl+V");
                    } else if (item.text == L"View") {
                        AppendMenuW(hMenu, MF_STRING, 3031, L"Mission Control\tF3");
                        AppendMenuW(hMenu, MF_STRING, 3032, L"Spotlight Search\tAlt+Space");
                        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(hMenu, MF_STRING, 3033, L"Control Center");
                    } else {
                        AppendMenuW(hMenu, MF_STRING, 3020, (item.text + L" Actions").c_str());
                    }

                    POINT pt = { static_cast<LONG>(item.bounds.left), static_cast<LONG>(m_height) };
                    ClientToScreen(m_hwnd, &pt);
                    int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, nullptr);
                    DestroyMenu(hMenu);

                    if (cmd == 3031) {
                        if (m_onMissionControl) m_onMissionControl();
                    } else if (cmd == 3032) {
                        if (m_onSpotlight) m_onSpotlight();
                    } else if (cmd == 3033) {
                        if (m_onControlCenter) m_onControlCenter();
                    }
                    return 0;
                }
            }
            return 0;
        }

        case WM_DESTROY:
            m_surface.Destroy();
            TaskbarController::Instance().RestoreTaskbar();
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

