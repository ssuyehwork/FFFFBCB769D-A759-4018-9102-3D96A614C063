#include "ConfigManager.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

ConfigManager::ConfigManager() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(path);
    m_settings = new QSettings(path + "/config.ini", QSettings::IniFormat);
    load();
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

void ConfigManager::load() {
    m_config.countdownMinutes = m_settings->value("countdownMinutes", 45).toInt();
    m_config.rememberPassword = m_settings->value("rememberPassword", true).toBool();
    m_config.passwordHash = m_settings->value("passwordHash", "").toString();
    m_config.overlayOpacity = m_settings->value("overlayOpacity", 80).toInt();
    m_config.backgroundImagePath = m_settings->value("backgroundImagePath", "").toString();
    m_config.showLockIcon = m_settings->value("showLockIcon", true).toBool();
    m_config.customMessage = m_settings->value("customMessage", "专注中，请勿打扰").toString();
    m_config.preventSleep = m_settings->value("preventSleep", true).toBool();
    m_config.stayRunningAfterUnlock = m_settings->value("stayRunningAfterUnlock", false).toBool();
    m_config.launchOnStartup = m_settings->value("launchOnStartup", false).toBool();
    m_config.maxPasswordAttempts = m_settings->value("maxPasswordAttempts", 5).toInt();
    m_config.lockoutDurationSecs = m_settings->value("lockoutDurationSecs", 30).toInt();
    m_config.warningDurationSeconds = m_settings->value("warningDurationSeconds", 20).toInt();
    m_config.targetEndTime = m_settings->value("targetEndTime").toDateTime();

    if (m_settings->contains("presetTemplates")) {
        m_config.presetTemplates = m_settings->value("presetTemplates").toStringList();
        QVariantList minutes = m_settings->value("presetMinutes").toList();
        m_config.presetMinutes.clear();
        for (const auto& v : minutes) m_config.presetMinutes << v.toInt();
    }
}

void ConfigManager::save() {
    m_settings->setValue("countdownMinutes", m_config.countdownMinutes);
    m_settings->setValue("rememberPassword", m_config.rememberPassword);
    m_settings->setValue("passwordHash", m_config.passwordHash);
    m_settings->setValue("overlayOpacity", m_config.overlayOpacity);
    m_settings->setValue("backgroundImagePath", m_config.backgroundImagePath);
    m_settings->setValue("showLockIcon", m_config.showLockIcon);
    m_settings->setValue("customMessage", m_config.customMessage);
    m_settings->setValue("preventSleep", m_config.preventSleep);
    m_settings->setValue("stayRunningAfterUnlock", m_config.stayRunningAfterUnlock);
    m_settings->setValue("launchOnStartup", m_config.launchOnStartup);
    m_settings->setValue("maxPasswordAttempts", m_config.maxPasswordAttempts);
    m_settings->setValue("lockoutDurationSecs", m_config.lockoutDurationSecs);
    m_settings->setValue("warningDurationSeconds", m_config.warningDurationSeconds);
    m_settings->setValue("targetEndTime", m_config.targetEndTime);
    m_settings->setValue("presetTemplates", m_config.presetTemplates);
    
    QVariantList minutes;
    for (int m : m_config.presetMinutes) minutes << m;
    m_settings->setValue("presetMinutes", minutes);
    
    m_settings->sync();
    
    setLaunchOnStartup(m_config.launchOnStartup);
}

QString ConfigManager::hashPassword(const QString& raw) {
    if (raw.isEmpty()) return "";
    // 使用固定盐值增强安全性
    static const QString salt = "CountdownLock_Salt_2024";
    QByteArray saltedData = (raw + salt).toUtf8();
    return QString(QCryptographicHash::hash(saltedData, QCryptographicHash::Sha256).toHex());
}

bool ConfigManager::verifyPassword(const QString& raw, const QString& hash) {
    if (hash.isEmpty()) return true; // 未设置密码
    
    // 1. 尝试使用当前（加盐）方式验证
    if (hashPassword(raw) == hash) return true;
    
    // 2. 兼容性回退：尝试使用旧版（不加盐）方式验证
    QString oldHash = QString(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (oldHash == hash) {
        // 如果旧版验证通过，可以在此处选择是否自动升级哈希到新版
        // 为了简化，我们暂时只返回 true
        return true;
    }
    
    return false;
}

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

void ConfigManager::setLaunchOnStartup(bool enable) {
#ifdef Q_OS_WIN
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    HKEY hKey;
    LPCWSTR keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LPCWSTR valueName = L"CountdownLock";

    if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExW(hKey, valueName, 0, REG_SZ, (BYTE*)appPath.utf16(), (appPath.length() + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, valueName);
        }
        RegCloseKey(hKey);
    }
#endif
}
