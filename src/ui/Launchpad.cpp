#define NOMINMAX
#include "Launchpad.h"
#include "IconManager.h"
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

namespace Aqua {

    const wchar_t LAUNCHPAD_CLASS[] = L"AquaLaunchpadWindow";

    Launchpad::Launchpad() = default;

    Launchpad::~Launchpad() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool Launchpad::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = Launchpad::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = LAUNCHPAD_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            LAUNCHPAD_CLASS,
            L"AquaOS Launchpad",
            WS_POPUP,
            0, 0, m_screenWidth, m_screenHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);

        if (!m_surface.Create(m_hwnd, m_screenWidth, m_screenHeight)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // 1. Label text format (underneath icons)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatLabel
        );
        if (!m_formatLabel) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_MEDIUM,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f * m_dpiScale,
                L"en-US",
                &m_formatLabel
            );
        }
        if (m_formatLabel) {
            m_formatLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            m_formatLabel->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        }

        // 2. Search text format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.5f * m_dpiScale,
            L"en-US",
            &m_formatSearch
        );
        if (!m_formatSearch) {
            dwrite->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                13.5f * m_dpiScale,
                L"en-US",
                &m_formatSearch
            );
        }
        if (m_formatSearch) {
            m_formatSearch->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_formatSearch->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 3. Search placeholder format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.5f * m_dpiScale,
            L"en-US",
            &m_formatSearchPlaceholder
        );
        if (m_formatSearchPlaceholder) {
            m_formatSearchPlaceholder->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_formatSearchPlaceholder->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 4. Page hint format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatPageHint
        );
        if (m_formatPageHint) {
            m_formatPageHint->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatPageHint->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Create Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushGlassBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.14f, 0.18f, 0.70f), &m_brushSearchBarBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.09f), &m_brushTileHover);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_brushDotActive);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.35f), &m_brushDotInactive);

        // Initial scan of applications
        ScanInstalledApplications();
        UpdateFilter();

        return true;
    }

    static void ScanDirectoryForShortcuts(const fs::path& dir, std::vector<LaunchpadApp>& apps) {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) continue;
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if (ext == L".lnk") {
                    std::wstring filename = entry.path().stem().wstring();
                    std::wstring lower = filename;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

                    // Filter out non-app entries
                    if (lower.find(L"uninstall") != std::wstring::npos ||
                        lower.find(L"remove") != std::wstring::npos ||
                        lower.find(L"setup") != std::wstring::npos ||
                        lower.find(L"readme") != std::wstring::npos ||
                        lower.find(L"help") != std::wstring::npos ||
                        lower.find(L"documentation") != std::wstring::npos ||
                        lower.find(L"manual") != std::wstring::npos ||
                        lower.find(L"license") != std::wstring::npos) {
                        continue;
                    }

                    // Deduplicate
                    bool exists = false;
                    for (const auto& app : apps) {
                        if (_wcsicmp(app.name.c_str(), filename.c_str()) == 0) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                        LaunchpadApp item;
                        item.name = filename;
                        item.path = entry.path().wstring();
                        apps.push_back(item);
                    }
                }
            }
        }
    }

    void Launchpad::ScanInstalledApplications() {
        m_allApps.clear();

        // 1. Common ProgramData Start Menu
        wchar_t commonPath[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPath) == S_OK) {
            ScanDirectoryForShortcuts(commonPath, m_allApps);
        }

        // 2. User AppData Start Menu
        wchar_t userPath[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPath) == S_OK) {
            ScanDirectoryForShortcuts(userPath, m_allApps);
        }

        // 3. Essential Windows Utilities fallback (if not already included)
        struct SysTool { const wchar_t* name; const wchar_t* exec; };
        SysTool sysTools[] = {
            { L"Terminal", L"wt.exe" },
            { L"Command Prompt", L"cmd.exe" },
            { L"PowerShell", L"powershell.exe" },
            { L"File Explorer", L"explorer.exe" },
            { L"Task Manager", L"taskmgr.exe" },
            { L"Settings", L"ms-settings:" },
            { L"Calculator", L"calc.exe" },
            { L"Notepad", L"notepad.exe" },
            { L"Paint", L"mspaint.exe" },
            { L"Microsoft Edge", L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe" }
        };

        for (const auto& tool : sysTools) {
            bool found = false;
            for (const auto& app : m_allApps) {
                if (_wcsicmp(app.name.c_str(), tool.name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LaunchpadApp item;
                item.name = tool.name;
                item.path = tool.exec;
                m_allApps.push_back(item);
            }
        }

        // Sort alphabetically
        std::sort(m_allApps.begin(), m_allApps.end(), [](const LaunchpadApp& a, const LaunchpadApp& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });
    }

    void Launchpad::UpdateFilter() {
        m_filteredIndices.clear();

        std::wstring query = m_searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::towlower);

        for (size_t i = 0; i < m_allApps.size(); ++i) {
            if (query.empty()) {
                m_filteredIndices.push_back(i);
            } else {
                std::wstring nameLower = m_allApps[i].name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
                if (nameLower.find(query) != std::wstring::npos) {
                    m_filteredIndices.push_back(i);
                }
            }
        }

        m_totalPages = std::max(1, (static_cast<int>(m_filteredIndices.size()) + m_itemsPerPage - 1) / m_itemsPerPage);
        m_currentPage = std::clamp(m_currentPage, 0, m_totalPages - 1);

        CalculateLayout();
    }

    void Launchpad::CalculateLayout() {
        float cx = static_cast<float>(m_screenWidth) * 0.5f;

        // Search Bar Rect (top centered)
        float searchW = 320.0f * m_dpiScale;
        float searchH = 36.0f * m_dpiScale;
        float searchY = 55.0f * m_dpiScale;
        m_searchBarRect = D2D1::RectF(cx - searchW * 0.5f, searchY, cx + searchW * 0.5f, searchY + searchH);

        // Grid Area (7 cols x 5 rows)
        int cols = 7;
        int rows = 5;

        float maxGridW = 1260.0f * m_dpiScale;
        float gridW = std::min(maxGridW, static_cast<float>(m_screenWidth) - (140.0f * m_dpiScale));
        float gridStartX = cx - (gridW * 0.5f);

        float gridStartY = 120.0f * m_dpiScale;
        float maxGridH = static_cast<float>(m_screenHeight) - gridStartY - (100.0f * m_dpiScale);
        float gridH = std::min(maxGridH, 650.0f * m_dpiScale);

        float cellW = gridW / static_cast<float>(cols);
        float cellH = gridH / static_cast<float>(rows);

        float iconSize = 60.0f * m_dpiScale;

        // Layout items on current page
        int startIdx = m_currentPage * m_itemsPerPage;
        int endIdx = std::min(static_cast<int>(m_filteredIndices.size()), startIdx + m_itemsPerPage);

        for (int i = startIdx; i < endIdx; ++i) {
            size_t appIdx = m_filteredIndices[i];
            auto& app = m_allApps[appIdx];

            int pageSlot = i - startIdx;
            int c = pageSlot % cols;
            int r = pageSlot / cols;

            float cellX = gridStartX + c * cellW;
            float cellY = gridStartY + r * cellH;

            app.tileRect = D2D1::RectF(cellX + 6.0f, cellY + 4.0f, cellX + cellW - 6.0f, cellY + cellH - 4.0f);

            float iconX = cellX + (cellW - iconSize) * 0.5f;
            float iconY = cellY + 12.0f * m_dpiScale;
            app.iconRect = D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize);

            float labelTop = iconY + iconSize + (8.0f * m_dpiScale);
            float labelBottom = cellY + cellH - (6.0f * m_dpiScale);
            app.labelRect = D2D1::RectF(cellX + 4.0f, labelTop, cellX + cellW - 4.0f, labelBottom);
        }
    }

    void Launchpad::Show() {
        if (m_visible) return;

        m_visible = true;
        m_searchQuery.clear();
        m_currentPage = 0;
        m_hoveredIndex = -1;

        // Cover active monitor under cursor
        POINT pt;
        GetCursorPos(&pt);
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
            mi.rcMonitor = { 0, 0, m_screenWidth, m_screenHeight };
        }

        int monW = mi.rcMonitor.right - mi.rcMonitor.left;
        int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        if (monW != m_screenWidth || monH != m_screenHeight) {
            m_screenWidth = monW;
            m_screenHeight = monH;
            m_surface.Resize(m_screenWidth, m_screenHeight);
        }

        SetWindowPos(m_hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, m_screenWidth, m_screenHeight, SWP_SHOWWINDOW);

        UpdateFilter();

        SetForegroundWindow(m_hwnd);
        SetActiveWindow(m_hwnd);

        Render();
    }

    void Launchpad::Hide() {
        if (!m_visible) return;

        m_visible = false;
        m_searchQuery.clear();
        m_hoveredIndex = -1;

        ShowWindow(m_hwnd, SW_HIDE);
    }

    void Launchpad::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    void Launchpad::LaunchApp(const LaunchpadApp& app) {
        if (app.path.empty()) return;
        Hide();
        ShellExecuteW(nullptr, L"open", app.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void Launchpad::Render() {
        if (!m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Frosted Dark Acrylic Backdrop
        ctx->Clear(D2D1::ColorF(0.05f, 0.05f, 0.07f, 0.90f));

        float cx = static_cast<float>(m_screenWidth) * 0.5f;

        // 2. Search Bar (macOS Sonoma / Sequoia Pill)
        float searchR = (m_searchBarRect.bottom - m_searchBarRect.top) * 0.5f;
        D2D1_ROUNDED_RECT searchBox = D2D1::RoundedRect(m_searchBarRect, searchR, searchR);

        ctx->FillRoundedRectangle(searchBox, m_brushSearchBarBg.Get());
        ctx->DrawRoundedRectangle(searchBox, m_brushGlassBorder.Get(), 1.0f);

        // Magnifying glass icon inside search bar
        float iconCx = m_searchBarRect.left + (18.0f * m_dpiScale);
        float iconCy = (m_searchBarRect.top + m_searchBarRect.bottom) * 0.5f;
        float rGlass = 5.5f * m_dpiScale;
        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(iconCx, iconCy - 1.0f), rGlass, rGlass), m_brushTextSecondary.Get(), 1.6f * m_dpiScale);
        ctx->DrawLine(
            D2D1::Point2F(iconCx + rGlass * 0.7f, iconCy - 1.0f + rGlass * 0.7f),
            D2D1::Point2F(iconCx + rGlass * 1.5f, iconCy - 1.0f + rGlass * 1.5f),
            m_brushTextSecondary.Get(), 1.6f * m_dpiScale
        );

        // Search text / Placeholder
        D2D1_RECT_F textRect = D2D1::RectF(
            iconCx + (16.0f * m_dpiScale),
            m_searchBarRect.top,
            m_searchBarRect.right - (14.0f * m_dpiScale),
            m_searchBarRect.bottom
        );

        if (m_searchQuery.empty()) {
            if (m_formatSearchPlaceholder) {
                ctx->DrawTextW(L"Search", 6, m_formatSearchPlaceholder.Get(), textRect, m_brushTextSecondary.Get());
            }
        } else {
            if (m_formatSearch) {
                ctx->DrawTextW(m_searchQuery.c_str(), static_cast<UINT32>(m_searchQuery.length()), m_formatSearch.Get(), textRect, m_brushTextPrimary.Get());
            }
        }

        // 3. Render Application Grid on Current Page
        int startIdx = m_currentPage * m_itemsPerPage;
        int endIdx = std::min(static_cast<int>(m_filteredIndices.size()), startIdx + m_itemsPerPage);

        if (m_filteredIndices.empty()) {
            // No matching applications
            if (m_formatPageHint) {
                float hintY = static_cast<float>(m_screenHeight) * 0.45f;
                D2D1_RECT_F emptyRect = D2D1::RectF(cx - 300.0f, hintY, cx + 300.0f, hintY + 40.0f);
                ctx->DrawTextW(L"No Matching Applications Found", 30, m_formatPageHint.Get(), emptyRect, m_brushTextSecondary.Get());
            }
        } else {
            for (int i = startIdx; i < endIdx; ++i) {
                size_t appIdx = m_filteredIndices[i];
                auto& app = m_allApps[appIdx];

                // Hover highlight tile box
                if (app.isHovered) {
                    D2D1_ROUNDED_RECT tileBox = D2D1::RoundedRect(app.tileRect, 14.0f * m_dpiScale, 14.0f * m_dpiScale);
                    ctx->FillRoundedRectangle(tileBox, m_brushTileHover.Get());
                    ctx->DrawRoundedRectangle(tileBox, m_brushGlassBorder.Get(), 1.0f);
                }

                // Lazy icon extraction
                if (!app.icon) {
                    app.icon = IconManager::Instance().GetIconForPath(ctx, app.path, 64);
                }

                // Draw App Icon
                D2D1_RECT_F curIconRect = app.iconRect;
                if (app.isHovered) {
                    float inflate = 3.0f * m_dpiScale;
                    curIconRect.left -= inflate;
                    curIconRect.right += inflate;
                    curIconRect.top -= inflate;
                    curIconRect.bottom += inflate;
                }

                if (app.icon) {
                    ctx->DrawBitmap(app.icon.Get(), curIconRect);
                } else {
                    // Fallback letter tile
                    ComPtr<ID2D1SolidColorBrush> tileBrush;
                    ctx->CreateSolidColorBrush(Colors::AccentBlue, &tileBrush);
                    D2D1_ROUNDED_RECT fallbackBox = D2D1::RoundedRect(curIconRect, 12.0f * m_dpiScale, 12.0f * m_dpiScale);
                    ctx->FillRoundedRectangle(fallbackBox, tileBrush.Get());

                    if (!app.name.empty() && m_formatLabel) {
                        ctx->DrawTextW(app.name.substr(0, 1).c_str(), 1, m_formatLabel.Get(), curIconRect, m_brushTextPrimary.Get());
                    }
                }

                // Draw App Name
                if (m_formatLabel) {
                    ctx->DrawTextW(app.name.c_str(), static_cast<UINT32>(app.name.length()), m_formatLabel.Get(), app.labelRect, m_brushTextPrimary.Get());
                }
            }
        }

        // 4. Pagination Dots (if multi-page)
        if (m_totalPages > 1) {
            float dotSpacing = 16.0f * m_dpiScale;
            float totalDotsW = static_cast<float>(m_totalPages - 1) * dotSpacing;
            float dotStartX = cx - (totalDotsW * 0.5f);
            float dotY = static_cast<float>(m_screenHeight) - (48.0f * m_dpiScale);

            for (int p = 0; p < m_totalPages; ++p) {
                float dx = dotStartX + p * dotSpacing;
                if (p == m_currentPage) {
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dx, dotY), 4.5f * m_dpiScale, 4.5f * m_dpiScale), m_brushDotActive.Get());
                } else {
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dx, dotY), 3.5f * m_dpiScale, 3.5f * m_dpiScale), m_brushDotInactive.Get());
                }
            }
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK Launchpad::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Launchpad* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<Launchpad*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            pThis->m_hwnd = hwnd;
        } else {
            pThis = reinterpret_cast<Launchpad*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT Launchpad::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_MOUSEMOVE: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            int startIdx = m_currentPage * m_itemsPerPage;
            int endIdx = std::min(static_cast<int>(m_filteredIndices.size()), startIdx + m_itemsPerPage);

            int newHovered = -1;
            bool needRender = false;

            for (int i = startIdx; i < endIdx; ++i) {
                size_t appIdx = m_filteredIndices[i];
                auto& app = m_allApps[appIdx];

                bool hovered = (mx >= app.tileRect.left && mx <= app.tileRect.right &&
                                my >= app.tileRect.top && my <= app.tileRect.bottom);

                if (app.isHovered != hovered) {
                    app.isHovered = hovered;
                    needRender = true;
                }

                if (hovered) {
                    newHovered = i;
                }
            }

            if (m_hoveredIndex != newHovered) {
                m_hoveredIndex = newHovered;
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
            int startIdx = m_currentPage * m_itemsPerPage;
            int endIdx = std::min(static_cast<int>(m_filteredIndices.size()), startIdx + m_itemsPerPage);

            bool needRender = false;
            for (int i = startIdx; i < endIdx; ++i) {
                size_t appIdx = m_filteredIndices[i];
                if (m_allApps[appIdx].isHovered) {
                    m_allApps[appIdx].isHovered = false;
                    needRender = true;
                }
            }
            m_hoveredIndex = -1;
            if (needRender) {
                Render();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            // 1. Check if clicked an app tile
            int startIdx = m_currentPage * m_itemsPerPage;
            int endIdx = std::min(static_cast<int>(m_filteredIndices.size()), startIdx + m_itemsPerPage);

            for (int i = startIdx; i < endIdx; ++i) {
                size_t appIdx = m_filteredIndices[i];
                const auto& app = m_allApps[appIdx];
                if (mx >= app.tileRect.left && mx <= app.tileRect.right &&
                    my >= app.tileRect.top && my <= app.tileRect.bottom) {
                    LaunchApp(app);
                    return 0;
                }
            }

            // 2. Check if clicked a pagination dot
            if (m_totalPages > 1) {
                float cx = static_cast<float>(m_screenWidth) * 0.5f;
                float dotSpacing = 16.0f * m_dpiScale;
                float totalDotsW = static_cast<float>(m_totalPages - 1) * dotSpacing;
                float dotStartX = cx - (totalDotsW * 0.5f);
                float dotY = static_cast<float>(m_screenHeight) - (48.0f * m_dpiScale);

                for (int p = 0; p < m_totalPages; ++p) {
                    float dx = dotStartX + p * dotSpacing;
                    float dist = std::hypot(mx - dx, my - dotY);
                    if (dist <= 12.0f * m_dpiScale) {
                        m_currentPage = p;
                        CalculateLayout();
                        Render();
                        return 0;
                    }
                }
            }

            // 3. Clicked empty space outside search bar -> dismiss
            if (!(mx >= m_searchBarRect.left && mx <= m_searchBarRect.right &&
                  my >= m_searchBarRect.top && my <= m_searchBarRect.bottom)) {
                Hide();
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta < 0 && m_currentPage < m_totalPages - 1) {
                m_currentPage++;
                CalculateLayout();
                Render();
            } else if (delta > 0 && m_currentPage > 0) {
                m_currentPage--;
                CalculateLayout();
                Render();
            }
            return 0;
        }

        case WM_CHAR: {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch >= 32 && ch != 127) {
                m_searchQuery += ch;
                m_currentPage = 0;
                UpdateFilter();
                Render();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_BACK) {
                if (!m_searchQuery.empty()) {
                    m_searchQuery.pop_back();
                    m_currentPage = 0;
                    UpdateFilter();
                    Render();
                }
                return 0;
            } else if (wParam == VK_ESCAPE || wParam == VK_F4) {
                if (!m_searchQuery.empty() && wParam == VK_ESCAPE) {
                    m_searchQuery.clear();
                    m_currentPage = 0;
                    UpdateFilter();
                    Render();
                } else {
                    Hide();
                }
                return 0;
            } else if (wParam == VK_RETURN) {
                if (!m_filteredIndices.empty()) {
                    LaunchApp(m_allApps[m_filteredIndices[0]]);
                }
                return 0;
            } else if (wParam == VK_LEFT) {
                if (m_currentPage > 0) {
                    m_currentPage--;
                    CalculateLayout();
                    Render();
                }
                return 0;
            } else if (wParam == VK_RIGHT) {
                if (m_currentPage < m_totalPages - 1) {
                    m_currentPage++;
                    CalculateLayout();
                    Render();
                }
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

