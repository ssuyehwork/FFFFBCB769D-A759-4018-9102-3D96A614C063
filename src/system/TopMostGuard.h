#ifndef TOPMOSTGUARD_H
#define TOPMOSTGUARD_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QWidget>

class TopMostGuard : public QObject {
    Q_OBJECT
public:
    explicit TopMostGuard(QObject *parent = nullptr);
    static TopMostGuard& instance();

    void startGuard();
    void stopGuard();
    void addWindow(QWidget* window);
    void clearWindows();

private slots:
    void onGuardTick();

private:
    QTimer *m_timer;
    QList<QWidget*> m_windows;
};

#endif // TOPMOSTGUARD_H
