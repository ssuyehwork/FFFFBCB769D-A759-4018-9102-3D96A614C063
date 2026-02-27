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
#include <QToolTip>

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
        connect(&CountdownEngine::instance(), &CountdownEngine::tickSecond, this, &AppController::handleTick);
        connect(&CountdownEngine::instance(), &CountdownEngine::warningPhaseStarted, this, &AppController::handleWarning);
        connect(&CountdownEngine::instance(), &CountdownEngine::lockActivated, this, &AppController::activateLock);
        connect(&CountdownEngine::instance(), &CountdownEngine::finished, this, &AppController::handleUnlock);
        
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

    void handleTick(int remaining) {
        Q_UNUSED(remaining);
        // 更新所有窗口重绘
        for (auto w : m_lockWindows) w->update();
    }

    void handleWarning() {
        // 预警期：创建遮罩窗口，但此时是全透明且穿透点击的，仅用于显示巨大倒计时
        activateLock();
    }

    void activateLock() {
        if (m_lockWindows.isEmpty()) {
            for (QScreen* screen : QGuiApplication::screens()) {
                bool isMain = (screen == QGuiApplication::primaryScreen());
                LockScreenWindow* w = new LockScreenWindow(screen->geometry(), isMain);
                
                // 真正的锁定：显示并置顶
                w->show();
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

        // 改进：将解锁对话框移动到当前鼠标所在的屏幕中心
        QScreen *currentScreen = QGuiApplication::screenAt(QCursor::pos());
        if (!currentScreen) currentScreen = QGuiApplication::primaryScreen();
        QRect screenGeometry = currentScreen->geometry();
        dlg.move(screenGeometry.center() - dlg.rect().center());

        connect(&dlg, &UnlockDialog::unlockSucceeded, this, &AppController::handleUnlock);
        
        // 如果用户关闭了对话框但未解锁（如点击了对话框外的取消，尽管我们设了置顶）
        if (dlg.exec() != QDialog::Accepted) {
            // 3. 恢复锁定状态
            SystemHookManager::instance().startHook();
            for (auto w : m_lockWindows) w->setClockPaused(false);
            TopMostGuard::instance().setFocusStealingEnabled(true);
            m_isUnlockDialogOpen = false;
        }
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

        // 修复：尊重 "解锁后自动重启" 配置
        if (ConfigManager::instance().getConfig().autoRestartAfterUnlock) {
            m_isUnlockDialogOpen = false;
            showSetup();
        } else {
            qApp->quit();
        }
    }

    void handleMouseTouch() {
        m_touchCount++;

        // 性能优化：对 UI 反馈进行节流处理，限制每 200ms 最多触发一次重绘
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastTouchFeedbackTime < TOUCH_FEEDBACK_THRESHOLD_MS) {
            return;
        }
        m_lastTouchFeedbackTime = currentTime;

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
    static constexpr int TOUCH_FEEDBACK_THRESHOLD_MS = 200;
    TrayManager *m_tray;
    QList<LockScreenWindow*> m_lockWindows;
    QDateTime m_sessionStartTime;
    qint64 m_lastTouchFeedbackTime = 0;
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
