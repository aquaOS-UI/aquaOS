#include "Dock.h"
#include "IconManager.h"
#include "TaskbarController.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <unordered_map>

namespace Aqua {

    const wchar_t DOCK_CLASS_NAME[] = L"AquaDockWindow";

    static std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
        return str;
    }

    static std::wstring GetFileName(const std::wstring& path) {
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            return path.substr(lastSlash + 1);
        }
        return path;
    }

    static std::wstring QueryDefaultBrowserPath() {
        wchar_t buf[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        if (SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, L"http", L"open", buf, &len)) && buf[0] != L'\0') {
            return buf;
        }
        return L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
    }

    static bool IsAquaWindow(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd)) return false;
        wchar_t cls[128] = {};
        GetClassNameW(hwnd, cls, 128);
        std::wstring sCls(cls);
        return (sCls.find(L"Aqua") != std::wstring::npos ||
                sCls == L"Progman" || sCls == L"WorkerW" ||
                sCls == L"Shell_TrayWnd" || sCls == L"Shell_SecondaryTrayWnd");
    }

    static void ForceForegroundWindow(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd)) return;

        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        } else {
            ShowWindow(hwnd, SW_SHOW);
        }

        HWND fg = GetForegroundWindow();
        DWORD foreThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
        DWORD appThread = GetCurrentThreadId();

        if (foreThread != 0 && foreThread != appThread) {
            AttachThreadInput(foreThread, appThread, TRUE);
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
            AttachThreadInput(foreThread, appThread, FALSE);
        } else {
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
        }

        SwitchToThisWindow(hwnd, TRUE);
    }

    struct AppWindowInfo {
        HWND hwnd = nullptr;
        std::wstring processPath;
        std::wstring processExe;
        std::wstring className;
        std::wstring title;
        bool isIconic = false;
    };

    static std::vector<AppWindowInfo> EnumerateAppWindows() {
        std::vector<AppWindowInfo> result;

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* pList = reinterpret_cast<std::vector<AppWindowInfo>*>(lParam);

            if (!IsWindow(hwnd)) return TRUE;
            if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) return TRUE;

            // Skip our own AquaShell windows and system desktop
            if (IsAquaWindow(hwnd)) return TRUE;

            LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

            // Skip tool windows unless they have WS_EX_APPWINDOW
            if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) return TRUE;

            // Skip owned windows unless they have WS_EX_APPWINDOW
            HWND owner = GetWindow(hwnd, GW_OWNER);
            if (owner != nullptr && !(exStyle & WS_EX_APPWINDOW)) return TRUE;

            // Cloaked check (Windows 10/11 virtual desktops & suspended apps)
            // Minimized windows can report cloaked by shell; do not skip them if iconic
            int cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
                if (cloaked != 0 && !IsIconic(hwnd)) return TRUE;
            }

            wchar_t classBuf[256] = {};
            GetClassNameW(hwnd, classBuf, 256);
            std::wstring cls = classBuf;

            // Skip known system background classes
            if (cls == L"Windows.UI.Core.CoreWindow" ||
                cls == L"ApplicationManager_DesktopShellWindow" ||
                cls == L"XamlExplorerHostIslandWindow" ||
                cls == L"EdgeUiInputWndClass" ||
                cls == L"DummyDWMListenerWindow") {
                return TRUE;
            }

            // Window dimension filter: skip 0x0 or degenerate helper windows
            if (!IsIconic(hwnd)) {
                RECT rc = {};
                GetWindowRect(hwnd, &rc);
                if ((rc.right - rc.left) < 24 || (rc.bottom - rc.top) < 24) return TRUE;
            }

            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

            std::wstring procPath = L"";
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t pathBuf[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &sz)) {
                    procPath = pathBuf;
                }
                CloseHandle(hProc);
            }

            std::wstring procExe = ToLower(GetFileName(procPath));

            // For UWP apps hosted in ApplicationFrameHost, discover the actual child process
            if (procExe == L"applicationframehost.exe") {
                EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
                    DWORD cPid = 0;
                    GetWindowThreadProcessId(child, &cPid);
                    if (cPid != 0 && cPid != GetCurrentProcessId()) {
                        HANDLE hCProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, cPid);
                        if (hCProc) {
                            wchar_t cBuf[MAX_PATH] = {};
                            DWORD cSz = MAX_PATH;
                            if (QueryFullProcessImageNameW(hCProc, 0, cBuf, &cSz)) {
                                std::wstring* pOut = reinterpret_cast<std::wstring*>(lp);
                                std::wstring cExe = ToLower(GetFileName(cBuf));
                                if (cExe != L"applicationframehost.exe") {
                                    *pOut = cBuf;
                                    CloseHandle(hCProc);
                                    return FALSE;
                                }
                            }
                            CloseHandle(hCProc);
                        }
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&procPath));
                procExe = ToLower(GetFileName(procPath));
            }

            wchar_t titleBuf[512] = {};
            GetWindowTextW(hwnd, titleBuf, 512);

            AppWindowInfo info;
            info.hwnd = hwnd;
            info.processPath = procPath;
            info.processExe = procExe;
            info.className = cls;
            info.title = titleBuf;
            info.isIconic = IsIconic(hwnd) != FALSE;

            pList->push_back(info);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&result));

        return result;
    }

    static bool DoesWindowMatchItem(const AppWindowInfo& w, const DockItem& item) {
        if (item.type == DockAppType::Separator || item.type == DockAppType::Launchpad) {
            return false;
        }

        // Unpinned dynamic apps match directly by unique appKey
        if (!item.isPinned && !item.appKey.empty()) {
            std::wstring currentKey = !w.processPath.empty() ? ToLower(w.processPath) : (!w.processExe.empty() ? ToLower(w.processExe) : ToLower(w.title));
            return currentKey == item.appKey;
        }

        std::wstring lowerTitle = ToLower(w.title);
        std::wstring lowerItemName = ToLower(item.name);
        std::wstring itemExe = ToLower(GetFileName(item.executePath));
        std::wstring iconExe = ToLower(GetFileName(item.iconPath));

        size_t qPos = itemExe.find(L'?');
        if (qPos != std::wstring::npos) itemExe = itemExe.substr(0, qPos);

        switch (item.type) {
        case DockAppType::Finder:
        case DockAppType::Explorer: {
            if (w.className == L"CabinetWClass" || w.className == L"ExploreWClass") {
                bool isTrash = (lowerTitle.find(L"recycle bin") != std::wstring::npos ||
                                lowerTitle.find(L"trash") != std::wstring::npos ||
                                lowerTitle.find(L"papierkorb") != std::wstring::npos ||
                                lowerTitle.find(L"corbeille") != std::wstring::npos);
                return !isTrash;
            }
            return false;
        }

        case DockAppType::Trash: {
            if (w.className == L"CabinetWClass" || w.className == L"ExploreWClass") {
                return (lowerTitle.find(L"recycle bin") != std::wstring::npos ||
                        lowerTitle.find(L"trash") != std::wstring::npos ||
                        lowerTitle.find(L"papierkorb") != std::wstring::npos ||
                        lowerTitle.find(L"corbeille") != std::wstring::npos);
            }
            return false;
        }

        case DockAppType::Browser: {
            // Check direct executable match
            if (!itemExe.empty() && itemExe.find(L".exe") != std::wstring::npos && w.processExe == itemExe) {
                return true;
            }
            if (!iconExe.empty() && iconExe.find(L".exe") != std::wstring::npos && w.processExe == iconExe) {
                return true;
            }
            // General browser process match
            if (w.processExe == L"msedge.exe" || w.processExe == L"chrome.exe" ||
                w.processExe == L"firefox.exe" || w.processExe == L"brave.exe" ||
                w.processExe == L"opera.exe" || w.processExe == L"vivaldi.exe" ||
                w.processExe == L"arc.exe" || w.processExe == L"zen.exe" ||
                w.processExe == L"waterfox.exe" || w.processExe == L"chromium.exe") {
                return true;
            }
            return false;
        }

        case DockAppType::Terminal: {
            if (w.processExe == L"windowsterminal.exe" || w.processExe == L"wt.exe" ||
                w.processExe == L"cmd.exe" || w.processExe == L"powershell.exe" ||
                w.processExe == L"pwsh.exe") {
                return true;
            }
            if (w.className == L"CASCADIA_HOSTING_WINDOW_CLASS" || w.className == L"ConsoleWindowClass") {
                return true;
            }
            return false;
        }

        case DockAppType::CodeEditor: {
            if (w.processExe == L"code.exe" || w.processExe == L"devenv.exe" ||
                w.processExe == L"idea64.exe" || w.processExe == L"sublime_text.exe" ||
                w.processExe == L"clion64.exe") {
                return true;
            }
            if (!itemExe.empty() && itemExe.find(L".exe") != std::wstring::npos && w.processExe == itemExe) {
                return true;
            }
            return false;
        }

        case DockAppType::Calculator: {
            if (w.processExe == L"calculatorapp.exe" || w.processExe == L"calc.exe") {
                return true;
            }
            if (w.className == L"ApplicationFrameWindow" &&
                (lowerTitle.find(L"calculator") != std::wstring::npos || lowerTitle.find(L"calc") != std::wstring::npos)) {
                return true;
            }
            return false;
        }

        case DockAppType::ActivityMonitor: {
            if (w.processExe == L"taskmgr.exe" || w.className == L"TaskManagerWindow") {
                return true;
            }
            return false;
        }

        case DockAppType::Settings: {
            if (w.processExe == L"systemsettings.exe") {
                return true;
            }
            if (w.className == L"ApplicationFrameWindow" && lowerTitle.find(L"settings") != std::wstring::npos) {
                return true;
            }
            return false;
        }

        case DockAppType::CustomApp:
        default: {
            std::wstring resolved = IconManager::Instance().ResolvePath(item.executePath);
            std::wstring resolvedExe = ToLower(GetFileName(resolved));
            if (!resolvedExe.empty() && w.processExe == resolvedExe) {
                return true;
            }
            if (!itemExe.empty() && itemExe.find(L".exe") != std::wstring::npos && w.processExe == itemExe) {
                return true;
            }
            if (!iconExe.empty() && iconExe.find(L".exe") != std::wstring::npos && w.processExe == iconExe) {
                return true;
            }
            if (!lowerItemName.empty()) {
                if (w.processExe.find(lowerItemName) != std::wstring::npos) {
                    return true;
                }
                if (lowerTitle.find(lowerItemName) != std::wstring::npos) {
                    return true;
                }
            }
            return false;
        }
        }
    }

    Dock::Dock() {
        LoadConfiguration();
        if (m_items.empty()) {
            std::wstring browserPath = QueryDefaultBrowserPath();
            m_items = {
                { DockAppType::Finder,          L"Finder",             L"explorer.exe",       L"", L"C:\\Windows\\explorer.exe", false, true, nullptr, L"" },
                { DockAppType::Launchpad,       L"Launchpad",          L"",                   L"", L"", false, true, nullptr, L"" },
                { DockAppType::Browser,         L"Browser",            browserPath,           L"", browserPath, false, true, nullptr, L"" },
                { DockAppType::Terminal,        L"Terminal",           L"wt.exe",             L"", L"wt.exe", false, true, nullptr, L"" },
                { DockAppType::CodeEditor,      L"Visual Studio Code", L"code.exe",           L"", L"code.exe", false, true, nullptr, L"" },
                { DockAppType::Calculator,      L"Calculator",         L"calc.exe",           L"", L"C:\\Windows\\System32\\calc.exe", false, true, nullptr, L"" },
                { DockAppType::ActivityMonitor, L"Activity Monitor",   L"taskmgr.exe",        L"", L"C:\\Windows\\System32\\Taskmgr.exe", false, true, nullptr, L"" },
                { DockAppType::Settings,        L"System Settings",    L"ms-settings:",       L"", L"C:\\Windows\\ImmersiveControlPanel\\SystemSettings.exe", false, true, nullptr, L"" },
                { DockAppType::Separator,       L"",                   L"",                   L"", L"", false, true, nullptr, L"" },
                { DockAppType::Trash,           L"Trash",              L"explorer.exe",       L"shell:RecycleBinFolder", L"C:\\Windows\\System32\\imageres.dll,-54", false, true, nullptr, L"" }
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
        // Query primary monitor for initial Dock placement
        HMONITOR hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            m_currentMonitor = hMon;
            m_monitorRect = mi.rcMonitor;
            m_screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
            m_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
        } else {
            m_screenWidth = screenWidth;
            m_screenHeight = screenHeight;
            m_monitorRect = { 0, 0, screenWidth, screenHeight };
        }

        m_windowWidth = 840;
        m_windowHeight = 120;
        m_windowX = m_monitorRect.left + (m_screenWidth - m_windowWidth) / 2;
        m_windowY = m_monitorRect.bottom - m_windowHeight - 6;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = Dock::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = DOCK_CLASS_NAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
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

        // Persistent Icon Palette Brushes (zero allocations per frame)
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushAccentBlue);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.44f, 0.82f), &m_brushFinderDarkBlue);
        ctx->CreateSolidColorBrush(Colors::AccentRed, &m_brushAccentRed);
        ctx->CreateSolidColorBrush(Colors::AccentOrange, &m_brushAccentOrange);
        ctx->CreateSolidColorBrush(Colors::AccentGreen, &m_brushAccentGreen);
        ctx->CreateSolidColorBrush(Colors::AccentPurple, &m_brushAccentPurple);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f), &m_brushDarkTile);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.72f, 0.76f), &m_brushLightGray);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.35f, 0.38f), &m_brushGearDark);

        AdjustWindowWidthForItems();
        CalculateLayout();
        UpdateRunningStates();

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

    void Dock::AdjustWindowWidthForItems() {
        float totalItemWidth = 0.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].type == DockAppType::Separator) {
                totalItemWidth += 16.0f * m_dpiScale;
            } else {
                totalItemWidth += m_baseIconSize;
            }
            if (i < m_items.size() - 1) {
                totalItemWidth += m_iconSpacing;
            }
        }

        // Add padding for magnification and margins
        int neededWidth = static_cast<int>(totalItemWidth + 140.0f * m_dpiScale);
        int maxWidth = m_screenWidth - 40;
        if (neededWidth > maxWidth) neededWidth = maxWidth;
        if (neededWidth < 840) neededWidth = 840;

        if (neededWidth != m_windowWidth) {
            m_windowWidth = neededWidth;
            m_windowX = m_monitorRect.left + (m_screenWidth - m_windowWidth) / 2;
            SetWindowPos(m_hwnd, HWND_TOPMOST, m_windowX, m_windowY, m_windowWidth, m_windowHeight, SWP_NOACTIVATE);
            m_surface.Resize(m_windowWidth, m_windowHeight);
        }
    }

    std::vector<HWND> Dock::GetWindowsForItem(const DockItem& item) {
        std::vector<HWND> matches;
        auto windows = EnumerateAppWindows();

        if (!item.isPinned && !item.appKey.empty()) {
            for (const auto& w : windows) {
                std::wstring key = !w.processPath.empty() ? ToLower(w.processPath) : (!w.processExe.empty() ? ToLower(w.processExe) : ToLower(w.title));
                if (key == item.appKey) {
                    matches.push_back(w.hwnd);
                }
            }
            if (!matches.empty()) return matches;
        }

        for (const auto& w : windows) {
            if (DoesWindowMatchItem(w, item)) {
                matches.push_back(w.hwnd);
            }
        }
        return matches;
    }

    void Dock::UpdateRunningStates() {
        auto windows = EnumerateAppWindows();
        bool layoutChanged = false;

        // 1. Reset running state on pinned items
        for (auto& item : m_items) {
            if (item.isPinned) {
                if (item.type == DockAppType::Separator || item.type == DockAppType::Launchpad) {
                    item.isRunning = false;
                    item.primaryHwnd = nullptr;
                    continue;
                }
                if (item.isBouncing) {
                    item.isRunning = true;
                    continue;
                }
                item.isRunning = false;
                item.primaryHwnd = nullptr;
            }
        }

        // 2. Track which windows are matched to pinned items
        std::vector<bool> windowMatched(windows.size(), false);

        for (size_t wi = 0; wi < windows.size(); ++wi) {
            const auto& w = windows[wi];
            for (auto& item : m_items) {
                if (!item.isPinned) continue;
                if (DoesWindowMatchItem(w, item)) {
                    item.isRunning = true;
                    if (!item.primaryHwnd) {
                        item.primaryHwnd = w.hwnd;
                    }
                    windowMatched[wi] = true;
                    break;
                }
            }
        }

        // 3. For unmatched windows, group them by application key
        std::unordered_map<std::wstring, std::vector<size_t>> unmatchedApps;
        for (size_t wi = 0; wi < windows.size(); ++wi) {
            if (windowMatched[wi]) continue;
            const auto& w = windows[wi];

            std::wstring key;
            if (!w.processPath.empty()) {
                key = ToLower(w.processPath);
            } else if (!w.processExe.empty()) {
                key = ToLower(w.processExe);
            } else {
                key = ToLower(w.title);
            }

            if (key.empty()) continue;
            unmatchedApps[key].push_back(wi);
        }

        // 4. Update existing dynamic items or remove them if their windows closed
        for (auto it = m_items.begin(); it != m_items.end(); ) {
            if (!it->isPinned) {
                auto appIt = unmatchedApps.find(it->appKey);
                if (appIt != unmatchedApps.end()) {
                    // Application is still running
                    it->isRunning = true;
                    const auto& firstWin = windows[appIt->second[0]];
                    it->primaryHwnd = firstWin.hwnd;
                    unmatchedApps.erase(appIt);
                    ++it;
                } else {
                    // Application has closed! Remove it from dock
                    it = m_items.erase(it);
                    layoutChanged = true;
                }
            } else {
                ++it;
            }
        }

        // 5. Add new dynamic items for any remaining unmatched apps
        // Insert right before Separator or Trash
        for (const auto& [key, winIndices] : unmatchedApps) {
            const auto& w = windows[winIndices[0]];

            DockItem newItem;
            newItem.type = DockAppType::CustomApp;
            newItem.isPinned = false;
            newItem.isRunning = true;
            newItem.appKey = key;
            newItem.primaryHwnd = w.hwnd;
            newItem.executePath = w.processPath;
            newItem.iconPath = w.processPath;
            newItem.name = IconManager::Instance().GetAppFriendlyName(w.processPath, w.title);

            // Smooth entry animation
            newItem.currentScale = 0.5f;
            newItem.targetScale = 1.0f;
            newItem.currentSize = m_baseIconSize * 0.5f;

            auto insertIt = std::find_if(m_items.begin(), m_items.end(), [](const DockItem& itm) {
                return itm.type == DockAppType::Separator || itm.type == DockAppType::Trash;
            });

            if (insertIt != m_items.end()) {
                m_items.insert(insertIt, newItem);
            } else {
                m_items.push_back(newItem);
            }
            layoutChanged = true;
        }

        IconManager::Instance().PruneClosedHwnds();

        if (layoutChanged) {
            AdjustWindowWidthForItems();
            CalculateLayout();
        }
        m_needsRedraw = true;
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

    void Dock::SummonToMonitor(HMONITOR hMon) {
        if (!hMon || hMon == m_currentMonitor) return;

        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (!GetMonitorInfoW(hMon, &mi)) return;

        m_currentMonitor = hMon;
        m_monitorRect = mi.rcMonitor;
        m_screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        m_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

        m_windowX = m_monitorRect.left + (m_screenWidth - m_windowWidth) / 2;
        m_windowY = m_monitorRect.bottom - m_windowHeight - 6;

        SetWindowPos(m_hwnd, HWND_TOPMOST, m_windowX, m_windowY, m_windowWidth, m_windowHeight, SWP_NOACTIVATE);
        m_needsRedraw = true;
    }

    void Dock::UpdatePhysics() {
        // Track last active non-Aqua foreground window
        HWND curFg = GetForegroundWindow();
        if (curFg && curFg != m_hwnd && !IsAquaWindow(curFg)) {
            m_lastActiveAppHwnd = curFg;
        }

        // Multi-monitor Dock Summoning:
        // When cursor pushes against the bottom edge of ANY monitor, summon Dock to that monitor!
        POINT cursorPt;
        if (GetCursorPos(&cursorPt)) {
            HMONITOR cursorMon = MonitorFromPoint(cursorPt, MONITOR_DEFAULTTONEAREST);
            if (cursorMon && cursorMon != m_currentMonitor) {
                MONITORINFO mi = { sizeof(MONITORINFO) };
                if (GetMonitorInfoW(cursorMon, &mi)) {
                    if (cursorPt.y >= mi.rcMonitor.bottom - 8 &&
                        cursorPt.x >= mi.rcMonitor.left &&
                        cursorPt.x <= mi.rcMonitor.right) {
                        SummonToMonitor(cursorMon);
                    }
                }
            }
        }

        // Periodic Z-Order enforcement (every ~1s / 60 frames)
        m_topmostTicks++;
        if (m_topmostTicks >= 60) {
            m_topmostTicks = 0;
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        // Periodically refresh running indicators & dynamic apps (~every 480ms at 60fps)
        m_stateUpdateFrame++;
        if (m_stateUpdateFrame >= 30) {
            m_stateUpdateFrame = 0;
            UpdateRunningStates();
        }

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
        int oldHovered = m_hoveredIndex;
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
        if (oldHovered != m_hoveredIndex) {
            animated = true;
        }

        if (animated || m_isHovered || m_needsRedraw) {
            m_needsRedraw = false;
            Render();
        }
    }

    void Dock::DrawIcon(ID2D1DeviceContext* ctx, const DockItem& item, float x, float y, float size) {
        float r = size * 0.22f;
        D2D1_ROUNDED_RECT iconBox = D2D1::RoundedRect(D2D1::RectF(x, y, x + size, y + size), r, r);
        int iconPx = static_cast<int>(size * m_dpiScale);

        // 1. If we have a live primary HWND, extract live window icon
        if (item.primaryHwnd && IsWindow(item.primaryHwnd)) {
            auto bmp = IconManager::Instance().GetIconForHwnd(ctx, item.primaryHwnd, iconPx);
            if (bmp) {
                ctx->DrawBitmap(bmp.Get(), D2D1::RectF(x, y, x + size, y + size));
                return;
            }
        }

        // 2. If item is marked running, check all its running HWNDs
        if (item.isRunning) {
            auto hwnds = GetWindowsForItem(item);
            for (HWND h : hwnds) {
                auto bmp = IconManager::Instance().GetIconForHwnd(ctx, h, iconPx);
                if (bmp) {
                    ctx->DrawBitmap(bmp.Get(), D2D1::RectF(x, y, x + size, y + size));
                    return;
                }
            }
        }

        // 3. Try real Windows icon path
        if (!item.iconPath.empty()) {
            auto bmp = IconManager::Instance().GetIconForPath(ctx, item.iconPath, iconPx);
            if (bmp) {
                ctx->DrawBitmap(bmp.Get(), D2D1::RectF(x, y, x + size, y + size));
                return;
            }
        }

        // 4. Try executePath
        if (!item.executePath.empty()) {
            auto bmp = IconManager::Instance().GetIconForPath(ctx, item.executePath, iconPx);
            if (bmp) {
                ctx->DrawBitmap(bmp.Get(), D2D1::RectF(x, y, x + size, y + size));
                return;
            }
        }

        // 5. Custom App Fallback tile
        if (item.type == DockAppType::CustomApp) {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentBlue.Get());
            if (!item.name.empty()) {
                D2D1_RECT_F tr = D2D1::RectF(x, y, x + size, y + size);
                ctx->DrawTextW(item.name.substr(0, 1).c_str(), 1, m_tooltipFormat.Get(), tr, m_brushTextPrimary.Get());
            }
            return;
        }

        // 6. Procedural Fallback Drawings (only for standard items if no icon found)
        switch (item.type) {
        case DockAppType::Finder: {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentBlue.Get());
            D2D1_ROUNDED_RECT rightHalf = D2D1::RoundedRect(D2D1::RectF(x + size * 0.5f, y, x + size, y + size), r, r);
            ctx->FillRoundedRectangle(rightHalf, m_brushFinderDarkBlue.Get());

            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - size * 0.2f, cy - size * 0.12f), size * 0.05f, size * 0.07f), m_brushTextPrimary.Get());
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + size * 0.2f, cy - size * 0.12f), size * 0.05f, size * 0.07f), m_brushTextPrimary.Get());
            D2D1_ELLIPSE smile = D2D1::Ellipse(D2D1::Point2F(cx, cy + size * 0.12f), size * 0.22f, size * 0.12f);
            ctx->DrawEllipse(smile, m_brushTextPrimary.Get(), size * 0.045f);
            break;
        }

        case DockAppType::Launchpad: {
            ctx->FillRoundedRectangle(iconBox, m_brushDarkTile.Get());
            float step = size * 0.22f;
            float startDotX = x + size * 0.28f;
            float startDotY = y + size * 0.28f;
            ID2D1SolidColorBrush* dotBrushes[9] = {
                m_brushAccentRed.Get(), m_brushAccentOrange.Get(), m_brushAccentGreen.Get(),
                m_brushAccentBlue.Get(), m_brushAccentPurple.Get(), m_brushAccentBlue.Get(),
                m_brushAccentOrange.Get(), m_brushAccentGreen.Get(), m_brushAccentRed.Get()
            };

            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startDotX + col * step, startDotY + row * step), size * 0.065f, size * 0.065f), dotBrushes[row * 3 + col]);
                }
            }
            break;
        }

        case DockAppType::Browser: {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentBlue.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            float compassR = size * 0.36f;
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), compassR, compassR), m_brushTextPrimary.Get(), size * 0.04f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.05f, size * 0.05f), m_brushAccentRed.Get());
            break;
        }

        case DockAppType::Terminal: {
            ctx->FillRoundedRectangle(iconBox, m_brushDarkTile.Get());
            ctx->DrawRoundedRectangle(iconBox, m_brushGlassBorder.Get(), 1.0f);
            float cx = x + size * 0.22f;
            float cy = y + size * 0.38f;
            ctx->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx + size * 0.18f, cy + size * 0.12f), m_brushAccentGreen.Get(), size * 0.05f);
            ctx->DrawLine(D2D1::Point2F(cx + size * 0.18f, cy + size * 0.12f), D2D1::Point2F(cx, cy + size * 0.24f), m_brushAccentGreen.Get(), size * 0.05f);
            ctx->DrawLine(D2D1::Point2F(cx + size * 0.26f, cy + size * 0.24f), D2D1::Point2F(cx + size * 0.44f, cy + size * 0.24f), m_brushAccentGreen.Get(), size * 0.05f);
            break;
        }

        case DockAppType::CodeEditor: {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentBlue.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            D2D1_ELLIPSE ring = D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.24f, size * 0.24f);
            ctx->DrawEllipse(ring, m_brushTextPrimary.Get(), size * 0.045f);
            break;
        }

        case DockAppType::Explorer: {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentBlue.Get());
            D2D1_ROUNDED_RECT tab = D2D1::RoundedRect(D2D1::RectF(x + size * 0.15f, y + size * 0.22f, x + size * 0.5f, y + size * 0.45f), 2.0f, 2.0f);
            ctx->FillRoundedRectangle(tab, m_brushFinderDarkBlue.Get());
            break;
        }

        case DockAppType::Calculator: {
            ctx->FillRoundedRectangle(iconBox, m_brushAccentOrange.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            ctx->DrawLine(D2D1::Point2F(cx - size * 0.16f, cy), D2D1::Point2F(cx + size * 0.16f, cy), m_brushTextPrimary.Get(), size * 0.045f);
            ctx->DrawLine(D2D1::Point2F(cx, cy - size * 0.16f), D2D1::Point2F(cx, cy + size * 0.16f), m_brushTextPrimary.Get(), size * 0.045f);
            break;
        }

        case DockAppType::ActivityMonitor: {
            ctx->FillRoundedRectangle(iconBox, m_brushDarkTile.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            D2D1_POINT_2F pts[5] = {
                D2D1::Point2F(x + size * 0.18f, cy),
                D2D1::Point2F(cx - size * 0.08f, cy),
                D2D1::Point2F(cx, cy - size * 0.22f),
                D2D1::Point2F(cx + size * 0.08f, cy + size * 0.18f),
                D2D1::Point2F(x + size * 0.82f, cy)
            };
            for (int p = 0; p < 4; ++p) {
                ctx->DrawLine(pts[p], pts[p + 1], m_brushAccentGreen.Get(), size * 0.045f);
            }
            break;
        }

        case DockAppType::Settings: {
            ctx->FillRoundedRectangle(iconBox, m_brushLightGray.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.20f, size * 0.20f), m_brushGearDark.Get());
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), size * 0.10f, size * 0.10f), m_brushLightGray.Get());
            break;
        }

        case DockAppType::Trash: {
            ctx->FillRoundedRectangle(iconBox, m_brushLightGray.Get());
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            D2D1_ROUNDED_RECT basket = D2D1::RoundedRect(D2D1::RectF(cx - size * 0.16f, cy - size * 0.10f, cx + size * 0.16f, cy + size * 0.22f), 2.0f, 2.0f);
            ctx->DrawRoundedRectangle(basket, m_brushGearDark.Get(), size * 0.04f);
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
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_TIMER:
            UpdatePhysics();
            return 0;

        case WM_MOUSEMOVE: {
            m_mouseWindowX = static_cast<float>(GET_X_LPARAM(lParam));
            m_mouseWindowY = static_cast<float>(GET_Y_LPARAM(lParam));
            m_isHovered = true;
            m_needsRedraw = true;

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_isHovered = false;
            m_mouseWindowX = -999.0f;
            m_mouseWindowY = -999.0f;
            m_hoveredIndex = -1;
            m_needsRedraw = true;
            return 0;
        }

        case WM_MBUTTONUP: {
            if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_items.size())) {
                auto& item = m_items[m_hoveredIndex];
                if (item.type == DockAppType::Separator || item.type == DockAppType::Launchpad) return 0;

                item.isBouncing = true;
                item.bounceTime = 0.0f;
                item.isRunning = true;

                if (!item.executePath.empty()) {
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

        case WM_LBUTTONDOWN: {
            if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_items.size())) {
                auto& item = m_items[m_hoveredIndex];
                if (item.type == DockAppType::Separator) return 0;

                item.isBouncing = true;
                item.bounceTime = 0.0f;

                // Action for Launchpad / Mission Control / Spotlight
                if (item.type == DockAppType::Launchpad) {
                    if (m_onLaunchpad) m_onLaunchpad();
                    else if (m_onMissionControl) m_onMissionControl();
                    else if (m_onSpotlight) m_onSpotlight();
                    return 0;
                }

                // Check for Shift key (explicit user request to open new instance/window)
                bool isShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                if (!isShiftDown) {
                    auto hwnds = GetWindowsForItem(item);
                    if (!hwnds.empty()) {
                        HWND fg = GetForegroundWindow();
                        HWND rootFg = fg ? GetAncestor(fg, GA_ROOTOWNER) : nullptr;
                        if (!rootFg) rootFg = fg;
                        if (rootFg == m_hwnd || IsAquaWindow(rootFg)) {
                            rootFg = m_lastActiveAppHwnd;
                        }

                        // Check if the current foreground window belongs to this app
                        bool isForeground = false;
                        HWND targetFg = nullptr;
                        for (HWND h : hwnds) {
                            if (h == fg || h == rootFg) {
                                isForeground = true;
                                targetFg = h;
                                break;
                            }
                        }

                        if (isForeground && targetFg) {
                            if (hwnds.size() > 1) {
                                // Cycle to the next window of this app
                                size_t currentIdx = 0;
                                for (size_t wi = 0; wi < hwnds.size(); ++wi) {
                                    if (hwnds[wi] == targetFg) {
                                        currentIdx = wi;
                                        break;
                                    }
                                }
                                size_t nextIdx = (currentIdx + 1) % hwnds.size();
                                HWND nextWnd = hwnds[nextIdx];
                                ForceForegroundWindow(nextWnd);
                                m_lastActiveAppHwnd = nextWnd;
                                return 0;
                            } else {
                                // Single window active -> Minimize it!
                                ShowWindow(targetFg, SW_MINIMIZE);
                                if (m_lastActiveAppHwnd == targetFg) {
                                    m_lastActiveAppHwnd = nullptr;
                                }
                                return 0;
                            }
                        } else {
                            // Inactive or minimized app clicked -> Restore and bring to front!
                            HWND target = hwnds[0];
                            ForceForegroundWindow(target);
                            m_lastActiveAppHwnd = target;
                            return 0;
                        }
                    }
                }

                // App is not currently running (or Shift was held) -> Launch new instance
                item.isRunning = true;
                if (!item.executePath.empty()) {
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
            if (!item.isPinned) continue; // NEVER persist unpinned dynamic running items!
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
                item.isPinned = true;
                item.isRunning = false;
                item.targetScale = 1.0f;
                item.currentScale = 1.0f;

                // Auto-upgrade real icon paths if empty
                if (item.iconPath.empty()) {
                    if (item.type == DockAppType::Finder) {
                        item.iconPath = L"C:\\Windows\\explorer.exe";
                    } else if (item.type == DockAppType::Settings) {
                        item.iconPath = L"C:\\Windows\\ImmersiveControlPanel\\SystemSettings.exe";
                    } else if (item.type == DockAppType::Trash) {
                        item.iconPath = L"C:\\Windows\\System32\\imageres.dll,-54";
                    } else if (item.type == DockAppType::Explorer) {
                        item.iconPath = L"C:\\Windows\\explorer.exe";
                    } else if (item.type == DockAppType::Calculator) {
                        item.iconPath = L"C:\\Windows\\System32\\calc.exe";
                    } else if (item.type == DockAppType::ActivityMonitor) {
                        item.iconPath = L"C:\\Windows\\System32\\Taskmgr.exe";
                    } else if (item.type == DockAppType::Browser) {
                        item.iconPath = QueryDefaultBrowserPath();
                    } else if (item.type == DockAppType::Terminal) {
                        item.iconPath = L"wt.exe";
                    } else if (item.type == DockAppType::CodeEditor) {
                        item.iconPath = L"code.exe";
                    }
                }

                m_items.push_back(item);
            }
        }
    }

    void Dock::PinApplication(const std::wstring& filePath) {
        if (filePath.empty()) return;

        DockItem newItem;
        newItem.type = DockAppType::CustomApp;
        newItem.isPinned = true;
        newItem.name = IconManager::Instance().GetAppFriendlyName(filePath);
        newItem.executePath = filePath;
        newItem.executeArgs = L"";
        newItem.iconPath = filePath;
        newItem.isRunning = false;
        newItem.targetScale = 1.0f;
        newItem.currentScale = 1.0f;

        // Insert before Separator or Trash
        auto it = std::find_if(m_items.begin(), m_items.end(), [](const DockItem& itm) {
            return itm.type == DockAppType::Separator || itm.type == DockAppType::Trash;
        });

        if (it != m_items.end()) {
            m_items.insert(it, newItem);
        } else {
            m_items.push_back(newItem);
        }

        AdjustWindowWidthForItems();
        CalculateLayout();
        SaveConfiguration();
        Render();
    }

    void Dock::RemoveItem(int index) {
        if (index >= 0 && index < static_cast<int>(m_items.size())) {
            if (m_items[index].type != DockAppType::Finder && m_items[index].type != DockAppType::Trash) {
                if (m_items[index].isRunning) {
                    // Keep running app visible as an unpinned dynamic item until it closes
                    m_items[index].isPinned = false;
                } else {
                    m_items.erase(m_items.begin() + index);
                }
                AdjustWindowWidthForItems();
                CalculateLayout();
                SaveConfiguration();
                Render();
            }
        }
    }

    void Dock::ShowContextMenu(int itemIndex, int screenX, int screenY) {
        HMENU hMenu = CreatePopupMenu();

        bool hasItem = (itemIndex >= 0 && itemIndex < static_cast<int>(m_items.size()));
        DockItem* pItem = hasItem ? &m_items[itemIndex] : nullptr;

        std::vector<HWND> runningHwnds;
        if (pItem && pItem->type != DockAppType::Separator) {
            runningHwnds = GetWindowsForItem(*pItem);
            bool isRunning = !runningHwnds.empty();

            if (isRunning) {
                AppendMenuW(hMenu, MF_STRING, 5001, (L"Bring " + pItem->name + L" to Front").c_str());
                AppendMenuW(hMenu, MF_STRING, 5004, (L"Minimize " + pItem->name).c_str());
                AppendMenuW(hMenu, MF_STRING, 5005, (L"New Window (" + pItem->name + L")").c_str());
                AppendMenuW(hMenu, MF_STRING, 5006, (L"Close " + pItem->name).c_str());
            } else {
                AppendMenuW(hMenu, MF_STRING, 5001, (L"Open " + pItem->name).c_str());
            }

            if (!pItem->executePath.empty() && pItem->executePath.find(L":") != std::wstring::npos) {
                AppendMenuW(hMenu, MF_STRING, 5002, L"Show in File Explorer");
            }
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

            if (!pItem->isPinned) {
                AppendMenuW(hMenu, MF_STRING, 5007, L"Keep in Dock");
            } else if (pItem->type != DockAppType::Finder && pItem->type != DockAppType::Trash) {
                AppendMenuW(hMenu, MF_STRING, 5003, L"Remove from Dock");
            }
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
                if (m_onLaunchpad) m_onLaunchpad();
                else if (m_onMissionControl) m_onMissionControl();
                else if (m_onSpotlight) m_onSpotlight();
            } else {
                if (!runningHwnds.empty()) {
                    ForceForegroundWindow(runningHwnds[0]);
                    m_lastActiveAppHwnd = runningHwnds[0];
                } else if (!pItem->executePath.empty()) {
                    ShellExecuteW(nullptr, L"open", pItem->executePath.c_str(), pItem->executeArgs.empty() ? nullptr : pItem->executeArgs.c_str(), nullptr, SW_SHOWNORMAL);
                }
            }
        } else if (cmd == 5004 && pItem) {
            for (HWND h : runningHwnds) {
                ShowWindow(h, SW_MINIMIZE);
            }
            if (m_lastActiveAppHwnd && std::find(runningHwnds.begin(), runningHwnds.end(), m_lastActiveAppHwnd) != runningHwnds.end()) {
                m_lastActiveAppHwnd = nullptr;
            }
        } else if (cmd == 5005 && pItem) {
            if (!pItem->executePath.empty()) {
                ShellExecuteW(nullptr, L"open", pItem->executePath.c_str(), pItem->executeArgs.empty() ? nullptr : pItem->executeArgs.c_str(), nullptr, SW_SHOWNORMAL);
            }
        } else if (cmd == 5006 && pItem) {
            for (HWND h : runningHwnds) {
                PostMessageW(h, WM_CLOSE, 0, 0);
            }
        } else if (cmd == 5007 && hasItem) {
            m_items[itemIndex].isPinned = true;
            SaveConfiguration();
            m_needsRedraw = true;
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
