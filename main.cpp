#include "common.h"
#include "config.h"
#include "ui_setup.h"
#include "tray.h"
#include <objbase.h>

#pragma comment(lib, "gdiplus.lib")

HINSTANCE g_hInstance = NULL;
Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
ULONG_PTR g_gdiplusToken;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // 初始化 GDI+
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);

    // 加载配置
    AppConfig config;
    Config::Load(config);

    // 显示设置窗口
    UISetup::Show(config);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    Tray::Destroy();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return (int)msg.wParam;
}
