#include "SystemHookManager.h"

#ifdef Q_OS_WIN
#include <windows.h>

HHOOK hKeyHook = NULL;
HHOOK hMouseHook = NULL;
HookThread* g_hookThread = nullptr;
std::atomic<bool> SystemHookManager::s_isBlocking{false};

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        // 始终监听 Esc 信号，用于唤起解锁对话框
        if (p->vkCode == VK_ESCAPE && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
             if (g_hookThread) emit g_hookThread->keyPressed(VK_ESCAPE);
        }

        // 如果当前处于非阻塞模式（如预警期或解锁对话框开启时），直接放行
        if (!SystemHookManager::s_isBlocking) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

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

        if (block) return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        // 始终统计触碰次数
        if (g_hookThread) {
            if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MOUSEMOVE) {
                emit g_hookThread->mouseTouched();
            }
        }

        // 如果处于非阻塞模式，放行鼠标点击
        if (!SystemHookManager::s_isBlocking) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || 
            wParam == WM_LBUTTONDBLCLK || wParam == WM_RBUTTONDBLCLK) {
            return 1; // 锁定模式下，彻底拦截点击
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
        s_isBlocking = false;
        PostThreadMessage(m_hookThread->winThreadId(), WM_QUIT, 0, 0);
        m_hookThread->wait(1000); // 增加超时保护，防止死锁
        if (m_hookThread->isRunning()) m_hookThread->terminate();
        delete m_hookThread;
        m_hookThread = nullptr;
    }
#endif
}

void SystemHookManager::setBlocking(bool blocking) {
    s_isBlocking = blocking;
}
