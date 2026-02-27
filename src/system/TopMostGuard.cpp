#include "TopMostGuard.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TopMostGuard::TopMostGuard(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(50);
    connect(m_timer, &QTimer::timeout, this, &TopMostGuard::onGuardTick);
}

TopMostGuard& TopMostGuard::instance() {
    static TopMostGuard inst;
    return inst;
}

void TopMostGuard::startGuard() {
    m_timer->start();
}

void TopMostGuard::stopGuard() {
    m_timer->stop();
}

void TopMostGuard::addWindow(QWidget* window) {
    if (window && !m_windows.contains(window)) {
        m_windows.append(window);
    }
}

void TopMostGuard::clearWindows() {
    m_windows.clear();
}

void TopMostGuard::setFocusStealingEnabled(bool enabled) {
    m_focusStealingEnabled = enabled;
}

void TopMostGuard::onGuardTick() {
#ifdef Q_OS_WIN
    // 如果禁用了焦点抢夺（通常是因为弹出了解锁对话框），
    // 我们也应该停止对遮罩窗口的置顶调用，避免它们盖过对话框
    if (!m_focusStealingEnabled) return;

    for (QWidget* w : m_windows) {
        if (w && w->isVisible()) {
            HWND hwnd = reinterpret_cast<HWND>(w->winId());
            
            // 确保窗口在最顶层
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            
            // 仅在正式锁定状态下执行抢焦点逻辑，预警期不干扰用户操作
            bool isLocked = w->property("isLocked").toBool();
            if (isLocked && w->property("isMainScreen").toBool()) {
                if (GetForegroundWindow() != hwnd) {
                    SetForegroundWindow(hwnd);
                }
            }
        }
    }

    // 对抗任务管理器：发现 taskmgr.exe 窗口存在且位于前台，立即抢回焦点
    HWND fgWnd = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fgWnd, &pid);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        wchar_t exePath[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
            QString path = QString::fromWCharArray(exePath).toLower();
            if (path.endsWith("taskmgr.exe")) {
                for (QWidget* w : m_windows) {
                    if (w->property("isMainScreen").toBool()) {
                        SetForegroundWindow(reinterpret_cast<HWND>(w->winId()));
                        break;
                    }
                }
            }
        }
        CloseHandle(hProc);
    }
#endif
}
