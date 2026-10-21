#include "TopBar.h"
#include "SystemInfo.h"
#include "TaskbarController.h"
#include "WindowTiler.h"
#include "LockScreen.h"
#include "ThemeManager.h"
#include "MediaControllerService.h"
#include <algorithm>
#include <cwctype>

namespace Aqua {

    const wchar_t TOPBAR_CLASS_NAME[] = L"AquaTopBarWindow";

    TopBar::TopBar() {
        m_activeAppName = L"Finder";
        m_activeCategory = AppCategory::Finder;
        m_leftMenuItems = {
            { L"Finder", {}, false, nullptr, -1 },
            { L"File",   {}, false, nullptr, -1 },
            { L"Edit",   {}, false, nullptr, -1 },
            { L"View",   {}, false, nullptr, -1 },
            { L"Go",     {}, false, nullptr, -1 },
            { L"Window", {}, false, nullptr, -1 },
            { L"Help",   {}, false, nullptr, -1 }
        };
    }

    TopBar::~TopBar() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool TopBar::Create(HINSTANCE hInstance, int x, int y, int width, int height) {
        m_posX = x;
        m_posY = y;
        m_width = width;
        m_height = height;

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
            m_posX, m_posY, m_width, m_height,
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
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
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
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f * m_dpiScale,
                L"en-US",
                &m_textFormatClock
            );
        }

        if (m_textFormatRegular) {
            m_textFormatRegular->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_textFormatRegular->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_textFormatBold) {
            m_textFormatBold->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_textFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (m_textFormatClock) {
            m_textFormatClock->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            m_textFormatClock->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Create Direct2D Brushes
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::HoverItem, &m_brushHover);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(Colors::GlassBackground, &m_brushGlassBg);
        ctx->CreateSolidColorBrush(Colors::AccentGreen, &m_brushBatteryGreen);

        SyncForegroundApp();
        UpdateLayout();

        // 300ms timer for fast active foreground tracking + live clock & battery
        SetTimer(m_hwnd, 1001, 300, nullptr);

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

    static std::wstring CleanMenuText(const wchar_t* raw) {
        std::wstring s;
        if (!raw) return s;
        for (const wchar_t* p = raw; *p; ++p) {
            if (*p != L'&') {
                if (*p == L'\t') break; // stop at tab
                s.push_back(*p);
            }
        }
        return s;
    }

    void TopBar::SyncForegroundApp() {
        HWND fg = GetForegroundWindow();
        if (!fg || !IsWindow(fg)) {
            fg = GetDesktopWindow();
        }

        // Ignore our own UI windows
        wchar_t cls[64] = {};
        GetClassNameW(fg, cls, 64);
        std::wstring sCls(cls);
        if (sCls.find(L"Aqua") != std::wstring::npos) {
            return;
        }

        if (fg == m_foregroundHwnd) return;
        m_foregroundHwnd = fg;

        // Query process executable path
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        std::wstring procExe;
        if (hProc) {
            wchar_t path[MAX_PATH] = {};
            DWORD sz = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, path, &sz)) {
                procExe = path;
            }
            CloseHandle(hProc);
        }

        std::wstring exeName;
        size_t lastSlash = procExe.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) exeName = procExe.substr(lastSlash + 1);
        std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);

        wchar_t titleBuf[256] = {};
        GetWindowTextW(fg, titleBuf, 256);
        std::wstring titleStr(titleBuf);

        if (exeName == L"explorer.exe" || exeName.empty()) {
            m_activeAppName = L"Finder";
            m_activeCategory = AppCategory::Finder;
        } else if (exeName.find(L"code") != std::wstring::npos) {
            m_activeAppName = L"Visual Studio Code";
            m_activeCategory = AppCategory::CodeEditor;
        } else if (exeName.find(L"devenv") != std::wstring::npos) {
            m_activeAppName = L"Visual Studio";
            m_activeCategory = AppCategory::CodeEditor;
        } else if (exeName.find(L"edge") != std::wstring::npos) {
            m_activeAppName = L"Microsoft Edge";
            m_activeCategory = AppCategory::Browser;
        } else if (exeName.find(L"chrome") != std::wstring::npos) {
            m_activeAppName = L"Google Chrome";
            m_activeCategory = AppCategory::Browser;
        } else if (exeName.find(L"firefox") != std::wstring::npos) {
            m_activeAppName = L"Firefox";
            m_activeCategory = AppCategory::Browser;
        } else if (exeName.find(L"brave") != std::wstring::npos) {
            m_activeAppName = L"Brave";
            m_activeCategory = AppCategory::Browser;
        } else if (exeName.find(L"notepad") != std::wstring::npos) {
            m_activeAppName = L"Notepad";
            m_activeCategory = AppCategory::General;
        } else if (exeName.find(L"taskmgr") != std::wstring::npos) {
            m_activeAppName = L"Activity Monitor";
            m_activeCategory = AppCategory::General;
        } else if (exeName.find(L"cmd") != std::wstring::npos || exeName.find(L"powershell") != std::wstring::npos || exeName.find(L"wt") != std::wstring::npos) {
            m_activeAppName = L"Terminal";
            m_activeCategory = AppCategory::Terminal;
        } else {
            // Extract from title
            if (!titleStr.empty()) {
                size_t dash = titleStr.rfind(L" - ");
                if (dash != std::wstring::npos && dash + 3 < titleStr.length()) {
                    m_activeAppName = titleStr.substr(dash + 3);
                } else {
                    m_activeAppName = titleStr.substr(0, std::min<size_t>(titleStr.length(), 20));
                }
            } else if (!exeName.empty()) {
                size_t dot = exeName.rfind(L'.');
                m_activeAppName = (dot != std::wstring::npos) ? exeName.substr(0, dot) : exeName;
                if (!m_activeAppName.empty()) m_activeAppName[0] = static_cast<wchar_t>(towupper(m_activeAppName[0]));
            } else {
                m_activeAppName = L"Finder";
            }
            m_activeCategory = AppCategory::General;
        }

        // Rebuild Menus
        m_leftMenuItems.clear();

        // 1. First item is always the active application name in Bold
        m_leftMenuItems.push_back({ m_activeAppName, {}, false, nullptr, -1 });

        // 2. Check if window has a native Win32 HMENU
        HMENU hMenu = GetMenu(fg);
        int menuCount = hMenu ? GetMenuItemCount(hMenu) : 0;

        if (menuCount > 0) {
            for (int i = 0; i < menuCount; ++i) {
                wchar_t buf[128] = {};
                MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
                mii.fMask = MIIM_STRING | MIIM_SUBMENU;
                mii.dwTypeData = buf;
                mii.cch = 128;
                if (GetMenuItemInfoW(hMenu, i, TRUE, &mii) && wcslen(buf) > 0) {
                    std::wstring label = CleanMenuText(buf);
                    m_leftMenuItems.push_back({ label, {}, false, mii.hSubMenu, i });
                }
            }
        } else {
            // Contextual macOS menus based on category
            if (m_activeCategory == AppCategory::CodeEditor) {
                m_leftMenuItems.push_back({ L"File", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Edit", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Selection", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"View", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Go", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Terminal", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Window", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Help", {}, false, nullptr, -1 });
            } else if (m_activeCategory == AppCategory::Browser) {
                m_leftMenuItems.push_back({ L"File", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Edit", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"View", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"History", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Bookmarks", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Window", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Help", {}, false, nullptr, -1 });
            } else if (m_activeCategory == AppCategory::Finder) {
                m_leftMenuItems.push_back({ L"File", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Edit", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"View", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Go", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Window", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Help", {}, false, nullptr, -1 });
            } else if (m_activeCategory == AppCategory::Terminal) {
                m_leftMenuItems.push_back({ L"Shell", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Edit", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"View", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Window", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Help", {}, false, nullptr, -1 });
            } else {
                m_leftMenuItems.push_back({ L"File", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Edit", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"View", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Window", {}, false, nullptr, -1 });
                m_leftMenuItems.push_back({ L"Help", {}, false, nullptr, -1 });
            }
        }

        UpdateLayout();
        Render();
    }

    void TopBar::UpdateLayout() {
        float x = 12.0f * m_dpiScale;
        float h = static_cast<float>(m_height);

        // Logo
        m_logoRect = D2D1::RectF(x, 4.0f, x + 24.0f * m_dpiScale, h - 4.0f);
        x += 30.0f * m_dpiScale;

        // Left Menu Items
        for (size_t i = 0; i < m_leftMenuItems.size(); ++i) {
            float charWidth = (i == 0) ? 8.2f : 7.6f;
            float itemWidth = static_cast<float>(m_leftMenuItems[i].text.length()) * charWidth * m_dpiScale + (16.0f * m_dpiScale);
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

        // Now Playing Media Widget Button
        float npWidth = 26.0f * m_dpiScale;
        m_nowPlayingRect = D2D1::RectF(rx - npWidth, 4.0f, rx, h - 4.0f);
        rx -= (npWidth + 6.0f * m_dpiScale);

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

    void TopBar::SendShortcut(WORD mod, WORD key) {
        if (m_foregroundHwnd && IsWindow(m_foregroundHwnd)) {
            SetForegroundWindow(m_foregroundHwnd);
            BringWindowToTop(m_foregroundHwnd);
        }

        std::vector<INPUT> inputs;
        auto addKey = [&](WORD vk, bool up) {
            INPUT in = { INPUT_KEYBOARD };
            in.ki.wVk = vk;
            if (up) in.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(in);
        };

        if (mod & MOD_CONTROL) addKey(VK_CONTROL, false);
        if (mod & MOD_SHIFT)   addKey(VK_SHIFT, false);
        if (mod & MOD_ALT)     addKey(VK_MENU, false);

        addKey(key, false);
        addKey(key, true);

        if (mod & MOD_ALT)     addKey(VK_MENU, true);
        if (mod & MOD_SHIFT)   addKey(VK_SHIFT, true);
        if (mod & MOD_CONTROL) addKey(VK_CONTROL, true);

        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    void TopBar::ShowMenuForIndex(size_t index, int screenX, int screenY) {
        if (index >= m_leftMenuItems.size()) return;
        const auto& item = m_leftMenuItems[index];

        // 1. If native Win32 submenu is present, show it directly with Windows routing
        if (item.hSubMenu && m_foregroundHwnd && IsWindow(m_foregroundHwnd)) {
            SetForegroundWindow(m_foregroundHwnd);
            TrackPopupMenu(item.hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN, screenX, screenY, 0, m_foregroundHwnd, nullptr);
            return;
        }

        HMENU hMenu = CreatePopupMenu();

        // 2. Application Menu (Index 0: Bold App Name)
        if (index == 0) {
            AppendMenuW(hMenu, MF_STRING, 4001, (L"About " + m_activeAppName).c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4002, L"Settings / Preferences...\tCtrl+,");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4003, (L"Hide " + m_activeAppName + L"\tWin+Down").c_str());
            AppendMenuW(hMenu, MF_STRING, 4004, (L"Quit " + m_activeAppName + L"\tAlt+F4").c_str());
        }
        // 3. File Menu
        else if (item.text == L"File" || item.text == L"Shell") {
            if (m_activeCategory == AppCategory::Browser) {
                AppendMenuW(hMenu, MF_STRING, 4101, L"New Tab\tCtrl+T");
                AppendMenuW(hMenu, MF_STRING, 4102, L"New Window\tCtrl+N");
                AppendMenuW(hMenu, MF_STRING, 4103, L"New InPrivate Window\tCtrl+Shift+N");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4104, L"Reopen Closed Tab\tCtrl+Shift+T");
                AppendMenuW(hMenu, MF_STRING, 4105, L"Print...\tCtrl+P");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4106, L"Close Tab\tCtrl+W");
                AppendMenuW(hMenu, MF_STRING, 4107, L"Close Window\tCtrl+Shift+W");
            } else if (m_activeCategory == AppCategory::CodeEditor) {
                AppendMenuW(hMenu, MF_STRING, 4111, L"New Text File\tCtrl+N");
                AppendMenuW(hMenu, MF_STRING, 4112, L"New Window\tCtrl+Shift+N");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4113, L"Open File...\tCtrl+O");
                AppendMenuW(hMenu, MF_STRING, 4114, L"Open Folder...\tCtrl+K, Ctrl+O");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4115, L"Save\tCtrl+S");
                AppendMenuW(hMenu, MF_STRING, 4116, L"Save As...\tCtrl+Shift+S");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4106, L"Close Editor\tCtrl+W");
                AppendMenuW(hMenu, MF_STRING, 4107, L"Close Window\tCtrl+Shift+W");
            } else {
                AppendMenuW(hMenu, MF_STRING, 4121, L"New Window\tCtrl+N");
                AppendMenuW(hMenu, MF_STRING, 4122, L"Open...\tCtrl+O");
                AppendMenuW(hMenu, MF_STRING, 4115, L"Save\tCtrl+S");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4106, L"Close Window\tCtrl+W");
            }
        }
        // 4. Edit Menu
        else if (item.text == L"Edit") {
            AppendMenuW(hMenu, MF_STRING, 4201, L"Undo\tCtrl+Z");
            AppendMenuW(hMenu, MF_STRING, 4202, L"Redo\tCtrl+Y");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4203, L"Cut\tCtrl+X");
            AppendMenuW(hMenu, MF_STRING, 4204, L"Copy\tCtrl+C");
            AppendMenuW(hMenu, MF_STRING, 4205, L"Paste\tCtrl+V");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4206, L"Select All\tCtrl+A");
            AppendMenuW(hMenu, MF_STRING, 4207, L"Find...\tCtrl+F");
            AppendMenuW(hMenu, MF_STRING, 4208, L"Replace...\tCtrl+H");
        }
        // 5. Selection Menu (Code Editors)
        else if (item.text == L"Selection") {
            AppendMenuW(hMenu, MF_STRING, 4206, L"Select All\tCtrl+A");
            AppendMenuW(hMenu, MF_STRING, 4301, L"Expand Selection\tShift+Alt+Right");
            AppendMenuW(hMenu, MF_STRING, 4302, L"Shrink Selection\tShift+Alt+Left");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4303, L"Copy Line Down\tShift+Alt+Down");
            AppendMenuW(hMenu, MF_STRING, 4304, L"Move Line Down\tAlt+Down");
        }
        // 6. View Menu
        else if (item.text == L"View") {
            AppendMenuW(hMenu, MF_STRING, 3031, L"Mission Control\tF3");
            AppendMenuW(hMenu, MF_STRING, 3034, L"Launchpad\tF4");
            AppendMenuW(hMenu, MF_STRING, 3035, L"Stage Manager\tCtrl+Alt+S");
            AppendMenuW(hMenu, MF_STRING, 3036, L"Notification Center\tCtrl+Alt+N");
            AppendMenuW(hMenu, MF_STRING, 3032, L"Spotlight Search\tAlt+Space");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4401, L"Reload / Refresh\tF5 / Ctrl+R");
            AppendMenuW(hMenu, MF_STRING, 4402, L"Actual Size\tCtrl+0");
            AppendMenuW(hMenu, MF_STRING, 4403, L"Zoom In\tCtrl++");
            AppendMenuW(hMenu, MF_STRING, 4404, L"Zoom Out\tCtrl+-");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4405, L"Enter Full Screen\tF11");
        }
        // 7. Go / History Menu
        else if (item.text == L"Go" || item.text == L"History") {
            AppendMenuW(hMenu, MF_STRING, 4501, L"Back\tAlt+Left");
            AppendMenuW(hMenu, MF_STRING, 4502, L"Forward\tAlt+Right");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            if (m_activeCategory == AppCategory::Browser) {
                AppendMenuW(hMenu, MF_STRING, 4503, L"Show Full History\tCtrl+H");
                AppendMenuW(hMenu, MF_STRING, 4504, L"Downloads\tCtrl+J");
            } else {
                AppendMenuW(hMenu, MF_STRING, 4505, L"Go to Line...\tCtrl+G");
            }
        }
        // 8. Bookmarks Menu
        else if (item.text == L"Bookmarks") {
            AppendMenuW(hMenu, MF_STRING, 4601, L"Bookmark This Tab\tCtrl+D");
            AppendMenuW(hMenu, MF_STRING, 4602, L"Bookmark All Tabs...\tCtrl+Shift+D");
            AppendMenuW(hMenu, MF_STRING, 4603, L"Bookmark Manager\tCtrl+Shift+O");
        }
        // 9. Terminal Menu
        else if (item.text == L"Terminal") {
            AppendMenuW(hMenu, MF_STRING, 4701, L"New Terminal\tCtrl+Shift+`");
            AppendMenuW(hMenu, MF_STRING, 4702, L"Toggle Terminal\tCtrl+`");
        }
        // 10. Window Menu
        else if (item.text == L"Window") {
            AppendMenuW(hMenu, MF_STRING, 4801, L"Minimize\tWin+Down");
            AppendMenuW(hMenu, MF_STRING, 4802, L"Zoom / Maximize\tCtrl+Alt+Up");
            AppendMenuW(hMenu, MF_STRING, 4803, L"Center Window\tCtrl+Alt+Down");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

            HMENU hTileMenu = CreatePopupMenu();
            AppendMenuW(hTileMenu, MF_STRING, 4811, L"Tile to Left\tCtrl+Alt+Left");
            AppendMenuW(hTileMenu, MF_STRING, 4812, L"Tile to Right\tCtrl+Alt+Right");
            AppendMenuW(hTileMenu, MF_STRING, 4813, L"Tile to Top");
            AppendMenuW(hTileMenu, MF_STRING, 4814, L"Tile to Bottom");
            AppendMenuW(hTileMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hTileMenu, MF_STRING, 4821, L"Top Left Quarter\tCtrl+Alt+1");
            AppendMenuW(hTileMenu, MF_STRING, 4822, L"Top Right Quarter\tCtrl+Alt+2");
            AppendMenuW(hTileMenu, MF_STRING, 4823, L"Bottom Left Quarter\tCtrl+Alt+3");
            AppendMenuW(hTileMenu, MF_STRING, 4824, L"Bottom Right Quarter\tCtrl+Alt+4");

            AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hTileMenu), L"Move & Resize (macOS Tiling)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 3031, L"Show All Windows (Mission Control)\tF3");
        }
        // 11. Help Menu
        else {
            AppendMenuW(hMenu, MF_STRING, 4901, (m_activeAppName + L" Help").c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 4902, L"About AquaOS");
        }

        int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, screenX, screenY, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        // Execute Selected Action
        if (cmd == 4001 || cmd == 4901) {
            MessageBoxW(m_hwnd, (m_activeAppName + L"\nRunning inside AquaOS macOS UI Layer").c_str(), m_activeAppName.c_str(), MB_OK | MB_ICONINFORMATION);
        } else if (cmd == 4002) {
            SendShortcut(MOD_CONTROL, VK_OEM_COMMA);
        } else if (cmd == 4003 || cmd == 4801) {
            if (m_foregroundHwnd) ShowWindow(m_foregroundHwnd, SW_MINIMIZE);
        } else if (cmd == 4004) {
            if (m_foregroundHwnd) PostMessageW(m_foregroundHwnd, WM_CLOSE, 0, 0);
        } else if (cmd == 4101) {
            SendShortcut(MOD_CONTROL, 'T');
        } else if (cmd == 4102 || cmd == 4121) {
            SendShortcut(MOD_CONTROL, 'N');
        } else if (cmd == 4103 || cmd == 4112) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'N');
        } else if (cmd == 4104) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'T');
        } else if (cmd == 4105) {
            SendShortcut(MOD_CONTROL, 'P');
        } else if (cmd == 4106) {
            SendShortcut(MOD_CONTROL, 'W');
        } else if (cmd == 4107) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'W');
        } else if (cmd == 4111) {
            SendShortcut(MOD_CONTROL, 'N');
        } else if (cmd == 4113 || cmd == 4122) {
            SendShortcut(MOD_CONTROL, 'O');
        } else if (cmd == 4115) {
            SendShortcut(MOD_CONTROL, 'S');
        } else if (cmd == 4116) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'S');
        } else if (cmd == 4201) {
            SendShortcut(MOD_CONTROL, 'Z');
        } else if (cmd == 4202) {
            SendShortcut(MOD_CONTROL, 'Y');
        } else if (cmd == 4203) {
            SendShortcut(MOD_CONTROL, 'X');
        } else if (cmd == 4204) {
            SendShortcut(MOD_CONTROL, 'C');
        } else if (cmd == 4205) {
            SendShortcut(MOD_CONTROL, 'V');
        } else if (cmd == 4206) {
            SendShortcut(MOD_CONTROL, 'A');
        } else if (cmd == 4207) {
            SendShortcut(MOD_CONTROL, 'F');
        } else if (cmd == 4208) {
            SendShortcut(MOD_CONTROL, 'H');
        } else if (cmd == 3031) {
            if (m_onMissionControl) m_onMissionControl();
        } else if (cmd == 3034) {
            if (m_onLaunchpad) m_onLaunchpad();
        } else if (cmd == 3035) {
            if (m_onStageManager) m_onStageManager();
        } else if (cmd == 3036) {
            if (m_onNotificationCenter) m_onNotificationCenter();
        } else if (cmd == 3032) {
            if (m_onSpotlight) m_onSpotlight();
        } else if (cmd == 4401) {
            SendShortcut(0, VK_F5);
        } else if (cmd == 4402) {
            SendShortcut(MOD_CONTROL, '0');
        } else if (cmd == 4403) {
            SendShortcut(MOD_CONTROL, VK_OEM_PLUS);
        } else if (cmd == 4404) {
            SendShortcut(MOD_CONTROL, VK_OEM_MINUS);
        } else if (cmd == 4405) {
            SendShortcut(0, VK_F11);
        } else if (cmd == 4501) {
            SendShortcut(MOD_ALT, VK_LEFT);
        } else if (cmd == 4502) {
            SendShortcut(MOD_ALT, VK_RIGHT);
        } else if (cmd == 4503) {
            SendShortcut(MOD_CONTROL, 'H');
        } else if (cmd == 4504) {
            SendShortcut(MOD_CONTROL, 'J');
        } else if (cmd == 4505) {
            SendShortcut(MOD_CONTROL, 'G');
        } else if (cmd == 4601) {
            SendShortcut(MOD_CONTROL, 'D');
        } else if (cmd == 4602) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'D');
        } else if (cmd == 4603) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, 'O');
        } else if (cmd == 4701) {
            SendShortcut(MOD_CONTROL | MOD_SHIFT, VK_OEM_3); // Ctrl+Shift+`
        } else if (cmd == 4702) {
            SendShortcut(MOD_CONTROL, VK_OEM_3); // Ctrl+`
        } else if (cmd == 4802) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::Maximize);
        } else if (cmd == 4803) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::Center);
        } else if (cmd == 4811) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::LeftHalf);
        } else if (cmd == 4812) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::RightHalf);
        } else if (cmd == 4813) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::TopHalf);
        } else if (cmd == 4814) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::BottomHalf);
        } else if (cmd == 4821) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::TopLeftQuarter);
        } else if (cmd == 4822) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::TopRightQuarter);
        } else if (cmd == 4823) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::BottomLeftQuarter);
        } else if (cmd == 4824) {
            WindowTiler::Instance().SnapForegroundWindow(SnapZone::BottomRightQuarter);
        } else if (cmd == 4902) {
            MessageBoxW(m_hwnd, L"AquaOS Desktop Shell v1.0\nBuilt with C++20 & DirectComposition\nmacOS Dynamic Application Menubar", L"About AquaOS", MB_OK | MB_ICONINFORMATION);
        }
    }

    void TopBar::DrawLogo(ID2D1DeviceContext* ctx, float x, float y, float size) {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        // Top Leaf
        D2D1_ELLIPSE leaf = D2D1::Ellipse(D2D1::Point2F(cx + 1.5f, cy - size * 0.38f), size * 0.16f, size * 0.10f);
        ctx->FillEllipse(leaf, m_brushTextPrimary.Get());

        // Body
        D2D1_ELLIPSE bodyLeft = D2D1::Ellipse(D2D1::Point2F(cx - size * 0.15f, cy + size * 0.05f), size * 0.28f, size * 0.32f);
        D2D1_ELLIPSE bodyRight = D2D1::Ellipse(D2D1::Point2F(cx + size * 0.15f, cy + size * 0.05f), size * 0.28f, size * 0.32f);
        ctx->FillEllipse(bodyLeft, m_brushTextPrimary.Get());
        ctx->FillEllipse(bodyRight, m_brushTextPrimary.Get());
    }

    void TopBar::DrawWifiIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.65f;

        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 1.8f, 1.8f), m_brushTextPrimary.Get());

        D2D1_ELLIPSE arc1 = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.32f, size * 0.32f);
        ctx->DrawEllipse(arc1, m_brushTextPrimary.Get(), 1.5f);

        D2D1_ELLIPSE arc2 = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.52f, size * 0.52f);
        ctx->DrawEllipse(arc2, m_brushTextPrimary.Get(), 1.5f);
    }

    void TopBar::DrawBatteryIcon(ID2D1DeviceContext* ctx, float x, float y, float width, float height, int percent, bool charging) {
        float bodyW = width - 5.0f;
        float bodyH = height - 8.0f;
        float by = y + 4.0f;

        D2D1_ROUNDED_RECT shell = D2D1::RoundedRect(D2D1::RectF(x, by, x + bodyW, by + bodyH), 3.0f, 3.0f);
        ctx->DrawRoundedRectangle(shell, m_brushTextSecondary.Get(), 1.2f);

        D2D1_ROUNDED_RECT tip = D2D1::RoundedRect(D2D1::RectF(x + bodyW + 1.0f, by + bodyH * 0.25f, x + bodyW + 3.0f, by + bodyH * 0.75f), 1.0f, 1.0f);
        ctx->FillRoundedRectangle(tip, m_brushTextSecondary.Get());

        if (percent > 0) {
            float pct = static_cast<float>(percent) / 100.0f;
            float fillW = (bodyW - 4.0f) * pct;
            D2D1_ROUNDED_RECT fill = D2D1::RoundedRect(D2D1::RectF(x + 2.0f, by + 2.0f, x + 2.0f + fillW, by + bodyH - 2.0f), 1.5f, 1.5f);
            ctx->FillRoundedRectangle(fill, charging ? m_brushBatteryGreen.Get() : m_brushTextPrimary.Get());
        }
    }

    void TopBar::DrawSearchIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        float cx = x + size * 0.42f;
        float cy = y + size * 0.42f;
        float r = size * 0.24f;

        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), m_brushTextPrimary.Get(), 1.5f);

        float hx1 = cx + r * 0.7f;
        float hy1 = cy + r * 0.7f;
        float hx2 = cx + r * 1.55f;
        float hy2 = cy + r * 1.55f;
        ctx->DrawLine(D2D1::Point2F(hx1, hy1), D2D1::Point2F(hx2, hy2), m_brushTextPrimary.Get(), 1.8f);
    }

    void TopBar::DrawControlCenterIcon(ID2D1DeviceContext* ctx, float x, float y, float size) {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        float barW = size * 0.55f;
        float barH = 3.2f;

        ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - barW * 0.5f, cy - 5.0f, cx + barW * 0.5f, cy - 5.0f + barH), 1.6f, 1.6f), m_brushTextPrimary.Get());
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + barW * 0.2f, cy - 3.4f), 3.2f, 3.2f), m_brushTextPrimary.Get());

        ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - barW * 0.5f, cy + 2.5f, cx + barW * 0.5f, cy + 2.5f + barH), 1.6f, 1.6f), m_brushTextPrimary.Get());
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - barW * 0.2f, cy + 4.1f), 3.2f, 3.2f), m_brushTextPrimary.Get());
    }

    void TopBar::DrawNowPlayingIcon(ID2D1DeviceContext* ctx, float x, float y, float size, bool isPlaying) {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        float barW = 2.4f * m_dpiScale;
        float gap = 3.2f * m_dpiScale;
        float bar1H = isPlaying ? (10.0f * m_dpiScale) : (5.0f * m_dpiScale);
        float bar2H = isPlaying ? (15.0f * m_dpiScale) : (8.0f * m_dpiScale);
        float bar3H = isPlaying ? (12.0f * m_dpiScale) : (6.0f * m_dpiScale);

        auto drawBar = [&](float bx, float bh) {
            ctx->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(bx - barW * 0.5f, cy - bh * 0.5f, bx + barW * 0.5f, cy + bh * 0.5f), 1.2f, 1.2f),
                isPlaying ? m_brushTextPrimary.Get() : m_brushTextSecondary.Get()
            );
        };

        drawBar(cx - gap, bar1H);
        drawBar(cx, bar2H);
        drawBar(cx + gap, bar3H);
    }

    void TopBar::Render() {
        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Draw Frosted Glass TopBar Background
        ctx->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(m_width), static_cast<float>(m_height)), m_brushGlassBg.Get());

        // 2. Draw Bottom Specular Line
        ctx->DrawLine(
            D2D1::Point2F(0, static_cast<float>(m_height) - 0.5f),
            D2D1::Point2F(static_cast<float>(m_width), static_cast<float>(m_height) - 0.5f),
            m_brushBorder.Get(),
            1.0f
        );

        // 3. Draw Apple Logo
        if (m_logoHovered) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(m_logoRect, 4.0f, 4.0f), m_brushHover.Get());
        }
        DrawLogo(ctx, m_logoRect.left, m_logoRect.top + 2.0f, 20.0f * m_dpiScale);

        // 4. Draw Left Dynamic Menu Items (Active App Name in Bold + Real Menus)
        for (size_t i = 0; i < m_leftMenuItems.size(); ++i) {
            auto& item = m_leftMenuItems[i];
            if (item.isHovered) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(item.bounds, 4.0f, 4.0f), m_brushHover.Get());
            }

            IDWriteTextFormat* format = (i == 0) ? m_textFormatBold.Get() : m_textFormatRegular.Get();
            D2D1_RECT_F textRect = D2D1::RectF(item.bounds.left + 4.0f, item.bounds.top, item.bounds.right - 4.0f, item.bounds.bottom);

            ctx->DrawTextW(
                item.text.c_str(),
                static_cast<UINT32>(item.text.length()),
                format,
                textRect,
                m_brushTextPrimary.Get()
            );
        }

        // 5. Draw Right Status Items
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

        // Now Playing Media Widget Button (when media session is active)
        auto track = MediaControllerService::Instance().GetCurrentTrack();
        if (track.hasMedia) {
            if (m_nowPlayingHovered) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(m_nowPlayingRect, 4.0f, 4.0f), m_brushHover.Get());
            }
            DrawNowPlayingIcon(ctx, m_nowPlayingRect.left + 3.0f, m_nowPlayingRect.top + 3.0f, 20.0f * m_dpiScale, track.isPlaying);
        }

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
        case WM_TIMER: {
            m_topmostTicks++;
            if (m_topmostTicks >= 4) { // every ~1.2s
                m_topmostTicks = 0;
                SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                if (TaskbarController::Instance().IsSuppressed()) {
                    TaskbarController::Instance().SuppressTaskbar();
                }
            }
            SyncForegroundApp();
            Render();
            return 0;
        }

        case WM_MOUSEMOVE: {
            float mx = static_cast<float>(GET_X_LPARAM(lParam));
            float my = static_cast<float>(GET_Y_LPARAM(lParam));

            bool needRepaint = false;

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);

            bool logoH = (mx >= m_logoRect.left && mx <= m_logoRect.right && my >= m_logoRect.top && my <= m_logoRect.bottom);
            if (logoH != m_logoHovered) { m_logoHovered = logoH; needRepaint = true; }

            for (auto& item : m_leftMenuItems) {
                bool h = (mx >= item.bounds.left && mx <= item.bounds.right && my >= item.bounds.top && my <= item.bounds.bottom);
                if (h != item.isHovered) {
                    item.isHovered = h;
                    needRepaint = true;
                }
            }

            bool sH = (mx >= m_searchRect.left && mx <= m_searchRect.right);
            if (sH != m_searchHovered) { m_searchHovered = sH; needRepaint = true; }

            bool ccH = (mx >= m_controlCenterRect.left && mx <= m_controlCenterRect.right);
            if (ccH != m_controlCenterHovered) { m_controlCenterHovered = ccH; needRepaint = true; }

            bool npH = (mx >= m_nowPlayingRect.left && mx <= m_nowPlayingRect.right);
            if (npH != m_nowPlayingHovered) { m_nowPlayingHovered = npH; needRepaint = true; }

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
            m_nowPlayingHovered = false;
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

            // 1. Apple Logo -> System Menu
            if (mx >= m_logoRect.left && mx <= m_logoRect.right) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, 2001, L"About AquaOS");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2002, L"System Settings...");
                AppendMenuW(hMenu, MF_STRING, 2003, L"Task Manager (Activity Monitor)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2021, L"Dynamic Wallpaper Style\tCtrl+Alt+W");
                AppendMenuW(hMenu, MF_STRING, 2022, L"Toggle Dark / Light Theme\tCtrl+Alt+D");
                AppendMenuW(hMenu, MF_STRING, 2023, L"Stage Manager\tCtrl+Alt+S");
                AppendMenuW(hMenu, MF_STRING, 2024, L"Mission Control\tF3");
                AppendMenuW(hMenu, MF_STRING, 2025, L"Launchpad\tF4");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 2004, L"Lock Screen\tCtrl+Alt+L");
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
                    MessageBoxW(m_hwnd, L"AquaOS Desktop Shell v2.0 (macOS Sequoia Experience)\nPure C++20 + DirectComposition + Direct2D\n\nShortcuts & Features:\n• Alt + Space: Spotlight Search & App Launcher\n• F3: Mission Control (Window Exposé Grid)\n• F4: Launchpad (Fullscreen App Grid)\n• Ctrl + Alt + S: Stage Manager Window Stacks\n• Ctrl + Alt + N: Notification Center & Widgets\n• Spacebar: Quick Look File Preview\n• Ctrl + Alt + Arrows/1-4: Sequoia Window Tiling\n• Ctrl + Alt + W: Dynamic Wallpaper Style\n• Ctrl + Alt + D: Toggle Dark/Light Mode\n• Ctrl + Alt + L: Lock Screen & Screensaver\n• Ctrl + Alt + T: Toggle Windows Taskbar", L"About AquaOS", MB_OK | MB_ICONINFORMATION);
                } else if (cmd == 2002) {
                    ShellExecuteW(nullptr, L"open", L"ms-settings:", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (cmd == 2003) {
                    ShellExecuteW(nullptr, L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (cmd == 2021) {
                    ThemeManager::Instance().CycleWallpaper();
                } else if (cmd == 2022) {
                    ThemeManager::Instance().ToggleTheme();
                } else if (cmd == 2023) {
                    if (m_onStageManager) m_onStageManager();
                } else if (cmd == 2024) {
                    if (m_onMissionControl) m_onMissionControl();
                } else if (cmd == 2025) {
                    if (m_onLaunchpad) m_onLaunchpad();
                } else if (cmd == 2004) {
                    LockScreen::Instance().Show();
                } else if (cmd == 2005) {
                    SetSuspendState(FALSE, FALSE, FALSE);
                } else if (cmd == 2006) {
                    if (MessageBoxW(m_hwnd, L"Are you sure you want to restart your computer?", L"Restart", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        TaskbarController::Instance().RestoreTaskbar();
                        system("shutdown /r /t 0");
                    }
                } else if (cmd == 2007) {
                    if (MessageBoxW(m_hwnd, L"Are you sure you want to shut down your computer?", L"Shut Down", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        TaskbarController::Instance().RestoreTaskbar();
                        system("shutdown /s /t 0");
                    }
                } else if (cmd == 2010) {
                    TaskbarController::Instance().ToggleTaskbar();
                } else if (cmd == 2099) {
                    TaskbarController::Instance().RestoreTaskbar();
                    PostQuitMessage(0);
                }
                return 0;
            }

            // 2. Search Icon
            if (mx >= m_searchRect.left && mx <= m_searchRect.right) {
                if (m_onSpotlight) m_onSpotlight();
                return 0;
            }

            // 3. Control Center Icon
            if (mx >= m_controlCenterRect.left && mx <= m_controlCenterRect.right) {
                if (m_onControlCenter) m_onControlCenter();
                return 0;
            }

            // 3b. Now Playing Icon -> Open Control Center or Toggle Play/Pause
            if (mx >= m_nowPlayingRect.left && mx <= m_nowPlayingRect.right) {
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                    MediaControllerService::Instance().TogglePlayPause();
                } else if (m_onControlCenter) {
                    m_onControlCenter();
                }
                return 0;
            }

            // 4. Wi-Fi Status -> Network Settings
            if (mx >= m_wifiRect.left && mx <= m_wifiRect.right) {
                ShellExecuteW(nullptr, L"open", L"ms-settings:network-wifi", nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }

            // 5. Battery Status -> Power Settings
            if (mx >= m_batteryRect.left && mx <= m_batteryRect.right) {
                ShellExecuteW(nullptr, L"open", L"ms-settings:powersleep", nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }

            // 6. Clock -> Notification Center
            if (mx >= m_clockRect.left && mx <= m_clockRect.right) {
                if (m_onNotificationCenter) m_onNotificationCenter();
                return 0;
            }

            // 5. Dynamic Menu item clicks
            for (size_t i = 0; i < m_leftMenuItems.size(); ++i) {
                auto& item = m_leftMenuItems[i];
                if (mx >= item.bounds.left && mx <= item.bounds.right) {
                    POINT pt = { static_cast<LONG>(item.bounds.left), static_cast<LONG>(m_height) };
                    ClientToScreen(m_hwnd, &pt);
                    ShowMenuForIndex(i, pt.x, pt.y);
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
