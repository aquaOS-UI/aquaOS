#include "StageManager.h"
#include "IconManager.h"
#include <algorithm>
#include <cmath>

namespace Aqua {

    const wchar_t STAGE_MANAGER_CLASS[] = L"AquaStageManagerWindow";

    StageManager::StageManager() = default;

    StageManager::~StageManager() {
        ClearThumbnails();
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool StageManager::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = StageManager::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = STAGE_MANAGER_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_dpiScale = 1.0f; // Initial, updated after window creation
        m_stripWidth = static_cast<int>(170.0f * m_dpiScale);
        m_topBarHeight = 32;
        m_stripHeight = m_screenHeight - m_topBarHeight - 76; // Leave space for TopBar and Dock

        // Create with WS_EX_NOACTIVATE so Stage Manager never steals focus from active apps
        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
            STAGE_MANAGER_CLASS,
            L"AquaOS Stage Manager",
            WS_POPUP,
            -m_stripWidth, m_topBarHeight + 4, m_stripWidth, m_stripHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_stripWidth = static_cast<int>(170.0f * m_dpiScale);
        SetWindowPos(m_hwnd, HWND_TOPMOST, -m_stripWidth, m_topBarHeight + 4, m_stripWidth, m_stripHeight, SWP_NOACTIVATE);

        if (!m_surface.Create(m_hwnd, m_stripWidth, m_stripHeight)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // 1. Text Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
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
                11.0f * m_dpiScale,
                L"en-US",
                &m_formatTitle
            );
        }
        if (m_formatTitle) {
            m_formatTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_formatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Badge Format (Count of windows in a stage)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.0f * m_dpiScale,
            L"en-US",
            &m_formatBadge
        );
        if (m_formatBadge) {
            m_formatBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatBadge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Direct2D Brushes
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.20f), &m_brushStripBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.14f, 0.16f, 0.75f), &m_brushCardBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.20f, 0.24f, 0.88f), &m_brushCardBgHover);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), &m_brushBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 0.85f), &m_brushBorderHover);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 0.90f), &m_brushBadgeBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.50f), &m_brushStackCard);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.85f), &m_brushIconPlate);

        // Timer for tracking active foreground changes
        SetTimer(m_hwnd, TIMER_TRACK, 250, nullptr);

        return true;
    }

    void StageManager::Toggle() {
        if (m_enabled) {
            Hide();
        } else {
            Show();
        }
    }

    void StageManager::Show() {
        m_enabled = true;
        m_animating = true;
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

        RefreshStages();

        SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
    }

    void StageManager::Hide() {
        m_enabled = false;
        m_animating = true;
        m_hoveredIndex = -1;

        SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
    }

    void StageManager::ClearThumbnails() {
        for (auto& stage : m_stages) {
            if (stage.hThumbnail) {
                DwmUnregisterThumbnail(stage.hThumbnail);
                stage.hThumbnail = nullptr;
            }
        }
    }

    void StageManager::RefreshStages() {
        if (!m_hwnd) return;

        m_currentForeground = GetForegroundWindow();

        struct EnumContext {
            HWND stageHwnd;
            HWND fgHwnd;
            std::vector<HWND> hwnds;
        };

        EnumContext ctx;
        ctx.stageHwnd = m_hwnd;
        ctx.fgHwnd = m_currentForeground;

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* pCtx = reinterpret_cast<EnumContext*>(lParam);

            if (!IsWindowVisible(hwnd)) return TRUE;
            if (hwnd == pCtx->stageHwnd) return TRUE;
            if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

            // Filter cloaked windows
            int cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
                if (cloaked != 0) return TRUE;
            }

            // Exclude current foreground window (it lives in Center Stage)
            if (hwnd == pCtx->fgHwnd) return TRUE;

            // Must have non-empty title
            wchar_t title[256] = {};
            int len = GetWindowTextW(hwnd, title, 256);
            if (len <= 0) return TRUE;

            // Class name filters
            wchar_t className[256] = {};
            GetClassNameW(hwnd, className, 256);
            std::wstring cls(className);

            if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" ||
                cls == L"Shell_SecondaryTrayWnd" || cls == L"Button" ||
                cls == L"ApplicationManager_DesktopShellWindow" ||
                cls == L"AquaTopBarWindow" || cls == L"AquaDockWindow" ||
                cls == L"AquaSpotlightWindow" || cls == L"AquaControlCenterWindow" ||
                cls == L"AquaMissionControlWindow" || cls == L"AquaLaunchpadWindow" ||
                cls == L"AquaNotificationCenterWindow" || cls == L"AquaStageManagerWindow" ||
                cls == L"Windows.UI.Core.CoreWindow") {
                return TRUE;
            }

            RECT rc = {};
            GetWindowRect(hwnd, &rc);
            if ((rc.right - rc.left) < 120 || (rc.bottom - rc.top) < 120) return TRUE;

            pCtx->hwnds.push_back(hwnd);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        // Group enumerated windows by Process Image Path
        ClearThumbnails();
        m_stages.clear();

        auto& engine = DCompEngine::Instance();
        auto d2dCtx = engine.GetD2DContext();

        for (HWND hwnd : ctx.hwnds) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);

            std::wstring procPath;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t exePath[MAX_PATH] = {};
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                    procPath = exePath;
                }
                CloseHandle(hProc);
            }

            // Check if group already exists
            auto it = std::find_if(m_stages.begin(), m_stages.end(), [&](const StageGroup& g) {
                return !procPath.empty() && g.processPath == procPath;
            });

            if (it != m_stages.end()) {
                it->hwnds.push_back(hwnd);
            } else {
                if (m_stages.size() >= 6) continue; // Show top 6 inactive stages

                StageGroup stage;
                stage.pid = pid;
                stage.processPath = procPath;
                stage.primaryHwnd = hwnd;
                stage.hwnds.push_back(hwnd);

                wchar_t titleBuf[256] = {};
                GetWindowTextW(hwnd, titleBuf, 256);
                stage.title = titleBuf;

                // Extract app name from process path
                if (!procPath.empty()) {
                    size_t slash = procPath.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) {
                        stage.appName = procPath.substr(slash + 1);
                        size_t dot = stage.appName.find_last_of(L'.');
                        if (dot != std::wstring::npos) {
                            stage.appName = stage.appName.substr(0, dot);
                        }
                    } else {
                        stage.appName = procPath;
                    }

                    // Capitalize first character
                    if (!stage.appName.empty() && stage.appName[0] >= L'a' && stage.appName[0] <= L'z') {
                        stage.appName[0] = stage.appName[0] - L'a' + L'A';
                    }

                    stage.icon = IconManager::Instance().GetIconForPath(d2dCtx, procPath, 32);
                } else {
                    stage.appName = stage.title;
                }

                m_stages.push_back(stage);
            }
        }

        // Register DWM Thumbnails for primary windows
        for (auto& stage : m_stages) {
            if (stage.primaryHwnd && IsWindow(stage.primaryHwnd)) {
                DwmRegisterThumbnail(m_hwnd, stage.primaryHwnd, &stage.hThumbnail);
            }
        }

        CalculateLayout();
        UpdateThumbnails();
        Render();
    }

    void StageManager::CalculateLayout() {
        if (m_stages.empty()) return;

        float cardW = 142.0f * m_dpiScale;
        float cardH = 92.0f * m_dpiScale;
        float gapY = 14.0f * m_dpiScale;
        float padX = 14.0f * m_dpiScale;

        size_t n = m_stages.size();
        float totalCardsH = static_cast<float>(n) * cardH + static_cast<float>(n - 1) * gapY;

        // Vertically center cards in the strip height
        float startY = (static_cast<float>(m_stripHeight) - totalCardsH) * 0.5f;
        startY = std::max(startY, 16.0f * m_dpiScale);

        for (size_t i = 0; i < n; ++i) {
            auto& stage = m_stages[i];

            float slideX = stage.hoverAnim * (14.0f * m_dpiScale);
            float cardX = padX + slideX;
            float cardY = startY + static_cast<float>(i) * (cardH + gapY);

            stage.cardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

            // Thumbnail inside card with 4px padding
            float padThumb = 4.0f * m_dpiScale;
            float maxThumbW = cardW - (padThumb * 2.0f);
            float maxThumbH = cardH - (padThumb * 2.0f);

            // Maintain window aspect ratio
            RECT srcRc = {};
            GetWindowRect(stage.primaryHwnd, &srcRc);
            int srcW = srcRc.right - srcRc.left;
            int srcH = srcRc.bottom - srcRc.top;
            if (srcW <= 0) srcW = 1920;
            if (srcH <= 0) srcH = 1080;

            float scale = std::min(maxThumbW / static_cast<float>(srcW), maxThumbH / static_cast<float>(srcH));
            float thumbW = std::max(20.0f, static_cast<float>(srcW) * scale);
            float thumbH = std::max(20.0f, static_cast<float>(srcH) * scale);

            float thumbX = cardX + (cardW - thumbW) * 0.5f;
            float thumbY = cardY + (cardH - thumbH) * 0.5f;

            stage.thumbRect = D2D1::RectF(thumbX, thumbY, thumbX + thumbW, thumbY + thumbH);
        }
    }

    void StageManager::UpdateThumbnails() {
        for (const auto& stage : m_stages) {
            if (!stage.hThumbnail) continue;

            if (m_slideProgress <= 0.05f) {
                DWM_THUMBNAIL_PROPERTIES props = {};
                props.dwFlags = DWM_TNP_VISIBLE;
                props.fVisible = FALSE;
                DwmUpdateThumbnailProperties(stage.hThumbnail, &props);
                continue;
            }

            DWM_THUMBNAIL_PROPERTIES props = {};
            props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
            props.rcDestination = RECT{
                static_cast<LONG>(stage.thumbRect.left),
                static_cast<LONG>(stage.thumbRect.top),
                static_cast<LONG>(stage.thumbRect.right),
                static_cast<LONG>(stage.thumbRect.bottom)
            };
            props.fVisible = TRUE;
            props.opacity = static_cast<BYTE>(std::clamp(m_slideProgress * 255.0f, 0.0f, 255.0f));

            DwmUpdateThumbnailProperties(stage.hThumbnail, &props);
        }
    }

    void StageManager::ActivateStage(size_t index) {
        if (index >= m_stages.size()) return;

        const auto& stage = m_stages[index];
        HWND targetHwnd = stage.primaryHwnd;
        if (!targetHwnd || !IsWindow(targetHwnd)) return;

        if (IsIconic(targetHwnd)) {
            ShowWindow(targetHwnd, SW_RESTORE);
        }

        // Bring secondary windows of the same stage forward as well
        for (auto it = stage.hwnds.rbegin(); it != stage.hwnds.rend(); ++it) {
            if (*it != targetHwnd && IsWindow(*it)) {
                if (IsIconic(*it)) ShowWindow(*it, SW_RESTORE);
                BringWindowToTop(*it);
            }
        }

        DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        DWORD appThread = GetCurrentThreadId();

        if (foreThread != appThread) {
            AttachThreadInput(foreThread, appThread, TRUE);
            BringWindowToTop(targetHwnd);
            ShowWindow(targetHwnd, SW_SHOW);
            SetForegroundWindow(targetHwnd);
            AttachThreadInput(foreThread, appThread, FALSE);
        } else {
            BringWindowToTop(targetHwnd);
            ShowWindow(targetHwnd, SW_SHOW);
            SetForegroundWindow(targetHwnd);
        }

        // Refresh stages to reflect the new Center Stage window
        RefreshStages();
    }

    void StageManager::PeekDesktop() {
        // macOS Stage Manager feature: clicking empty area peeks desktop
        HWND fg = GetForegroundWindow();
        if (fg && IsWindow(fg) && fg != m_hwnd) {
            ShowWindow(fg, SW_MINIMIZE);
            RefreshStages();
        }
    }

    void StageManager::Render() {
        if (!m_hwnd) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // Clear transparent surface
        ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        float radius = 10.0f * m_dpiScale;

        for (size_t i = 0; i < m_stages.size(); ++i) {
            const auto& stage = m_stages[i];
            bool hovered = (static_cast<int>(i) == m_hoveredIndex);

            // 1. Stacked Silhouette (if app has 2 or more windows)
            if (stage.hwnds.size() > 1) {
                float stackOffset = 4.0f * m_dpiScale;
                D2D1_ROUNDED_RECT stackRect = D2D1::RoundedRect(
                    D2D1::RectF(
                        stage.cardRect.left + stackOffset,
                        stage.cardRect.top - stackOffset,
                        stage.cardRect.right + stackOffset,
                        stage.cardRect.bottom - stackOffset
                    ),
                    radius, radius
                );
                ctx->FillRoundedRectangle(stackRect, m_brushStackCard.Get());
                ctx->DrawRoundedRectangle(stackRect, m_brushBorder.Get(), 1.0f);
            }

            // 2. Card Background & Border
            D2D1_ROUNDED_RECT cardRound = D2D1::RoundedRect(stage.cardRect, radius, radius);
            ctx->FillRoundedRectangle(cardRound, hovered ? m_brushCardBgHover.Get() : m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(cardRound, hovered ? m_brushBorderHover.Get() : m_brushBorder.Get(), hovered ? 1.5f : 1.0f);

            // 3. Circular App Icon Badge (Top-Left of Card)
            float plateR = 13.0f * m_dpiScale;
            float iconX = stage.cardRect.left + 8.0f * m_dpiScale;
            float iconY = stage.cardRect.top + 8.0f * m_dpiScale;

            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(iconX + plateR, iconY + plateR), plateR, plateR), m_brushIconPlate.Get());
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(iconX + plateR, iconY + plateR), plateR, plateR), m_brushBorder.Get(), 1.0f);

            if (stage.icon) {
                float iconSize = 18.0f * m_dpiScale;
                D2D1_RECT_F iconDest = D2D1::RectF(
                    iconX + plateR - (iconSize * 0.5f),
                    iconY + plateR - (iconSize * 0.5f),
                    iconX + plateR + (iconSize * 0.5f),
                    iconY + plateR + (iconSize * 0.5f)
                );
                ctx->DrawBitmap(stage.icon.Get(), iconDest);
            }

            // 4. Window Count Badge (if app has 2 or more windows)
            if (stage.hwnds.size() > 1 && m_formatBadge) {
                float badgeW = 20.0f * m_dpiScale;
                float badgeH = 16.0f * m_dpiScale;
                float badgeX = stage.cardRect.right - badgeW - 6.0f * m_dpiScale;
                float badgeY = stage.cardRect.top + 6.0f * m_dpiScale;

                D2D1_ROUNDED_RECT badgeRound = D2D1::RoundedRect(
                    D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH),
                    badgeH * 0.5f, badgeH * 0.5f
                );
                ctx->FillRoundedRectangle(badgeRound, m_brushBadgeBg.Get());

                std::wstring countStr = std::to_wstring(stage.hwnds.size());
                ctx->DrawTextW(countStr.c_str(), static_cast<UINT32>(countStr.size()), m_formatBadge.Get(), badgeRound.rect, m_brushTextPrimary.Get());
            }

            // 5. App / Window Title Label (Shown when hovered or compact title at bottom)
            if (hovered && m_formatTitle) {
                float labelH = 18.0f * m_dpiScale;
                D2D1_RECT_F labelRect = D2D1::RectF(
                    stage.cardRect.left + 4.0f * m_dpiScale,
                    stage.cardRect.bottom - labelH - 2.0f * m_dpiScale,
                    stage.cardRect.right - 4.0f * m_dpiScale,
                    stage.cardRect.bottom - 2.0f * m_dpiScale
                );

                // Frosted text backing pill
                D2D1_ROUNDED_RECT labelPill = D2D1::RoundedRect(labelRect, 4.0f * m_dpiScale, 4.0f * m_dpiScale);
                ctx->FillRoundedRectangle(labelPill, m_brushIconPlate.Get());

                std::wstring displayLabel = stage.appName;
                ctx->DrawTextW(displayLabel.c_str(), static_cast<UINT32>(displayLabel.size()), m_formatTitle.Get(), labelRect, m_brushTextPrimary.Get());
            }
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK StageManager::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        StageManager* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<StageManager*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<StageManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT StageManager::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_MOUSEACTIVATE:
            // Crucial: Never activate Stage Manager window on click
            return MA_NOACTIVATE;

        case WM_TIMER:
            if (wParam == TIMER_TRACK) {
                if (m_enabled) {
                    HWND fg = GetForegroundWindow();
                    if (fg != m_currentForeground && fg != m_hwnd) {
                        RefreshStages();
                    }
                }
                return 0;
            }
            if (wParam == TIMER_ANIM) {
                float targetSlide = m_enabled ? 1.0f : 0.0f;
                float slideDelta = targetSlide - m_slideProgress;

                if (std::abs(slideDelta) > 0.01f) {
                    m_slideProgress += slideDelta * 0.22f; // Cubic spring feel
                } else {
                    m_slideProgress = targetSlide;
                    if (!m_enabled) {
                        m_animating = false;
                        KillTimer(m_hwnd, TIMER_ANIM);
                        ShowWindow(m_hwnd, SW_HIDE);
                        ClearThumbnails();
                        return 0;
                    }
                }

                // Update hover animation for all stage cards
                bool anyHoverAnim = false;
                for (size_t i = 0; i < m_stages.size(); ++i) {
                    float target = (static_cast<int>(i) == m_hoveredIndex) ? 1.0f : 0.0f;
                    float delta = target - m_stages[i].hoverAnim;
                    if (std::abs(delta) > 0.01f) {
                        m_stages[i].hoverAnim += delta * 0.25f;
                        anyHoverAnim = true;
                    } else {
                        m_stages[i].hoverAnim = target;
                    }
                }

                // Animate window X position sliding in from left edge
                int targetX = static_cast<int>(-static_cast<float>(m_stripWidth) * (1.0f - m_slideProgress));
                SetWindowPos(m_hwnd, HWND_TOPMOST, targetX, m_topBarHeight + 4, m_stripWidth, m_stripHeight, SWP_NOACTIVATE);

                CalculateLayout();
                UpdateThumbnails();
                Render();

                if (!m_animating && !anyHoverAnim && m_slideProgress >= 0.99f) {
                    KillTimer(m_hwnd, TIMER_ANIM);
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int newHover = -1;

            for (size_t i = 0; i < m_stages.size(); ++i) {
                const auto& r = m_stages[i].cardRect;
                if (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom) {
                    newHover = static_cast<int>(i);
                    break;
                }
            }

            if (newHover != m_hoveredIndex) {
                m_hoveredIndex = newHover;
                SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (m_hoveredIndex != -1) {
                m_hoveredIndex = -1;
                SetTimer(m_hwnd, TIMER_ANIM, 16, nullptr);
            }
            return 0;

        case WM_LBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            for (size_t i = 0; i < m_stages.size(); ++i) {
                const auto& r = m_stages[i].cardRect;
                if (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom) {
                    ActivateStage(i);
                    return 0;
                }
            }

            // Clicked background area of stage manager
            PeekDesktop();
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
            ClearThumbnails();
            KillTimer(hwnd, TIMER_ANIM);
            KillTimer(hwnd, TIMER_TRACK);
            return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

}

