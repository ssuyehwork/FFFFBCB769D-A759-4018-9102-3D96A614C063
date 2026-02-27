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
#include "ui/TrayManager.h"
#include "ui/PreLockNotification.h"
#include <QToolTip>

class AppController : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    AppController() {
#ifdef Q_OS_WIN
        // 创建辅助窗口用于稳定接收热键消息
        m_hotkeyWindow = new QWidget();
        if (!RegisterHotKey(reinterpret_cast<HWND>(m_hotkeyWindow->winId()), 1, MOD_CONTROL | MOD_ALT, 'L')) {
            // 热键注册失败
        }
        qApp->installNativeEventFilter(this);
#endif
        m_tray = new TrayManager(this);
        connect(m_tray, &TrayManager::exitRequested, this, &AppController::handleExit);
        connect(m_tray, &TrayManager::unlockRequested, this, &AppController::handleImmediateLock); // 托盘紧急解锁改为直接触发(如果未锁)或由遮罩处理

        connect(&CountdownEngine::instance(), &CountdownEngine::tickSecond, m_tray, &TrayManager::updateRemainingTime);
        connect(&CountdownEngine::instance(), &CountdownEngine::tickSecond, this, &AppController::handleTick);
        connect(&CountdownEngine::instance(), &CountdownEngine::warningPhaseStarted, this, &AppController::handleWarning);
        connect(&CountdownEngine::instance(), &CountdownEngine::warningTick, this, &AppController::handleWarningTick);
        connect(&CountdownEngine::instance(), &CountdownEngine::lockActivated, this, &AppController::activateLock);
        connect(&CountdownEngine::instance(), &CountdownEngine::unlockSucceeded, this, &AppController::handleUnlock);
        // 彻底移除5分钟逻辑，所以不再需要监听 finished (倒计时自然结束) 信号
        
        // 移除 Esc 触发对话框的全局连接，改为遮罩内部处理
        connect(&SystemHookManager::instance(), &SystemHookManager::mouseTouched, this, &AppController::handleMouseTouch);
    }

    void start() {
        m_tray->setVisible(true);
        SystemHookManager::instance().startHook(); // 启动常驻钩子，默认不阻塞
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
        if (!m_preLockNotify) {
            m_preLockNotify = new PreLockNotification();
            m_preLockNotify->show();
        }
    }

    void handleWarningTick(int remaining) {
        if (m_preLockNotify) {
            m_preLockNotify->updateRemaining(remaining);
            if (remaining == 0) {
                m_preLockNotify->startFadeOut();
                m_preLockNotify = nullptr;
            }
        }
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
            // 进入锁定：切换钩子为阻塞模式
            // 注意：现在解锁框在遮罩上，钩子需要允许基本输入，
            // 实际上 s_isBlocking 应该拦截系统快捷键，但允许字符输入
            SystemHookManager::instance().setBlocking(true);
            for (auto w : m_lockWindows) {
                w->setLockMode(true);
            }
        }
    }

    void showUnlockDialog() {
        // 解锁界面现在已整合到遮罩窗口中
    }

    void handleUnlock() {
        // 1. 立即解除系统级阻塞和置顶
        SystemHookManager::instance().setBlocking(false);
        TopMostGuard::instance().stopGuard();
        TopMostGuard::instance().clearWindows();
        PowerManager::allowSleepAndScreenOff();

        // 2. 物理隐藏所有遮罩窗口，防止 deleteLater 延迟
        for (auto w : m_lockWindows) {
            w->hide();
            w->deleteLater();
        }
        m_lockWindows.clear();

        CountdownEngine::instance().stop();

        // 记录日志
        SessionRecord rec;
        rec.startTime = m_sessionStartTime;
        rec.endTime = QDateTime::currentDateTime();
        rec.touchCount = m_touchCount;
        rec.manualUnlock = true;
        SessionLogger::instance().logSession(rec);

        // 解锁后立即回到设置界面
        m_isUnlockDialogOpen = false;
        showSetup();
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
        if (m_hotkeyWindow) {
            UnregisterHotKey(reinterpret_cast<HWND>(m_hotkeyWindow->winId()), 1);
        }
#endif
        if (!m_lockWindows.isEmpty()) {
            // 锁定中，不响应退出
        } else {
            qApp->quit();
        }
    }

    void handleImmediateLock() {
        if (!m_lockWindows.isEmpty()) return; // 已经在锁屏中
        
        m_sessionStartTime = QDateTime::currentDateTime();
        m_touchCount = 0;
        
        // 强制进入锁定前先激活钩子阻塞，确保 Win 键立即被拦
        SystemHookManager::instance().setBlocking(true);
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
    PreLockNotification *m_preLockNotify = nullptr;
    QWidget *m_hotkeyWindow = nullptr;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    AppController controller;
    controller.start();

    return a.exec();
}

#include "main.moc"
