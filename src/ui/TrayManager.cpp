#include "TrayManager.h"
#include "../utils/SvgIcon.h"
#include <QIcon>
#include <QCoreApplication>

TrayManager::TrayManager(QObject *parent) : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(SvgIcon::get(SvgIcon::Lock, QSize(16, 16), Qt::white)));

    QMenu *menu = new QMenu();

    m_timeAction = new QAction("剩余时间：--", menu);
    m_timeAction->setEnabled(false);
    menu->addAction(m_timeAction);

    menu->addSeparator();

    QAction *unlockAction = new QAction("紧急解锁...", menu);
    connect(unlockAction, &QAction::triggered, this, &TrayManager::unlockRequested);
    menu->addAction(unlockAction);

    menu->addSeparator();

    QAction *exitAction = new QAction("退出", menu);
    connect(exitAction, &QAction::triggered, this, &TrayManager::exitRequested);
    menu->addAction(exitAction);

    m_trayIcon->setContextMenu(menu);
}

void TrayManager::setVisible(bool visible) {
    m_trayIcon->setVisible(visible);
}

void TrayManager::updateRemainingTime(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    m_timeAction->setText(QString("剩余时间：%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
}
