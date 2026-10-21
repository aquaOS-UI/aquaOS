#pragma once
#include "Common.h"
#include "DCompRenderer.h"
#include <wincodec.h>
#include <shlobj.h>
#include <exdisp.h>
#include <shlwapi.h>
#include <string>
#include <vector>

namespace Aqua {

    enum class QuickLookFileType {
        Image,
        CodeText,
        Generic
    };

    struct CodeLine {
        int lineNumber;
        std::wstring text;
    };

    class QuickLook {
    public:
        static QuickLook& Instance();

        bool Initialize(HINSTANCE hInstance, int screenWidth, int screenHeight);
        void Shutdown();

        // Preview a specified file path
        void PreviewFile(const std::wstring& filePath);
        void Hide();
        bool IsVisible() const { return m_visible; }
        HWND GetHwnd() const { return m_hwnd; }

        void OpenCurrentFile();
        void RefreshSelectionIfExplorerActive();

    private:
        QuickLook();
        ~QuickLook();

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

        bool LoadFile(const std::wstring& filePath);
        bool LoadImagePreview(const std::wstring& filePath);
        bool LoadTextPreview(const std::wstring& filePath);
        bool LoadGenericPreview(const std::wstring& filePath);

        void Render();

        HWND m_hwnd = nullptr;
        DCompSurface m_surface;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_width = 720;
        int m_height = 520;
        float m_dpiScale = 1.0f;
        bool m_visible = false;

        // Active Preview Data
        std::wstring m_currentFilePath;
        std::wstring m_fileName;
        std::wstring m_fileTypeDesc;
        std::wstring m_fileSizeStr;
        std::wstring m_fileModDateStr;
        std::wstring m_imageDimStr;

        QuickLookFileType m_fileType = QuickLookFileType::Generic;

        // Image Preview
        ComPtr<IWICImagingFactory> m_wicFactory;
        ComPtr<ID2D1Bitmap1> m_imageBitmap;
        int m_imageWidth = 0;
        int m_imageHeight = 0;

        // Code / Text Preview
        std::vector<CodeLine> m_codeLines;

        // Generic File Preview
        ComPtr<ID2D1Bitmap> m_fileIcon;

        // Interactive Rectangles
        D2D1_RECT_F m_closeButtonRect = {};
        D2D1_RECT_F m_openButtonRect = {};
        bool m_closeHovered = false;
        bool m_openHovered = false;

        // DirectWrite Formats
        ComPtr<IDWriteTextFormat> m_formatTitle;
        ComPtr<IDWriteTextFormat> m_formatMeta;
        ComPtr<IDWriteTextFormat> m_formatCode;
        ComPtr<IDWriteTextFormat> m_formatGutter;
        ComPtr<IDWriteTextFormat> m_formatButton;

        // Direct2D Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushBg;
        ComPtr<ID2D1SolidColorBrush> m_brushHeaderBg;
        ComPtr<ID2D1SolidColorBrush> m_brushBorder;
        ComPtr<ID2D1SolidColorBrush> m_brushTextPrimary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextSecondary;
        ComPtr<ID2D1SolidColorBrush> m_brushTextMuted;
        ComPtr<ID2D1SolidColorBrush> m_brushGutterBg;
        ComPtr<ID2D1SolidColorBrush> m_brushButtonBg;
        ComPtr<ID2D1SolidColorBrush> m_brushButtonHover;
        ComPtr<ID2D1SolidColorBrush> m_brushAccent;

        // Syntax Highlighting Brushes
        ComPtr<ID2D1SolidColorBrush> m_brushKeyword;
        ComPtr<ID2D1SolidColorBrush> m_brushString;
        ComPtr<ID2D1SolidColorBrush> m_brushComment;
        ComPtr<ID2D1SolidColorBrush> m_brushNumber;

        HHOOK m_hKeyboardHook = nullptr;

        static constexpr UINT_PTR TIMER_SELECTION_POLL = 301;
    };

}

