#ifndef SYSTEMHOOKMANAGER_H
#define SYSTEMHOOKMANAGER_H

#include <QObject>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class HookThread : public QThread {
    Q_OBJECT
public:
#ifdef Q_OS_WIN
    DWORD winThreadId() const { return m_winThreadId; }
#endif

protected:
    void run() override;

signals:
    void keyPressed(int vkCode);
    void mouseTouched();

private:
#ifdef Q_OS_WIN
    DWORD m_winThreadId = 0;
#endif
};

class SystemHookManager : public QObject {
    Q_OBJECT
public:
    explicit SystemHookManager(QObject *parent = nullptr);
    static SystemHookManager& instance();

    void startHook();
    void stopHook();

signals:
    void dangerousKeyCombinationDetected();
    void mouseTouched();
    void escPressed();

private:
    HookThread *m_hookThread = nullptr;
};

#endif // SYSTEMHOOKMANAGER_H
