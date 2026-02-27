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
    setProperty("isMainScreen", isMain);

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, QOverload<>::of(&LockScreenWindow::update));
    m_clockTimer->start(1000);

    m_warningTimer = new QTimer(this);
    connect(m_warningTimer, &QTimer::timeout, this, &LockScreenWindow::fadeOutWarning);
}

void LockScreenWindow::applyAcrylic() {
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

    // 1. 背景图 (如果有)
    if (!config.backgroundImagePath.isEmpty()) {
        QPixmap bg(config.backgroundImagePath);
        if (!bg.isNull()) {
            painter.drawPixmap(rect(), bg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            painter.fillRect(rect(), QColor(0, 0, 0, 50)); // 叠加一层半透明黑确保文字可见
        }
    }

    // 2. 锁图标
    if (config.showLockIcon) {
        QPixmap lock = SvgIcon::get(SvgIcon::Lock, QSize(120, 120), Qt::white);
        painter.drawPixmap((width() - 120) / 2, (height() - 120) / 2 - 40, lock);
    }

    // 3. 自定义文字
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(24);
    painter.setFont(font);
    painter.drawText(rect().adjusted(0, 100, 0, 0), Qt::AlignCenter, config.customMessage);

    // 4. 实时时钟 (右下角)
    font.setPixelSize(18);
    painter.setFont(font);
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    painter.drawText(rect().adjusted(0, 0, -20, -20), Qt::AlignRight | Qt::AlignBottom, timeStr);

    // 5. 最后20秒倒计时 (顶部)
    if (CountdownEngine::instance().state() == CountdownEngine::LastWarning ||
        (CountdownEngine::instance().state() == CountdownEngine::Counting && CountdownEngine::instance().remainingSeconds() <= 20)) {
        font.setPixelSize(80);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::red);
        painter.drawText(rect().adjusted(0, 40, 0, 0), Qt::AlignHCenter | Qt::AlignTop, QString::number(CountdownEngine::instance().remainingSeconds()));
    }

    // 6. 警告提示 (左下角)
    // 逻辑将在 SystemHookManager 触发时调用
}

void LockScreenWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
}

void LockScreenWindow::updateClock() {
    update();
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
