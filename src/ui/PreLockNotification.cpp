#include "PreLockNotification.h"
#include "../utils/SvgIcon.h"
#include "../core/ConfigManager.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QPainterPath>

PreLockNotification::PreLockNotification(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    m_remaining = ConfigManager::instance().getConfig().warningDurationSeconds;
    setFixedSize(360, 140);

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

    // 1. 绘制主体圆角矩形背景 (使用微透明深灰色，符合图片风格)
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    painter.fillPath(path, QColor(25, 25, 25, 230));

    // 2. 移除多余的顶部标题条背景，直接在左上角绘制图标和文字
    QPixmap lock = SvgIcon::get(SvgIcon::Lock, QSize(18, 18), QColor("#4CAF50"));
    painter.drawPixmap(15, 15, lock);

    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setPixelSize(14);
    titleFont.setBold(false);
    painter.setFont(titleFont);
    painter.drawText(40, 29, "即将锁屏");

    // 3. 绘制核心倒计时文字 "还剩 XX 秒"
    QFont font = painter.font();
    font.setPixelSize(42);
    font.setBold(true);
    painter.setFont(font);

    QColor accentColor = (m_remaining <= 10) ? QColor(255, 69, 58) : QColor(255, 153, 0); // 橙黄色调
    painter.setPen(accentColor);

    QString text = QString("还剩 %1 秒").arg(m_remaining);
    // 居中绘制文字
    painter.drawText(rect().adjusted(0, 15, 0, 0), Qt::AlignCenter, text);

    // 4. 绘制底部进度条
    int totalWarning = ConfigManager::instance().getConfig().warningDurationSeconds;
    int progressWidth = (width() - 40) * m_remaining / (totalWarning > 0 ? totalWarning : 1);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 60, 60, 150));
    painter.drawRoundedRect(20, height() - 25, width() - 40, 8, 4, 4);

    painter.setBrush(accentColor);
    painter.drawRoundedRect(20, height() - 25, progressWidth, 8, 4, 4);
}
