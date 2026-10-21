#include "Spotlight.h"
#include <algorithm>
#include <sstream>

namespace Aqua {

    const wchar_t SPOTLIGHT_CLASS_NAME[] = L"AquaSpotlightWindow";

    Spotlight::Spotlight() {
    }

    Spotlight::~Spotlight() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool Spotlight::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        m_width = 640;
        m_baseHeight = 56;
        m_height = m_baseHeight;

        int posX = (m_screenWidth - m_width) / 2;
        int posY = static_cast<int>(m_screenHeight * 0.22);

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = Spotlight::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = SPOTLIGHT_CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            SPOTLIGHT_CLASS_NAME,
            L"AquaOS Spotlight",
            WS_POPUP,
            posX, posY, m_width, m_height,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_width = static_cast<int>(640 * m_dpiScale);
        m_baseHeight = static_cast<int>(56 * m_dpiScale);
        m_height = m_baseHeight;

        if (!m_surface.Create(m_hwnd, m_width, m_height)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // DirectWrite Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f * m_dpiScale,
            L"en-US",
            &m_queryFormat
        );
        if (!m_queryFormat) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                18.0f * m_dpiScale,
                L"en-US",
                &m_queryFormat
            );
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.5f * m_dpiScale,
            L"en-US",
            &m_resultTitleFormat
        );
        if (!m_resultTitleFormat) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                13.5f * m_dpiScale,
                L"en-US",
                &m_resultTitleFormat
            );
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.5f * m_dpiScale,
            L"en-US",
            &m_resultSubFormat
        );
        if (!m_resultSubFormat) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                11.5f * m_dpiScale,
                L"en-US",
                &m_resultSubFormat
            );
        }

        if (m_queryFormat) {
            m_queryFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_resultTitleFormat) {
            m_resultTitleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_resultSubFormat) {
            m_resultSubFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::GlassBackgroundDark, &m_brushBg);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushSelection);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::TextMuted, &m_brushTextPlaceholder);
        ctx->CreateSolidColorBrush(Colors::GlassSeparator, &m_brushSeparator);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushChip);

        UpdateSearchResults();

        // 500ms Timer for cursor blinking
        SetTimer(m_hwnd, 3001, 500, nullptr);

        return true;
    }

    void Spotlight::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    void Spotlight::Show() {
        m_visible = true;
        m_query.clear();
        m_selectedIndex = 0;
        UpdateSearchResults();

        int posX = (m_screenWidth - m_width) / 2;
        int posY = static_cast<int>(m_screenHeight * 0.22);
        SetWindowPos(m_hwnd, HWND_TOPMOST, posX, posY, m_width, m_height, SWP_SHOWWINDOW);

        SetForegroundWindow(m_hwnd);
        SetFocus(m_hwnd);
        Render();
    }

    void Spotlight::Hide() {
        m_visible = false;
        ShowWindow(m_hwnd, SW_HIDE);
    }

    void Spotlight::UpdateSearchResults() {
        m_results.clear();

        struct BuiltinApp {
            std::wstring name;
            std::wstring desc;
            std::wstring path;
            std::wstring args;
            D2D1_COLOR_F color;
        };

        static const std::vector<BuiltinApp> apps = {
            { L"Terminal",          L"Windows Terminal & PowerShell", L"wt.exe",       L"",               Colors::AccentGreen },
            { L"File Explorer",     L"Files, Folders & Drives",       L"explorer.exe", L"",               Colors::AccentBlue },
            { L"System Settings",   L"Windows Configuration",         L"ms-settings:", L"",               Colors::AccentPurple },
            { L"Task Manager",      L"Activity & Performance Monitor",L"taskmgr.exe",  L"",               Colors::AccentOrange },
            { L"Calculator",        L"Arithmetic & Scientific",       L"calc.exe",     L"",               Colors::AccentOrange },
            { L"Notepad",           L"Quick Text Editor",             L"notepad.exe",  L"",               Colors::AccentBlue },
            { L"Visual Studio Code",L"Code & Script Editor",          L"code.exe",     L"",               Colors::AccentBlue },
            { L"Microsoft Edge",    L"Web Browser",                   L"msedge.exe",   L"",               Colors::AccentBlue },
            { L"Paint",             L"Image & Drawing Tool",          L"mspaint.exe",  L"",               Colors::AccentPurple },
            { L"Command Prompt",    L"Classic Windows Shell",         L"cmd.exe",      L"",               Colors::AccentGreen }
        };

        if (m_query.empty()) {
            for (int i = 0; i < 4; ++i) {
                m_results.push_back({ apps[i].name, apps[i].desc, apps[i].path, apps[i].args, apps[i].color });
            }
        } else {
            // Check for simple math expression (e.g. 25 * 4, 100 / 2, 50 + 75, 90 - 45)
            bool isMath = false;
            double num1 = 0, num2 = 0;
            wchar_t op = 0;
            std::wstringstream ss(m_query);
            if (ss >> num1 >> op >> num2 && (op == L'+' || op == L'-' || op == L'*' || op == L'/')) {
                double result = 0;
                if (op == L'+') result = num1 + num2;
                else if (op == L'-') result = num1 - num2;
                else if (op == L'*') result = num1 * num2;
                else if (op == L'/' && num2 != 0) result = num1 / num2;

                wchar_t resBuf[64];
                swprintf_s(resBuf, L"= %.4g", result);
                m_results.push_back({ std::wstring(resBuf), L"Calculation Result", L"", L"", Colors::AccentOrange });
                isMath = true;
            }

            // Case-insensitive substring search in apps
            std::wstring qLower = m_query;
            std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::towlower);

            for (const auto& a : apps) {
                std::wstring nameLower = a.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
                if (nameLower.find(qLower) != std::wstring::npos) {
                    m_results.push_back({ a.name, a.desc, a.path, a.args, a.color });
                }
            }

            // Add Web Search Option
            m_results.push_back({
                L"Search Web for \"" + m_query + L"\"",
                L"Google Search in default browser",
                L"https://www.google.com/search?q=" + m_query,
                L"",
                Colors::AccentBlue
            });
        }

        // Clamp selection
        if (m_selectedIndex >= static_cast<int>(m_results.size())) {
            m_selectedIndex = std::max(0, static_cast<int>(m_results.size()) - 1);
        }

        // Resize window to fit results
        int resultCount = std::min(static_cast<int>(m_results.size()), 5);
        int targetHeight = m_baseHeight + (resultCount > 0 ? (resultCount * static_cast<int>(50 * m_dpiScale) + static_cast<int>(12 * m_dpiScale)) : 0);

        if (targetHeight != m_height) {
            m_height = targetHeight;
            int posX = (m_screenWidth - m_width) / 2;
            int posY = static_cast<int>(m_screenHeight * 0.22);
            SetWindowPos(m_hwnd, HWND_TOPMOST, posX, posY, m_width, m_height, SWP_NOACTIVATE | SWP_NOMOVE);
            m_surface.Resize(m_width, m_height);
        }
    }

    void Spotlight::ExecuteSelected() {
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_results.size())) {
            const auto& res = m_results[m_selectedIndex];
            if (!res.execPath.empty()) {
                ShellExecuteW(nullptr, L"open", res.execPath.c_str(), res.execArgs.empty() ? nullptr : res.execArgs.c_str(), nullptr, SW_SHOWNORMAL);
            }
        }
        Hide();
    }

    void Spotlight::Render() {
        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        float w = static_cast<float>(m_width);
        float h = static_cast<float>(m_height);
        float corner = 16.0f * m_dpiScale;

        // 1. Frosted Glass Capsule
        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), corner, corner);
        ctx->FillRoundedRectangle(box, m_brushBg.Get());
        ctx->DrawRoundedRectangle(box, m_brushBorder.Get(), 1.2f);

        // 2. Search Icon (Left)
        float cx = 28.0f * m_dpiScale;
        float cy = (m_baseHeight * 0.5f);
        float r = 7.0f * m_dpiScale;
        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), m_brushTextSecondary.Get(), 1.8f);
        ctx->DrawLine(D2D1::Point2F(cx + r * 0.7f, cy + r * 0.7f), D2D1::Point2F(cx + r * 1.5f, cy + r * 1.5f), m_brushTextSecondary.Get(), 2.0f);

        // 3. Search Query / Placeholder
        float textLeft = 48.0f * m_dpiScale;
        float textRight = w - (20.0f * m_dpiScale);
        D2D1_RECT_F queryRect = D2D1::RectF(textLeft, 0, textRight, static_cast<float>(m_baseHeight));

        if (m_query.empty()) {
            ctx->DrawTextW(
                L"Spotlight Search",
                16,
                m_queryFormat.Get(),
                queryRect,
                m_brushTextPlaceholder.Get()
            );
        } else {
            ctx->DrawTextW(
                m_query.c_str(),
                static_cast<UINT32>(m_query.length()),
                m_queryFormat.Get(),
                queryRect,
                m_brushTextPrimary.Get()
            );

            // Blinking cursor
            if (m_showCursor) {
                float cursorX = textLeft + static_cast<float>(m_query.length()) * 10.0f * m_dpiScale + 2.0f;
                ctx->DrawLine(
                    D2D1::Point2F(cursorX, static_cast<float>(m_baseHeight) * 0.28f),
                    D2D1::Point2F(cursorX, static_cast<float>(m_baseHeight) * 0.72f),
                    m_brushSelection.Get(),
                    2.0f
                );
            }
        }

        // 4. Draw Results List
        if (!m_results.empty() && m_height > m_baseHeight) {
            // Horizontal Separator
            ctx->DrawLine(
                D2D1::Point2F(14.0f * m_dpiScale, static_cast<float>(m_baseHeight)),
                D2D1::Point2F(w - (14.0f * m_dpiScale), static_cast<float>(m_baseHeight)),
                m_brushSeparator.Get(),
                1.0f
            );

            float itemY = static_cast<float>(m_baseHeight) + (6.0f * m_dpiScale);
            float itemH = 48.0f * m_dpiScale;
            float padX = 10.0f * m_dpiScale;

            int displayCount = std::min(static_cast<int>(m_results.size()), 5);
            for (int i = 0; i < displayCount; ++i) {
                const auto& item = m_results[i];
                D2D1_RECT_F itemRect = D2D1::RectF(padX, itemY, w - padX, itemY + itemH);

                // Highlight selected item
                if (i == m_selectedIndex) {
                    D2D1_ROUNDED_RECT selBox = D2D1::RoundedRect(itemRect, 8.0f * m_dpiScale, 8.0f * m_dpiScale);
                    ctx->FillRoundedRectangle(selBox, m_brushSelection.Get());
                }

                // Item color chip
                m_brushChip->SetColor(item.iconColor);
                D2D1_ROUNDED_RECT chipBox = D2D1::RoundedRect(
                    D2D1::RectF(itemRect.left + 12.0f * m_dpiScale, itemY + 11.0f * m_dpiScale, itemRect.left + 38.0f * m_dpiScale, itemY + 37.0f * m_dpiScale),
                    6.0f * m_dpiScale, 6.0f * m_dpiScale
                );
                ctx->FillRoundedRectangle(chipBox, m_brushChip.Get());

                // Title
                D2D1_RECT_F titleRect = D2D1::RectF(itemRect.left + 48.0f * m_dpiScale, itemY + 4.0f * m_dpiScale, itemRect.right - 10.0f, itemY + 26.0f * m_dpiScale);
                ctx->DrawTextW(
                    item.title.c_str(),
                    static_cast<UINT32>(item.title.length()),
                    m_resultTitleFormat.Get(),
                    titleRect,
                    m_brushTextPrimary.Get()
                );

                // Subtitle
                D2D1_RECT_F subRect = D2D1::RectF(itemRect.left + 48.0f * m_dpiScale, itemY + 24.0f * m_dpiScale, itemRect.right - 10.0f, itemY + 44.0f * m_dpiScale);
                ctx->DrawTextW(
                    item.subtitle.c_str(),
                    static_cast<UINT32>(item.subtitle.length()),
                    m_resultSubFormat.Get(),
                    subRect,
                    (i == m_selectedIndex) ? m_brushTextPrimary.Get() : m_brushTextSecondary.Get()
                );

                itemY += itemH + (2.0f * m_dpiScale);
            }
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK Spotlight::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Spotlight* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<Spotlight*>(cs->lpCreateParams);
            if (pThis) {
                pThis->m_hwnd = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        } else {
            pThis = reinterpret_cast<Spotlight*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT Spotlight::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            m_showCursor = !m_showCursor;
            Render();
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                Hide();
            }
            return 0;

        case WM_CHAR: {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch >= 32) {
                m_query += ch;
                m_selectedIndex = 0;
                UpdateSearchResults();
                Render();
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            switch (wParam) {
            case VK_BACK:
                if (!m_query.empty()) {
                    m_query.pop_back();
                    m_selectedIndex = 0;
                    UpdateSearchResults();
                    Render();
                }
                return 0;

            case VK_ESCAPE:
                Hide();
                return 0;

            case VK_UP:
                if (m_selectedIndex > 0) {
                    m_selectedIndex--;
                    Render();
                }
                return 0;

            case VK_DOWN:
                if (m_selectedIndex < static_cast<int>(m_results.size()) - 1) {
                    m_selectedIndex++;
                    Render();
                }
                return 0;

            case VK_RETURN:
                ExecuteSelected();
                return 0;
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            float my = static_cast<float>(GET_Y_LPARAM(lParam));
            if (my > static_cast<float>(m_baseHeight)) {
                float relY = my - static_cast<float>(m_baseHeight) - (6.0f * m_dpiScale);
                int idx = static_cast<int>(relY / (50.0f * m_dpiScale));
                if (idx >= 0 && idx < static_cast<int>(m_results.size())) {
                    m_selectedIndex = idx;
                    ExecuteSelected();
                }
            }
            return 0;
        }

        case WM_DESTROY:
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

