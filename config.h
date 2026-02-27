#pragma once
#include "common.h"

struct AppConfig {
    int defaultMinutes = 45;
    bool rememberPassword = true;
    int hotkeyVK = 'L';
    int hotkeyMod = MOD_CONTROL | MOD_SHIFT;
    int alpha = 85;
    COLORREF bgColor = RGB(0, 0, 0);
    bool showLockIcon = true;
    std::wstring promptText = L"专注时间，请勿打扰";
    int maxAttempts = 3;
    int cooldownSeconds = 30;
    int endAction = 0; // 0: Auto, 1: Keep Lock, 2: Beep
};

namespace Config {
    void Load(AppConfig& config);
    void Save(const AppConfig& config);
}
