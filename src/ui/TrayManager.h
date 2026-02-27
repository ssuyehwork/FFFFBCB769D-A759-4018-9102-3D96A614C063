#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QSystemTrayIcon>
#include <QMenu>
#include <QObject>

class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(QObject *parent = nullptr);
    void setVisible(bool visible);
    void updateRemainingTime(int seconds);

signals:
    void exitRequested();
    void unlockRequested();

private:
    QSystemTrayIcon *m_trayIcon;
    QAction *m_timeAction;
};

#endif // TRAYMANAGER_H
