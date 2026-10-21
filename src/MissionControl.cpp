#include "MissionControl.h"
#include "IconManager.h"
#include <algorithm>
#include <cmath>

namespace Aqua {

    const wchar_t MISSION_CONTROL_CLASS[] = L"AquaMissionControlWindow";

    MissionControl::MissionControl() = default;

    MissionControl::~MissionControl() {
        ClearThumbnails();
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool MissionControl::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = MissionControl::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = MISSION_CONTROL_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            MISSION_CONTROL_CLASS,
            L"AquaOS Mission Control",
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

        // 1. Title format (Card header)
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
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
                12.0f * m_dpiScale,
                L"en-US",
                &m_formatTitle
            );
        }
        if (m_formatTitle) {
            m_formatTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_formatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Spaces / Desktops Header Format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f * m_dpiScale,
            L"en-US",
            &m_formatHeader
        );
        if (m_formatHeader) {
            m_formatHeader->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatHeader->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 3. Bottom hint format
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatHint
        );
        if (m_formatHint) {
            m_formatHint->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatHint->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Create Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.85f), &m_brushCardBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.23f, 0.95f), &m_brushCardBgHover);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushAccent);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.28f, 0.25f, 0.95f), &m_brushCloseBtn);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.55f, 0.70f), &m_brushBtnIdle);

        return true;
    }

    void MissionControl::ClearThumbnails() {
        for (auto& card : m_cards) {
            if (card.hThumbnail) {
                DwmUnregisterThumbnail(card.hThumbnail);
                card.hThumbnail = nullptr;
            }
        }
    }

    void MissionControl::Show() {
        if (m_visible) return;

        m_visible = true;
        m_hoveredIndex = -1;

        CollectWindows();
        CalculateLayout();

        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        SetActiveWindow(m_hwnd);

        Render();
    }

    void MissionControl::Hide() {
        if (!m_visible) return;

        m_visible = false;
        ClearThumbnails();
        m_cards.clear();

        ShowWindow(m_hwnd, SW_HIDE);
    }

    void MissionControl::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    struct EnumWinContext {
        MissionControl* pThis;
        std::vector<HWND> hwnds;
    };

    void MissionControl::CollectWindows() {
        ClearThumbnails();
        m_cards.clear();

        EnumWinContext ctx = { this, {} };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* pCtx = reinterpret_cast<EnumWinContext*>(lParam);

            if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return TRUE;
            if (hwnd == pCtx->pThis->GetHwnd()) return TRUE;

            // Exclude tool windows and child windows
            LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

            // Must not have an owner
            if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

            // DWM Cloaking check (virtual desktop filtering / hidden UWP frame)
            int cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
                if (cloaked != 0) return TRUE;
            }

            // Must have a non-empty title
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
                cls == L"AquaMissionControlWindow" || cls == L"Windows.UI.Core.CoreWindow") {
                return TRUE;
            }

            // Minimum dimensions check
            RECT rc = {};
            GetWindowRect(hwnd, &rc);
            if ((rc.right - rc.left) < 120 || (rc.bottom - rc.top) < 120) return TRUE;

            pCtx->hwnds.push_back(hwnd);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        auto& engine = DCompEngine::Instance();
        auto d2dCtx = engine.GetD2DContext();

        for (HWND hwnd : ctx.hwnds) {
            WindowCard card;
            card.hwnd = hwnd;

            wchar_t titleBuf[256] = {};
            GetWindowTextW(hwnd, titleBuf, 256);
            card.title = titleBuf;

            // Query process executable path for high-res icon extraction
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t exePath[MAX_PATH] = {};
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                    card.processPath = exePath;
                }
                CloseHandle(hProc);
            }

            if (!card.processPath.empty()) {
                card.icon = IconManager::Instance().GetIconForPath(d2dCtx, card.processPath, 32);
            }

            m_cards.push_back(card);
        }
    }

    void MissionControl::CalculateLayout() {
        if (m_cards.empty()) return;

        float topMargin = 85.0f * m_dpiScale;
        float bottomMargin = 60.0f * m_dpiScale;
        float sideMargin = 70.0f * m_dpiScale;

        float availW = static_cast<float>(m_screenWidth) - (sideMargin * 2.0f);
        float availH = static_cast<float>(m_screenHeight) - topMargin - bottomMargin;

        size_t n = m_cards.size();
        int cols = 1;
        int rows = 1;

        if (n == 1) {
            cols = 1; rows = 1;
        } else if (n == 2) {
            cols = 2; rows = 1;
        } else if (n <= 4) {
            cols = 2; rows = 2;
        } else if (n <= 6) {
            cols = 3; rows = 2;
        } else if (n <= 9) {
            cols = 3; rows = 3;
        } else if (n <= 12) {
            cols = 4; rows = 3;
        } else {
            cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
            if (cols > 5) cols = 5;
            rows = static_cast<int>((n + cols - 1) / cols);
        }

        float cellW = availW / static_cast<float>(cols);
        float cellH = availH / static_cast<float>(rows);

        float gapX = 20.0f * m_dpiScale;
        float gapY = 20.0f * m_dpiScale;

        for (size_t i = 0; i < n; ++i) {
            auto& card = m_cards[i];

            int c = static_cast<int>(i % cols);
            int r = static_cast<int>(i / cols);

            // In the last row, center remaining cards if fewer than cols
            int cardsInThisRow = cols;
            if (r == rows - 1) {
                int remainder = static_cast<int>(n % cols);
                if (remainder != 0) cardsInThisRow = remainder;
            }

            float rowStartX = sideMargin + ((static_cast<float>(cols) - static_cast<float>(cardsInThisRow)) * cellW * 0.5f);

            float cardX = rowStartX + c * cellW + (gapX * 0.5f);
            float cardY = topMargin + r * cellH + (gapY * 0.5f);
            float cardW = cellW - gapX;
            float cardH = cellH - gapY;

            // Cap maximum card dimensions for 1 or 2 windows
            if (n == 1) {
                cardW = std::min(cardW, 880.0f * m_dpiScale);
                cardH = std::min(cardH, 560.0f * m_dpiScale);
                cardX = (static_cast<float>(m_screenWidth) - cardW) * 0.5f;
                cardY = topMargin + (availH - cardH) * 0.5f;
            } else if (n == 2) {
                cardW = std::min(cardW, 640.0f * m_dpiScale);
                cardH = std::min(cardH, 440.0f * m_dpiScale);
                cardY = topMargin + (availH - cardH) * 0.5f;
            }

            card.cardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

            // Close button at top right: 18x18 circle
            float btnSize = 18.0f * m_dpiScale;
            float btnX = card.cardRect.right - btnSize - (10.0f * m_dpiScale);
            float btnY = card.cardRect.top + (10.0f * m_dpiScale);
            card.closeButtonRect = D2D1::RectF(btnX, btnY, btnX + btnSize, btnY + btnSize);

            // Register DWM Thumbnail
            HRESULT hr = DwmRegisterThumbnail(m_hwnd, card.hwnd, &card.hThumbnail);
            if (SUCCEEDED(hr) && card.hThumbnail) {
                SIZE srcSize = { 1920, 1080 };
                DwmQueryThumbnailSourceSize(card.hThumbnail, &srcSize);
                if (srcSize.cx <= 0 || srcSize.cy <= 0) {
                    srcSize.cx = 16;
                    srcSize.cy = 9;
                }

                // Available thumbnail area inside card
                float headerH = 38.0f * m_dpiScale;
                float pad = 10.0f * m_dpiScale;
                float maxThumbW = cardW - (pad * 2.0f);
                float maxThumbH = cardH - headerH - (pad * 1.5f);

                float scale = std::min(maxThumbW / static_cast<float>(srcSize.cx), maxThumbH / static_cast<float>(srcSize.cy));
                float thumbW = std::max(20.0f, static_cast<float>(srcSize.cx) * scale);
                float thumbH = std::max(20.0f, static_cast<float>(srcSize.cy) * scale);

                float thumbX = cardX + (cardW - thumbW) * 0.5f;
                float thumbY = cardY + headerH + (maxThumbH - thumbH) * 0.5f;

                card.thumbRect = D2D1::RectF(thumbX, thumbY, thumbX + thumbW, thumbY + thumbH);

                DWM_THUMBNAIL_PROPERTIES props = {};
                props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
                props.rcDestination = RECT{
                    static_cast<LONG>(thumbX),
                    static_cast<LONG>(thumbY),
                    static_cast<LONG>(thumbX + thumbW),
                    static_cast<LONG>(thumbY + thumbH)
                };
                props.fVisible = TRUE;
                props.opacity = 255;

                DwmUpdateThumbnailProperties(card.hThumbnail, &props);
            }
        }
    }

    void MissionControl::ActivateWindow(HWND targetHwnd) {
        if (!targetHwnd || !IsWindow(targetHwnd)) return;

        Hide();

        if (IsIconic(targetHwnd)) {
            ShowWindow(targetHwnd, SW_RESTORE);
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
    }

    void MissionControl::CloseTargetWindow(HWND targetHwnd) {
        if (!targetHwnd || !IsWindow(targetHwnd)) return;
        PostMessageW(targetHwnd, WM_CLOSE, 0, 0);

        // Refresh Mission Control after brief delay
        SetTimer(m_hwnd, 1001, 120, nullptr);
    }

    void MissionControl::Render() {
        if (!m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Frosted Dark Acrylic Backdrop
        ctx->Clear(D2D1::ColorF(0.06f, 0.06f, 0.08f, 0.85f));

        float cx = static_cast<float>(m_screenWidth) * 0.5f;

        // 2. macOS Top Spaces Bar
        float spacesW = 280.0f * m_dpiScale;
        float spacesH = 36.0f * m_dpiScale;
        float spacesY = 24.0f * m_dpiScale;
        D2D1_ROUNDED_RECT spacesPill = D2D1::RoundedRect(
            D2D1::RectF(cx - spacesW * 0.5f, spacesY, cx + spacesW * 0.5f, spacesY + spacesH),
            spacesH * 0.5f, spacesH * 0.5f
        );

        ctx->FillRoundedRectangle(spacesPill, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(spacesPill, m_brushBorder.Get(), 1.0f);

        // Active Space Badge
        float badgeW = 120.0f * m_dpiScale;
        float badgeH = 26.0f * m_dpiScale;
        D2D1_ROUNDED_RECT activeBadge = D2D1::RoundedRect(
            D2D1::RectF(cx - badgeW * 0.5f, spacesY + (spacesH - badgeH) * 0.5f, cx + badgeW * 0.5f, spacesY + (spacesH + badgeH) * 0.5f),
            badgeH * 0.5f, badgeH * 0.5f
        );
        ctx->FillRoundedRectangle(activeBadge, m_brushCardBgHover.Get());
        ctx->DrawRoundedRectangle(activeBadge, m_brushBorder.Get(), 1.0f);

        if (m_formatHeader) {
            D2D1_RECT_F textRect = activeBadge.rect;
            ctx->DrawTextW(L"Desktop 1", 9, m_formatHeader.Get(), textRect, m_brushTextPrimary.Get());
        }

        // 3. Render Window Cards
        if (m_cards.empty()) {
            // Empty state
            float boxW = 380.0f * m_dpiScale;
            float boxH = 140.0f * m_dpiScale;
            float boxY = (static_cast<float>(m_screenHeight) - boxH) * 0.5f;
            D2D1_ROUNDED_RECT emptyBox = D2D1::RoundedRect(
                D2D1::RectF(cx - boxW * 0.5f, boxY, cx + boxW * 0.5f, boxY + boxH),
                16.0f * m_dpiScale, 16.0f * m_dpiScale
            );
            ctx->FillRoundedRectangle(emptyBox, m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(emptyBox, m_brushBorder.Get(), 1.0f);

            if (m_formatHeader) {
                D2D1_RECT_F t1 = D2D1::RectF(emptyBox.rect.left, boxY + 35.0f * m_dpiScale, emptyBox.rect.right, boxY + 65.0f * m_dpiScale);
                ctx->DrawTextW(L"No Open Application Windows", 27, m_formatHeader.Get(), t1, m_brushTextPrimary.Get());
            }
            if (m_formatHint) {
                D2D1_RECT_F t2 = D2D1::RectF(emptyBox.rect.left, boxY + 75.0f * m_dpiScale, emptyBox.rect.right, boxY + 105.0f * m_dpiScale);
                ctx->DrawTextW(L"Open an app or press F3 to exit", 30, m_formatHint.Get(), t2, m_brushTextSecondary.Get());
            }
        } else {
            for (size_t i = 0; i < m_cards.size(); ++i) {
                const auto& card = m_cards[i];

                D2D1_ROUNDED_RECT cardBox = D2D1::RoundedRect(card.cardRect, 12.0f * m_dpiScale, 12.0f * m_dpiScale);

                // Card Background
                if (card.isHovered) {
                    ctx->FillRoundedRectangle(cardBox, m_brushCardBgHover.Get());
                    ctx->DrawRoundedRectangle(cardBox, m_brushAccent.Get(), 2.0f * m_dpiScale);
                } else {
                    ctx->FillRoundedRectangle(cardBox, m_brushCardBg.Get());
                    ctx->DrawRoundedRectangle(cardBox, m_brushBorder.Get(), 1.0f * m_dpiScale);
                }

                // Card Header: App Icon
                float iconSize = 20.0f * m_dpiScale;
                float iconX = card.cardRect.left + (12.0f * m_dpiScale);
                float iconY = card.cardRect.top + (9.0f * m_dpiScale);

                if (card.icon) {
                    ctx->DrawBitmap(card.icon.Get(), D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize));
                } else {
                    // Fallback small colored dot
                    ctx->FillEllipse(
                        D2D1::Ellipse(D2D1::Point2F(iconX + iconSize * 0.5f, iconY + iconSize * 0.5f), iconSize * 0.4f, iconSize * 0.4f),
                        m_brushAccent.Get()
                    );
                }

                // Card Header: Title Text
                float textLeft = iconX + iconSize + (8.0f * m_dpiScale);
                float textRight = card.closeButtonRect.left - (8.0f * m_dpiScale);
                if (textRight > textLeft && m_formatTitle) {
                    D2D1_RECT_F titleBox = D2D1::RectF(textLeft, card.cardRect.top + (4.0f * m_dpiScale), textRight, card.cardRect.top + (34.0f * m_dpiScale));
                    ctx->DrawTextW(card.title.c_str(), static_cast<UINT32>(card.title.length()), m_formatTitle.Get(), titleBox, m_brushTextPrimary.Get());
                }

                // Card Header: macOS Red Close Button
                if (card.isHovered) {
                    float btnR = (card.closeButtonRect.right - card.closeButtonRect.left) * 0.5f;
                    D2D1_POINT_2F btnCenter = D2D1::Point2F(
                        card.closeButtonRect.left + btnR,
                        card.closeButtonRect.top + btnR
                    );

                    if (card.isCloseHovered) {
                        ctx->FillEllipse(D2D1::Ellipse(btnCenter, btnR, btnR), m_brushCloseBtn.Get());

                        // Draw white 'x'
                        float xCross = btnR * 0.45f;
                        ctx->DrawLine(
                            D2D1::Point2F(btnCenter.x - xCross, btnCenter.y - xCross),
                            D2D1::Point2F(btnCenter.x + xCross, btnCenter.y + xCross),
                            m_brushTextPrimary.Get(), 1.5f * m_dpiScale
                        );
                        ctx->DrawLine(
                            D2D1::Point2F(btnCenter.x + xCross, btnCenter.y - xCross),
                            D2D1::Point2F(btnCenter.x - xCross, btnCenter.y + xCross),
                            m_brushTextPrimary.Get(), 1.5f * m_dpiScale
                        );
                    } else {
                        ctx->FillEllipse(D2D1::Ellipse(btnCenter, btnR, btnR), m_brushBtnIdle.Get());
                    }
                }

                // Thumbnail Cutout: Clear alpha to 0 so DWM composite shines through with 100% clarity
                if (card.thumbRect.right > card.thumbRect.left && card.thumbRect.bottom > card.thumbRect.top) {
                    ctx->PushAxisAlignedClip(card.thumbRect, D2D1_ANTIALIAS_MODE_ALIASED);
                    ctx->Clear(D2D1::ColorF(0, 0, 0, 0));
                    ctx->PopAxisAlignedClip();

                    // Crisp 1px border around the thumbnail
                    ctx->DrawRectangle(card.thumbRect, m_brushBorder.Get(), 1.0f);
                }
            }
        }

        // 4. Bottom Hint Text
        if (m_formatHint) {
            float hintW = 500.0f * m_dpiScale;
            float hintH = 30.0f * m_dpiScale;
            float hintY = static_cast<float>(m_screenHeight) - (45.0f * m_dpiScale);
            D2D1_RECT_F hintBox = D2D1::RectF(cx - hintW * 0.5f, hintY, cx + hintW * 0.5f, hintY + hintH);
            ctx->DrawTextW(L"Click a window to switch  •  Esc or F3 to exit", 43, m_formatHint.Get(), hintBox, m_brushTextSecondary.Get());
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK MissionControl::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MissionControl* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<MissionControl*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            pThis->m_hwnd = hwnd;
        } else {
            pThis = reinterpret_cast<MissionControl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT MissionControl::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_MOUSEMOVE: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            int newHovered = -1;
            bool needRender = false;

            for (size_t i = 0; i < m_cards.size(); ++i) {
                auto& card = m_cards[i];
                bool hovered = (mx >= card.cardRect.left && mx <= card.cardRect.right &&
                                my >= card.cardRect.top && my <= card.cardRect.bottom);

                bool closeHovered = false;
                if (hovered) {
                    closeHovered = (mx >= card.closeButtonRect.left && mx <= card.closeButtonRect.right &&
                                    my >= card.closeButtonRect.top && my <= card.closeButtonRect.bottom);
                }

                if (card.isHovered != hovered || card.isCloseHovered != closeHovered) {
                    card.isHovered = hovered;
                    card.isCloseHovered = closeHovered;
                    needRender = true;
                }

                if (hovered) {
                    newHovered = static_cast<int>(i);
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
            bool needRender = false;
            for (auto& card : m_cards) {
                if (card.isHovered || card.isCloseHovered) {
                    card.isHovered = false;
                    card.isCloseHovered = false;
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

            for (size_t i = 0; i < m_cards.size(); ++i) {
                auto& card = m_cards[i];
                if (mx >= card.closeButtonRect.left && mx <= card.closeButtonRect.right &&
                    my >= card.closeButtonRect.top && my <= card.closeButtonRect.bottom) {
                    CloseTargetWindow(card.hwnd);
                    return 0;
                }

                if (mx >= card.cardRect.left && mx <= card.cardRect.right &&
                    my >= card.cardRect.top && my <= card.cardRect.bottom) {
                    ActivateWindow(card.hwnd);
                    return 0;
                }
            }

            // Clicked outside cards -> dismiss
            Hide();
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE || wParam == VK_F3) {
                Hide();
                return 0;
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1001) {
                KillTimer(m_hwnd, 1001);
                if (m_visible) {
                    CollectWindows();
                    CalculateLayout();
                    Render();
                }
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            ClearThumbnails();
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

