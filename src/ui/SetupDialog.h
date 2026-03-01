#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(QWidget *parent = nullptr);

private slots:
    void startCountdown();
    void showSettings();
    void useTemplate();

private:
    QSpinBox *m_minutesSpin = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QLineEdit *m_confirmPassEdit = nullptr;
    QCheckBox *m_rememberPass = nullptr;
};

#endif // SETUPDIALOG_H
