#include "ui_overlay.h"
#include "timer_engine.h"
#include "overlay_engine.h"
#include "ui_unlock.h"
#include <vector>

namespace UIOverlay {

struct MonitorInfo {
    HWND hwnd;
    RECT rect;
};

std::vector<MonitorInfo> g_monitors;
int g_remaining = 0;
const AppConfig* g_pConfig = nullptr;

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Gdiplus::Graphics graphics(hdc);

        Gdiplus::SolidBrush backBrush(Gdiplus::Color((BYTE)(g_pConfig->alpha * 2.55), 0, 0, 0));
        graphics.FillRectangle(&backBrush, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom);

        // 绘制倒计时
        wchar_t timeStr[10];
        swprintf(timeStr, 10, L"%02d:%02d", g_remaining / 60, g_remaining % 60);

        Gdiplus::Font font(L"Segoe UI", 72, Gdiplus::FontStyleBold);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color::White);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);

        RECT rc; GetClientRect(hWnd, &rc);
        Gdiplus::RectF rectF(0, (float)rc.bottom / 3, (float)rc.right, 150);
        graphics.DrawString(timeStr, -1, &font, rectF, &format, &textBrush);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            UIUnlock::Show();
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"FocusLockOverlay", NULL, WS_POPUP,
        lprcMonitor->left, lprcMonitor->top,
        lprcMonitor->right - lprcMonitor->left,
        lprcMonitor->bottom - lprcMonitor->top,
        NULL, NULL, g_hInstance, NULL);

    SetLayeredWindowAttributes(hwnd, 0, (BYTE)(g_pConfig->alpha * 2.55), LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOW);
    g_monitors.push_back({hwnd, *lprcMonitor});
    return TRUE;
}

void StartFocus(int minutes, const AppConfig& config) {
    g_pConfig = &config;
    g_remaining = minutes * 60;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"FocusLockOverlay";
    RegisterClassW(&wc);

    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);

    OverlayEngine::InstallHooks();
    if (!g_monitors.empty()) {
        OverlayEngine::StartTopmostThread(g_monitors[0].hwnd);
    }

    TimerEngine::Start(minutes, [](int rem) {
        g_remaining = rem;
        for (auto& m : g_monitors) {
            InvalidateRect(m.hwnd, NULL, FALSE);
        }
        if (rem <= 0) {
            // 自动解锁逻辑
            StopFocus();
        }
    });
}

void StopFocus() {
    TimerEngine::Stop();
    OverlayEngine::UninstallHooks();
    OverlayEngine::StopTopmostThread();
    for (auto& m : g_monitors) {
        DestroyWindow(m.hwnd);
    }
    g_monitors.clear();
    // 重新显示主窗口
    // ...
}

}
