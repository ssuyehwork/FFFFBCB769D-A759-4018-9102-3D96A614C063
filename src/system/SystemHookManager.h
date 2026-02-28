#ifndef SYSTEMHOOKMANAGER_H
#define SYSTEMHOOKMANAGER_H

#include <QObject>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <atomic>
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
    void setBlocking(bool blocking);

signals:
    void dangerousKeyCombinationDetected();
    void taskManagerHotKeyDetected(); // 新增：检测到任务管理器快捷键
    void mouseTouched();
    void escPressed();

public slots:
    // 内部转发槽
    void handleKeyPressed(int vk);
    void handleMouseTouched();

private:
    HookThread *m_hookThread = nullptr;
    static std::atomic<bool> s_isBlocking;
    friend LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif // SYSTEMHOOKMANAGER_H
