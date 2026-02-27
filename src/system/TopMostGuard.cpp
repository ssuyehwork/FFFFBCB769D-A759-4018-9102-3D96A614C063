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

void TopMostGuard::onGuardTick() {
#ifdef Q_OS_WIN
    for (QWidget* w : m_windows) {
        if (w && w->isVisible()) {
            HWND hwnd = reinterpret_cast<HWND>(w->winId());
            
            // 确保窗口在最顶层
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            
            // 如果不是前台窗口，且我们想要它是（主屏窗口），则抢回焦点
            // 注意：副屏窗口不需要抢焦点，否则会干扰输入
            if (w->property("isMainScreen").toBool()) {
                if (GetForegroundWindow() != hwnd) {
                    SetForegroundWindow(hwnd);
                }
            }
        }
    }

    // 对抗任务管理器：查找 taskmgr 窗口并压低或重新置顶我们的窗口
    HWND taskMgr = FindWindowW(L"TaskManagerWindow", NULL);
    if (taskMgr && IsWindowVisible(taskMgr)) {
         // 只要任务管理器可见，我们就持续对我们的窗口调用 SetWindowPos
         // 这里的循环已经处理了 SetWindowPos
    }
#endif
}
