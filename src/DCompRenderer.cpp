#include "DCompRenderer.h"
#include <stdexcept>

namespace Aqua {

    DCompEngine& DCompEngine::Instance() {
        static DCompEngine instance;
        return instance;
    }

    bool DCompEngine::Initialize() {
        if (m_initialized) return true;

        UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL createdFeatureLevel;

        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            d3dFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &createdFeatureLevel,
            &m_d3dContext
        );

        if (FAILED(hr)) {
            // Fallback to WARP software renderer if hardware device fails
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                d3dFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                &m_d3dDevice,
                &createdFeatureLevel,
                &m_d3dContext
            );
        }

        if (FAILED(hr)) return false;

        // Retrieve DXGI Device
        hr = m_d3dDevice.As(&m_dxgiDevice);
        if (FAILED(hr)) return false;

        // Retrieve DXGI Factory
        ComPtr<IDXGIAdapter> adapter;
        hr = m_dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr)) return false;

        hr = adapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory));
        if (FAILED(hr)) return false;

        // Initialize Direct2D Factory & Device
        D2D1_FACTORY_OPTIONS d2dOptions = {};
#if defined(_DEBUG)
        d2dOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), &d2dOptions, &m_d2dFactory);
        if (FAILED(hr)) return false;

        hr = m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &m_d2dDevice);
        if (FAILED(hr)) return false;

        hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
        if (FAILED(hr)) return false;

        // Set high-quality antialiasing and text rendering
        m_d2dContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

        // DirectWrite Factory
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
        );
        if (FAILED(hr)) return false;

        // DirectComposition Device
        hr = DCompositionCreateDevice(
            m_dxgiDevice.Get(),
            __uuidof(IDCompositionDevice),
            reinterpret_cast<void**>(m_dcompDevice.GetAddressOf())
        );
        if (FAILED(hr)) return false;

        m_initialized = true;
        return true;
    }

    void DCompEngine::Shutdown() {
        m_dcompDevice.Reset();
        m_dwriteFactory.Reset();
        m_d2dContext.Reset();
        m_d2dDevice.Reset();
        m_d2dFactory.Reset();
        m_dxgiFactory.Reset();
        m_dxgiDevice.Reset();
        m_d3dContext.Reset();
        m_d3dDevice.Reset();
        m_initialized = false;
    }

    // ==========================================
    // DCompSurface Implementation
    // ==========================================

    DCompSurface::~DCompSurface() {
        Destroy();
    }

    bool DCompSurface::Create(HWND hwnd, int width, int height) {
        m_hwnd = hwnd;
        m_width = (width > 0) ? width : 1;
        m_height = (height > 0) ? height : 1;

        auto& engine = DCompEngine::Instance();
        if (!engine.Initialize()) return false;

        // Configure SwapChain for Composition (Pre-multiplied Alpha)
        DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
        swapDesc.Width = static_cast<UINT>(m_width);
        swapDesc.Height = static_cast<UINT>(m_height);
        swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapDesc.Stereo = FALSE;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.SampleDesc.Quality = 0;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 2;
        swapDesc.Scaling = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        swapDesc.Flags = 0;

        HRESULT hr = engine.GetDXGIFactory()->CreateSwapChainForComposition(
            engine.GetD3DDevice(),
            &swapDesc,
            nullptr,
            &m_swapChain
        );
        if (FAILED(hr)) return false;

        // DirectComposition Target
        hr = engine.GetDCompDevice()->CreateTargetForHwnd(hwnd, TRUE, &m_dcompTarget);
        if (FAILED(hr)) return false;

        // DirectComposition Visual
        hr = engine.GetDCompDevice()->CreateVisual(&m_dcompVisual);
        if (FAILED(hr)) return false;

        hr = m_dcompVisual->SetContent(m_swapChain.Get());
        if (FAILED(hr)) return false;

        hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
        if (FAILED(hr)) return false;

        hr = engine.GetDCompDevice()->Commit();
        if (FAILED(hr)) return false;

        // Bind Direct2D Bitmap Target
        ComPtr<IDXGISurface> surface;
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
        if (FAILED(hr)) return false;

        D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );

        hr = engine.GetD2DContext()->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &m_targetBitmap);
        return SUCCEEDED(hr);
    }

    bool DCompSurface::Resize(int width, int height) {
        if (!m_swapChain) return false;

        m_width = (width > 0) ? width : 1;
        m_height = (height > 0) ? height : 1;

        auto& engine = DCompEngine::Instance();
        engine.GetD2DContext()->SetTarget(nullptr);
        m_targetBitmap.Reset();

        HRESULT hr = m_swapChain->ResizeBuffers(
            2,
            static_cast<UINT>(m_width),
            static_cast<UINT>(m_height),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0
        );
        if (FAILED(hr)) return false;

        ComPtr<IDXGISurface> surface;
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
        if (FAILED(hr)) return false;

        D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );

        hr = engine.GetD2DContext()->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &m_targetBitmap);
        return SUCCEEDED(hr);
    }

    void DCompSurface::Destroy() {
        if (auto ctx = DCompEngine::Instance().GetD2DContext()) {
            ctx->SetTarget(nullptr);
        }
        m_targetBitmap.Reset();
        m_dcompVisual.Reset();
        m_dcompTarget.Reset();
        m_swapChain.Reset();
        m_hwnd = nullptr;
    }

    ID2D1DeviceContext* DCompSurface::BeginDraw() {
        if (!m_targetBitmap) return nullptr;

        auto ctx = DCompEngine::Instance().GetD2DContext();
        ctx->SetTarget(m_targetBitmap.Get());
        ctx->BeginDraw();
        // Fully clear to transparent so non-drawn areas are see-through
        ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        return ctx;
    }

    void DCompSurface::EndDraw() {
        auto ctx = DCompEngine::Instance().GetD2DContext();
        HRESULT hr = ctx->EndDraw();
        ctx->SetTarget(nullptr);

        if (SUCCEEDED(hr) && m_swapChain) {
            m_swapChain->Present(1, 0); // 1 = VSync on
            DCompEngine::Instance().GetDCompDevice()->Commit();
        }
    }

}

