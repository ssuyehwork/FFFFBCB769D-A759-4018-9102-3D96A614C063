#include "UnlockDialog.h"
#include "../core/ConfigManager.h"
#include "../core/CountdownEngine.h"
#include "../utils/SvgIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QLabel>
#include <QIcon>

UnlockDialog::UnlockDialog(QWidget *parent) : QDialog(parent) {
    // 明确设置置顶标志，并确保它是模态对话框
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowSystemMenuHint);
    setWindowModality(Qt::ApplicationModal);
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
        CountdownEngine::instance().reportUnlockAttempt(true);
        emit unlockSucceeded();
        accept();
    } else {
        CountdownEngine::instance().reportUnlockAttempt(false);
        m_attempts++;
        m_passwordEdit->clear();
        
        // 抖动动画 (针对输入框)
        QPropertyAnimation *animation = new QPropertyAnimation(m_passwordEdit, "pos");
        animation->setDuration(300);
        QPoint origPos = m_passwordEdit->pos();
        animation->setStartValue(origPos);
        animation->setKeyValueAt(0.125, origPos + QPoint(8, 0));
        animation->setKeyValueAt(0.25, origPos + QPoint(-8, 0));
        animation->setKeyValueAt(0.375, origPos + QPoint(6, 0));
        animation->setKeyValueAt(0.5, origPos + QPoint(-6, 0));
        animation->setKeyValueAt(0.625, origPos + QPoint(4, 0));
        animation->setKeyValueAt(0.75, origPos + QPoint(-4, 0));
        animation->setEndValue(origPos);
        animation->start(QAbstractAnimation::DeleteWhenStopped);

        if (m_attempts >= config.maxPasswordAttempts) {
            m_lockoutTime = config.lockoutDurationSecs;
            m_unlockBtn->setEnabled(false);
            m_passwordEdit->setEnabled(false);
            m_lockoutTimer->start(1000);
            CountdownEngine::instance().triggerLockedOut(m_lockoutTime);
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
