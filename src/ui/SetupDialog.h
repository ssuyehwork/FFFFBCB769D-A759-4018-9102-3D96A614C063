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
    QSpinBox *m_minutesSpin;
    QLineEdit *m_passEdit;
    QLineEdit *m_confirmPassEdit;
    QCheckBox *m_rememberPass;
};

#endif // SETUPDIALOG_H
