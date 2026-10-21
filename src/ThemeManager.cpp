#include "ThemeManager.h"
#include "TaskbarController.h"
#include <shlobj.h>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Aqua {

    ThemeManager& ThemeManager::Instance() {
        static ThemeManager s_instance;
        return s_instance;
    }

    ThemeManager::ThemeManager() = default;

    ThemeManager::~ThemeManager() {
        Shutdown();
    }

    bool ThemeManager::Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_hInstance = hInstance;
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Setup Wallpaper cache directory in LocalAppData
        wchar_t localApp[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
            std::wstring dir = std::wstring(localApp) + L"\\AquaOS";
            CreateDirectoryW(dir.c_str(), nullptr);
            dir += L"\\Wallpapers";
            CreateDirectoryW(dir.c_str(), nullptr);
            m_wallpaperCachePath = dir + L"\\AquaDynamicWallpaper.bmp";
        } else {
            m_wallpaperCachePath = L"AquaDynamicWallpaper.bmp";
        }

        // Install Low-Level Mouse Hook for desktop right-click context menu
        m_hMouseHook = SetWindowsHookExW(
            WH_MOUSE_LL,
            ThemeManager::LowLevelMouseProc,
            hInstance,
            0
        );

        // Apply initial dynamic wallpaper
        UpdateDynamicWallpaper(true);

        return true;
    }

    void ThemeManager::Shutdown() {
        if (m_hMouseHook) {
            UnhookWindowsHookEx(m_hMouseHook);
            m_hMouseHook = nullptr;
        }
    }

    bool ThemeManager::IsDarkMode() const {
        if (m_mode == ThemeMode::Dark) return true;
        if (m_mode == ThemeMode::Light) return false;

        // Auto (Time-Adaptive): 7:00 AM to 7:00 PM is Light, 7:00 PM to 7:00 AM is Dark
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        return (st.wHour >= 19 || st.wHour < 7);
    }

    void ThemeManager::SetThemeMode(ThemeMode mode) {
        if (m_mode != mode) {
            m_mode = mode;
            NotifyListeners();
            UpdateDynamicWallpaper(true);
        }
    }

    void ThemeManager::ToggleTheme() {
        SetThemeMode(IsDarkMode() ? ThemeMode::Light : ThemeMode::Dark);
    }

    void ThemeManager::SetWallpaperStyle(WallpaperStyle style) {
        m_style = style;
        UpdateDynamicWallpaper(true);
    }

    void ThemeManager::CycleWallpaper() {
        int next = (static_cast<int>(m_style) + 1) % 4;
        SetWallpaperStyle(static_cast<WallpaperStyle>(next));
    }

    void ThemeManager::RegisterListener(std::function<void(bool isDark)> listener) {
        m_listeners.push_back(listener);
    }

    void ThemeManager::NotifyListeners() {
        bool dark = IsDarkMode();
        for (auto& cb : m_listeners) {
            if (cb) cb(dark);
        }
    }

    // Dynamic Theme Color Palette
    D2D1_COLOR_F ThemeManager::GetGlassBg() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.78f)
            : D2D1::ColorF(0.95f, 0.95f, 0.97f, 0.82f);
    }

    D2D1_COLOR_F ThemeManager::GetGlassBgDark() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.90f)
            : D2D1::ColorF(0.90f, 0.90f, 0.93f, 0.92f);
    }

    D2D1_COLOR_F ThemeManager::GetGlassBorder() const {
        return IsDarkMode()
            ? D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.14f)
            : D2D1::ColorF(0.00f, 0.00f, 0.00f, 0.12f);
    }

    D2D1_COLOR_F ThemeManager::GetTextPrimary() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.00f)
            : D2D1::ColorF(0.11f, 0.11f, 0.13f, 1.00f);
    }

    D2D1_COLOR_F ThemeManager::GetTextSecondary() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.65f, 0.65f, 0.70f, 1.00f)
            : D2D1::ColorF(0.40f, 0.40f, 0.44f, 1.00f);
    }

    D2D1_COLOR_F ThemeManager::GetTextMuted() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.45f, 0.45f, 0.50f, 1.00f)
            : D2D1::ColorF(0.60f, 0.60f, 0.64f, 1.00f);
    }

    D2D1_COLOR_F ThemeManager::GetCardBg() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.16f, 0.16f, 0.18f, 0.75f)
            : D2D1::ColorF(0.88f, 0.88f, 0.92f, 0.75f);
    }

    D2D1_COLOR_F ThemeManager::GetCardBgHover() const {
        return IsDarkMode()
            ? D2D1::ColorF(0.22f, 0.22f, 0.26f, 0.85f)
            : D2D1::ColorF(0.80f, 0.80f, 0.85f, 0.88f);
    }

    D2D1_COLOR_F ThemeManager::GetAccent() const {
        return D2D1::ColorF(0.00f, 0.48f, 1.00f, 1.00f);
    }

    // High-performance Direct-memory BMP Wallpaper Generation
    bool ThemeManager::GenerateWallpaperImage(const std::wstring& outPath, WallpaperStyle style, bool isDark) {
        int w = m_screenWidth;
        int h = m_screenHeight;
        if (w <= 0 || h <= 0) return false;

        // Colors (RGB)
        struct RGBColor { float r, g, b; };
        RGBColor cTop, cMid, cBot;

        switch (style) {
        case WallpaperStyle::Sonoma:
            if (isDark) {
                cTop = { 15.0f, 23.0f, 42.0f };   // Slate midnight
                cMid = { 59.0f, 31.0f, 98.0f };   // Sonoma purple dusk
                cBot = { 88.0f, 28.0f, 135.0f };  // Deep twilight magenta
            } else {
                cTop = { 33.0f, 147.0f, 176.0f };  // Crisp azure sky
                cMid = { 86.0f, 185.0f, 218.0f };  // Luminous cyan
                cBot = { 240.0f, 147.0f, 251.0f }; // Warm morning haze
            }
            break;

        case WallpaperStyle::Sequoia:
            if (isDark) {
                cTop = { 6.0f, 24.0f, 30.0f };     // Obsidian spruce
                cMid = { 18.0f, 53.0f, 36.0f };    // Deep pine green
                cBot = { 28.0f, 40.0f, 38.0f };    // Mist valley
            } else {
                cTop = { 19.0f, 78.0f, 94.0f };    // Emerald lake
                cMid = { 60.0f, 135.0f, 114.0f };  // Forest green
                cBot = { 113.0f, 178.0f, 128.0f }; // Meadow daylight
            }
            break;

        case WallpaperStyle::Ventura:
            if (isDark) {
                cTop = { 20.0f, 12.0f, 36.0f };   // Deep dune night
                cMid = { 46.0f, 20.0f, 55.0f };   // Twilight shadow
                cBot = { 78.0f, 28.0f, 50.0f };   // Warm horizon
            } else {
                cTop = { 58.0f, 28.0f, 113.0f };  // Sky violet
                cMid = { 215.0f, 109.0f, 119.0f };// Rose dune
                cBot = { 255.0f, 175.0f, 123.0f };// Golden sand
            }
            break;

        case WallpaperStyle::Monterey:
        default:
            if (isDark) {
                cTop = { 10.0f, 14.0f, 28.0f };   // Monterey deep indigo
                cMid = { 24.0f, 32.0f, 68.0f };   // Oceanic dark
                cBot = { 64.0f, 22.0f, 84.0f };   // Deep plum
            } else {
                cTop = { 0.0f, 114.0f, 255.0f };   // Brilliant blue
                cMid = { 72.0f, 90.0f, 230.0f };   // Vivid ocean
                cBot = { 198.0f, 68.0f, 252.0f };  // Radiant lavender
            }
            break;
        }

        // Align row size to 4 bytes for BMP format
        int rowBytes = ((w * 3 + 3) / 4) * 4;
        int imageSize = rowBytes * h;

        BITMAPFILEHEADER bfh = {};
        bfh.bfType = 0x4D42; // "BM"
        bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        BITMAPINFOHEADER bih = {};
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = w;
        bih.biHeight = h; // Bottom-up DIB
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        bih.biSizeImage = imageSize;

        std::vector<BYTE> pixelData(imageSize, 0);

        for (int y = 0; y < h; ++y) {
            float v = static_cast<float>(y) / static_cast<float>(h); // 0.0 (bottom) to 1.0 (top)

            // Interpolate color: bottom to mid, then mid to top
            RGBColor col;
            if (v < 0.5f) {
                float t = v / 0.5f;
                col.r = cBot.r + (cMid.r - cBot.r) * t;
                col.g = cBot.g + (cMid.g - cBot.g) * t;
                col.b = cBot.b + (cMid.b - cBot.b) * t;
            } else {
                float t = (v - 0.5f) / 0.5f;
                col.r = cMid.r + (cTop.r - cMid.r) * t;
                col.g = cMid.g + (cTop.g - cMid.g) * t;
                col.b = cMid.b + (cTop.b - cMid.b) * t;
            }

            BYTE* rowPtr = &pixelData[y * rowBytes];

            for (int x = 0; x < w; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(w);

                // Subtle radial light vignette
                float dx = u - 0.5f;
                float dy = v - 0.45f;
                float dist = std::sqrt(dx * dx + dy * dy);
                float vignette = 1.0f - (dist * 0.22f);

                float r = std::clamp(col.r * vignette, 0.0f, 255.0f);
                float g = std::clamp(col.g * vignette, 0.0f, 255.0f);
                float b = std::clamp(col.b * vignette, 0.0f, 255.0f);

                // BMP is BGR order
                rowPtr[x * 3 + 0] = static_cast<BYTE>(b);
                rowPtr[x * 3 + 1] = static_cast<BYTE>(g);
                rowPtr[x * 3 + 2] = static_cast<BYTE>(r);
            }
        }

        // Write to disk
        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open()) return false;

        out.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
        out.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
        out.write(reinterpret_cast<const char*>(pixelData.data()), pixelData.size());
        out.close();

        return true;
    }

    void ThemeManager::UpdateDynamicWallpaper(bool force) {
        SYSTEMTIME st = {};
        GetLocalTime(&st);

        if (!force && st.wHour == m_lastHour) {
            return;
        }
        m_lastHour = st.wHour;

        bool dark = IsDarkMode();
        if (GenerateWallpaperImage(m_wallpaperCachePath, m_style, dark)) {
            SystemParametersInfoW(
                SPI_SETDESKWALLPAPER,
                0,
                const_cast<wchar_t*>(m_wallpaperCachePath.c_str()),
                SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
            );
        }
    }

    // Low-level mouse hook for desktop right-click context menu
    LRESULT CALLBACK ThemeManager::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && wParam == WM_RBUTTONUP) {
            auto* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

            HWND hUnder = WindowFromPoint(pMouse->pt);
            if (hUnder) {
                wchar_t cls[256] = {};
                GetClassNameW(hUnder, cls, 256);
                std::wstring className(cls);

                // Desktop shell class names
                if (className == L"Progman" || className == L"WorkerW" ||
                    className == L"SHELLDLL_DefView" || className == L"SysListView32") {
                    // Show macOS Desktop Context Menu
                    ThemeManager::Instance().ShowDesktopContextMenu(pMouse->pt.x, pMouse->pt.y);
                    return 1; // Consume default Windows Explorer right-click
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    void ThemeManager::ShowDesktopContextMenu(int screenX, int screenY) {
        HMENU hMenu = CreatePopupMenu();

        AppendMenuW(hMenu, MF_STRING, 5001, L"New Folder\tCtrl+Shift+N");
        AppendMenuW(hMenu, MF_STRING, 5002, L"Get Info\tAlt+Enter");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        // Wallpaper Submenu
        HMENU hWallMenu = CreatePopupMenu();
        std::wstring s1 = L"Sonoma Horizon" + std::wstring(m_style == WallpaperStyle::Sonoma ? L"  ✓" : L"");
        std::wstring s2 = L"Sequoia Wilderness" + std::wstring(m_style == WallpaperStyle::Sequoia ? L"  ✓" : L"");
        std::wstring s3 = L"Ventura Dunes" + std::wstring(m_style == WallpaperStyle::Ventura ? L"  ✓" : L"");
        std::wstring s4 = L"Monterey Ocean" + std::wstring(m_style == WallpaperStyle::Monterey ? L"  ✓" : L"");

        AppendMenuW(hWallMenu, MF_STRING, 5101, s1.c_str());
        AppendMenuW(hWallMenu, MF_STRING, 5102, s2.c_str());
        AppendMenuW(hWallMenu, MF_STRING, 5103, s3.c_str());
        AppendMenuW(hWallMenu, MF_STRING, 5104, s4.c_str());
        AppendMenuW(hWallMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hWallMenu, MF_STRING, 5105, L"Cycle Next Wallpaper\tCtrl+Alt+W");

        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hWallMenu), L"Change Wallpaper");

        // Appearance Submenu
        HMENU hAppMenu = CreatePopupMenu();
        std::wstring a1 = L"Light Mode" + std::wstring(m_mode == ThemeMode::Light ? L"  ✓" : L"");
        std::wstring a2 = L"Dark Mode" + std::wstring(m_mode == ThemeMode::Dark ? L"  ✓" : L"");
        std::wstring a3 = L"Auto (Time-Adaptive)" + std::wstring(m_mode == ThemeMode::Auto ? L"  ✓" : L"");

        AppendMenuW(hAppMenu, MF_STRING, 5201, a1.c_str());
        AppendMenuW(hAppMenu, MF_STRING, 5202, a2.c_str());
        AppendMenuW(hAppMenu, MF_STRING, 5203, a3.c_str());

        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hAppMenu), L"Appearance");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, 5010, TaskbarController::Instance().IsSuppressed() ? L"Show Windows Taskbar\tCtrl+Alt+T" : L"Hide Windows Taskbar\tCtrl+Alt+T");

        HWND dummyHwnd = GetForegroundWindow();
        int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, screenX, screenY, 0, dummyHwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd == 5001) {
            // New folder shortcut on desktop
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event(VK_SHIFT, 0, 0, 0);
            keybd_event('N', 0, 0, 0);
            keybd_event('N', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        } else if (cmd == 5002) {
            keybd_event(VK_MENU, 0, 0, 0);
            keybd_event(VK_RETURN, 0, 0, 0);
            keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        } else if (cmd == 5101) {
            SetWallpaperStyle(WallpaperStyle::Sonoma);
        } else if (cmd == 5102) {
            SetWallpaperStyle(WallpaperStyle::Sequoia);
        } else if (cmd == 5103) {
            SetWallpaperStyle(WallpaperStyle::Ventura);
        } else if (cmd == 5104) {
            SetWallpaperStyle(WallpaperStyle::Monterey);
        } else if (cmd == 5105) {
            CycleWallpaper();
        } else if (cmd == 5201) {
            SetThemeMode(ThemeMode::Light);
        } else if (cmd == 5202) {
            SetThemeMode(ThemeMode::Dark);
        } else if (cmd == 5203) {
            SetThemeMode(ThemeMode::Auto);
        } else if (cmd == 5010) {
            TaskbarController::Instance().ToggleTaskbar();
        }
    }

}

