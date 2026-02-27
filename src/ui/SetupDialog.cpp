#include "SetupDialog.h"
#include "SettingsPanel.h"
#include "../core/ConfigManager.h"
#include "../core/CountdownEngine.h"
#include "../utils/SvgIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("CountdownLock - 倒计时锁屏");
    setFixedSize(350, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(SvgIcon::get(SvgIcon::Shield, QSize(32, 32), QColor("#0078d7")));
    QLabel *titleLabel = new QLabel("倒计时锁屏", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    mainLayout->addSpacing(20);

    // Countdown Time
    mainLayout->addWidget(new QLabel("倒计时时长 (分钟)：", this));
    m_minutesSpin = new QSpinBox(this);
    m_minutesSpin->setRange(1, 480);
    m_minutesSpin->setValue(ConfigManager::instance().getConfig().countdownMinutes);
    m_minutesSpin->setStyleSheet("padding: 5px;");
    mainLayout->addWidget(m_minutesSpin);

    // Templates
    QHBoxLayout *templateLayout = new QHBoxLayout();
    AppConfig config = ConfigManager::instance().getConfig();
    for (int i = 0; i < config.presetTemplates.size(); ++i) {
        QPushButton *btn = new QPushButton(config.presetTemplates[i], this);
        btn->setProperty("mins", config.presetMinutes[i]);
        connect(btn, &QPushButton::clicked, this, &SetupDialog::useTemplate);
        templateLayout->addWidget(btn);
    }
    mainLayout->addLayout(templateLayout);

    mainLayout->addSpacing(20);

    // Password
    mainLayout->addWidget(new QLabel("解锁密码：", this));
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    if (config.rememberPassword && !config.passwordHash.isEmpty()) {
        m_passEdit->setPlaceholderText("已保存 (留空保持不变)");
    }
    m_passEdit->setStyleSheet("padding: 5px;");
    mainLayout->addWidget(m_passEdit);

    mainLayout->addWidget(new QLabel("确认密码：", this));
    m_confirmPassEdit = new QLineEdit(this);
    m_confirmPassEdit->setEchoMode(QLineEdit::Password);
    m_confirmPassEdit->setStyleSheet("padding: 5px;");
    mainLayout->addWidget(m_confirmPassEdit);

    m_rememberPass = new QCheckBox("记住密码", this);
    m_rememberPass->setChecked(config.rememberPassword);
    mainLayout->addWidget(m_rememberPass);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnSettings = new QPushButton(this);
    btnSettings->setIcon(QIcon(SvgIcon::get(SvgIcon::Settings, QSize(20, 20), Qt::black)));
    btnSettings->setFixedSize(36, 36);
    connect(btnSettings, &QPushButton::clicked, this, &SetupDialog::showSettings);
    
    QPushButton *btnStart = new QPushButton("开始锁屏", this);
    btnStart->setFixedHeight(36);
    btnStart->setStyleSheet("background: #0078d7; color: white; font-weight: bold; border-radius: 4px;");
    connect(btnStart, &QPushButton::clicked, this, &SetupDialog::startCountdown);

    btnLayout->addWidget(btnSettings);
    btnLayout->addWidget(btnStart);
    mainLayout->addLayout(btnLayout);
}

void SetupDialog::useTemplate() {
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        m_minutesSpin->setValue(btn->property("mins").toInt());
    }
}

void SetupDialog::showSettings() {
    SettingsPanel panel(this);
    panel.exec();
}

void SetupDialog::startCountdown() {
    AppConfig config = ConfigManager::instance().getConfig();
    
    QString p1 = m_passEdit->text();
    QString p2 = m_confirmPassEdit->text();

    if (config.passwordHash.isEmpty() && p1.isEmpty()) {
        QMessageBox::warning(this, "错误", "必须设置解锁密码！");
        return;
    }

    if (!p1.isEmpty()) {
        if (p1 != p2) {
            QMessageBox::warning(this, "错误", "两次输入的密码不一致！");
            return;
        }
        config.passwordHash = ConfigManager::hashPassword(p1);
    }

    config.countdownMinutes = m_minutesSpin->value();
    config.rememberPassword = m_rememberPass->isChecked();
    
    ConfigManager::instance().setConfig(config);
    ConfigManager::instance().save();

    accept();
}
