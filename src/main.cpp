#include <QApplication>
#include <QMessageBox>
#include <QScreen>
#include <QAbstractNativeEventFilter>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "core/ConfigManager.h"
#include "core/CountdownEngine.h"
#include "core/SessionLogger.h"
#include "system/TopMostGuard.h"
#include "system/SystemHookManager.h"
#include "system/PowerManager.h"
#include "ui/SetupDialog.h"
#include "ui/LockScreenWindow.h"
#include "ui/UnlockDialog.h"
#include "ui/TrayManager.h"

class AppController : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    AppController() {
#ifdef Q_OS_WIN
        if (!RegisterHotKey(NULL, 1, MOD_CONTROL | MOD_ALT, 'L')) {
            // 热键注册失败（可能被占用）
        }
        qApp->installNativeEventFilter(this);
#endif
        m_tray = new TrayManager(this);
        connect(m_tray, &TrayManager::exitRequested, this, &AppController::handleExit);
        connect(m_tray, &TrayManager::unlockRequested, this, &AppController::showUnlockDialog);

        connect(&CountdownEngine::instance(), &CountdownEngine::tickSecond, m_tray, &TrayManager::updateRemainingTime);
        connect(&CountdownEngine::instance(), &CountdownEngine::warningPhaseStarted, this, &AppController::activateLock);
        connect(&CountdownEngine::instance(), &CountdownEngine::lockActivated, this, &AppController::activateLock);
        
        connect(&SystemHookManager::instance(), &SystemHookManager::escPressed, this, &AppController::showUnlockDialog);
        connect(&SystemHookManager::instance(), &SystemHookManager::mouseTouched, this, &AppController::handleMouseTouch);
    }

    void start() {
        m_tray->setVisible(true);
        showSetup();
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG") {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_HOTKEY && msg->wParam == 1) {
                handleImmediateLock();
                return true;
            }
        }
#endif
        return false;
    }

private slots:
    void showSetup() {
        SetupDialog dlg;
        if (dlg.exec() == QDialog::Accepted) {
            int mins = ConfigManager::instance().getConfig().countdownMinutes;
            m_sessionStartTime = QDateTime::currentDateTime();
            m_touchCount = 0;
            CountdownEngine::instance().start(mins);
        } else {
            if (m_lockWindows.isEmpty()) qApp->quit();
        }
    }

    void activateLock() {
        if (m_lockWindows.isEmpty()) {
            for (QScreen* screen : QGuiApplication::screens()) {
                bool isMain = (screen == QGuiApplication::primaryScreen());
                LockScreenWindow* w = new LockScreenWindow(screen->geometry(), isMain);
                
                // 以不激活焦点的方式显示，避免打断用户当前操作
#ifdef Q_OS_WIN
                HWND hwnd = reinterpret_cast<HWND>(w->winId());
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
#else
                w->show();
#endif
                m_lockWindows.append(w);
                TopMostGuard::instance().addWindow(w);
            }
            TopMostGuard::instance().startGuard();
        }

        if (CountdownEngine::instance().state() == CountdownEngine::Locked) {
            if (ConfigManager::instance().getConfig().preventSleep) {
                PowerManager::preventSleepAndScreenOff();
            }
            SystemHookManager::instance().startHook();
            for (auto w : m_lockWindows) {
                w->setLockMode(true);
            }
        }
    }

    void showUnlockDialog() {
        if (m_lockWindows.isEmpty() || m_isUnlockDialogOpen) return;

        m_isUnlockDialogOpen = true;
        
        // 1. 暂停干扰源
        TopMostGuard::instance().setFocusStealingEnabled(false);
        for (auto w : m_lockWindows) w->setClockPaused(true);
        
        // 2. 暂停系统钩子，允许用户与 UnlockDialog 交互
        SystemHookManager::instance().stopHook();

        UnlockDialog dlg;
        connect(&dlg, &UnlockDialog::unlockSucceeded, this, &AppController::handleUnlock);
        
        // 如果用户关闭了对话框但未解锁（如点击了对话框外的取消，尽管我们设了置顶）
        if (dlg.exec() != QDialog::Accepted) {
            // 3. 恢复锁定状态
            SystemHookManager::instance().startHook();
            for (auto w : m_lockWindows) w->setClockPaused(false);
            TopMostGuard::instance().setFocusStealingEnabled(true);
            m_isUnlockDialogOpen = false;
        }
        // 如果 Accepted，handleUnlock 会处理后续清理并退出
    }

    void handleUnlock() {
        TopMostGuard::instance().stopGuard();
        TopMostGuard::instance().clearWindows();
        SystemHookManager::instance().stopHook();
        PowerManager::allowSleepAndScreenOff();

        for (auto w : m_lockWindows) w->deleteLater();
        m_lockWindows.clear();

        CountdownEngine::instance().stop();

        // 记录日志
        SessionRecord rec;
        rec.startTime = m_sessionStartTime;
        rec.endTime = QDateTime::currentDateTime();
        rec.touchCount = m_touchCount;
        rec.manualUnlock = true;
        SessionLogger::instance().logSession(rec);

        // 解锁后直接退出程序
        qApp->quit();
    }

    void handleMouseTouch() {
        m_touchCount++;
        for (auto w : m_lockWindows) {
            QMetaObject::invokeMethod(w, "showTouchWarning", Qt::QueuedConnection);
        }
    }

    void handleExit() {
#ifdef Q_OS_WIN
        UnregisterHotKey(NULL, 1);
#endif
        if (!m_lockWindows.isEmpty()) {
            showUnlockDialog();
        } else {
            qApp->quit();
        }
    }

    void handleImmediateLock() {
        if (!m_lockWindows.isEmpty()) return; // 已经在锁屏中
        
        m_sessionStartTime = QDateTime::currentDateTime();
        m_touchCount = 0;
        
        // 强制引擎进入锁定状态
        CountdownEngine::instance().forceLock();
    }

private:
    TrayManager *m_tray;
    QList<LockScreenWindow*> m_lockWindows;
    QDateTime m_sessionStartTime;
    int m_touchCount = 0;
    bool m_isUnlockDialogOpen = false;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    AppController controller;
    controller.start();

    return a.exec();
}

#include "main.moc"
