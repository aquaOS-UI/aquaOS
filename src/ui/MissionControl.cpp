#include "MissionControl.h"
#include "IconManager.h"
#include "SystemInfo.h"
#include <shobjidl.h>
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
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (vw <= 0) vw = screenWidth;
        if (vh <= 0) vh = screenHeight;

        m_vx = vx;
        m_vy = vy;
        m_screenWidth = vw;
        m_screenHeight = vh;

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
            m_vx, m_vy, m_screenWidth, m_screenHeight,
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
        IVirtualDesktopManager* pVdm = nullptr;
    };

    void MissionControl::CollectWindows() {
        ClearThumbnails();
        m_cards.clear();

        IVirtualDesktopManager* pVdm = nullptr;
        CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pVdm));

        EnumWinContext ctx = { this, {}, pVdm };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* pCtx = reinterpret_cast<EnumWinContext*>(lParam);

            if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return TRUE;
            if (hwnd == pCtx->pThis->GetHwnd()) return TRUE;

            // Virtual Desktop check: filter windows to current desktop
            if (pCtx->pVdm) {
                BOOL onCurrent = TRUE;
                if (SUCCEEDED(pCtx->pVdm->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrent))) {
                    if (!onCurrent) return TRUE;
                }
            }

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

        if (pVdm) {
            pVdm->Release();
            pVdm = nullptr;
        }
    }

    void MissionControl::CalculateLayout() {
        auto monitors = MonitorManager::GetMonitors();
        if (monitors.empty()) {
            MonitorData defMon;
            defMon.rcMonitor = { m_vx, m_vy, m_vx + m_screenWidth, m_vy + m_screenHeight };
            monitors.push_back(defMon);
        }

        // 1. Layout Top Spaces Bar
        m_spaces.clear();
        float spaceW = 108.0f * m_dpiScale;
        float spaceH = 64.0f * m_dpiScale;
        float gap = 12.0f * m_dpiScale;
        float addBtnW = 30.0f * m_dpiScale;
        float totalSpacesW = static_cast<float>(m_spaceCount) * spaceW + static_cast<float>(m_spaceCount - 1) * gap + gap + addBtnW;

        float mon0LocalX = static_cast<float>(monitors[0].rcMonitor.left - m_vx);
        float mon0LocalY = static_cast<float>(monitors[0].rcMonitor.top - m_vy);
        float mon0LocalW = static_cast<float>(monitors[0].rcMonitor.right - monitors[0].rcMonitor.left);
        float spacesStartX = mon0LocalX + (mon0LocalW - totalSpacesW) * 0.5f;
        float spacesStartY = mon0LocalY + 16.0f * m_dpiScale;

        for (int i = 0; i < m_spaceCount; ++i) {
            SpaceItem space;
            space.id = i + 1;
            space.name = L"Desktop " + std::to_wstring(i + 1);
            float sx = spacesStartX + static_cast<float>(i) * (spaceW + gap);
            space.rect = D2D1::RectF(sx, spacesStartY, sx + spaceW, spacesStartY + spaceH);
            space.thumbRect = D2D1::RectF(sx + 4.0f * m_dpiScale, spacesStartY + 4.0f * m_dpiScale, sx + spaceW - 4.0f * m_dpiScale, spacesStartY + 44.0f * m_dpiScale);
            space.isActive = (i == m_currentSpace);
            space.isHovered = false;
            m_spaces.push_back(space);
        }

        float addX = spacesStartX + static_cast<float>(m_spaceCount) * (spaceW + gap);
        float addY = spacesStartY + (spaceH - addBtnW) * 0.5f;
        m_addSpaceRect = D2D1::RectF(addX, addY, addX + addBtnW, addY + addBtnW);
        m_isAddSpaceHovered = false;

        if (m_cards.empty()) return;

        // Partition cards by monitor
        std::vector<std::vector<size_t>> monitorCards(monitors.size());
        for (size_t i = 0; i < m_cards.size(); ++i) {
            HMONITOR hMon = MonitorFromWindow(m_cards[i].hwnd, MONITOR_DEFAULTTONEAREST);
            size_t targetIdx = 0;
            for (size_t m = 0; m < monitors.size(); ++m) {
                if (monitors[m].hMonitor == hMon) {
                    targetIdx = m;
                    break;
                }
            }
            monitorCards[targetIdx].push_back(i);
        }

        // Layout cards for each monitor independently
        for (size_t m = 0; m < monitors.size(); ++m) {
            const auto& mon = monitors[m];
            const auto& cardIndices = monitorCards[m];
            if (cardIndices.empty()) continue;

            float monLocalX = static_cast<float>(mon.rcMonitor.left - m_vx);
            float monLocalY = static_cast<float>(mon.rcMonitor.top - m_vy);
            float monLocalW = static_cast<float>(mon.rcMonitor.right - mon.rcMonitor.left);
            float monLocalH = static_cast<float>(mon.rcMonitor.bottom - mon.rcMonitor.top);

            float topMargin = monLocalY + 105.0f * m_dpiScale;
            float bottomMargin = 60.0f * m_dpiScale;
            float sideMargin = 70.0f * m_dpiScale;

            float availW = monLocalW - (sideMargin * 2.0f);
            float availH = monLocalH - (topMargin - monLocalY) - bottomMargin;

            size_t n = cardIndices.size();
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
            } else {
                cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
                if (cols > 4) cols = 4;
                rows = static_cast<int>((n + cols - 1) / cols);
            }

            float cellW = availW / static_cast<float>(cols);
            float cellH = availH / static_cast<float>(rows);

            float gapX = 20.0f * m_dpiScale;
            float gapY = 20.0f * m_dpiScale;

            for (size_t ci = 0; ci < n; ++ci) {
                size_t cardIdx = cardIndices[ci];
                auto& card = m_cards[cardIdx];

                int c = static_cast<int>(ci % cols);
                int r = static_cast<int>(ci / cols);

                int cardsInThisRow = cols;
                if (r == rows - 1) {
                    int remainder = static_cast<int>(n % cols);
                    if (remainder != 0) cardsInThisRow = remainder;
                }

                float rowStartX = monLocalX + sideMargin + ((static_cast<float>(cols) - static_cast<float>(cardsInThisRow)) * cellW * 0.5f);

                float cardX = rowStartX + c * cellW + (gapX * 0.5f);
                float cardY = topMargin + r * cellH + (gapY * 0.5f);
                float cardW = cellW - gapX;
                float cardH = cellH - gapY;

                if (n == 1) {
                    cardW = std::min(cardW, 880.0f * m_dpiScale);
                    cardH = std::min(cardH, 560.0f * m_dpiScale);
                    cardX = monLocalX + (monLocalW - cardW) * 0.5f;
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

    void MissionControl::SwitchToSpace(int targetIdx) {
        if (targetIdx < 0 || targetIdx >= m_spaceCount) return;
        int delta = targetIdx - m_currentSpace;
        m_currentSpace = targetIdx;

        if (delta != 0) {
            WORD key = (delta > 0) ? VK_RIGHT : VK_LEFT;
            int steps = std::abs(delta);
            for (int i = 0; i < steps; ++i) {
                INPUT inputs[6] = {};
                inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_LWIN;
                inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = VK_CONTROL;
                inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = key;

                inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = key; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
                inputs[4].type = INPUT_KEYBOARD; inputs[4].ki.wVk = VK_CONTROL; inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
                inputs[5].type = INPUT_KEYBOARD; inputs[5].ki.wVk = VK_LWIN; inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

                SendInput(6, inputs, sizeof(INPUT));
                Sleep(35);
            }
        }

        Hide();
    }

    void MissionControl::AddNewSpace() {
        if (m_spaceCount >= 8) return;
        m_spaceCount++;
        m_currentSpace = m_spaceCount - 1;

        INPUT inputs[6] = {};
        inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_LWIN;
        inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = VK_CONTROL;
        inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = 'D';

        inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = 'D'; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[4].type = INPUT_KEYBOARD; inputs[4].ki.wVk = VK_CONTROL; inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[5].type = INPUT_KEYBOARD; inputs[5].ki.wVk = VK_LWIN; inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(6, inputs, sizeof(INPUT));

        CalculateLayout();
        Render();
    }

    void MissionControl::Render() {
        if (!m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Frosted Dark Acrylic Backdrop
        ctx->Clear(D2D1::ColorF(0.06f, 0.06f, 0.08f, 0.88f));

        float cx = static_cast<float>(m_screenWidth) * 0.5f;

        // 2. macOS Top Spaces Bar (Virtual Desktops Shelf)
        if (!m_spaces.empty()) {
            float shelfPadX = 14.0f * m_dpiScale;
            float shelfPadY = 8.0f * m_dpiScale;
            float shelfLeft = m_spaces.front().rect.left - shelfPadX;
            float shelfTop = m_spaces.front().rect.top - shelfPadY;
            float shelfRight = m_addSpaceRect.right + shelfPadX;
            float shelfBottom = m_spaces.front().rect.bottom + shelfPadY;

            D2D1_ROUNDED_RECT shelfRect = D2D1::RoundedRect(
                D2D1::RectF(shelfLeft, shelfTop, shelfRight, shelfBottom),
                16.0f * m_dpiScale, 16.0f * m_dpiScale
            );
            ctx->FillRoundedRectangle(shelfRect, m_brushCardBg.Get());
            ctx->DrawRoundedRectangle(shelfRect, m_brushBorder.Get(), 1.0f);

            // Draw each Space thumbnail capsule
            for (size_t si = 0; si < m_spaces.size(); ++si) {
                const auto& space = m_spaces[si];

                D2D1_ROUNDED_RECT spaceThumb = D2D1::RoundedRect(space.thumbRect, 6.0f * m_dpiScale, 6.0f * m_dpiScale);

                if (space.isActive) {
                    ctx->FillRoundedRectangle(spaceThumb, m_brushCardBgHover.Get());
                    ctx->DrawRoundedRectangle(spaceThumb, m_brushAccent.Get(), 2.0f * m_dpiScale);

                    // Miniature interior window simulation
                    float mwW = (space.thumbRect.right - space.thumbRect.left) * 0.55f;
                    float mwH = (space.thumbRect.bottom - space.thumbRect.top) * 0.55f;
                    float mwX = space.thumbRect.left + ((space.thumbRect.right - space.thumbRect.left) - mwW) * 0.5f;
                    float mwY = space.thumbRect.top + ((space.thumbRect.bottom - space.thumbRect.top) - mwH) * 0.5f;
                    D2D1_RECT_F miniWin = D2D1::RectF(mwX, mwY, mwX + mwW, mwY + mwH);
                    ctx->FillRectangle(miniWin, m_brushCardBg.Get());
                    ctx->DrawRectangle(miniWin, m_brushBorder.Get(), 1.0f);
                } else if (space.isHovered) {
                    ctx->FillRoundedRectangle(spaceThumb, m_brushCardBgHover.Get());
                    ctx->DrawRoundedRectangle(spaceThumb, m_brushBorder.Get(), 1.5f * m_dpiScale);
                } else {
                    ctx->FillRoundedRectangle(spaceThumb, m_brushCardBg.Get());
                    ctx->DrawRoundedRectangle(spaceThumb, m_brushBorder.Get(), 1.0f);
                }

                // Desktop Label
                if (m_formatHeader) {
                    D2D1_RECT_F labelBox = D2D1::RectF(
                        space.rect.left,
                        space.thumbRect.bottom + 2.0f * m_dpiScale,
                        space.rect.right,
                        space.rect.bottom
                    );
                    auto textBrush = space.isActive ? m_brushAccent.Get() : m_brushTextSecondary.Get();
                    ctx->DrawTextW(space.name.c_str(), static_cast<UINT32>(space.name.length()), m_formatHeader.Get(), labelBox, textBrush);
                }
            }

            // Draw Add Space (+) Button
            float addR = (m_addSpaceRect.right - m_addSpaceRect.left) * 0.5f;
            D2D1_POINT_2F addCenter = D2D1::Point2F(m_addSpaceRect.left + addR, m_addSpaceRect.top + addR);
            ctx->FillEllipse(D2D1::Ellipse(addCenter, addR, addR), m_isAddSpaceHovered ? m_brushCardBgHover.Get() : m_brushCardBg.Get());
            ctx->DrawEllipse(D2D1::Ellipse(addCenter, addR, addR), m_isAddSpaceHovered ? m_brushAccent.Get() : m_brushBorder.Get(), 1.0f);

            float crossLen = 5.0f * m_dpiScale;
            auto crossBrush = m_isAddSpaceHovered ? m_brushAccent.Get() : m_brushTextSecondary.Get();
            ctx->DrawLine(
                D2D1::Point2F(addCenter.x - crossLen, addCenter.y),
                D2D1::Point2F(addCenter.x + crossLen, addCenter.y),
                crossBrush, 1.8f * m_dpiScale
            );
            ctx->DrawLine(
                D2D1::Point2F(addCenter.x, addCenter.y - crossLen),
                D2D1::Point2F(addCenter.x, addCenter.y + crossLen),
                crossBrush, 1.8f * m_dpiScale
            );
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

            // Spaces Bar Hover Detection
            for (auto& space : m_spaces) {
                bool hovered = (mx >= space.rect.left && mx <= space.rect.right &&
                                my >= space.rect.top && my <= space.rect.bottom);
                if (space.isHovered != hovered) {
                    space.isHovered = hovered;
                    needRender = true;
                }
            }

            bool addHovered = (mx >= m_addSpaceRect.left && mx <= m_addSpaceRect.right &&
                               my >= m_addSpaceRect.top && my <= m_addSpaceRect.bottom);
            if (m_isAddSpaceHovered != addHovered) {
                m_isAddSpaceHovered = addHovered;
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
            for (auto& space : m_spaces) {
                if (space.isHovered) {
                    space.isHovered = false;
                    needRender = true;
                }
            }
            if (m_isAddSpaceHovered) {
                m_isAddSpaceHovered = false;
                needRender = true;
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

            // Check Add Space click
            if (mx >= m_addSpaceRect.left && mx <= m_addSpaceRect.right &&
                my >= m_addSpaceRect.top && my <= m_addSpaceRect.bottom) {
                AddNewSpace();
                return 0;
            }

            // Check Spaces Bar click
            for (size_t i = 0; i < m_spaces.size(); ++i) {
                const auto& space = m_spaces[i];
                if (mx >= space.rect.left && mx <= space.rect.right &&
                    my >= space.rect.top && my <= space.rect.bottom) {
                    SwitchToSpace(static_cast<int>(i));
                    return 0;
                }
            }

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

