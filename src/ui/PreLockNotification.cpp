#include "PreLockNotification.h"
#include "../utils/SvgIcon.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QPainterPath>

PreLockNotification::PreLockNotification(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    m_remaining = 60; // 同步修改初始剩余时间
    setFixedSize(340, 130);

    QScreen *primary = QGuiApplication::primaryScreen();
    QRect screen = primary->geometry();
    int targetX = screen.center().x() - width() / 2;
    int targetY = screen.height() * 0.25 - height() / 2;

    move(targetX, -height());

    QPropertyAnimation *anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(250);
    anim->setStartValue(QPoint(targetX, -height()));
    anim->setEndValue(QPoint(targetX, targetY));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PreLockNotification::updateRemaining(int seconds) {
    m_remaining = seconds;
    update();
}

void PreLockNotification::startFadeOut() {
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(150);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, &PreLockNotification::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PreLockNotification::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    painter.fillPath(path, QColor(30, 30, 30, 200));

    painter.setClipPath(path);
    painter.fillRect(0, 0, width(), 30, QColor(0, 0, 0, 100));

    QPixmap lock = SvgIcon::get(SvgIcon::Lock, QSize(16, 16), Qt::white);
    painter.drawPixmap(10, 7, lock);
    painter.setPen(Qt::white);
    painter.drawText(35, 20, "即将锁屏");

    QFont font = painter.font();
    font.setPixelSize(48);
    font.setBold(true);
    painter.setFont(font);

    QColor textColor = (m_remaining <= 10) ? QColor(255, 69, 58) : QColor(255, 159, 10);
    painter.setPen(textColor);

    QString text = QString("还剩  %1  秒").arg(m_remaining);
    painter.drawText(rect().adjusted(0, 30, 0, -20), Qt::AlignCenter, text);

    int progressWidth = (width() - 40) * m_remaining / 60;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 60, 60));
    painter.drawRoundedRect(20, 100, width() - 40, 8, 4, 4);
    painter.setBrush(textColor);
    painter.drawRoundedRect(20, 100, progressWidth, 8, 4, 4);
}
