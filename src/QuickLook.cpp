#include "QuickLook.h"
#include "IconManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <unordered_set>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace Aqua {

    const wchar_t QUICKLOOK_CLASS[] = L"AquaQuickLookWindow";

    QuickLook& QuickLook::Instance() {
        static QuickLook s_instance;
        return s_instance;
    }

    QuickLook::QuickLook() = default;

    QuickLook::~QuickLook() {
        Shutdown();
    }

    // Helper: Shell COM to extract selected file path in Explorer or Desktop
    static std::wstring GetSelectedExplorerOrDesktopFile() {
        HWND fg = GetForegroundWindow();
        if (!fg) return L"";

        wchar_t cls[256] = {};
        GetClassNameW(fg, cls, 256);
        std::wstring className(cls);

        bool isExplorer = (className == L"CabinetWClass" || className == L"ExploreWClass");
        bool isDesktop = (className == L"Progman" || className == L"WorkerW");

        if (!isExplorer && !isDesktop) return L"";

        ComPtr<IShellWindows> shellWindows;
        HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&shellWindows));
        if (FAILED(hr) || !shellWindows) return L"";

        if (isDesktop) {
            ComPtr<IDispatch> disp;
            VARIANT vEmpty = {};
            VariantInit(&vEmpty);
            long hwndVal = 0;
            hr = shellWindows->FindWindowSW(&vEmpty, &vEmpty, SWC_DESKTOP, &hwndVal, SWFO_NEEDDISPATCH, &disp);
            if (SUCCEEDED(hr) && disp) {
                ComPtr<IServiceProvider> sp;
                if (SUCCEEDED(disp.As(&sp))) {
                    ComPtr<IShellBrowser> sb;
                    if (SUCCEEDED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&sb)))) {
                        ComPtr<IShellView> sv;
                        if (SUCCEEDED(sb->QueryActiveShellView(&sv))) {
                            ComPtr<IFolderView> fv;
                            if (SUCCEEDED(sv.As(&fv))) {
                                int count = 0;
                                fv->ItemCount(SVGIO_SELECTION, &count);
                                if (count > 0) {
                                    int focusItem = 0;
                                    fv->GetFocusedItem(&focusItem);
                                    PITEMID_CHILD pidl = nullptr;
                                    if (SUCCEEDED(fv->Item(focusItem, &pidl)) && pidl) {
                                        ComPtr<IShellFolder> folder;
                                        if (SUCCEEDED(fv->GetFolder(IID_PPV_ARGS(&folder)))) {
                                            STRRET str = {};
                                            if (SUCCEEDED(folder->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &str))) {
                                                wchar_t path[MAX_PATH] = {};
                                                StrRetToBufW(&str, pidl, path, MAX_PATH);
                                                CoTaskMemFree(pidl);
                                                return path;
                                            }
                                        }
                                        CoTaskMemFree(pidl);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (isExplorer) {
            long count = 0;
            shellWindows->get_Count(&count);
            for (long i = 0; i < count; ++i) {
                VARIANT v = {};
                v.vt = VT_I4;
                v.lVal = i;
                ComPtr<IDispatch> disp;
                if (SUCCEEDED(shellWindows->Item(v, &disp)) && disp) {
                    ComPtr<IWebBrowserApp> wb;
                    if (SUCCEEDED(disp.As(&wb))) {
                        SHANDLE_PTR h = 0;
                        wb->get_HWND(&h);
                        if (reinterpret_cast<HWND>(h) == fg) {
                            ComPtr<IServiceProvider> sp;
                            if (SUCCEEDED(disp.As(&sp))) {
                                ComPtr<IShellBrowser> sb;
                                if (SUCCEEDED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&sb)))) {
                                    ComPtr<IShellView> sv;
                                    if (SUCCEEDED(sb->QueryActiveShellView(&sv))) {
                                        ComPtr<IFolderView> fv;
                                        if (SUCCEEDED(sv.As(&fv))) {
                                            int selCount = 0;
                                            fv->ItemCount(SVGIO_SELECTION, &selCount);
                                            if (selCount > 0) {
                                                int focusItem = 0;
                                                fv->GetFocusedItem(&focusItem);
                                                PITEMID_CHILD pidl = nullptr;
                                                if (SUCCEEDED(fv->Item(focusItem, &pidl)) && pidl) {
                                                    ComPtr<IShellFolder> folder;
                                                    if (SUCCEEDED(fv->GetFolder(IID_PPV_ARGS(&folder)))) {
                                                        STRRET str = {};
                                                        if (SUCCEEDED(folder->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &str))) {
                                                            wchar_t path[MAX_PATH] = {};
                                                            StrRetToBufW(&str, pidl, path, MAX_PATH);
                                                            CoTaskMemFree(pidl);
                                                            return path;
                                                        }
                                                    }
                                                    CoTaskMemFree(pidl);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        return L"";
    }

    bool QuickLook::Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Initialize WIC Imaging Factory
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&m_wicFactory)
        );

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = QuickLook::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = QUICKLOOK_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_width = 720;
        m_height = 520;
        int x = (m_screenWidth - m_width) / 2;
        int y = (m_screenHeight - m_height) / 2;

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            QUICKLOOK_CLASS,
            L"AquaOS Quick Look",
            WS_POPUP,
            x, y, m_width, m_height,
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

        // 1. Text Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f * m_dpiScale,
            L"en-US",
            &m_formatTitle
        );
        if (m_formatTitle) {
            m_formatTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
            L"en-US",
            &m_formatMeta
        );
        if (m_formatMeta) {
            m_formatMeta->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            m_formatMeta->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Cascadia Code",
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.5f * m_dpiScale,
            L"en-US",
            &m_formatCode
        );
        if (!m_formatCode) {
            dwrite->CreateTextFormat(
                L"Consolas",
                nullptr,
                DWRITE_FONT_WEIGHT_REGULAR,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                10.5f * m_dpiScale,
                L"en-US",
                &m_formatCode
            );
        }
        if (m_formatCode) {
            m_formatCode->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }

        dwrite->CreateTextFormat(
            L"Consolas",
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.0f * m_dpiScale,
            L"en-US",
            &m_formatGutter
        );
        if (m_formatGutter) {
            m_formatGutter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            m_formatGutter->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
            L"en-US",
            &m_formatButton
        );
        if (m_formatButton) {
            m_formatButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatButton->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 2. Direct2D Brushes
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.94f), &m_brushBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.16f, 0.19f, 0.95f), &m_brushHeaderBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), &m_brushBorder);
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::TextMuted, &m_brushTextMuted);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.09f, 0.11f, 0.70f), &m_brushGutterBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &m_brushButtonBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.22f), &m_brushButtonHover);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.48f, 1.0f, 1.0f), &m_brushAccent);

        // Syntax Highlighting Brushes
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.74f, 0.32f, 0.93f, 1.0f), &m_brushKeyword); // Purple
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.78f, 0.35f, 1.0f), &m_brushString);  // Green
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.58f, 1.0f), &m_brushComment); // Muted Gray
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.58f, 0.0f, 1.0f), &m_brushNumber);    // Orange

        // Install Low-Level Keyboard Hook to intercept Spacebar on Explorer/Desktop
        m_hKeyboardHook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            QuickLook::LowLevelKeyboardProc,
            hInstance,
            0
        );

        return true;
    }

    void QuickLook::Shutdown() {
        if (m_hKeyboardHook) {
            UnhookWindowsHookEx(m_hKeyboardHook);
            m_hKeyboardHook = nullptr;
        }
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    LRESULT CALLBACK QuickLook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            if (pKbd->vkCode == VK_SPACE) {
                if (QuickLook::Instance().IsVisible()) {
                    QuickLook::Instance().Hide();
                    return 1; // Consume spacebar
                } else {
                    HWND fg = GetForegroundWindow();
                    if (fg) {
                        wchar_t cls[256] = {};
                        GetClassNameW(fg, cls, 256);
                        std::wstring className(cls);

                        bool isExplorer = (className == L"CabinetWClass" || className == L"ExploreWClass");
                        bool isDesktop = (className == L"Progman" || className == L"WorkerW");

                        if (isExplorer || isDesktop) {
                            // Check if user is typing into an edit/rename box
                            DWORD tid = GetWindowThreadProcessId(fg, nullptr);
                            GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
                            GetGUIThreadInfo(tid, &gti);
                            if (gti.hwndCaret != nullptr) {
                                return CallNextHookEx(nullptr, nCode, wParam, lParam);
                            }

                            std::wstring sel = GetSelectedExplorerOrDesktopFile();
                            if (!sel.empty()) {
                                QuickLook::Instance().PreviewFile(sel);
                                return 1; // Consume spacebar
                            }
                        }
                    }
                }
            } else if (pKbd->vkCode == VK_ESCAPE && QuickLook::Instance().IsVisible()) {
                QuickLook::Instance().Hide();
                return 1;
            } else if (pKbd->vkCode == VK_RETURN && QuickLook::Instance().IsVisible()) {
                QuickLook::Instance().OpenCurrentFile();
                QuickLook::Instance().Hide();
                return 1;
            } else if ((pKbd->vkCode == VK_UP || pKbd->vkCode == VK_DOWN || pKbd->vkCode == VK_LEFT || pKbd->vkCode == VK_RIGHT) && QuickLook::Instance().IsVisible()) {
                // Set brief poll timer to update preview as selection moves
                SetTimer(QuickLook::Instance().GetHwnd(), TIMER_SELECTION_POLL, 60, nullptr);
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    void QuickLook::RefreshSelectionIfExplorerActive() {
        std::wstring sel = GetSelectedExplorerOrDesktopFile();
        if (!sel.empty() && sel != m_currentFilePath) {
            PreviewFile(sel);
        }
    }

    void QuickLook::OpenCurrentFile() {
        if (!m_currentFilePath.empty()) {
            ShellExecuteW(nullptr, L"open", m_currentFilePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void QuickLook::PreviewFile(const std::wstring& filePath) {
        if (filePath.empty()) return;

        if (!LoadFile(filePath)) return;

        m_visible = true;

        // Position window centered
        int x = (m_screenWidth - m_width) / 2;
        int y = (m_screenHeight - m_height) / 2;
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height, SWP_SHOWWINDOW);

        m_surface.Resize(m_width, m_height);
        Render();
    }

    void QuickLook::Hide() {
        if (m_visible) {
            m_visible = false;
            ShowWindow(m_hwnd, SW_HIDE);
            m_imageBitmap.Reset();
            m_codeLines.clear();
            m_currentFilePath.clear();
        }
    }

    bool QuickLook::LoadFile(const std::wstring& filePath) {
        m_currentFilePath = filePath;
        m_imageBitmap.Reset();
        m_codeLines.clear();

        // Extract File Name
        size_t slash = filePath.find_last_of(L"\\/");
        m_fileName = (slash != std::wstring::npos) ? filePath.substr(slash + 1) : filePath;

        // Query Attributes for size and timestamp
        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad)) {
            uint64_t fileSize = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
            if (fileSize < 1024) {
                m_fileSizeStr = std::to_wstring(fileSize) + L" B";
            } else if (fileSize < 1024 * 1024) {
                wchar_t buf[32];
                swprintf_s(buf, L"%.1f KB", static_cast<double>(fileSize) / 1024.0);
                m_fileSizeStr = buf;
            } else if (fileSize < 1024 * 1024 * 1024) {
                wchar_t buf[32];
                swprintf_s(buf, L"%.1f MB", static_cast<double>(fileSize) / (1024.0 * 1024.0));
                m_fileSizeStr = buf;
            } else {
                wchar_t buf[32];
                swprintf_s(buf, L"%.2f GB", static_cast<double>(fileSize) / (1024.0 * 1024.0 * 1024.0));
                m_fileSizeStr = buf;
            }

            SYSTEMTIME st = {};
            FileTimeToSystemTime(&fad.ftLastWriteTime, &st);
            wchar_t dateBuf[64] = {};
            GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, dateBuf, 64);
            m_fileModDateStr = dateBuf;
        }

        // Type description via Shell
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(filePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_TYPENAME)) {
            m_fileTypeDesc = sfi.szTypeName;
        }

        // Determine File Type by extension
        std::wstring ext;
        size_t dot = filePath.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            ext = filePath.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        }

        static const std::unordered_set<std::wstring> imgExts = {
            L".png", L".jpg", L".jpeg", L".bmp", L".ico", L".gif", L".webp", L".tif", L".tiff"
        };

        static const std::unordered_set<std::wstring> codeExts = {
            L".txt", L".cpp", L".h", L".c", L".hpp", L".cs", L".js", L".ts", L".jsx", L".tsx",
            L".py", L".json", L".md", L".markdown", L".html", L".htm", L".css", L".scss",
            L".xml", L".yaml", L".yml", L".ini", L".bat", L".cmd", L".ps1", L".log", L".cmake",
            L".sql", L".sh", L".java", L".go", L".rs", L".toml", L".config"
        };

        if (imgExts.find(ext) != imgExts.end()) {
            return LoadImagePreview(filePath);
        } else if (codeExts.find(ext) != codeExts.end()) {
            return LoadTextPreview(filePath);
        } else {
            return LoadGenericPreview(filePath);
        }
    }

    bool QuickLook::LoadImagePreview(const std::wstring& filePath) {
        m_fileType = QuickLookFileType::Image;

        if (!m_wicFactory) return LoadGenericPreview(filePath);

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = m_wicFactory->CreateDecoderFromFilename(
            filePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &decoder
        );
        if (FAILED(hr) || !decoder) return LoadGenericPreview(filePath);

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) return LoadGenericPreview(filePath);

        ComPtr<IWICFormatConverter> converter;
        hr = m_wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr) || !converter) return LoadGenericPreview(filePath);

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0f,
            WICBitmapPaletteTypeCustom
        );
        if (FAILED(hr)) return LoadGenericPreview(filePath);

        UINT w = 0, h = 0;
        converter->GetSize(&w, &h);
        m_imageWidth = static_cast<int>(w);
        m_imageHeight = static_cast<int>(h);

        wchar_t dimBuf[64];
        swprintf_s(dimBuf, L"%d × %d", m_imageWidth, m_imageHeight);
        m_imageDimStr = dimBuf;

        auto ctx = DCompEngine::Instance().GetD2DContext();
        ctx->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &m_imageBitmap);

        // Dynamically compute window dimensions matching image aspect ratio
        float maxW = 860.0f * m_dpiScale;
        float maxH = 620.0f * m_dpiScale;
        float headerH = 44.0f * m_dpiScale;
        float availH = maxH - headerH;

        float aspect = static_cast<float>(m_imageWidth) / static_cast<float>(std::max(1, m_imageHeight));
        float winW = maxW;
        float winH = (winW / aspect) + headerH;

        if (winH > maxH) {
            winH = maxH;
            winW = (availH * aspect);
        }

        m_width = std::max(static_cast<int>(460.0f * m_dpiScale), static_cast<int>(winW));
        m_height = std::max(static_cast<int>(360.0f * m_dpiScale), static_cast<int>(winH));

        return true;
    }

    bool QuickLook::LoadTextPreview(const std::wstring& filePath) {
        m_fileType = QuickLookFileType::CodeText;

        std::ifstream file(filePath, std::ios::in);
        if (!file.is_open()) return LoadGenericPreview(filePath);

        m_codeLines.clear();
        std::string line;
        int lineNum = 1;

        while (std::getline(file, line) && lineNum <= 400) {
            // Convert line to wstring
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.length()), nullptr, 0);
            std::wstring wline(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.length()), &wline[0], size_needed);

            m_codeLines.push_back({ lineNum, wline });
            lineNum++;
        }

        m_width = static_cast<int>(760.0f * m_dpiScale);
        m_height = static_cast<int>(540.0f * m_dpiScale);

        return true;
    }

    bool QuickLook::LoadGenericPreview(const std::wstring& filePath) {
        m_fileType = QuickLookFileType::Generic;

        auto ctx = DCompEngine::Instance().GetD2DContext();
        m_fileIcon = IconManager::Instance().GetIconForPath(ctx, filePath, 64);

        m_width = static_cast<int>(540.0f * m_dpiScale);
        m_height = static_cast<int>(380.0f * m_dpiScale);

        return true;
    }

    void QuickLook::Render() {
        if (!m_hwnd || !m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        float radius = 12.0f * m_dpiScale;
        float w = static_cast<float>(m_width);
        float h = static_cast<float>(m_height);

        D2D1_ROUNDED_RECT modalRound = D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), radius, radius);

        // 1. Acrylic Background & Sleek Border
        ctx->FillRoundedRectangle(modalRound, m_brushBg.Get());
        ctx->DrawRoundedRectangle(modalRound, m_brushBorder.Get(), 1.0f);

        // 2. Header Bar
        float headerH = 44.0f * m_dpiScale;
        D2D1_ROUNDED_RECT headerRound = D2D1::RoundedRect(D2D1::RectF(0, 0, w, headerH), radius, radius);
        ctx->FillRoundedRectangle(headerRound, m_brushHeaderBg.Get());
        ctx->DrawLine(D2D1::Point2F(0, headerH), D2D1::Point2F(w, headerH), m_brushBorder.Get(), 1.0f);

        // Header Buttons (Right side)
        float btnPad = 10.0f * m_dpiScale;

        // Close Button (macOS Red circle style or X)
        float closeSize = 20.0f * m_dpiScale;
        float closeX = w - btnPad - closeSize;
        float closeY = (headerH - closeSize) * 0.5f;
        m_closeButtonRect = D2D1::RectF(closeX, closeY, closeX + closeSize, closeY + closeSize);

        D2D1_ROUNDED_RECT closeRound = D2D1::RoundedRect(m_closeButtonRect, closeSize * 0.5f, closeSize * 0.5f);
        ctx->FillRoundedRectangle(closeRound, m_closeHovered ? m_brushButtonHover.Get() : m_brushButtonBg.Get());
        if (m_formatTitle) {
            ctx->DrawTextW(L"×", 1, m_formatTitle.Get(), m_closeButtonRect, m_brushTextPrimary.Get());
        }

        // "Open" Action Button
        float openW = 68.0f * m_dpiScale;
        float openH = 26.0f * m_dpiScale;
        float openX = closeX - openW - 10.0f * m_dpiScale;
        float openY = (headerH - openH) * 0.5f;
        m_openButtonRect = D2D1::RectF(openX, openY, openX + openW, openY + openH);

        D2D1_ROUNDED_RECT openRound = D2D1::RoundedRect(m_openButtonRect, openH * 0.5f, openH * 0.5f);
        ctx->FillRoundedRectangle(openRound, m_openHovered ? m_brushButtonHover.Get() : m_brushButtonBg.Get());
        ctx->DrawRoundedRectangle(openRound, m_brushBorder.Get(), 1.0f);
        if (m_formatButton) {
            ctx->DrawTextW(L"Open", 4, m_formatButton.Get(), m_openButtonRect, m_brushTextPrimary.Get());
        }

        // Header Title & Metadata
        float titleX = 14.0f * m_dpiScale;
        D2D1_RECT_F titleRect = D2D1::RectF(titleX, 4.0f * m_dpiScale, openX - 10.0f * m_dpiScale, 24.0f * m_dpiScale);
        if (m_formatTitle) {
            ctx->DrawTextW(m_fileName.c_str(), static_cast<UINT32>(m_fileName.size()), m_formatTitle.Get(), titleRect, m_brushTextPrimary.Get());
        }

        std::wstring metaText = m_fileTypeDesc;
        if (!m_fileSizeStr.empty()) metaText += L" — " + m_fileSizeStr;
        if (!m_imageDimStr.empty()) metaText += L" (" + m_imageDimStr + L")";

        D2D1_RECT_F metaRect = D2D1::RectF(titleX, 24.0f * m_dpiScale, openX - 10.0f * m_dpiScale, 40.0f * m_dpiScale);
        if (m_formatMeta) {
            ctx->DrawTextW(metaText.c_str(), static_cast<UINT32>(metaText.size()), m_formatMeta.Get(), metaRect, m_brushTextSecondary.Get());
        }

        // 3. Body Viewport
        float bodyY = headerH + 6.0f * m_dpiScale;
        float bodyH = h - bodyY - 8.0f * m_dpiScale;

        if (m_fileType == QuickLookFileType::Image && m_imageBitmap) {
            // Center scaled image
            float availW = w - (24.0f * m_dpiScale);
            float availH = bodyH - (10.0f * m_dpiScale);

            float scale = std::min(availW / static_cast<float>(m_imageWidth), availH / static_cast<float>(m_imageHeight));
            float destW = static_cast<float>(m_imageWidth) * scale;
            float destH = static_cast<float>(m_imageHeight) * scale;

            float destX = (w - destW) * 0.5f;
            float destY = bodyY + (bodyH - destH) * 0.5f;

            D2D1_RECT_F imgRect = D2D1::RectF(destX, destY, destX + destW, destY + destH);
            ctx->DrawBitmap(m_imageBitmap.Get(), imgRect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            ctx->DrawRectangle(imgRect, m_brushBorder.Get(), 1.0f);
        }
        else if (m_fileType == QuickLookFileType::CodeText) {
            // Code Editor View with Line Numbers
            float gutterW = 46.0f * m_dpiScale;
            D2D1_RECT_F gutterRect = D2D1::RectF(8.0f * m_dpiScale, bodyY, 8.0f * m_dpiScale + gutterW, bodyY + bodyH);
            ctx->FillRectangle(gutterRect, m_brushGutterBg.Get());
            ctx->DrawLine(
                D2D1::Point2F(gutterRect.right, bodyY),
                D2D1::Point2F(gutterRect.right, bodyY + bodyH),
                m_brushBorder.Get(), 1.0f
            );

            float lineHeight = 18.0f * m_dpiScale;
            float textX = gutterRect.right + 12.0f * m_dpiScale;
            float textY = bodyY + 4.0f * m_dpiScale;

            static const std::unordered_set<std::wstring> keywords = {
                L"const", L"let", L"var", L"function", L"class", L"return", L"if", L"else",
                L"for", L"while", L"import", L"from", L"export", L"int", L"float", L"double",
                L"bool", L"void", L"true", L"false", L"nullptr", L"null", L"auto", L"public",
                L"private", L"protected", L"struct", L"namespace", L"include", L"def", L"package"
            };

            for (const auto& cl : m_codeLines) {
                if (textY + lineHeight > bodyY + bodyH) break;

                // Gutter Line Number
                std::wstring numStr = std::to_wstring(cl.lineNumber);
                D2D1_RECT_F lineNumRect = D2D1::RectF(gutterRect.left, textY, gutterRect.right - 8.0f * m_dpiScale, textY + lineHeight);
                if (m_formatGutter) {
                    ctx->DrawTextW(numStr.c_str(), static_cast<UINT32>(numStr.size()), m_formatGutter.Get(), lineNumRect, m_brushTextMuted.Get());
                }

                // Code Line with Simple Syntax Highlighting
                D2D1_RECT_F codeRect = D2D1::RectF(textX, textY, w - 16.0f * m_dpiScale, textY + lineHeight);

                // Quick syntax color check
                ID2D1SolidColorBrush* lineBrush = m_brushTextPrimary.Get();
                std::wstring trimmed = cl.text;
                size_t first = trimmed.find_first_not_of(L" \t");
                if (first != std::wstring::npos) {
                    trimmed = trimmed.substr(first);
                    if (trimmed.rfind(L"//", 0) == 0 || trimmed.rfind(L"#", 0) == 0 || trimmed.rfind(L"/*", 0) == 0) {
                        lineBrush = m_brushComment.Get();
                    } else if (trimmed.rfind(L"\"", 0) == 0 || trimmed.rfind(L"'", 0) == 0) {
                        lineBrush = m_brushString.Get();
                    } else {
                        // Check first word
                        size_t space = trimmed.find_first_of(L" \t(;");
                        std::wstring firstWord = (space != std::wstring::npos) ? trimmed.substr(0, space) : trimmed;
                        if (keywords.find(firstWord) != keywords.end()) {
                            lineBrush = m_brushKeyword.Get();
                        }
                    }
                }

                if (m_formatCode) {
                    ctx->DrawTextW(cl.text.c_str(), static_cast<UINT32>(cl.text.size()), m_formatCode.Get(), codeRect, lineBrush);
                }

                textY += lineHeight;
            }
        }
        else {
            // Generic File View
            float cx = w * 0.5f;
            float cy = bodyY + bodyH * 0.40f;

            if (m_fileIcon) {
                float iconSize = 64.0f * m_dpiScale;
                D2D1_RECT_F iconDest = D2D1::RectF(cx - iconSize * 0.5f, cy - iconSize * 0.5f, cx + iconSize * 0.5f, cy + iconSize * 0.5f);
                ctx->DrawBitmap(m_fileIcon.Get(), iconDest);
            }

            float labelY = cy + 46.0f * m_dpiScale;
            D2D1_RECT_F nameRect = D2D1::RectF(20.0f * m_dpiScale, labelY, w - 20.0f * m_dpiScale, labelY + 24.0f * m_dpiScale);
            if (m_formatTitle) {
                m_formatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawTextW(m_fileName.c_str(), static_cast<UINT32>(m_fileName.size()), m_formatTitle.Get(), nameRect, m_brushTextPrimary.Get());
                m_formatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            D2D1_RECT_F subRect = D2D1::RectF(20.0f * m_dpiScale, labelY + 26.0f * m_dpiScale, w - 20.0f * m_dpiScale, labelY + 46.0f * m_dpiScale);
            std::wstring info = m_fileTypeDesc + L" • " + m_fileSizeStr + (m_fileModDateStr.empty() ? L"" : L" • Modified " + m_fileModDateStr);
            if (m_formatMeta) {
                m_formatMeta->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawTextW(info.c_str(), static_cast<UINT32>(info.size()), m_formatMeta.Get(), subRect, m_brushTextSecondary.Get());
                m_formatMeta->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        m_surface.EndDraw();
    }

    LRESULT CALLBACK QuickLook::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        QuickLook* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<QuickLook*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<QuickLook*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT QuickLook::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_SELECTION_POLL) {
                KillTimer(hwnd, TIMER_SELECTION_POLL);
                RefreshSelectionIfExplorerActive();
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool closeH = (pt.x >= m_closeButtonRect.left && pt.x <= m_closeButtonRect.right &&
                           pt.y >= m_closeButtonRect.top && pt.y <= m_closeButtonRect.bottom);
            bool openH = (pt.x >= m_openButtonRect.left && pt.x <= m_openButtonRect.right &&
                          pt.y >= m_openButtonRect.top && pt.y <= m_openButtonRect.bottom);

            if (closeH != m_closeHovered || openH != m_openHovered) {
                m_closeHovered = closeH;
                m_openHovered = openH;
                Render();
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (m_closeHovered || m_openHovered) {
                m_closeHovered = false;
                m_openHovered = false;
                Render();
            }
            return 0;

        case WM_LBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (pt.x >= m_closeButtonRect.left && pt.x <= m_closeButtonRect.right &&
                pt.y >= m_closeButtonRect.top && pt.y <= m_closeButtonRect.bottom) {
                Hide();
                return 0;
            }
            if (pt.x >= m_openButtonRect.left && pt.x <= m_openButtonRect.right &&
                pt.y >= m_openButtonRect.top && pt.y <= m_openButtonRect.bottom) {
                OpenCurrentFile();
                Hide();
                return 0;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

}
