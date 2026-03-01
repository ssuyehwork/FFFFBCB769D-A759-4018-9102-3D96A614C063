#ifndef TOPMOSTGUARD_H
#define TOPMOSTGUARD_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QWidget>

class TopMostGuard : public QObject {
    Q_OBJECT
signals:
    void taskManagerDetected(); // 方案 A：检测到任务管理器时发出信号

public:
    explicit TopMostGuard(QObject *parent = nullptr);
    static TopMostGuard& instance();

    void startGuard();
    void stopGuard();
    void addWindow(QWidget* window);
    void clearWindows();
    void setFocusStealingEnabled(bool enabled);

private slots:
    void onGuardTick();

private:
    QTimer *m_timer;
    QList<QWidget*> m_windows;
    bool m_focusStealingEnabled = true;
};

#endif // TOPMOSTGUARD_H
