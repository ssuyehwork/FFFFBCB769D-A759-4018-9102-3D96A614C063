#include "ui_setup.h"
#include "svg_icons.h"
#include "security.h"
#include "ui_overlay.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace UISetup {

HWND g_hMainWnd = NULL;
AppConfig* g_pConfig = nullptr;
HBITMAP g_hLockIcon = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_hLockIcon = RenderSvgIcon(SvgPaths::LOCK_CLOSED, 64, 64, ColorTheme::Blue);

        // 创建简单的控件示例 (实际应按规格布局)
        CreateWindowW(L"STATIC", L"专注时长 (分钟)", WS_VISIBLE | WS_CHILD, 50, 100, 150, 20, hWnd, NULL, g_hInstance, NULL);
        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"45", WS_VISIBLE | WS_CHILD | ES_NUMBER, 50, 125, 100, 25, hWnd, (HMENU)1001, g_hInstance, NULL);

        CreateWindowW(L"STATIC", L"解锁密码", WS_VISIBLE | WS_CHILD, 50, 170, 150, 20, hWnd, NULL, g_hInstance, NULL);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_PASSWORD, 50, 195, 200, 25, hWnd, (HMENU)1002, g_hInstance, NULL);

        CreateWindowW(L"BUTTON", L"开始专注", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 480, 200, 44, hWnd, (HMENU)1003, g_hInstance, NULL);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Gdiplus::Graphics graphics(hdc);
        graphics.Clear(ColorTheme::Background);

        if (g_hLockIcon) {
            Gdiplus::Bitmap bmp(g_hLockIcon, NULL);
            graphics.DrawImage(&bmp, 208, 20, 64, 64);
        }

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1003) { // 开始专注
            wchar_t pass[256];
            GetDlgItemTextW(hWnd, 1002, pass, 256);
            if (wcslen(pass) < 1) {
                MessageBoxW(hWnd, L"请设置密码", L"提示", MB_ICONWARNING);
                break;
            }
            Security::SavePassword(pass);

            wchar_t mins[10];
            GetDlgItemTextW(hWnd, 1001, mins, 10);
            int m = _wtoi(mins);

            g_pConfig->defaultMinutes = m;
            Config::Save(*g_pConfig);

            ShowWindow(hWnd, SW_HIDE);
            UIOverlay::StartFocus(m, *g_pConfig);
        }
        break;
    }
    case WM_DESTROY:
        if (g_hLockIcon) DeleteObject(g_hLockIcon);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void Show(AppConfig& config) {
    g_pConfig = &config;
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 46));
    wc.lpszClassName = L"FocusLockSetup";
    RegisterClassW(&wc);

    g_hMainWnd = CreateWindowW(L"FocusLockSetup", L"FocusLock", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 580, NULL, NULL, g_hInstance, NULL);

    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);
}

}
