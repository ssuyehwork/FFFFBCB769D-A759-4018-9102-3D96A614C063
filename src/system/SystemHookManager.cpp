#include "SystemHookManager.h"

#ifdef Q_OS_WIN
#include <windows.h>

HHOOK hKeyHook = NULL;
HHOOK hMouseHook = NULL;
HookThread* g_hookThread = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
        bool block = false;

        // 拦截 Win 键
        if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) block = true;
        
        // 拦截 Alt+Tab, Alt+F4, Alt+Esc
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {
            if (p->vkCode == VK_TAB || p->vkCode == VK_F4 || p->vkCode == VK_ESCAPE) block = true;
        }

        // 拦截 Ctrl+Esc
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
            if (p->vkCode == VK_ESCAPE) block = true;
        }
        
        // 拦截 PrintScreen
        if (p->vkCode == VK_SNAPSHOT) block = true;

        if (p->vkCode == VK_ESCAPE && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
             if (g_hookThread) emit g_hookThread->keyPressed(VK_ESCAPE);
        }

        if (block) return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN ||
            wParam == WM_LBUTTONDBLCLK || wParam == WM_RBUTTONDBLCLK) {
            if (g_hookThread) emit g_hookThread->mouseTouched();
            return 1; // 彻底拦截点击事件
        }
        if (wParam == WM_MOUSEMOVE) {
            if (g_hookThread) emit g_hookThread->mouseTouched();
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void HookThread::run() {
    g_hookThread = this;
    m_winThreadId = GetCurrentThreadId();
    hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hKeyHook) UnhookWindowsHookEx(hKeyHook);
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    hKeyHook = NULL;
    hMouseHook = NULL;
    g_hookThread = nullptr;
}
#endif

SystemHookManager::SystemHookManager(QObject *parent) : QObject(parent) {}

SystemHookManager& SystemHookManager::instance() {
    static SystemHookManager inst;
    return inst;
}

void SystemHookManager::startHook() {
#ifdef Q_OS_WIN
    if (!m_hookThread) {
        m_hookThread = new HookThread();
        connect(m_hookThread, &HookThread::keyPressed, this, [this](int vk){
            if (vk == VK_ESCAPE) emit escPressed();
        });
        connect(m_hookThread, &HookThread::mouseTouched, this, &SystemHookManager::mouseTouched);
        m_hookThread->start();
    }
#endif
}

void SystemHookManager::stopHook() {
#ifdef Q_OS_WIN
    if (m_hookThread) {
        PostThreadMessage(m_hookThread->winThreadId(), WM_QUIT, 0, 0);
        m_hookThread->wait();
        delete m_hookThread;
        m_hookThread = nullptr;
    }
#endif
}
