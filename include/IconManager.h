#pragma once
#include "Common.h"
#include <commctrl.h>
#include <commoncontrols.h>
#include <unordered_map>
#include <string>

namespace Aqua {

    class IconManager {
    public:
        static IconManager& Instance();

        // Extracts the highest-resolution icon for any executable/shortcut and returns an ID2D1Bitmap
        ComPtr<ID2D1Bitmap> GetIconForPath(ID2D1DeviceContext* ctx, const std::wstring& path, int size = 64);

        // Extracts the live icon directly from an open HWND, with fallbacks to process image and class icons
        ComPtr<ID2D1Bitmap> GetIconForHwnd(ID2D1DeviceContext* ctx, HWND hwnd, int size = 64);

        // Resolves friendly application display name from file version info or window title
        std::wstring GetAppFriendlyName(const std::wstring& path, const std::wstring& fallbackTitle = L"");

        // Resolves shortcut (.lnk), environment variables, and PATH searches
        std::wstring ResolvePath(const std::wstring& rawPath);

        // Clears the cache
        void ClearCache();

        // Prunes cache entries for closed HWNDs
        void PruneClosedHwnds();

    private:
        IconManager() = default;
        ~IconManager() = default;

        HICON ExtractBestIcon(const std::wstring& path, int requestedSize);
        HICON ExtractHwndIcon(HWND hwnd);
        ComPtr<ID2D1Bitmap> HIconToD2DBitmap(ID2D1DeviceContext* ctx, HICON hIcon, int size);

        std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap>> m_cache;
        std::unordered_map<HWND, ComPtr<ID2D1Bitmap>> m_hwndCache;
    };

}

