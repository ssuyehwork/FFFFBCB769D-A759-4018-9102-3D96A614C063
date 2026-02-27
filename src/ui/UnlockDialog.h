#ifndef UNLOCKDIALOG_H
#define UNLOCKDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class UnlockDialog : public QDialog {
    Q_OBJECT
public:
    explicit UnlockDialog(QWidget *parent = nullptr);

signals:
    void unlockSucceeded();

private slots:
    void attemptUnlock();
    void togglePasswordVisible();
    void updateLockout();

private:
    QLineEdit *m_passwordEdit;
    QPushButton *m_unlockBtn;
    QLabel *m_statusLabel;
    int m_attempts = 0;
    int m_lockoutTime = 0;
    QTimer *m_lockoutTimer;
};

#endif // UNLOCKDIALOG_H
#endif
