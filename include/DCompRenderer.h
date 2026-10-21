#pragma once
#include "Common.h"

namespace Aqua {

    // Shared Graphics Engine (Direct3D 11, Direct2D, DirectWrite, DirectComposition)
    class DCompEngine {
    public:
        static DCompEngine& Instance();

        bool Initialize();
        void Shutdown();

        ID3D11Device* GetD3DDevice() const { return m_d3dDevice.Get(); }
        ID2D1DeviceContext* GetD2DContext() const { return m_d2dContext.Get(); }
        IDWriteFactory* GetDWriteFactory() const { return m_dwriteFactory.Get(); }
        IDCompositionDevice* GetDCompDevice() const { return m_dcompDevice.Get(); }
        IDXGIFactory2* GetDXGIFactory() const { return m_dxgiFactory.Get(); }

    private:
        DCompEngine() = default;
        ~DCompEngine() = default;

        bool m_initialized = false;
        ComPtr<ID3D11Device> m_d3dDevice;
        ComPtr<ID3D11DeviceContext> m_d3dContext;
        ComPtr<IDXGIDevice> m_dxgiDevice;
        ComPtr<IDXGIFactory2> m_dxgiFactory;
        ComPtr<ID2D1Factory2> m_d2dFactory;
        ComPtr<ID2D1Device1> m_d2dDevice;
        ComPtr<ID2D1DeviceContext> m_d2dContext;
        ComPtr<IDWriteFactory> m_dwriteFactory;
        ComPtr<IDCompositionDevice> m_dcompDevice;
    };

    // Per-window DirectComposition & Direct2D Presentation Surface
    class DCompSurface {
    public:
        DCompSurface() = default;
        ~DCompSurface();

        bool Create(HWND hwnd, int width, int height);
        bool Resize(int width, int height);
        void Destroy();

        ID2D1DeviceContext* BeginDraw();
        void EndDraw();

        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }

    private:
        HWND m_hwnd = nullptr;
        int m_width = 0;
        int m_height = 0;

        ComPtr<IDXGISwapChain1> m_swapChain;
        ComPtr<ID2D1Bitmap1> m_targetBitmap;
        ComPtr<IDCompositionTarget> m_dcompTarget;
        ComPtr<IDCompositionVisual> m_dcompVisual;
    };

}

