#include "LockScreenWindow.h"
#include "../utils/SvgIcon.h"
#include "../core/CountdownEngine.h"
#include <QPainter>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QCloseEvent>

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
    }
    update();
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

    // 2. 锁图标 (预警期和锁定模式均显示)
    if (config.showLockIcon && (m_locked || remaining <= 20)) {
        QPixmap lock = SvgIcon::get(SvgIcon::Lock, QSize(120, 120), Qt::white);
        painter.drawPixmap((width() - 120) / 2, (height() - 120) / 2 - 40, lock);
    }

    // 3. 自定义文字 (预警期和锁定模式均显示)
    if (m_locked || remaining <= 20) {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(24);
        painter.setFont(font);
        painter.drawText(rect().adjusted(0, 100, 0, 0), Qt::AlignCenter, config.customMessage);
    }

    // [已移除] 4. 实时时钟 (右下角时间已根据用户要求彻底移除)

    // 5. 倒计时显示
    if (remaining <= 20 && remaining > 0) {
        painter.setPen(QColor(255, 0, 0, 200)); // 鲜艳的红色
        QFont font = painter.font();
        font.setBold(true);
        if (m_locked) {
            // 锁定状态下，倒计时显示在顶部
            font.setPixelSize(80);
            painter.setFont(font);
            painter.drawText(rect().adjusted(0, 40, 0, 0), Qt::AlignHCenter | Qt::AlignTop, QString::number(remaining));
        } else {
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
