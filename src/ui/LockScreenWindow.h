#ifndef LOCKSCREENWINDOW_H
#define LOCKSCREENWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include "../core/ConfigManager.h"

class LockScreenWindow : public QWidget {
    Q_OBJECT
public:
    explicit LockScreenWindow(const QRect& geometry, bool isMain, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void updateClock();
    void fadeOutWarning();

private:
    void applyAcrylic();
    void showTouchWarning();

    bool m_isMain;
    QTimer *m_clockTimer;
    QTimer *m_warningTimer;
    bool m_showWarning = false;
    float m_warningOpacity = 0.0f;
};

#endif // LOCKSCREENWINDOW_H
