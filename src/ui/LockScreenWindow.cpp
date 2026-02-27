#include "LockScreenWindow.h"
#include "../utils/SvgIcon.h"
#include "../core/CountdownEngine.h"
#include "../core/ConfigManager.h"
#include "../system/SystemHookManager.h"
#include <QPainter>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL (WINAPI *pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
#endif

LockScreenWindow::LockScreenWindow(const QRect& geometry, bool isMain, QWidget *parent)
    : QWidget(parent), m_isMain(isMain) 
{
    setGeometry(geometry);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // 确保窗口能穿透点击（在预警期）
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);
#endif
    setProperty("isMainScreen", isMain);

    m_warningTimer = new QTimer(this);
    connect(m_warningTimer, &QTimer::timeout, this, &LockScreenWindow::fadeOutWarning);

    // 修复：增加内部秒级定时器，确保锁定状态下右下角时钟每秒刷新
    QTimer* clockRefreshTimer = new QTimer(this);
    connect(clockRefreshTimer, &QTimer::timeout, this, qOverload<>(&LockScreenWindow::update));
    clockRefreshTimer->start(1000);

    if (m_isMain) {
        setupUnlockUi();
    }
}

void LockScreenWindow::setupUnlockUi() {
    m_unlockWidget = new QWidget(this);
    m_unlockWidget->setFixedSize(300, 150);
    m_unlockWidget->setStyleSheet("background: transparent;");

    QVBoxLayout *layout = new QVBoxLayout(m_unlockWidget);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("输入解锁密码");
    // 优化：使用更平滑的边框渲染，消除圆角抗锯齿瑕疵，增强深色模式视觉效果
    m_passwordEdit->setStyleSheet(
        "QLineEdit {"
        "  padding: 10px;"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 10px;"
        "  background: rgba(30, 30, 30, 180);"
        "  color: white;"
        "  font-size: 16px;"
        "  selection-background-color: rgba(255, 255, 255, 60);"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid rgba(255, 255, 255, 100);"
        "  background: rgba(40, 40, 40, 200);"
        "}"
    );

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #ff5555; font-size: 14px; font-weight: bold;");

    layout->addStretch();
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    m_unlockWidget->hide();

    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LockScreenWindow::attemptUnlock);

    m_lockoutTimer = new QTimer(this);
    connect(m_lockoutTimer, &QTimer::timeout, this, &LockScreenWindow::updateLockout);
}

void LockScreenWindow::setLockMode(bool locked) {
    m_locked = locked;
    setProperty("isLocked", locked);
    if (m_locked) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(winId());
        // 移除穿透属性
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
#endif
        applyAcrylic();

        if (m_isMain && m_unlockWidget) {
            m_unlockWidget->show();
            m_unlockWidget->move((width() - m_unlockWidget->width()) / 2, height() / 2 + 100);
            m_passwordEdit->setFocus();
        }
    } else {
        if (m_unlockWidget) m_unlockWidget->hide();
    }
    update();
}

void LockScreenWindow::attemptUnlock() {
    if (m_lockoutTime > 0) return;

    QString raw = m_passwordEdit->text();
    AppConfig config = ConfigManager::instance().getConfig();

    if (ConfigManager::verifyPassword(raw, config.passwordHash)) {
        CountdownEngine::instance().reportUnlockAttempt(true);
    } else {
        CountdownEngine::instance().reportUnlockAttempt(false);
        m_attempts++;
        m_passwordEdit->clear();

        // 抖动动画
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
            m_passwordEdit->setEnabled(false);
            m_lockoutTimer->start(1000);
            CountdownEngine::instance().triggerLockedOut(m_lockoutTime);
            updateLockout();
        } else {
            m_statusLabel->setText(QString("密码错误，还剩 %1 次机会").arg(config.maxPasswordAttempts - m_attempts));
        }
    }
}

void LockScreenWindow::togglePasswordVisible() {
    // 简化：目前直接附在遮罩上，暂时不放切换按钮，保持界面简洁
}

void LockScreenWindow::updateLockout() {
    if (m_lockoutTime > 0) {
        m_statusLabel->setText(QString("尝试次数过多，请等待 %1 秒").arg(m_lockoutTime));
        m_lockoutTime--;
    } else {
        m_lockoutTimer->stop();
        m_passwordEdit->setEnabled(true);
        m_passwordEdit->setFocus();
        m_statusLabel->clear();
        m_attempts = 0;
    }
}

void LockScreenWindow::focusInput() {
    if (m_passwordEdit) {
        m_passwordEdit->setFocus();
    }
}

void LockScreenWindow::setClockPaused(bool paused) {
    Q_UNUSED(paused);
}

void LockScreenWindow::applyAcrylic() {
    if (!m_locked) return;
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        pSetWindowCompositionAttribute setWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (setWindowCompositionAttribute) {
            AppConfig config = ConfigManager::instance().getConfig();
            int alpha = (config.overlayOpacity * 255 / 100);
            
            ACCENT_POLICY accent = { 4, 0, (alpha << 24) | 0x000000, 0 }; // 4 = ACCENT_ENABLE_ACRYLICBLURBEHIND
            WINDOWCOMPOSITIONATTRIBDATA data = { 19, &accent, sizeof(accent) }; // 19 = WCA_ACCENT_POLICY
            setWindowCompositionAttribute(hwnd, &data);
        }
    }
#endif
}

void LockScreenWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    applyAcrylic();
}

void LockScreenWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    AppConfig config = ConfigManager::instance().getConfig();
    int remaining = CountdownEngine::instance().remainingSeconds();
    bool isWarning = (CountdownEngine::instance().state() == CountdownEngine::PreLockWarning);

    // 1. 绘制背景 (如果是锁定模式)
    if (m_locked) {
        // 核心修复：即使没有图片，也必须绘制一层底色，确保界面可见
        painter.fillRect(rect(), QColor(0, 0, 0, 180)); 

        if (!config.backgroundImagePath.isEmpty()) {
            QPixmap bg(config.backgroundImagePath);
            if (!bg.isNull()) {
                painter.drawPixmap(rect(), bg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                // 图片之上再叠加一层半透明，保证文字清晰
                painter.fillRect(rect(), QColor(0, 0, 0, 100)); 
            }
        }
    }

    // 2. 锁图标 (仅在正式锁定模式下显示)
    if (m_locked && config.showLockIcon) {
        QPixmap lock = SvgIcon::get(SvgIcon::Lock, QSize(120, 120), Qt::white);
        painter.drawPixmap((width() - 120) / 2, (height() - 120) / 2 - 40, lock);
    }

    // 3. 自定义文字 (仅在正式锁定模式下显示)
    if (m_locked) {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(24);
        painter.setFont(font);
        painter.drawText(rect().adjusted(0, 100, 0, 0), Qt::AlignCenter, config.customMessage);
    }

    // 4. 实时时钟 (右下角, HH:MM:SS)
    painter.setOpacity(0.7);
    painter.setPen(Qt::white);
    QFont clockFont = painter.font();
    clockFont.setPixelSize(16);
    painter.setFont(clockFont);
    QString currentTime = QDateTime::currentDateTime().toString("HH:mm:ss");
    painter.drawText(rect().adjusted(0, 0, -20, -20), Qt::AlignRight | Qt::AlignBottom, currentTime);
    painter.setOpacity(1.0);

    // 5. 倒计时显示
    if (remaining <= 60 && remaining > 0 && isWarning) {
        painter.setPen(QColor(255, 0, 0, 200)); // 鲜艳的红色
        QFont font = painter.font();
        font.setBold(true);
        if (isWarning) {
            // 预警状态下，倒计时显示在中央，120px 巨大数字
            font.setPixelSize(120);
            painter.setFont(font);
            painter.drawText(rect(), Qt::AlignCenter, QString::number(remaining));
        }
    }
    
    // 6. 警告提示 (仅锁定模式有效)
    if (m_locked && m_showWarning) {
        painter.setOpacity(m_warningOpacity);
        QPixmap warn = SvgIcon::get(SvgIcon::Warning, QSize(32, 32), Qt::yellow);
        painter.drawPixmap(20, height() - 52, warn);
        
        QFont warnFont = painter.font();
        warnFont.setPixelSize(18);
        warnFont.setBold(false);
        painter.setFont(warnFont);
        painter.setPen(Qt::yellow);
        painter.drawText(60, height() - 30, "请勿操作，电脑已锁定");
        painter.setOpacity(1.0);
    }
}

void LockScreenWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
}

void LockScreenWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        // 兜底逻辑：如果系统钩子没接管到 Esc，通过窗口事件再次触发信号
        emit SystemHookManager::instance().escPressed();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void LockScreenWindow::showTouchWarning() {
    m_showWarning = true;
    m_warningOpacity = 1.0f;
    update();
    m_warningTimer->start(2000);
}

void LockScreenWindow::fadeOutWarning() {
    m_showWarning = false;
    update();
}
