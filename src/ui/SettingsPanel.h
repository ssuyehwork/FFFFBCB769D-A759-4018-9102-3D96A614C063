#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QDialog>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include "../core/ConfigManager.h"

class SettingsPanel : public QDialog {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

private slots:
    void selectBackgroundImage();
    void clearBackgroundImage();
    void saveAndClose();
    void showLog();

private:
    QSlider *m_opacitySlider;
    QLineEdit *m_bgPathEdit;
    QCheckBox *m_showLockIcon;
    QLineEdit *m_customMessage;
    QCheckBox *m_preventSleep;
    QCheckBox *m_launchOnStartup;
    QSpinBox *m_maxAttempts;
    QSpinBox *m_lockoutSecs;
    QSpinBox *m_warningSecs;

    QLineEdit *m_oldPassEdit;
    QLineEdit *m_newPassEdit;
    QLineEdit *m_confirmNewPassEdit;
};

#endif // SETTINGSPANEL_H
