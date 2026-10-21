#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <wincodec.h>
#include <string>
#include <vector>
#include <functional>

namespace Aqua {

    enum class ThemeMode {
        Auto,
        Dark,
        Light
    };

    enum class WallpaperStyle {
        Sonoma,
        Sequoia,
        Ventura,
        Monterey
    };

    class ThemeManager {
    public:
        static ThemeManager& Instance();

        bool Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Shutdown();

        // Theme Mode Control
        void SetThemeMode(ThemeMode mode);
        ThemeMode GetThemeMode() const { return m_mode; }
        bool IsDarkMode() const;
        void ToggleTheme();

        // Wallpaper Control
        void SetWallpaperStyle(WallpaperStyle style);
        WallpaperStyle GetWallpaperStyle() const { return m_style; }
        void CycleWallpaper();
        void UpdateDynamicWallpaper(bool force = false);

        // Theme Palette Getters
        D2D1_COLOR_F GetGlassBg() const;
        D2D1_COLOR_F GetGlassBgDark() const;
        D2D1_COLOR_F GetGlassBorder() const;
        D2D1_COLOR_F GetTextPrimary() const;
        D2D1_COLOR_F GetTextSecondary() const;
        D2D1_COLOR_F GetTextMuted() const;
        D2D1_COLOR_F GetCardBg() const;
        D2D1_COLOR_F GetCardBgHover() const;
        D2D1_COLOR_F GetAccent() const;

        // Register callback for theme change events
        void RegisterListener(std::function<void(bool isDark)> listener);

        // Show Desktop macOS Context Menu
        void ShowDesktopContextMenu(int screenX, int screenY);

    private:
        ThemeManager();
        ~ThemeManager();

        static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

        void NotifyListeners();
        bool GenerateWallpaperImage(const std::wstring& outPath, WallpaperStyle style, bool isDark);

        HINSTANCE m_hInstance = nullptr;
        int m_screenWidth = 1920;
        int m_screenHeight = 1080;

        ThemeMode m_mode = ThemeMode::Dark;
        WallpaperStyle m_style = WallpaperStyle::Sonoma;

        std::vector<std::function<void(bool isDark)>> m_listeners;
        std::wstring m_wallpaperCachePath;

        HHOOK m_hMouseHook = nullptr;
        ComPtr<IWICImagingFactory> m_wicFactory;

        int m_lastHour = -1;
    };

}

