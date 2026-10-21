#include "Dock.h"
#include "IconManager.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <commdlg.h>

namespace Aqua {

    const wchar_t DOCK_CLASS_NAME[] = L"AquaDockWindow";

    Dock::Dock() {
        LoadConfiguration();
        if (m_items.empty()) {
            m_items = {
                { DockAppType::Finder,          L"Finder",            L"explorer.exe",       L"", L"", true,  1.0f, 1.0f },
                { DockAppType::Launchpad,       L"Launchpad",         L"",                   L"", L"", false, 1.0f, 1.0f },
                { DockAppType::Browser,         L"Browser",           L"https://google.com", L"", L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe", false, 1.0f, 1.0f },
                { DockAppType::Terminal,        L"Terminal",          L"wt.exe",             L"", L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\wt.exe", true, 1.0f, 1.0f },
                { DockAppType::CodeEditor,      L"Visual Studio Code",L"code.exe",           L"", L"%LOCALAPPDATA%\\Programs\\Microsoft VS Code\\Code.exe", false, 1.0f, 1.0f },
                { DockAppType::Explorer,        L"Files",             L"explorer.exe",       L"", L"C:\\Windows\\explorer.exe", false, 1.0f, 1.0f },
                { DockAppType::Calculator,      L"Calculator",        L"calc.exe",           L"", L"C:\\Windows\\System32\\calc.exe", false, 1.0f, 1.0f },
                { DockAppType::ActivityMonitor, L"Activity Monitor",  L"taskmgr.exe",        L"", L"C:\\Windows\\System32\\Taskmgr.exe", false, 1.0f, 1.0f },
                { DockAppType::Settings,        L"System Settings",   L"ms-settings:",       L"", L"", false, 1.0f, 1.0f },
                { DockAppType::Separator,       L"",                  L"",                   L"", L"", false, 1.0f, 1.0f },
                { DockAppType::Trash,           L"Trash",             L"explorer.exe",       L"shell:RecycleBinFolder", L"", false, 1.0f, 1.0f }
            };
            SaveConfiguration();
        }
    }

    Dock::~Dock() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool Dock::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Window size accommodates magnified dock + tooltips
        m_windowWidth = 840;
        m_windowHeight = 120;
        m_windowX = (m_screenWidth - m_windowWidth) / 2;
        m_windowY = m_screenHeight - m_windowHeight - 6;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = Dock::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = DOCK_CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            DOCK_CLASS_NAME,
            L"AquaOS Dock",
            WS_POPUP,
            m_windowX, m_windowY, m_windowWidth, m_windowHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_baseIconSize = 46.0f * m_dpiScale;
        m_iconSpacing = 10.0f * m_dpiScale;
        m_magnificationRadius = 110.0f * m_dpiScale;

        if (!m_surface.Create(m_hwnd, m_windowWidth, m_windowHeight)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // Tooltip Font Format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_tooltipFormat
        );
        if (!m_tooltipFormat) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f * m_dpiScale,
                L"en-US",
                &m_tooltipFormat
            );
        }

        if (m_tooltipFormat) {
            m_tooltipFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tooltipFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::GlassBackground, &m_brushGlassBg);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushGlassBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.92f), &m_brushTooltipBg);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::RunningDot, &m_brushDot);
        ctx->CreateSolidColorBrush(Colors::GlassSeparator, &m_brushSeparator);

        CalculateLayout();

        // 60-120Hz Animation Timer (16ms)
        SetTimer(m_hwnd, 2001, 16, nullptr);

        return true;
    }

    void Dock::Show() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
            Render();
        }
    }

    void Dock::Hide() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    void Dock::CalculateLayout() {
        // Calculate total content width
        float totalWidth = 0.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].type == DockAppType::Separator) {
                totalWidth += 16.0f * m_dpiScale;
            } else {
                totalWidth += m_items[i].currentSize;
            }
            if (i < m_items.size() - 1) {
                totalWidth += m_iconSpacing;
            }
        }

        float startX = (static_cast<float>(m_windowWidth) - totalWidth) * 0.5f;
        float baseY = static_cast<float>(m_windowHeight) - (14.0f * m_dpiScale);

        float currentX = startX;
        for (auto& item : m_items) {
            if (item.type == DockAppType::Separator) {
                float sepW = 16.0f * m_dpiScale;
                item.currentX = currentX;
                item.currentY = baseY - (m_baseIconSize * 0.7f);
                item.currentSize = sepW;
                item.bounds = D2D1::RectF(currentX, item.currentY, currentX + sepW, baseY);
                currentX += sepW + m_iconSpacing;
            } else {
                float size = item.currentSize;
                float y = baseY - size;

                // Handle bounce animation
                if (item.isBouncing) {
                    float bounceOffset = 18.0f * std::sin(item.bounceTime * 8.0f) * std::exp(-item.bounceTime * 1.5f);
                    y -= std::abs(bounceOffset);
                }

                item.currentX = currentX;
                item.currentY = y;
                item.bounds = D2D1::RectF(currentX, y, currentX + size, y + size);
                currentX += size + m_iconSpacing;
            }
        }
    }

    void Dock::UpdatePhysics() {
        bool animated = false;

        for (size_t i = 0; i < m_items.size(); ++i) {
            auto& item = m_items[i];
            if (item.type == DockAppType::Separator) continue;

            if (m_isHovered) {
                float itemCenterX = item.currentX + (item.currentSize * 0.5f);
                float dist = std::abs(m_mouseWindowX - itemCenterX);

                if (dist < m_magnificationRadius) {
                    float factor = std::cos((dist / m_magnificationRadius) * 1.5707963f);
                    item.targetScale = 1.0f + ((m_maxScale - 1.0f) * factor);
                } else {
                    item.targetScale = 1.0f;
                }
            } else {
                item.targetScale = 1.0f;
            }

            // Smooth interpolation (spring-like response)
            float prevScale = item.currentScale;
            item.currentScale += (item.targetScale - item.currentScale) * 0.32f;
            if (std::abs(item.currentScale - item.targetScale) < 0.001f) {
                item.currentScale = item.targetScale;
            } else {
                animated = true;
            }

            item.currentSize = m_baseIconSize * item.currentScale;

            // Update bounce physics
            if (item.isBouncing) {
                item.bounceTime += 0.04f;
                if (item.bounceTime > 1.8f) {
                    item.isBouncing = false;
                    item.bounceTime = 0.0f;
                }
                animated = true;
            }
        }

        CalculateLayout();

        // Determine currently hovered item
        m_hoveredIndex = -1;
        if (m_isHovered) {
            for (size_t i = 0; i < m_items.size(); ++i) {
                const auto& b = m_items[i].bounds;
                if (m_mouseWindowX >= b.left && m_mouseWindowX <= b.right &&
                    m_mouseWindowY >= b.top - 10.0f && m_mouseWindowY <= b.bottom + 10.0f) {
                    m_hoveredIndex = static_cast<int>(i);
                    break;
                }
            }
        }

        Render();
    }

    void Dock::DrawIcon(ID2D1DeviceContext* ctx, const DockItem& item, float x, float y, float size) {
        float r = size * 0.22f;
        D2D1_ROUNDED_RECT iconBox = D2D1::RoundedRect(D2D1::RectF(x, y, x + size, y + size), r, r);

        // 1. If item has a real Windows icon path, extract and draw high-res icon
        if (!item.iconPath.empty()) {
            auto bmp = IconManager::Instance().GetIconForPath(ctx, item.iconPath, static_cast<int>(size * m_dpiScale));
            if (bmp) {
                ctx->DrawBitmap(bmp.Get(), D2D1::RectF(x, y, x + size, y + size));
                return;
            }
        }

        // 2. Custom App Fallback tile
        if (item.type == DockAppType::CustomApp) {
            ComPtr<ID2D1SolidColorBrush> brush;
            ctx->CreateSolidColorBrush(Colors::AccentBlue, &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());
            if (!item.name.empty()) {
                D2D1_RECT_F tr = D2D1::RectF(x, y, x + size, y + size);
                ctx->DrawTextW(item.name.substr(0, 1).c_str(), 1, m_tooltipFormat.Get(), tr, m_brushTextPrimary.Get());
            }
            return;
        }

        ComPtr<ID2D1SolidColorBrush> brush;

        switch (item.type) {
        case DockAppType::Finder: {
            // macOS Finder Face (Two-tone blue)
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.58f, 0.95f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            // Right half darker blue
            D2D1_ROUNDED_RECT rightHalf = D2D1::RoundedRect(D2D1::RectF(x + size * 0.5f, y, x + size, y + size), r, r);
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.44f, 0.82f), &brush);
            ctx->FillRoundedRectangle(rightHalf, brush.Get());

            // Center smile line & eyes
            ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            // Eyes
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - size * 0.2f, cy - size * 0.12f), size * 0.05f, size * 0.07f), brush.Get());
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + size * 0.2f, cy - size * 0.12f), size * 0.05f, size * 0.07f), brush.Get());
            // Smile
            D2D1_ELLIPSE smile = D2D1::Ellipse(D2D1::Point2F(cx, cy + size * 0.12f), size * 0.22f, size * 0.12f);
            ctx->DrawEllipse(smile, brush.Get(), size * 0.045f);
            break;
        }

        case DockAppType::Launchpad: {
            // Rocket / 3x3 App Grid
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.25f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            // 3x3 colorful circles
            float step = size * 0.22f;
            float startDotX = x + size * 0.28f;
            float startDotY = y + size * 0.28f;
            D2D1_COLOR_F dotColors[9] = {
                Colors::AccentRed, Colors::AccentOrange, Colors::AccentGreen,
                Colors::AccentBlue, Colors::AccentPurple, Colors::AccentBlue,
                Colors::AccentOrange, Colors::AccentGreen, Colors::AccentRed
            };

            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    ctx->CreateSolidColorBrush(dotColors[row * 3 + col], &brush);
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startDotX + col * step, startDotY + row * step), size * 0.065f, size * 0.065f), brush.Get());
                }
            }
            break;
        }

        case DockAppType::Browser: {
            // Safari Compass
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.65f, 1.0f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            float compassR = size * 0.36f;

            // Dial white circle
            ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &brush);
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), compassR, compassR), brush.Get(), size * 0.04f);

            // Red & White needle
            ctx->CreateSolidColorBrush(Colors::AccentRed, &brush);
            D2D1_POINT_2F redNeedle[3] = {
                D2D1::Point2F(cx, cy),
                D2D1::Point2F(cx - size * 0.06f, cy - size * 0.06f),
                D2D1::Point2F(cx + size * 0.25f, cy - size * 0.25f)
            };
            // Needle center pin
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.05f, size * 0.05f), brush.Get());
            break;
        }

        case DockAppType::Terminal: {
            // Dark Terminal Prompt
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.12f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            // Border highlight
            ctx->CreateSolidColorBrush(Colors::GlassBorder, &brush);
            ctx->DrawRoundedRectangle(iconBox, brush.Get(), 1.0f);

            // Green '>_' text
            ctx->CreateSolidColorBrush(Colors::AccentGreen, &brush);
            float cx = x + size * 0.22f;
            float cy = y + size * 0.38f;
            // Draw '>'
            ctx->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx + size * 0.18f, cy + size * 0.12f), brush.Get(), size * 0.05f);
            ctx->DrawLine(D2D1::Point2F(cx + size * 0.18f, cy + size * 0.12f), D2D1::Point2F(cx, cy + size * 0.24f), brush.Get(), size * 0.05f);
            // Draw '_'
            ctx->DrawLine(D2D1::Point2F(cx + size * 0.26f, cy + size * 0.24f), D2D1::Point2F(cx + size * 0.44f, cy + size * 0.24f), brush.Get(), size * 0.05f);
            break;
        }

        case DockAppType::CodeEditor: {
            // Code braces
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.45f, 0.85f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            // Draw '{ }'
            D2D1_ELLIPSE ring = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.24f, size * 0.24f);
            ctx->DrawEllipse(ring, brush.Get(), size * 0.045f);
            break;
        }

        case DockAppType::Explorer: {
            // Blue macOS Folder
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.65f, 0.95f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            // Folder tab
            D2D1_ROUNDED_RECT tab = D2D1::RoundedRect(D2D1::RectF(x + size * 0.15f, y + size * 0.22f, x + size * 0.5f, y + size * 0.45f), 2.0f, 2.0f);
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.52f, 0.85f), &brush);
            ctx->FillRoundedRectangle(tab, brush.Get());
            break;
        }

        case DockAppType::Calculator: {
            // Orange calculator
            ctx->CreateSolidColorBrush(Colors::AccentOrange, &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            ctx->CreateSolidColorBrush(Colors::TextPrimary, &brush);
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            // Plus / Minus symbol
            ctx->DrawLine(D2D1::Point2F(cx - size * 0.16f, cy), D2D1::Point2F(cx + size * 0.16f, cy), brush.Get(), size * 0.045f);
            ctx->DrawLine(D2D1::Point2F(cx, cy - size * 0.16f), D2D1::Point2F(cx, cy + size * 0.16f), brush.Get(), size * 0.045f);
            break;
        }

        case DockAppType::ActivityMonitor: {
            // Activity ECG Wave
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.14f, 0.16f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            ctx->CreateSolidColorBrush(Colors::AccentGreen, &brush);
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            // Pulse wave
            D2D1_POINT_2F pts[5] = {
                D2D1::Point2F(x + size * 0.18f, cy),
                D2D1::Point2F(cx - size * 0.08f, cy),
                D2D1::Point2F(cx, cy - size * 0.22f),
                D2D1::Point2F(cx + size * 0.08f, cy + size * 0.18f),
                D2D1::Point2F(x + size * 0.82f, cy)
            };
            for (int p = 0; p < 4; ++p) {
                ctx->DrawLine(pts[p], pts[p + 1], brush.Get(), size * 0.045f);
            }
            break;
        }

        case DockAppType::Settings: {
            // Gear icon
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.58f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.35f, 0.38f), &brush);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.20f, size * 0.20f), brush.Get());
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.58f), &brush);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.10f, size * 0.10f), brush.Get());
            break;
        }

        case DockAppType::Trash: {
            // Trash can
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.72f, 0.76f), &brush);
            ctx->FillRoundedRectangle(iconBox, brush.Get());

            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            ctx->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.40f, 0.45f), &brush);
            // Trash basket
            D2D1_ROUNDED_RECT basket = D2D1::RoundedRect(D2D1::RectF(cx - size * 0.16f, cy - size * 0.10f, cx + size * 0.16f, cy + size * 0.22f), 2.0f, 2.0f);
            ctx->DrawRoundedRectangle(basket, brush.Get(), size * 0.04f);
            break;
        }

        default:
            break;
        }
    }

    void Dock::DrawTooltip(ID2D1DeviceContext* ctx, const std::wstring& text, float centerX, float topY) {
        float textLen = static_cast<float>(text.length());
        float tipWidth = textLen * 8.5f * m_dpiScale + 20.0f;
        float tipHeight = 26.0f * m_dpiScale;
        float tipX = centerX - (tipWidth * 0.5f);
        float tipY = topY - tipHeight - 6.0f;

        D2D1_ROUNDED_RECT tipBox = D2D1::RoundedRect(D2D1::RectF(tipX, tipY, tipX + tipWidth, tipY + tipHeight), 6.0f, 6.0f);
        ctx->FillRoundedRectangle(tipBox, m_brushTooltipBg.Get());
        ctx->DrawRoundedRectangle(tipBox, m_brushGlassBorder.Get(), 1.0f);

        ctx->DrawTextW(
            text.c_str(),
            static_cast<UINT32>(text.length()),
            m_tooltipFormat.Get(),
            tipBox.rect,
            m_brushTextPrimary.Get()
        );
    }

    void Dock::Render() {
        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Calculate capsule background dimensions based on icon extents
        float firstX = m_items.front().bounds.left;
        float lastX = m_items.back().bounds.right;
        float pad = 12.0f * m_dpiScale;
        float capsuleH = 64.0f * m_dpiScale;
        float capsuleY = static_cast<float>(m_windowHeight) - capsuleH - (6.0f * m_dpiScale);

        D2D1_ROUNDED_RECT capsule = D2D1::RoundedRect(
            D2D1::RectF(firstX - pad, capsuleY, lastX + pad, capsuleY + capsuleH),
            18.0f * m_dpiScale,
            18.0f * m_dpiScale
        );

        // 2. Draw Frosted Glass Dock Capsule
        ctx->FillRoundedRectangle(capsule, m_brushGlassBg.Get());
        ctx->DrawRoundedRectangle(capsule, m_brushGlassBorder.Get(), 1.2f);

        // 3. Draw Each Dock Item
        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];
            if (item.type == DockAppType::Separator) {
                float sx = item.currentX + item.currentSize * 0.5f;
                ctx->DrawLine(
                    D2D1::Point2F(sx, capsuleY + 12.0f),
                    D2D1::Point2F(sx, capsuleY + capsuleH - 12.0f),
                    m_brushSeparator.Get(),
                    1.5f
                );
            } else {
                DrawIcon(ctx, item, item.currentX, item.currentY, item.currentSize);

                // Running Indicator Dot
                if (item.isRunning) {
                    float dotX = item.currentX + (item.currentSize * 0.5f);
                    float dotY = capsuleY + capsuleH - 5.0f;
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 2.2f, 2.2f), m_brushDot.Get());
                }
            }
        }

        // 4. Draw Floating Tooltip
        if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_items.size())) {
            const auto& item = m_items[m_hoveredIndex];
            if (item.type != DockAppType::Separator && !item.name.empty()) {
                float cx = item.currentX + (item.currentSize * 0.5f);
                DrawTooltip(ctx, item.name, cx, item.currentY);
            }
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK Dock::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Dock* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<Dock*>(cs->lpCreateParams);
            if (pThis) {
                pThis->m_hwnd = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        } else {
            pThis = reinterpret_cast<Dock*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT Dock::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            UpdatePhysics();
            return 0;

        case WM_MOUSEMOVE: {
            m_mouseWindowX = static_cast<float>(GET_X_LPARAM(lParam));
            m_mouseWindowY = static_cast<float>(GET_Y_LPARAM(lParam));
            m_isHovered = true;

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_isHovered = false;
            m_mouseWindowX = -999.0f;
            m_mouseWindowY = -999.0f;
            m_hoveredIndex = -1;
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_items.size())) {
                auto& item = m_items[m_hoveredIndex];
                if (item.type == DockAppType::Separator) return 0;

                // Trigger Bounce
                item.isBouncing = true;
                item.bounceTime = 0.0f;
                item.isRunning = true;

                // Action
                if (item.type == DockAppType::Launchpad) {
                    if (m_onSpotlight) m_onSpotlight();
                } else if (!item.executePath.empty()) {
                    ShellExecuteW(
                        nullptr,
                        L"open",
                        item.executePath.c_str(),
                        item.executeArgs.empty() ? nullptr : item.executeArgs.c_str(),
                        nullptr,
                        SW_SHOWNORMAL
                    );
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(m_hoveredIndex, pt.x, pt.y);
            return 0;
        }

        case WM_DESTROY:
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    static std::wstring GetDockConfigPath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring path(exePath);
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            return path.substr(0, lastSlash) + L"\\dock_items.cfg";
        }
        return L"dock_items.cfg";
    }

    void Dock::SaveConfiguration() {
        std::wofstream out(GetDockConfigPath());
        if (!out.is_open()) return;

        for (const auto& item : m_items) {
            out << static_cast<int>(item.type) << L"|"
                << item.name << L"|"
                << item.executePath << L"|"
                << item.executeArgs << L"|"
                << item.iconPath << L"\n";
        }
    }

    void Dock::LoadConfiguration() {
        std::wifstream in(GetDockConfigPath());
        if (!in.is_open()) return;

        m_items.clear();
        std::wstring line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::wstringstream ss(line);
            std::wstring sType, sName, sExec, sArgs, sIcon;
            if (std::getline(ss, sType, L'|') &&
                std::getline(ss, sName, L'|') &&
                std::getline(ss, sExec, L'|') &&
                std::getline(ss, sArgs, L'|')) {
                std::getline(ss, sIcon, L'|');

                DockItem item;
                item.type = static_cast<DockAppType>(_wtoi(sType.c_str()));
                item.name = sName;
                item.executePath = sExec;
                item.executeArgs = sArgs;
                item.iconPath = sIcon;
                item.isRunning = false;
                item.targetScale = 1.0f;
                item.currentScale = 1.0f;
                m_items.push_back(item);
            }
        }
    }

    void Dock::PinApplication(const std::wstring& filePath) {
        if (filePath.empty()) return;

        size_t lastSlash = filePath.find_last_of(L"\\/");
        std::wstring fileName = (lastSlash != std::wstring::npos) ? filePath.substr(lastSlash + 1) : filePath;
        size_t extPos = fileName.rfind(L'.');
        std::wstring appName = (extPos != std::wstring::npos) ? fileName.substr(0, extPos) : fileName;

        DockItem newItem;
        newItem.type = DockAppType::CustomApp;
        newItem.name = appName;
        newItem.executePath = filePath;
        newItem.executeArgs = L"";
        newItem.iconPath = filePath;
        newItem.isRunning = false;
        newItem.targetScale = 1.0f;
        newItem.currentScale = 1.0f;

        // Insert before Separator if one exists
        auto it = std::find_if(m_items.begin(), m_items.end(), [](const DockItem& itm) {
            return itm.type == DockAppType::Separator;
        });

        if (it != m_items.end()) {
            m_items.insert(it, newItem);
        } else {
            m_items.push_back(newItem);
        }

        CalculateLayout();
        SaveConfiguration();
        Render();
    }

    void Dock::RemoveItem(int index) {
        if (index >= 0 && index < static_cast<int>(m_items.size())) {
            if (m_items[index].type != DockAppType::Finder && m_items[index].type != DockAppType::Trash) {
                m_items.erase(m_items.begin() + index);
                CalculateLayout();
                SaveConfiguration();
                Render();
            }
        }
    }

    void Dock::ShowContextMenu(int itemIndex, int screenX, int screenY) {
        HMENU hMenu = CreatePopupMenu();

        bool hasItem = (itemIndex >= 0 && itemIndex < static_cast<int>(m_items.size()));
        const DockItem* pItem = hasItem ? &m_items[itemIndex] : nullptr;

        if (pItem && pItem->type != DockAppType::Separator) {
            AppendMenuW(hMenu, MF_STRING, 5001, (L"Open " + pItem->name).c_str());
            if (!pItem->executePath.empty() && pItem->executePath.find(L":") != std::wstring::npos) {
                AppendMenuW(hMenu, MF_STRING, 5002, L"Show in File Explorer");
            }
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

            UINT uFlags = MF_STRING;
            if (pItem->type == DockAppType::Finder || pItem->type == DockAppType::Trash) {
                uFlags |= MF_GRAYED;
            }
            AppendMenuW(hMenu, uFlags, 5003, L"Remove from Dock");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }

        AppendMenuW(hMenu, MF_STRING, 5010, L"Pin Application to Dock...");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 5020, TaskbarController::Instance().IsSuppressed() ? L"Show Windows Taskbar\tCtrl+Alt+T" : L"Hide Windows Taskbar\tCtrl+Alt+T");
        AppendMenuW(hMenu, MF_STRING, 5099, L"Exit AquaOS");

        int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, screenX, screenY, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd == 5001 && pItem) {
            if (pItem->type == DockAppType::Launchpad) {
                if (m_onSpotlight) m_onSpotlight();
            } else if (!pItem->executePath.empty()) {
                ShellExecuteW(nullptr, L"open", pItem->executePath.c_str(), pItem->executeArgs.empty() ? nullptr : pItem->executeArgs.c_str(), nullptr, SW_SHOWNORMAL);
            }
        } else if (cmd == 5002 && pItem) {
            ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + pItem->executePath + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
        } else if (cmd == 5003 && hasItem) {
            RemoveItem(itemIndex);
        } else if (cmd == 5010) {
            wchar_t szFile[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
            ofn.hwndOwner = m_hwnd;
            ofn.lpstrFilter = L"Applications & Shortcuts (*.exe;*.lnk)\0*.exe;*.lnk\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrTitle = L"Select Application to Pin to Dock";

            if (GetOpenFileNameW(&ofn)) {
                PinApplication(szFile);
            }
        } else if (cmd == 5020) {
            TaskbarController::Instance().ToggleTaskbar();
        } else if (cmd == 5099) {
            TaskbarController::Instance().RestoreTaskbar();
            PostQuitMessage(0);
        }
    }

}
