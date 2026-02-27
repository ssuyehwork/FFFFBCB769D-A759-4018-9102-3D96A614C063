#ifndef SYSTEMHOOKMANAGER_H
#define SYSTEMHOOKMANAGER_H

#include <QObject>
#include <QThread>

class HookThread : public QThread {
    Q_OBJECT
protected:
    void run() override;
signals:
    void keyPressed(int vkCode);
    void mouseTouched();
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
