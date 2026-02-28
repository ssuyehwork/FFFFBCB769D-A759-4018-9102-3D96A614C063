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
#include <QInputDialog>

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

    m_warningSecs = new QSpinBox();
    m_warningSecs->setRange(5, 300);
    m_warningSecs->setValue(config.warningDurationSeconds);
    formLayout->addRow("预警触发时长 (秒)：", m_warningSecs);

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
    config.backgroundImagePath = m_bgPathEdit->text();
    config.showLockIcon = m_showLockIcon->isChecked();
    config.customMessage = m_customMessage->text();
    config.preventSleep = m_preventSleep->isChecked();
    config.launchOnStartup = m_launchOnStartup->isChecked();
    config.maxPasswordAttempts = m_maxAttempts->value();
    config.lockoutDurationSecs = m_lockoutSecs->value();
    config.warningDurationSeconds = m_warningSecs->value();

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

    QPushButton *btnClear = new QPushButton("清空所有日志", dlg);
    btnClear->setStyleSheet("color: red; padding: 5px;");
    connect(btnClear, &QPushButton::clicked, [this, dlg, table]() {
        bool ok;
        QString pwd = QInputDialog::getText(dlg, "安全验证", "请输入解锁密码以清空日志：", QLineEdit::Password, "", &ok);
        if (ok && !pwd.isEmpty()) {
            if (ConfigManager::verifyPassword(pwd, ConfigManager::instance().getConfig().passwordHash)) {
                SessionLogger::instance().clearHistory();
                table->setRowCount(0);
                QMessageBox::information(dlg, "成功", "日志已清空。");
            } else {
                QMessageBox::warning(dlg, "错误", "密码不正确，操作已取消。");
            }
        }
    });
    layout->addWidget(btnClear);

    dlg->exec();
}
