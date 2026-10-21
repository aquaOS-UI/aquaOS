#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_2.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <powrprof.h>

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cmath>
#include <functional>
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace Aqua {

    // macOS Dark Theme Palette
    namespace Colors {
        // Frosted Glass Background
        const D2D1_COLOR_F GlassBackground      = D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.76f);
        const D2D1_COLOR_F GlassBackgroundDark  = D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.88f);
        const D2D1_COLOR_F GlassBorder          = D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.14f);
        const D2D1_COLOR_F GlassHighlight       = D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.22f);
        const D2D1_COLOR_F GlassSeparator       = D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.10f);

        // Text & Glyphs
        const D2D1_COLOR_F TextPrimary          = D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.00f);
        const D2D1_COLOR_F TextSecondary        = D2D1::ColorF(0.65f, 0.65f, 0.70f, 1.00f);
        const D2D1_COLOR_F TextMuted            = D2D1::ColorF(0.45f, 0.45f, 0.50f, 1.00f);

        // Hover & Selection
        const D2D1_COLOR_F HoverItem            = D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.12f);
        const D2D1_COLOR_F ActiveItem           = D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.20f);

        // macOS System Accents
        const D2D1_COLOR_F AccentBlue           = D2D1::ColorF(0.00f, 0.48f, 1.00f, 1.00f);
        const D2D1_COLOR_F AccentGreen          = D2D1::ColorF(0.20f, 0.78f, 0.35f, 1.00f);
        const D2D1_COLOR_F AccentOrange         = D2D1::ColorF(1.00f, 0.58f, 0.00f, 1.00f);
        const D2D1_COLOR_F AccentRed            = D2D1::ColorF(1.00f, 0.27f, 0.23f, 1.00f);
        const D2D1_COLOR_F AccentPurple         = D2D1::ColorF(0.69f, 0.32f, 0.87f, 1.00f);

        // Indicator dot
        const D2D1_COLOR_F RunningDot           = D2D1::ColorF(0.95f, 0.95f, 0.98f, 0.80f);
    }

    // Helper for DPI scaling
    inline float GetDpiScale(HWND hwnd) {
        UINT dpi = GetDpiForWindow(hwnd);
        return dpi ? (static_cast<float>(dpi) / 96.0f) : 1.0f;
    }

    inline int Scale(int value, float scale) {
        return static_cast<int>(std::round(value * scale));
    }

    inline float ScaleF(float value, float scale) {
        return value * scale;
    }
}
