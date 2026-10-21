#define NOMINMAX
#include "NotificationCenter.h"
#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Aqua {

    const wchar_t NOTIFICATION_CENTER_CLASS[] = L"AquaNotificationCenterWindow";
    const float PI = 3.14159265358979323846f;

    static uint64_t FileTimeToU64(const FILETIME& ft) {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    NotificationCenter::NotificationCenter() = default;

    NotificationCenter::~NotificationCenter() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool NotificationCenter::Create(HINSTANCE hInstance, int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        m_dpiScale = 1.0f;
        m_panelWidth = static_cast<int>(380 * m_dpiScale);
        m_panelHeight = m_screenHeight - 32;
        m_windowX = m_screenWidth - m_panelWidth;
        m_windowY = 32;

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = NotificationCenter::WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = NOTIFICATION_CENTER_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            NOTIFICATION_CENTER_CLASS,
            L"AquaOS Notification Center",
            WS_POPUP,
            m_windowX, m_windowY, m_panelWidth, m_panelHeight,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) return false;

        m_dpiScale = GetDpiScale(m_hwnd);
        m_panelWidth = static_cast<int>(380 * m_dpiScale);

        if (!m_surface.Create(m_hwnd, m_panelWidth, m_panelHeight)) {
            return false;
        }

        auto& engine = DCompEngine::Instance();
        auto dwrite = engine.GetDWriteFactory();
        auto ctx = engine.GetD2DContext();

        // DirectWrite Text Formats
        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.0f * m_dpiScale,
            L"en-US",
            &m_formatHeaderDate
        );

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.5f * m_dpiScale,
            L"en-US",
            &m_formatCardTitle
        );

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f * m_dpiScale,
            L"en-US",
            &m_formatMetricValue
        );
        if (m_formatMetricValue) {
            m_formatMetricValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatMetricValue->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
            L"en-US",
            &m_formatMetricLabel
        );
        if (m_formatMetricLabel) {
            m_formatMetricLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatMetricLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.5f * m_dpiScale,
            L"en-US",
            &m_formatCalendarHeader
        );

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * m_dpiScale,
            L"en-US",
            &m_formatCalendarDay
        );
        if (m_formatCalendarDay) {
            m_formatCalendarDay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_formatCalendarDay->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        dwrite->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f * m_dpiScale,
            L"en-US",
            &m_formatNoteText
        );

        // Direct2D Solid Color Brushes
        ctx->CreateSolidColorBrush(Colors::TextPrimary, &m_brushTextPrimary);
        ctx->CreateSolidColorBrush(Colors::TextSecondary, &m_brushTextSecondary);
        ctx->CreateSolidColorBrush(Colors::GlassBorder, &m_brushBorder);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.78f), &m_brushCardBg);
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.16f, 0.20f, 0.90f), &m_brushCardHighlight);
        ctx->CreateSolidColorBrush(Colors::AccentBlue, &m_brushAccentBlue);
        ctx->CreateSolidColorBrush(Colors::AccentPurple, &m_brushAccentPurple);
        ctx->CreateSolidColorBrush(Colors::AccentOrange, &m_brushAccentOrange);
        ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f), &m_brushTrack);

        // Initial system metrics query
        FILETIME idle, kern, user;
        if (GetSystemTimes(&idle, &kern, &user)) {
            m_prevIdleTime = FileTimeToU64(idle);
            m_prevKernelTime = FileTimeToU64(kern);
            m_prevUserTime = FileTimeToU64(user);
        }
        UpdateMetrics();

        // 1-second timer for live system metrics & time
        SetTimer(m_hwnd, 2001, 1000, nullptr);

        return true;
    }

    void NotificationCenter::UpdateMetrics() {
        // RAM Load
        MEMORYSTATUSEX mem = { sizeof(mem) };
        if (GlobalMemoryStatusEx(&mem)) {
            m_ramPercent = static_cast<int>(mem.dwMemoryLoad);
            m_ramTotalGB = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
            double availGB = static_cast<double>(mem.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
            m_ramUsedGB = m_ramTotalGB - availGB;
        }

        // CPU Load
        FILETIME idle, kern, user;
        if (GetSystemTimes(&idle, &kern, &user)) {
            uint64_t curIdle = FileTimeToU64(idle);
            uint64_t curKern = FileTimeToU64(kern);
            uint64_t curUser = FileTimeToU64(user);

            uint64_t dIdle = curIdle - m_prevIdleTime;
            uint64_t dKern = curKern - m_prevKernelTime;
            uint64_t dUser = curUser - m_prevUserTime;
            uint64_t dTotal = dKern + dUser;

            if (dTotal > 0) {
                m_cpuPercent = static_cast<int>((100 * (dTotal - dIdle)) / dTotal);
                m_cpuPercent = std::clamp(m_cpuPercent, 0, 100);
            }

            m_prevIdleTime = curIdle;
            m_prevKernelTime = curKern;
            m_prevUserTime = curUser;
        }
    }

    void NotificationCenter::DrawProgressRing(ID2D1DeviceContext* ctx, float cx, float cy, float radius, float thickness, float percent, D2D1_COLOR_F color) {
        percent = std::clamp(percent, 0.0f, 1.0f);

        // 1. Background Track
        ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), m_brushTrack.Get(), thickness);

        if (percent <= 0.001f) return;

        ComPtr<ID2D1SolidColorBrush> progressBrush;
        ctx->CreateSolidColorBrush(color, &progressBrush);

        if (percent >= 0.999f) {
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), progressBrush.Get(), thickness);
            return;
        }

        // 2. Active Progress Arc
        auto d2dFactory = DCompEngine::Instance().GetD2DFactory();
        ComPtr<ID2D1PathGeometry> path;
        d2dFactory->CreatePathGeometry(&path);

        if (path) {
            ComPtr<ID2D1GeometrySink> sink;
            path->Open(&sink);

            float startAngle = -PI * 0.5f; // 12 o'clock
            float sweepAngle = percent * 2.0f * PI;
            float endAngle = startAngle + sweepAngle;

            float startX = cx + radius * std::cos(startAngle);
            float startY = cy + radius * std::sin(startAngle);
            float endX = cx + radius * std::cos(endAngle);
            float endY = cy + radius * std::sin(endAngle);

            sink->BeginFigure(D2D1::Point2F(startX, startY), D2D1_FIGURE_BEGIN_HOLLOW);

            D2D1_ARC_SEGMENT arc = {};
            arc.point = D2D1::Point2F(endX, endY);
            arc.size = D2D1::SizeF(radius, radius);
            arc.rotationAngle = 0.0f;
            arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            arc.arcSize = (percent > 0.5f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

            sink->AddArc(arc);
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            // Rounded cap stroke style
            D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
            );
            ComPtr<ID2D1StrokeStyle> strokeStyle;
            d2dFactory->CreateStrokeStyle(strokeProps, nullptr, 0, &strokeStyle);

            ctx->DrawGeometry(path.Get(), progressBrush.Get(), thickness, strokeStyle.Get());
        }
    }

    void NotificationCenter::Show() {
        if (m_visible) return;

        m_visible = true;
        UpdateMetrics();

        // Slide from right
        m_currentOffset = static_cast<float>(m_panelWidth);
        m_targetOffset = 0.0f;
        m_isAnimating = true;

        SetWindowPos(m_hwnd, HWND_TOPMOST, m_windowX + static_cast<int>(m_currentOffset), m_windowY, m_panelWidth, m_panelHeight, SWP_SHOWWINDOW);

        SetTimer(m_hwnd, 2002, 16, nullptr); // 60 FPS animation timer
        Render();
    }

    void NotificationCenter::Hide() {
        if (!m_visible) return;

        m_visible = false;
        KillTimer(m_hwnd, 2002);
        ShowWindow(m_hwnd, SW_HIDE);
    }

    void NotificationCenter::Toggle() {
        if (m_visible) {
            Hide();
        } else {
            Show();
        }
    }

    void NotificationCenter::Render() {
        if (!m_visible) return;

        auto ctx = m_surface.BeginDraw();
        if (!ctx) return;

        // 1. Frosted Dark Glass Sidebar Background
        ctx->Clear(D2D1::ColorF(0.08f, 0.08f, 0.10f, 0.82f));

        float pad = 16.0f * m_dpiScale;
        float curY = pad;
        float cardW = static_cast<float>(m_panelWidth) - (pad * 2.0f);

        // 2. Header: Date & Day
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow;
        localtime_s(&tmNow, &t);

        wchar_t dateStr[64] = {};
        wcsftime(dateStr, 64, L"%A, %B %d", &tmNow);

        if (m_formatHeaderDate) {
            D2D1_RECT_F dateRect = D2D1::RectF(pad + 4.0f, curY, pad + cardW, curY + 28.0f * m_dpiScale);
            ctx->DrawTextW(dateStr, static_cast<UINT32>(wcslen(dateStr)), m_formatHeaderDate.Get(), dateRect, m_brushTextPrimary.Get());
        }
        curY += 34.0f * m_dpiScale;

        // 3. Widget 1: System Performance Dual Rings (CPU & RAM)
        float perfH = 120.0f * m_dpiScale;
        D2D1_ROUNDED_RECT perfCard = D2D1::RoundedRect(D2D1::RectF(pad, curY, pad + cardW, curY + perfH), 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(perfCard, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(perfCard, m_brushBorder.Get(), 1.0f);

        float halfW = cardW * 0.5f;

        // Left: CPU Ring
        float cpuCx = pad + halfW * 0.5f;
        float ringCy = curY + 54.0f * m_dpiScale;
        float ringRadius = 32.0f * m_dpiScale;
        float ringThick = 6.5f * m_dpiScale;
        DrawProgressRing(ctx, cpuCx, ringCy, ringRadius, ringThick, static_cast<float>(m_cpuPercent) / 100.0f, Colors::AccentBlue);

        // CPU Percentage Text
        std::wstring cpuStr = std::to_wstring(m_cpuPercent) + L"%";
        D2D1_RECT_F cpuValRect = D2D1::RectF(cpuCx - 28.0f, ringCy - 12.0f, cpuCx + 28.0f, ringCy + 12.0f);
        ctx->DrawTextW(cpuStr.c_str(), static_cast<UINT32>(cpuStr.length()), m_formatMetricValue.Get(), cpuValRect, m_brushTextPrimary.Get());

        // CPU Label
        D2D1_RECT_F cpuLblRect = D2D1::RectF(pad, curY + 92.0f * m_dpiScale, pad + halfW, curY + 112.0f * m_dpiScale);
        ctx->DrawTextW(L"CPU Usage", 9, m_formatMetricLabel.Get(), cpuLblRect, m_brushTextSecondary.Get());

        // Right: RAM Ring
        float ramCx = pad + halfW + halfW * 0.5f;
        DrawProgressRing(ctx, ramCx, ringCy, ringRadius, ringThick, static_cast<float>(m_ramPercent) / 100.0f, Colors::AccentPurple);

        std::wstring ramStr = std::to_wstring(m_ramPercent) + L"%";
        D2D1_RECT_F ramValRect = D2D1::RectF(ramCx - 28.0f, ringCy - 12.0f, ramCx + 28.0f, ringCy + 12.0f);
        ctx->DrawTextW(ramStr.c_str(), static_cast<UINT32>(ramStr.length()), m_formatMetricValue.Get(), ramValRect, m_brushTextPrimary.Get());

        std::wstringstream ramSs;
        ramSs << std::fixed << std::setprecision(1) << m_ramUsedGB << L" / " << m_ramTotalGB << L" GB";
        std::wstring ramLbl = ramSs.str();
        D2D1_RECT_F ramLblRect = D2D1::RectF(pad + halfW, curY + 92.0f * m_dpiScale, pad + cardW, curY + 112.0f * m_dpiScale);
        ctx->DrawTextW(ramLbl.c_str(), static_cast<UINT32>(ramLbl.length()), m_formatMetricLabel.Get(), ramLblRect, m_brushTextSecondary.Get());

        curY += perfH + 12.0f * m_dpiScale;

        // 4. Widget 2: Live macOS Calendar Widget
        float calH = 175.0f * m_dpiScale;
        D2D1_ROUNDED_RECT calCard = D2D1::RoundedRect(D2D1::RectF(pad, curY, pad + cardW, curY + calH), 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(calCard, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(calCard, m_brushBorder.Get(), 1.0f);

        // Month Header
        wchar_t monthHeader[64] = {};
        wcsftime(monthHeader, 64, L"%B %Y", &tmNow);
        D2D1_RECT_F calTitleRect = D2D1::RectF(pad + 16.0f * m_dpiScale, curY + 10.0f * m_dpiScale, pad + cardW - 16.0f, curY + 28.0f * m_dpiScale);
        ctx->DrawTextW(monthHeader, static_cast<UINT32>(wcslen(monthHeader)), m_formatCalendarHeader.Get(), calTitleRect, m_brushAccentOrange.Get());

        // Weekday labels: S M T W T F S
        const wchar_t* daysOfWeek[] = { L"S", L"M", L"T", L"W", L"T", L"F", L"S" };
        float calGridStartX = pad + 16.0f * m_dpiScale;
        float calGridW = cardW - (32.0f * m_dpiScale);
        float cellW = calGridW / 7.0f;
        float calGridStartY = curY + 34.0f * m_dpiScale;
        float cellH = 22.0f * m_dpiScale;

        for (int d = 0; d < 7; ++d) {
            D2D1_RECT_F dayHdr = D2D1::RectF(calGridStartX + d * cellW, calGridStartY, calGridStartX + (d + 1) * cellW, calGridStartY + cellH);
            ctx->DrawTextW(daysOfWeek[d], 1, m_formatCalendarDay.Get(), dayHdr, m_brushTextSecondary.Get());
        }

        // Days of Month
        std::tm firstDay = tmNow;
        firstDay.tm_mday = 1;
        std::mktime(&firstDay);
        int firstWeekday = firstDay.tm_wday; // 0 = Sun

        // Number of days in current month
        int daysInMonth = 31;
        int m = tmNow.tm_mon;
        if (m == 3 || m == 5 || m == 8 || m == 10) daysInMonth = 30;
        else if (m == 1) {
            int y = tmNow.tm_year + 1900;
            daysInMonth = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 29 : 28;
        }

        float daysStartY = calGridStartY + cellH + 2.0f;
        for (int day = 1; day <= daysInMonth; ++day) {
            int slot = firstWeekday + day - 1;
            int col = slot % 7;
            int row = slot / 7;

            float cellCx = calGridStartX + col * cellW + (cellW * 0.5f);
            float cellCy = daysStartY + row * cellH + (cellH * 0.5f);

            // Highlight Today
            if (day == tmNow.tm_mday) {
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cellCx, cellCy), 10.5f * m_dpiScale, 10.5f * m_dpiScale), m_brushAccentBlue.Get());
            }

            std::wstring dStr = std::to_wstring(day);
            D2D1_RECT_F cellRect = D2D1::RectF(cellCx - 10.0f, cellCy - 10.0f, cellCx + 10.0f, cellCy + 10.0f);
            ctx->DrawTextW(dStr.c_str(), static_cast<UINT32>(dStr.length()), m_formatCalendarDay.Get(), cellRect, (day == tmNow.tm_mday) ? m_brushTextPrimary.Get() : m_brushTextSecondary.Get());
        }

        curY += calH + 12.0f * m_dpiScale;

        // 5. Widget 3: World Clock Widget
        float clockH = 75.0f * m_dpiScale;
        D2D1_ROUNDED_RECT clockCard = D2D1::RoundedRect(D2D1::RectF(pad, curY, pad + cardW, curY + clockH), 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(clockCard, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(clockCard, m_brushBorder.Get(), 1.0f);

        // Local Time
        wchar_t localTimeBuf[32] = {};
        wcsftime(localTimeBuf, 32, L"%I:%M %p", &tmNow);
        D2D1_RECT_F localTimeRect = D2D1::RectF(pad + 16.0f * m_dpiScale, curY + 12.0f * m_dpiScale, pad + halfW, curY + 38.0f * m_dpiScale);
        ctx->DrawTextW(localTimeBuf, static_cast<UINT32>(wcslen(localTimeBuf)), m_formatCardTitle.Get(), localTimeRect, m_brushTextPrimary.Get());

        D2D1_RECT_F localLblRect = D2D1::RectF(pad + 16.0f * m_dpiScale, curY + 40.0f * m_dpiScale, pad + halfW, curY + 60.0f * m_dpiScale);
        ctx->DrawTextW(L"Local Time", 10, m_formatMetricLabel.Get(), localLblRect, m_brushTextSecondary.Get());

        // Cupertino Time
        int cupHour = (tmNow.tm_hour + 24 - 12) % 24; // Approximation for Cupertino UTC-7
        wchar_t cupBuf[32] = {};
        swprintf_s(cupBuf, L"%02d:%02d %s", (cupHour % 12 == 0) ? 12 : (cupHour % 12), tmNow.tm_min, (cupHour >= 12) ? L"PM" : L"AM");
        D2D1_RECT_F cupTimeRect = D2D1::RectF(pad + halfW, curY + 12.0f * m_dpiScale, pad + cardW - 16.0f, curY + 38.0f * m_dpiScale);
        ctx->DrawTextW(cupBuf, static_cast<UINT32>(wcslen(cupBuf)), m_formatCardTitle.Get(), cupTimeRect, m_brushTextPrimary.Get());

        D2D1_RECT_F cupLblRect = D2D1::RectF(pad + halfW, curY + 40.0f * m_dpiScale, pad + cardW - 16.0f, curY + 60.0f * m_dpiScale);
        ctx->DrawTextW(L"Cupertino, CA", 13, m_formatMetricLabel.Get(), cupLblRect, m_brushTextSecondary.Get());

        curY += clockH + 12.0f * m_dpiScale;

        // 6. Widget 4: Quick Notes Card
        float noteH = 80.0f * m_dpiScale;
        D2D1_ROUNDED_RECT noteCard = D2D1::RoundedRect(D2D1::RectF(pad, curY, pad + cardW, curY + noteH), 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(noteCard, m_brushCardBg.Get());
        ctx->DrawRoundedRectangle(noteCard, m_brushBorder.Get(), 1.0f);

        // Note Icon & Title
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(pad + 20.0f * m_dpiScale, curY + 18.0f * m_dpiScale), 4.5f * m_dpiScale, 4.5f * m_dpiScale), m_brushAccentOrange.Get());
        D2D1_RECT_F noteTitleRect = D2D1::RectF(pad + 32.0f * m_dpiScale, curY + 10.0f * m_dpiScale, pad + cardW - 16.0f, curY + 28.0f * m_dpiScale);
        ctx->DrawTextW(L"Quick Note", 10, m_formatCalendarHeader.Get(), noteTitleRect, m_brushTextPrimary.Get());

        D2D1_RECT_F noteBodyRect = D2D1::RectF(pad + 16.0f * m_dpiScale, curY + 32.0f * m_dpiScale, pad + cardW - 16.0f, curY + noteH - 10.0f);
        ctx->DrawTextW(m_quickNote.c_str(), static_cast<UINT32>(m_quickNote.length()), m_formatNoteText.Get(), noteBodyRect, m_brushTextSecondary.Get());

        curY += noteH + 12.0f * m_dpiScale;

        // 7. Widget 5: Recent Notification Pill
        float notifH = 65.0f * m_dpiScale;
        D2D1_ROUNDED_RECT notifCard = D2D1::RoundedRect(D2D1::RectF(pad, curY, pad + cardW, curY + notifH), 14.0f * m_dpiScale, 14.0f * m_dpiScale);
        ctx->FillRoundedRectangle(notifCard, m_brushCardHighlight.Get());
        ctx->DrawRoundedRectangle(notifCard, m_brushBorder.Get(), 1.0f);

        D2D1_RECT_F notifTitle = D2D1::RectF(pad + 14.0f * m_dpiScale, curY + 10.0f * m_dpiScale, pad + cardW - 14.0f, curY + 28.0f * m_dpiScale);
        ctx->DrawTextW(L"AquaOS System  •  Now", 21, m_formatCalendarHeader.Get(), notifTitle, m_brushTextPrimary.Get());

        D2D1_RECT_F notifBody = D2D1::RectF(pad + 14.0f * m_dpiScale, curY + 30.0f * m_dpiScale, pad + cardW - 14.0f, curY + notifH - 8.0f);
        ctx->DrawTextW(L"DirectComposition hardware-accelerated 120 FPS desktop active.", 62, m_formatNoteText.Get(), notifBody, m_brushTextSecondary.Get());

        m_surface.EndDraw();
    }

    LRESULT CALLBACK NotificationCenter::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        NotificationCenter* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<NotificationCenter*>(cs->lpCreateParams);
            if (pThis) {
                pThis->m_hwnd = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        } else {
            pThis = reinterpret_cast<NotificationCenter*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            return pThis->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT NotificationCenter::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER: {
            if (wParam == 2001) {
                // 1s update
                UpdateMetrics();
                Render();
                return 0;
            } else if (wParam == 2002) {
                // Slide animation
                float diff = m_targetOffset - m_currentOffset;
                if (std::abs(diff) > 0.5f) {
                    m_currentOffset += diff * 0.35f;
                    SetWindowPos(m_hwnd, HWND_TOPMOST, m_windowX + static_cast<int>(m_currentOffset), m_windowY, m_panelWidth, m_panelHeight, SWP_NOACTIVATE);
                } else {
                    m_currentOffset = m_targetOffset;
                    SetWindowPos(m_hwnd, HWND_TOPMOST, m_windowX + static_cast<int>(m_currentOffset), m_windowY, m_panelWidth, m_panelHeight, SWP_NOACTIVATE);
                    KillTimer(m_hwnd, 2002);
                    m_isAnimating = false;
                }
                return 0;
            }
            break;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE && m_visible) {
                Hide();
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                Hide();
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            m_surface.Destroy();
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}

