#include "IconManager.h"
#include <cstdint>
#include <vector>
#include <algorithm>

namespace Aqua {

    IconManager& IconManager::Instance() {
        static IconManager instance;
        return instance;
    }

    void IconManager::ClearCache() {
        m_cache.clear();
    }

    ComPtr<ID2D1Bitmap> IconManager::GetIconForPath(ID2D1DeviceContext* ctx, const std::wstring& path, int size) {
        if (!ctx || path.empty()) return nullptr;

        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            return it->second;
        }

        HICON hIcon = ExtractBestIcon(path, size);
        if (!hIcon) {
            m_cache[path] = nullptr; // Prevent repeated disk probes every frame
            return nullptr;
        }

        ComPtr<ID2D1Bitmap> bitmap = HIconToD2DBitmap(ctx, hIcon, size);
        DestroyIcon(hIcon);

        m_cache[path] = bitmap;
        return bitmap;
    }

    HICON IconManager::ExtractBestIcon(const std::wstring& path, int requestedSize) {
        if (path.empty()) return nullptr;

        wchar_t expandedPath[MAX_PATH];
        ExpandEnvironmentStringsW(path.c_str(), expandedPath, MAX_PATH);

        // 1. Try SHGetImageList with SHIL_JUMBO (256x256) or SHIL_EXTRALARGE (48x48)
        SHFILEINFOW sfi = {};
        DWORD_PTR res = SHGetFileInfoW(expandedPath, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX);
        if (res != 0) {
            ComPtr<IImageList> imageList;
            HRESULT hr = SHGetImageList(SHIL_JUMBO, IID_PPV_ARGS(&imageList));
            if (FAILED(hr) || !imageList) {
                hr = SHGetImageList(SHIL_EXTRALARGE, IID_PPV_ARGS(&imageList));
            }

            if (SUCCEEDED(hr) && imageList) {
                HICON hIcon = nullptr;
                hr = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
                if (SUCCEEDED(hr) && hIcon) {
                    return hIcon;
                }
            }
        }

        // 2. Try ExtractIconExW
        HICON hLarge = nullptr;
        UINT numExtracted = ExtractIconExW(expandedPath, 0, &hLarge, nullptr, 1);
        if (numExtracted > 0 && hLarge) {
            return hLarge;
        }

        // 3. Fallback to SHGetFileInfo Large Icon
        sfi = {};
        if (SHGetFileInfoW(expandedPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
            return sfi.hIcon;
        }

        return nullptr;
    }

    ComPtr<ID2D1Bitmap> IconManager::HIconToD2DBitmap(ID2D1DeviceContext* ctx, HICON hIcon, int size) {
        if (!ctx || !hIcon) return nullptr;

        ICONINFO iconInfo = {};
        if (!GetIconInfo(hIcon, &iconInfo)) return nullptr;

        BITMAP bm = {};
        GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask, sizeof(bm), &bm);
        int iconW = bm.bmWidth;
        int iconH = iconInfo.hbmColor ? bm.bmHeight : (bm.bmHeight / 2);

        int targetSize = (size > 0) ? size : ((iconW > 0) ? iconW : 64);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = targetSize;
        bmi.bmiHeader.biHeight = -targetSize; // Top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* pixels = nullptr;
        HDC hdcMem = CreateCompatibleDC(nullptr);
        HBITMAP hDIB = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!hDIB || !pixels) {
            if (hdcMem) DeleteDC(hdcMem);
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            return nullptr;
        }

        HGDIOBJ oldBm = SelectObject(hdcMem, hDIB);

        // Clear buffer
        memset(pixels, 0, targetSize * targetSize * sizeof(uint32_t));

        // Draw icon onto DIB surface
        DrawIconEx(hdcMem, 0, 0, hIcon, targetSize, targetSize, 0, nullptr, DI_NORMAL);

        // Detect if icon has valid alpha bytes
        bool hasAlpha = false;
        int totalPixels = targetSize * targetSize;
        for (int i = 0; i < totalPixels; ++i) {
            if ((pixels[i] & 0xFF000000) != 0) {
                hasAlpha = true;
                break;
            }
        }

        // If legacy icon without alpha, synthesize alpha from mask
        if (!hasAlpha && iconInfo.hbmMask && iconW > 0 && iconH > 0 && targetSize > 0) {
            HDC hdcMask = CreateCompatibleDC(nullptr);
            HGDIOBJ oldMaskBm = SelectObject(hdcMask, iconInfo.hbmMask);
            for (int y = 0; y < targetSize; ++y) {
                for (int x = 0; x < targetSize; ++x) {
                    COLORREF maskColor = GetPixel(hdcMask, (x * iconW) / targetSize, (y * iconH) / targetSize);
                    int idx = y * targetSize + x;
                    if (maskColor == RGB(0, 0, 0)) {
                        pixels[idx] |= 0xFF000000; // Opaque
                    } else {
                        pixels[idx] = 0x00000000; // Transparent
                    }
                }
            }
            SelectObject(hdcMask, oldMaskBm);
            DeleteDC(hdcMask);
        }

        // Ensure strict pre-multiplied alpha values
        for (int i = 0; i < totalPixels; ++i) {
            uint32_t p = pixels[i];
            uint32_t a = (p >> 24) & 0xFF;
            if (a == 0) {
                pixels[i] = 0;
            } else if (a < 255) {
                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;

                if (r > a) r = a;
                if (g > a) g = a;
                if (b > a) b = a;

                pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }

        // Copy pixels into safe buffer before destroying GDI DIB
        std::vector<uint32_t> pixelData(pixels, pixels + totalPixels);

        SelectObject(hdcMem, oldBm);
        DeleteDC(hdcMem);
        DeleteObject(hDIB);

        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

        // Create Direct2D Bitmap from pixelData
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );

        ComPtr<ID2D1Bitmap> d2dBitmap;
        HRESULT hr = ctx->CreateBitmap(
            D2D1::SizeU(targetSize, targetSize),
            pixelData.data(),
            targetSize * 4,
            &props,
            &d2dBitmap
        );

        return SUCCEEDED(hr) ? d2dBitmap : nullptr;
    }

}
