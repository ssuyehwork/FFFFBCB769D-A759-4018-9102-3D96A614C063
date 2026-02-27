#include "svg_icons.h"
#include <vector>
#include <cwchar>
#include <cmath>
#include <algorithm>

Gdiplus::GraphicsPath* ParseSvgPath(const wchar_t* svgPathD) {
    Gdiplus::GraphicsPath* path = new Gdiplus::GraphicsPath();
    const wchar_t* p = svgPathD;
    float lastX = 0, lastY = 0;
    wchar_t command = L'\0';

    while (*p) {
        // 跳过空格和逗号
        while (*p && (*p == L' ' || *p == L',')) p++;
        if (!*p) break;

        // 检查是否是命令
        if (iswalpha(*p)) {
            command = *p++;
        }

        auto readFloat = [&p]() -> float {
            while (*p && (*p == L' ' || *p == L',')) p++;
            wchar_t* end;
            float val = std::wcstof(p, &end);
            p = end;
            return val;
        };

        if (command == L'M' || command == L'm') {
            float x = readFloat();
            float y = readFloat();
            if (command == L'm') { x += lastX; y += lastY; }
            path->StartFigure();
            lastX = x; lastY = y;
            command = (command == L'M') ? L'L' : L'l'; // M 后的坐标视为 L
        } else if (command == L'L' || command == L'l') {
            float x = readFloat();
            float y = readFloat();
            if (command == L'l') { x += lastX; y += lastY; }
            path->AddLine(lastX, lastY, x, y);
            lastX = x; lastY = y;
        } else if (command == L'H' || command == L'h') {
            float x = readFloat();
            if (command == L'h') x += lastX;
            path->AddLine(lastX, lastY, x, lastY);
            lastX = x;
        } else if (command == L'V' || command == L'v') {
            float y = readFloat();
            if (command == L'v') y += lastY;
            path->AddLine(lastX, lastY, lastX, y);
            lastY = y;
        } else if (command == L'C' || command == L'c') {
            float x1 = readFloat(); float y1 = readFloat();
            float x2 = readFloat(); float y2 = readFloat();
            float x = readFloat();  float y = readFloat();
            if (command == L'c') {
                x1 += lastX; y1 += lastY;
                x2 += lastX; y2 += lastY;
                x += lastX;  y += lastY;
            }
            path->AddBezier(lastX, lastY, x1, y1, x2, y2, x, y);
            lastX = x; lastY = y;
        } else if (command == L'Z' || command == L'z') {
            path->CloseFigure();
            command = L'\0';
        } else {
            // 简单处理：跳过未知命令的参数
            readFloat();
        }
    }
    return path;
}

HBITMAP RenderSvgIcon(const wchar_t* svgPathD, int width, int height, Gdiplus::Color fillColor, Gdiplus::Color bgColor) {
    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.Clear(bgColor);

    Gdiplus::GraphicsPath* path = ParseSvgPath(svgPathD);

    // 自动缩放路径以适应容器
    Gdiplus::RectF bounds;
    path->GetBounds(&bounds);

    if (bounds.Width > 0 && bounds.Height > 0) {
        float scale = std::min((float)width / bounds.Width, (float)height / bounds.Height) * 0.8f;
        Gdiplus::Matrix matrix;
        matrix.Scale(scale, scale);
        matrix.Translate(-bounds.X + (width / scale - bounds.Width) / 2.0f,
                         -bounds.Y + (height / scale - bounds.Height) / 2.0f, Gdiplus::MatrixOrderAppend);
        path->Transform(&matrix);
    }

    Gdiplus::SolidBrush brush(fillColor);
    graphics.FillPath(&brush, path);

    delete path;

    HBITMAP hBitmap;
    bitmap.GetHBITMAP(Gdiplus::Color(0,0,0,0), &hBitmap);
    return hBitmap;
}

HICON BitmapToIcon(HBITMAP hbmp) {
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmp;
    ii.hbmColor = hbmp;
    return CreateIconIndirect(&ii);
}
