#ifndef LOCKSCREENWINDOW_H
#define LOCKSCREENWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "../core/ConfigManager.h"

class LockScreenWindow : public QWidget {
    Q_OBJECT
public:
    explicit LockScreenWindow(const QRect& geometry, bool isMain, QWidget *parent = nullptr);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public slots:
    void fadeOutWarning();
    void showTouchWarning();
    void setLockMode(bool locked);
    void setClockPaused(bool paused);
    void updateLockout();

private slots:
    void attemptUnlock();
    void togglePasswordVisible();

private:
    void applyAcrylic();
    void setupUnlockUi();

    bool m_isMain;
    bool m_locked = false;
    QTimer *m_warningTimer;
    bool m_showWarning = false;
    float m_warningOpacity = 0.0f;

    // 解锁组件 (仅主屏使用)
    QWidget *m_unlockWidget = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QPushButton *m_togglePassBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    int m_attempts = 0;
    int m_lockoutTime = 0;
    QTimer *m_lockoutTimer = nullptr;
};

#endif // LOCKSCREENWINDOW_H
