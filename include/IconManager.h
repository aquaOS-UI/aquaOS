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

        // Clears the cache
        void ClearCache();

    private:
        IconManager() = default;
        ~IconManager() = default;

        HICON ExtractBestIcon(const std::wstring& path, int requestedSize);
        ComPtr<ID2D1Bitmap> HIconToD2DBitmap(ID2D1DeviceContext* ctx, HICON hIcon, int size);

        std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap>> m_cache;
    };

}

