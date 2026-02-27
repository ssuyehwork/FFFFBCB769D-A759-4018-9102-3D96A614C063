#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <memory>

// 常用颜色定义 (Catppuccin Mocha 风格)
namespace ColorTheme {
    const Gdiplus::Color Background = Gdiplus::Color(255, 30, 30, 46);   // #1E1E2E
    const Gdiplus::Color Foreground = Gdiplus::Color(255, 205, 214, 244); // #CDD6F4
    const Gdiplus::Color Blue = Gdiplus::Color(255, 137, 180, 250);       // #89B4FA
    const Gdiplus::Color Subtext = Gdiplus::Color(255, 166, 173, 200);    // #A6ADC8
    const Gdiplus::Color Overlay = Gdiplus::Color(255, 69, 71, 90);       // #45475A
    const Gdiplus::Color Green = Gdiplus::Color(255, 166, 227, 161);      // #A6E3A1
    const Gdiplus::Color Red = Gdiplus::Color(255, 243, 139, 168);        // #F38BA8
}

// 自定义消息
#define WM_TRAY_ICON (WM_USER + 100)
#define WM_TIMER_DONE (WM_USER + 101)
#define WM_UPDATE_COUNTDOWN (WM_USER + 102)

// 全局变量声明 (在 main.cpp 中定义)
extern HINSTANCE g_hInstance;
