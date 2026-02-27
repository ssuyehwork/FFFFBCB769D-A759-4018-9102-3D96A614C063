#include "tray.h"
#include <shellapi.h>

namespace Tray {

NOTIFYICONDATAW g_nid = {0};

#include "svg_icons.h"

void Create(HWND hWnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_ICON;

    HBITMAP hBmp = RenderSvgIcon(SvgPaths::LOCK_CLOSED, 16, 16, ColorTheme::Green);
    g_nid.hIcon = BitmapToIcon(hBmp);
    DeleteObject(hBmp);

    wcscpy(g_nid.szTip, L"FocusLock - 运行中");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void Destroy() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_nid.hIcon) {
        DestroyIcon(g_nid.hIcon);
        g_nid.hIcon = NULL;
    }
}

void Update(bool locked) {
    // 实际应根据锁定状态更换图标颜色
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

}
