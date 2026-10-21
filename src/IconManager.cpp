#include "IconManager.h"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <shlobj.h>
#include <winver.h>

namespace Aqua {

    static std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
        return str;
    }

    IconManager& IconManager::Instance() {
        static IconManager instance;
        return instance;
    }

    void IconManager::ClearCache() {
        m_cache.clear();
        m_hwndCache.clear();
    }

    void IconManager::PruneClosedHwnds() {
        for (auto it = m_hwndCache.begin(); it != m_hwndCache.end(); ) {
            if (!IsWindow(it->first)) {
                it = m_hwndCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::wstring IconManager::ResolvePath(const std::wstring& rawPath) {
        if (rawPath.empty()) return L"";

        wchar_t expanded[MAX_PATH] = {};
        ExpandEnvironmentStringsW(rawPath.c_str(), expanded, MAX_PATH);
        std::wstring pathStr(expanded);

        // Check if it's a shell shortcut (.lnk)
        if (pathStr.length() >= 4 && _wcsicmp(pathStr.c_str() + pathStr.length() - 4, L".lnk") == 0) {
            ComPtr<IShellLinkW> shellLink;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
                ComPtr<IPersistFile> persistFile;
                if (SUCCEEDED(shellLink.As(&persistFile))) {
                    if (SUCCEEDED(persistFile->Load(pathStr.c_str(), STGM_READ))) {
                        wchar_t target[MAX_PATH] = {};
                        if (SUCCEEDED(shellLink->GetPath(target, MAX_PATH, nullptr, SLGP_UNCPRIORITY))) {
                            if (target[0] != L'\0') {
                                pathStr = target;
                            }
                        }
                    }
                }
            }
        }

        // If path has no directory separator and no comma, search system PATH
        if (pathStr.find(L'\\') == std::wstring::npos && pathStr.find(L'/') == std::wstring::npos && pathStr.find(L',') == std::wstring::npos) {
            wchar_t searched[MAX_PATH] = {};
            if (SearchPathW(nullptr, pathStr.c_str(), L".exe", MAX_PATH, searched, nullptr) > 0) {
                return searched;
            }
        }

        return pathStr;
    }

    ComPtr<ID2D1Bitmap> IconManager::GetIconForPath(ID2D1DeviceContext* ctx, const std::wstring& path, int size) {
        if (!ctx || path.empty()) return nullptr;

        std::wstring resolved = ResolvePath(path);
        if (resolved.empty()) return nullptr;

        std::wstring cacheKey = ToLower(resolved) + L"#" + std::to_wstring(size);
        auto it = m_cache.find(cacheKey);
        if (it != m_cache.end()) {
            return it->second;
        }

        HICON hIcon = ExtractBestIcon(resolved, size);
        if (!hIcon) {
            m_cache[cacheKey] = nullptr;
            return nullptr;
        }

        ComPtr<ID2D1Bitmap> bitmap = HIconToD2DBitmap(ctx, hIcon, size);
        DestroyIcon(hIcon);

        m_cache[cacheKey] = bitmap;
        return bitmap;
    }

    HICON IconManager::ExtractHwndIcon(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd)) return nullptr;

        HICON hIcon = nullptr;
        DWORD_PTR dwResult = 0;

        // 1. Try WM_GETICON (ICON_BIG) with 100ms timeout
        if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &dwResult) && dwResult) {
            hIcon = reinterpret_cast<HICON>(dwResult);
        }

        // 2. Try WM_GETICON (ICON_SMALL2)
        if (!hIcon) {
            if (SendMessageTimeoutW(hwnd, WM_GETICON, 2 /*ICON_SMALL2*/, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &dwResult) && dwResult) {
                hIcon = reinterpret_cast<HICON>(dwResult);
            }
        }

        // 3. Try WM_GETICON (ICON_SMALL)
        if (!hIcon) {
            if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &dwResult) && dwResult) {
                hIcon = reinterpret_cast<HICON>(dwResult);
            }
        }

        // 4. Try GetClassLongPtrW (GCLP_HICON)
        if (!hIcon) {
            hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
        }

        // 5. Try GetClassLongPtrW (GCLP_HICONSM)
        if (!hIcon) {
            hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM));
        }

        if (hIcon) {
            return CopyIcon(hIcon);
        }

        return nullptr;
    }

    ComPtr<ID2D1Bitmap> IconManager::GetIconForHwnd(ID2D1DeviceContext* ctx, HWND hwnd, int size) {
        if (!ctx || !hwnd || !IsWindow(hwnd)) return nullptr;

        auto it = m_hwndCache.find(hwnd);
        if (it != m_hwndCache.end() && it->second) {
            return it->second;
        }

        // 1. Try direct window icon
        HICON hIcon = ExtractHwndIcon(hwnd);
        if (hIcon) {
            ComPtr<ID2D1Bitmap> bmp = HIconToD2DBitmap(ctx, hIcon, size);
            DestroyIcon(hIcon);
            if (bmp) {
                m_hwndCache[hwnd] = bmp;
                return bmp;
            }
        }

        // 2. Fallback to process image path
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t pathBuf[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &sz)) {
                    CloseHandle(hProc);
                    ComPtr<ID2D1Bitmap> bmp = GetIconForPath(ctx, pathBuf, size);
                    if (bmp) {
                        m_hwndCache[hwnd] = bmp;
                        return bmp;
                    }
                } else {
                    CloseHandle(hProc);
                }
            }
        }

        return nullptr;
    }

    HICON IconManager::ExtractBestIcon(const std::wstring& path, int requestedSize) {
        if (path.empty()) return nullptr;

        // Check for comma index (e.g. "shell32.dll,31" or "imageres.dll,-54")
        int iconIndex = 0;
        wchar_t cleanPath[MAX_PATH] = {};
        wcscpy_s(cleanPath, path.c_str());
        wchar_t* comma = wcsrchr(cleanPath, L',');
        if (comma) {
            *comma = L'\0';
            iconIndex = _wtoi(comma + 1);
        }

        // 1. If not a specific index, try SHGetImageList with SHIL_JUMBO (256x256) or SHIL_EXTRALARGE (48x48)
        if (iconIndex == 0) {
            SHFILEINFOW sfi = {};
            DWORD_PTR res = SHGetFileInfoW(cleanPath, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX);
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
        }

        // 2. Try ExtractIconExW (works for .dll, .exe, resource indexes)
        HICON hLarge = nullptr;
        UINT numExtracted = ExtractIconExW(cleanPath, iconIndex, &hLarge, nullptr, 1);
        if (numExtracted > 0 && hLarge) {
            return hLarge;
        }

        // 3. Fallback to SHGetFileInfo Large Icon
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(cleanPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
            return sfi.hIcon;
        }

        return nullptr;
    }

    std::wstring IconManager::GetAppFriendlyName(const std::wstring& path, const std::wstring& fallbackTitle) {
        if (!path.empty()) {
            std::wstring resolved = ResolvePath(path);
            DWORD dwDummy = 0;
            DWORD size = GetFileVersionInfoSizeW(resolved.c_str(), &dwDummy);
            if (size > 0) {
                std::vector<BYTE> data(size);
                if (GetFileVersionInfoW(resolved.c_str(), 0, size, data.data())) {
                    struct LANGANDCODEPAGE {
                        WORD wLanguage;
                        WORD wCodePage;
                    } *lpTranslate = nullptr;
                    UINT cbTranslate = 0;

                    if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&lpTranslate), &cbTranslate) && cbTranslate >= sizeof(LANGANDCODEPAGE)) {
                        wchar_t subBlock[128];
                        swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
                        wchar_t* desc = nullptr;
                        UINT descLen = 0;
                        if (VerQueryValueW(data.data(), subBlock, reinterpret_cast<LPVOID*>(&desc), &descLen) && desc && descLen > 1) {
                            return std::wstring(desc);
                        }

                        swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
                        wchar_t* prod = nullptr;
                        UINT prodLen = 0;
                        if (VerQueryValueW(data.data(), subBlock, reinterpret_cast<LPVOID*>(&prod), &prodLen) && prod && prodLen > 1) {
                            return std::wstring(prod);
                        }
                    }
                }
            }

            // Extract from executable name
            size_t lastSlash = resolved.find_last_of(L"\\/");
            std::wstring fileName = (lastSlash != std::wstring::npos) ? resolved.substr(lastSlash + 1) : resolved;
            size_t dot = fileName.rfind(L'.');
            if (dot != std::wstring::npos) {
                fileName = fileName.substr(0, dot);
            }
            std::wstring lowerFile = ToLower(fileName);
            if (!fileName.empty()) {
                if (lowerFile == L"code") return L"Visual Studio Code";
                if (lowerFile == L"wt" || lowerFile == L"windowsterminal") return L"Terminal";
                if (lowerFile == L"msedge") return L"Microsoft Edge";
                if (lowerFile == L"chrome") return L"Google Chrome";
                if (lowerFile == L"firefox") return L"Firefox";
                if (lowerFile == L"mpc-hc64" || lowerFile == L"mpc-hc") return L"MPC-HC";
                if (lowerFile == L"calc" || lowerFile == L"calculatorapp") return L"Calculator";
                if (lowerFile == L"taskmgr") return L"Activity Monitor";
                if (lowerFile == L"systemsettings") return L"Settings";
                if (lowerFile == L"explorer") return L"Finder";
                if (lowerFile == L"powershell" || lowerFile == L"pwsh") return L"PowerShell";
                if (lowerFile == L"cmd") return L"Command Prompt";
                if (lowerFile == L"spotify") return L"Spotify";
                if (lowerFile == L"discord") return L"Discord";
                if (lowerFile == L"slack") return L"Slack";
                if (lowerFile == L"steam") return L"Steam";
                if (lowerFile == L"notepad") return L"Notepad";

                if (fileName[0] >= L'a' && fileName[0] <= L'z') {
                    fileName[0] = towupper(fileName[0]);
                }
                return fileName;
            }
        }

        if (!fallbackTitle.empty()) {
            size_t dash = fallbackTitle.rfind(L" - ");
            if (dash != std::wstring::npos && dash + 3 < fallbackTitle.length()) {
                return fallbackTitle.substr(dash + 3);
            }
            return fallbackTitle;
        }

        return L"Application";
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
