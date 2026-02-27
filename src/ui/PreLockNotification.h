#ifndef PRELOCKNOTIFICATION_H
#define PRELOCKNOTIFICATION_H

#include <QWidget>

class PreLockNotification : public QWidget {
    Q_OBJECT
public:
    explicit PreLockNotification(QWidget *parent = nullptr);
    void updateRemaining(int seconds);
    void startFadeOut();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_remaining = 20;
};

#endif // PRELOCKNOTIFICATION_H
