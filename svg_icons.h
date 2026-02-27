#pragma once
#include "common.h"

// SVG 路径定义
namespace SvgPaths {
    // Lock closed icon
    const wchar_t* const LOCK_CLOSED = L"M12 1C8.676 1 6 3.676 6 7v1H3v15h18V8h-3V7c0-3.324-2.676-6-6-6z M12 3c2.276 0 4 1.724 4 4v1H8V7c0-2.276 1.724-4 4-4z M13 14.723V17h-2v-2.277A2 2 0 1 1 13 14.723z";

    // Clock icon
    const wchar_t* const CLOCK = L"M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2zm0 18c-4.4 0-8-3.6-8-8s3.6-8 8-8 8 3.6 8 8-3.6 8-8 8zm1-13h-2v7l6 3.5 1-1.6-5-3V7z";

    // Eye icon
    const wchar_t* const EYE = L"M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z";

    // Gear icon
    const wchar_t* const GEAR = L"M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z";

    // Warn icon
    const wchar_t* const WARN = L"M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z M12 9v4 M12 17h.01";
}

// SVG 渲染函数声明
Gdiplus::GraphicsPath* ParseSvgPath(const wchar_t* svgPathD);
HBITMAP RenderSvgIcon(const wchar_t* svgPathD, int width, int height, Gdiplus::Color fillColor, Gdiplus::Color bgColor = Gdiplus::Color(0, 0, 0, 0));
HICON BitmapToIcon(HBITMAP hbmp);
