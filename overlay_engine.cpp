#include "overlay_engine.h"
#include <thread>
#include <atomic>

namespace OverlayEngine {

HHOOK g_hKeyboardHook = NULL;
HHOOK g_hMouseHook = NULL;
std::atomic<bool> g_stopThread(false);
std::thread g_topmostThread;
HWND g_targetHwnd = NULL;
bool g_isLocked = false;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        bool fail = false;

        // 拦截 Win 键
        if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) fail = true;
        // 拦截 Alt+Tab
        if (p->vkCode == VK_TAB && (p->flags & LLKHF_ALTDOWN)) fail = true;
        // 拦截 Alt+F4
        if (p->vkCode == VK_F4 && (p->flags & LLKHF_ALTDOWN)) fail = true;
        // 拦截 Ctrl+Esc
        if (p->vkCode == VK_ESCAPE && (GetKeyState(VK_CONTROL) & 0x8000)) fail = true;
        // 拦截 Ctrl+Shift+Esc
        if (p->vkCode == VK_ESCAPE && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) fail = true;

        // 遮罩期间拦截绝大多数键，Esc 除外（用于解锁）
        if (p->vkCode != VK_ESCAPE) {
            fail = true;
        } else {
            // Esc 被按下，发送消息显示解锁窗口
            if (wParam == WM_KEYDOWN) {
                PostMessage(g_targetHwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            }
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        if (fail) return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        // 除非显式允许，否则拦截所有鼠标操作
        // 注意：这里需要根据解锁窗口是否可见来动态决定是否拦截
        return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void InstallHooks() {
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    // g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    g_isLocked = true;
}

void UninstallHooks() {
    if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = NULL; }
    if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = NULL; }
    g_isLocked = false;
}

void StartTopmostThread(HWND hwnd) {
    g_targetHwnd = hwnd;
    g_stopThread = false;
    g_topmostThread = std::thread([]() {
        while (!g_stopThread) {
            if (g_targetHwnd) {
                SetWindowPos(g_targetHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
}

void StopTopmostThread() {
    g_stopThread = true;
    if (g_topmostThread.joinable()) g_topmostThread.join();
}

bool IsLocked() { return g_isLocked; }

}
