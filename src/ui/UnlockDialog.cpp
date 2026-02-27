#include "UnlockDialog.h"
#include "../core/ConfigManager.h"
#include "../utils/SvgIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>

UnlockDialog::UnlockDialog(QWidget *parent) : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFixedSize(300, 200);
    setStyleSheet("background-color: rgba(30, 30, 30, 230); color: white; border-radius: 10px;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(SvgIcon::get(SvgIcon::Unlock, QSize(48, 48), Qt::white));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    QHBoxLayout *passLayout = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("输入解锁密码");
    m_passwordEdit->setStyleSheet("padding: 8px; border: 1px solid #555; border-right: none; border-top-left-radius: 4px; border-bottom-left-radius: 4px; background: #222;");

    m_togglePassBtn = new QPushButton(this);
    m_togglePassBtn->setFixedSize(34, 34);
    m_togglePassBtn->setIcon(QIcon(SvgIcon::get(SvgIcon::Eye, QSize(20, 20), Qt::gray)));
    m_togglePassBtn->setStyleSheet("background: #222; border: 1px solid #555; border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px; margin-left: 0px;");
    connect(m_togglePassBtn, &QPushButton::clicked, this, &UnlockDialog::togglePasswordVisible);

    passLayout->addWidget(m_passwordEdit);
    passLayout->addWidget(m_togglePassBtn);
    passLayout->setSpacing(0);
    layout->addLayout(passLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #ff5555; font-size: 12px;");
    layout->addWidget(m_statusLabel);

    m_unlockBtn = new QPushButton("解锁", this);
    m_unlockBtn->setStyleSheet("padding: 8px; background: #0078d7; border: none; border-radius: 4px;");
    connect(m_unlockBtn, &QPushButton::clicked, this, &UnlockDialog::attemptUnlock);
    layout->addWidget(m_unlockBtn);

    m_lockoutTimer = new QTimer(this);
    connect(m_lockoutTimer, &QTimer::timeout, this, &UnlockDialog::updateLockout);

    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &UnlockDialog::attemptUnlock);
}

void UnlockDialog::attemptUnlock() {
    if (m_lockoutTime > 0) return;

    QString raw = m_passwordEdit->text();
    AppConfig config = ConfigManager::instance().getConfig();

    if (ConfigManager::verifyPassword(raw, config.passwordHash)) {
        emit unlockSucceeded();
        accept();
    } else {
        m_attempts++;
        m_passwordEdit->clear();
        
        // 抖动动画
        QPropertyAnimation *animation = new QPropertyAnimation(this, "pos");
        animation->setDuration(500);
        animation->setStartValue(pos());
        animation->setKeyValueAt(0.2, pos() + QPoint(10, 0));
        animation->setKeyValueAt(0.4, pos() + QPoint(-10, 0));
        animation->setKeyValueAt(0.6, pos() + QPoint(10, 0));
        animation->setKeyValueAt(0.8, pos() + QPoint(-10, 0));
        animation->setEndValue(pos());
        animation->start(QAbstractAnimation::DeleteWhenStopped);

        if (m_attempts >= config.maxPasswordAttempts) {
            m_lockoutTime = config.lockoutDurationSecs;
            m_unlockBtn->setEnabled(false);
            m_passwordEdit->setEnabled(false);
            m_lockoutTimer->start(1000);
            updateLockout();
        } else {
            m_statusLabel->setText(QString("密码错误，还剩 %1 次机会").arg(config.maxPasswordAttempts - m_attempts));
        }
    }
}

void UnlockDialog::togglePasswordVisible() {
    if (m_passwordEdit->echoMode() == QLineEdit::Password) {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePassBtn->setIcon(QIcon(SvgIcon::get(SvgIcon::EyeOff, QSize(20, 20), Qt::gray)));
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePassBtn->setIcon(QIcon(SvgIcon::get(SvgIcon::Eye, QSize(20, 20), Qt::gray)));
    }
}

void UnlockDialog::updateLockout() {
    if (m_lockoutTime > 0) {
        m_statusLabel->setText(QString("尝试次数过多，请等待 %1 秒").arg(m_lockoutTime));
        m_lockoutTime--;
    } else {
        m_lockoutTimer->stop();
        m_unlockBtn->setEnabled(true);
        m_passwordEdit->setEnabled(true);
        m_statusLabel->clear();
        m_attempts = 0;
    }
}
