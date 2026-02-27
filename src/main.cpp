#include <QApplication>
#include <QMessageBox>
#include <QScreen>
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

class AppController : public QObject {
    Q_OBJECT
public:
    AppController() {
        m_tray = new TrayManager(this);
        connect(m_tray, &TrayManager::exitRequested, this, &AppController::handleExit);
        connect(m_tray, &TrayManager::unlockRequested, this, &AppController::showUnlockDialog);

        connect(&CountdownEngine::instance(), &CountdownEngine::tickSecond, m_tray, &TrayManager::updateRemainingTime);
        connect(&CountdownEngine::instance(), &CountdownEngine::lockActivated, this, &AppController::activateLock);

        connect(&SystemHookManager::instance(), &SystemHookManager::escPressed, this, &AppController::showUnlockDialog);
        connect(&SystemHookManager::instance(), &SystemHookManager::mouseTouched, this, &AppController::handleMouseTouch);
    }

    void start() {
        m_tray->setVisible(true);
        showSetup();
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
        if (ConfigManager::instance().getConfig().preventSleep) {
            PowerManager::preventSleepAndScreenOff();
        }

        SystemHookManager::instance().startHook();

        for (QScreen* screen : QGuiApplication::screens()) {
            bool isMain = (screen == QGuiApplication::primaryScreen());
            LockScreenWindow* w = new LockScreenWindow(screen->geometry(), isMain);
            w->show();
            m_lockWindows.append(w);
            TopMostGuard::instance().addWindow(w);
        }

        TopMostGuard::instance().startGuard();
    }

    void showUnlockDialog() {
        if (m_lockWindows.isEmpty()) return;

        UnlockDialog dlg;
        connect(&dlg, &UnlockDialog::unlockSucceeded, this, &AppController::handleUnlock);
        dlg.exec();
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

        if (ConfigManager::instance().getConfig().autoRestartAfterUnlock) {
            showSetup();
        }
    }

    void handleMouseTouch() {
        m_touchCount++;
        for (auto w : m_lockWindows) {
            QMetaObject::invokeMethod(w, "showTouchWarning", Qt::QueuedConnection);
        }
    }

    void handleExit() {
        if (!m_lockWindows.isEmpty()) {
            showUnlockDialog();
        } else {
            qApp->quit();
        }
    }

private:
    TrayManager *m_tray;
    QList<LockScreenWindow*> m_lockWindows;
    QDateTime m_sessionStartTime;
    int m_touchCount = 0;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    AppController controller;
    controller.start();

    return a.exec();
}

#include "main.moc"
