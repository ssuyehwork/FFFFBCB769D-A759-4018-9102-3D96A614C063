#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QList>

struct AppConfig {
    int     countdownMinutes = 45;      // 倒计时分钟数，默认45
    int     lockDurationMinutes = 5;    // 锁屏持续分钟数，默认5
    bool    rememberPassword = true;    // 是否记住密码
    QString passwordHash = "";          // SHA-256哈希后的密码
    int     overlayOpacity = 80;        // 遮罩不透明度 0~100，默认80
    QString backgroundImagePath = "";   // 背景图路径
    bool    showLockIcon = true;        // 是否显示锁SVG图标
    QString customMessage = "专注中，请勿打扰"; // 自定义锁屏文字
    bool    preventSleep = true;        // 阻止系统息屏/睡眠
    bool    autoRestartAfterUnlock = false; // 解锁后自动重启下一轮
    bool    launchOnStartup = false;    // 开机自启
    int     maxPasswordAttempts = 5;    // 最大密码错误次数
    int     lockoutDurationSecs = 30;   // 超过错误次数后的惩罚锁定秒数
    QStringList presetTemplates = {"番茄", "课堂", "深空"}; 
    QList<int>  presetMinutes = {25, 45, 60};
};

class ConfigManager {
public:
    static ConfigManager& instance();

    void load();
    void save();

    const AppConfig& getConfig() const { return m_config; }
    void setConfig(const AppConfig& config) { m_config = config; }

    static QString hashPassword(const QString& raw);
    static bool verifyPassword(const QString& raw, const QString& hash);

    void setLaunchOnStartup(bool enable);

private:
    ConfigManager();
    AppConfig m_config;
    QSettings* m_settings;
};

#endif // CONFIGMANAGER_H
