#include "ui_unlock.h"
#include "security.h"
#include "ui_overlay.h"

namespace UIUnlock {

HWND g_hUnlockWnd = NULL;

LRESULT CALLBACK UnlockWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"输入密码解锁", WS_VISIBLE | WS_CHILD | SS_CENTER, 30, 20, 300, 25, hWnd, NULL, g_hInstance, NULL);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_PASSWORD, 40, 60, 280, 25, hWnd, (HMENU)2001, g_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"解锁", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 130, 110, 100, 35, hWnd, (HMENU)2002, g_hInstance, NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 2002) {
            wchar_t pass[256];
            GetDlgItemTextW(hWnd, 2001, pass, 256);
            if (Security::VerifyPassword(pass)) {
                UIOverlay::StopFocus();
                DestroyWindow(hWnd);
            } else {
                MessageBoxW(hWnd, L"密码错误", L"错误", MB_ICONERROR);
            }
        }
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void Show() {
    if (g_hUnlockWnd) return;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = UnlockWndProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 46));
    wc.lpszClassName = L"FocusLockUnlock";
    RegisterClassW(&wc);

    g_hUnlockWnd = CreateWindowExW(WS_EX_TOPMOST, L"FocusLockUnlock", L"解锁", WS_OVERLAPPED | WS_CAPTION,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 200, NULL, NULL, g_hInstance, NULL);

    ShowWindow(g_hUnlockWnd, SW_SHOW);
}

}
