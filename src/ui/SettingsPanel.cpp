#include "SettingsPanel.h"
#include "../utils/SvgIcon.h"
#include "../core/SessionLogger.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>

SettingsPanel::SettingsPanel(QWidget *parent) : QDialog(parent) {
    setWindowTitle("设置");
    setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    AppConfig config = ConfigManager::instance().getConfig();

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(config.overlayOpacity);
    formLayout->addRow("遮罩不透明度：", m_opacitySlider);

    m_lockDuration = new QSpinBox();
    m_lockDuration->setRange(1, 60);
    m_lockDuration->setValue(config.lockDurationMinutes);
    formLayout->addRow("锁屏持续时长 (分)：", m_lockDuration);

    QHBoxLayout *bgLayout = new QHBoxLayout();
    m_bgPathEdit = new QLineEdit(config.backgroundImagePath);
    m_bgPathEdit->setReadOnly(true);
    QPushButton *btnSelectBg = new QPushButton("选择");
    QPushButton *btnClearBg = new QPushButton("清除");
    connect(btnSelectBg, &QPushButton::clicked, this, &SettingsPanel::selectBackgroundImage);
    connect(btnClearBg, &QPushButton::clicked, this, &SettingsPanel::clearBackgroundImage);
    bgLayout->addWidget(m_bgPathEdit);
    bgLayout->addWidget(btnSelectBg);
    bgLayout->addWidget(btnClearBg);
    formLayout->addRow("背景图片：", bgLayout);

    m_showLockIcon = new QCheckBox("显示锁图标 (SVG)");
    m_showLockIcon->setChecked(config.showLockIcon);
    formLayout->addRow(m_showLockIcon);

    m_customMessage = new QLineEdit(config.customMessage);
    formLayout->addRow("自定义文字：", m_customMessage);

    m_preventSleep = new QCheckBox("阻止系统息屏/睡眠");
    m_preventSleep->setChecked(config.preventSleep);
    formLayout->addRow(m_preventSleep);

    m_autoRestart = new QCheckBox("解锁后自动开始下一轮");
    m_autoRestart->setChecked(config.autoRestartAfterUnlock);
    formLayout->addRow(m_autoRestart);

    m_launchOnStartup = new QCheckBox("开机自启动");
    m_launchOnStartup->setChecked(config.launchOnStartup);
    formLayout->addRow(m_launchOnStartup);

    m_maxAttempts = new QSpinBox();
    m_maxAttempts->setRange(1, 10);
    m_maxAttempts->setValue(config.maxPasswordAttempts);
    formLayout->addRow("最大密码错误次数：", m_maxAttempts);

    m_lockoutSecs = new QSpinBox();
    m_lockoutSecs->setRange(10, 3600);
    m_lockoutSecs->setValue(config.lockoutDurationSecs);
    formLayout->addRow("锁定持续秒数：", m_lockoutSecs);

    mainLayout->addLayout(formLayout);

    QPushButton *btnLog = new QPushButton("查看锁屏日志");
    connect(btnLog, &QPushButton::clicked, this, &SettingsPanel::showLog);
    mainLayout->addWidget(btnLog);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnSave = new QPushButton("保存");
    QPushButton *btnCancel = new QPushButton("取消");
    connect(btnSave, &QPushButton::clicked, this, &SettingsPanel::saveAndClose);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addStretch();
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);
}

void SettingsPanel::selectBackgroundImage() {
    QString path = QFileDialog::getOpenFileName(this, "选择背景图", "", "Images (*.png *.jpg *.jpeg)");
    if (!path.isEmpty()) {
        m_bgPathEdit->setText(path);
    }
}

void SettingsPanel::clearBackgroundImage() {
    m_bgPathEdit->clear();
}

void SettingsPanel::saveAndClose() {
    AppConfig config = ConfigManager::instance().getConfig();
    config.overlayOpacity = m_opacitySlider->value();
    config.lockDurationMinutes = m_lockDuration->value();
    config.backgroundImagePath = m_bgPathEdit->text();
    config.showLockIcon = m_showLockIcon->isChecked();
    config.customMessage = m_customMessage->text();
    config.preventSleep = m_preventSleep->isChecked();
    config.autoRestartAfterUnlock = m_autoRestart->isChecked();
    config.launchOnStartup = m_launchOnStartup->isChecked();
    config.maxPasswordAttempts = m_maxAttempts->value();
    config.lockoutDurationSecs = m_lockoutSecs->value();

    ConfigManager::instance().setConfig(config);
    ConfigManager::instance().save();
    accept();
}

void SettingsPanel::showLog() {
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("锁屏历史日志");
    dlg->resize(600, 400);
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    
    QTableWidget *table = new QTableWidget(dlg);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"日期", "开始", "结束", "时长(分)", "触碰次数"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto history = SessionLogger::instance().getHistory();
    table->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
        const auto& rec = history[i];
        table->setItem(i, 0, new QTableWidgetItem(rec.startTime.toString("yyyy-MM-dd")));
        table->setItem(i, 1, new QTableWidgetItem(rec.startTime.toString("HH:mm:ss")));
        table->setItem(i, 2, new QTableWidgetItem(rec.endTime.toString("HH:mm:ss")));
        int mins = rec.startTime.secsTo(rec.endTime) / 60;
        table->setItem(i, 3, new QTableWidgetItem(QString::number(mins)));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(rec.touchCount)));
    }
    
    layout->addWidget(table);
    dlg->exec();
}
