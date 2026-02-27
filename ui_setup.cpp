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
HFONT g_hFont = NULL;
HBRUSH g_hBackBrush = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_hBackBrush = CreateSolidBrush(RGB(30, 30, 46));
        g_hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"微软雅黑");

        g_hLockIcon = RenderSvgIcon(SvgPaths::LOCK_CLOSED, 48, 48, ColorTheme::Blue, ColorTheme::Background);

        // 重新布局
        HWND hLabel1 = CreateWindowW(L"STATIC", L"专注时长 (分钟)", WS_VISIBLE | WS_CHILD, 40, 80, 120, 20, hWnd, NULL, g_hInstance, NULL);
        HWND hEdit1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"45", WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER, 170, 77, 60, 24, hWnd, (HMENU)1001, g_hInstance, NULL);

        HWND hLabel2 = CreateWindowW(L"STATIC", L"解锁密码", WS_VISIBLE | WS_CHILD, 40, 120, 120, 20, hWnd, NULL, g_hInstance, NULL);
        HWND hEdit2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_PASSWORD, 170, 117, 200, 24, hWnd, (HMENU)1002, g_hInstance, NULL);

        HWND hBtn = CreateWindowW(L"BUTTON", L"开始专注", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125, 180, 200, 40, hWnd, (HMENU)1003, g_hInstance, NULL);

        SendMessage(hLabel1, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hEdit1, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hLabel2, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hEdit2, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(205, 214, 244));
        SetBkColor(hdcStatic, RGB(30, 30, 46));
        return (INT_PTR)g_hBackBrush;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Gdiplus::Graphics graphics(hdc);
        graphics.Clear(ColorTheme::Background);

        if (g_hLockIcon) {
            Gdiplus::Bitmap bmp(g_hLockIcon, NULL);
            graphics.DrawImage(&bmp, 201, 15, 48, 48);
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
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hBackBrush) DeleteObject(g_hBackBrush);
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

    int width = 450;
    int height = 250;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hMainWnd = CreateWindowW(L"FocusLockSetup", L"FocusLock", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (screenWidth - width) / 2, (screenHeight - height) / 2, width, height, NULL, NULL, g_hInstance, NULL);

    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);
}

}
