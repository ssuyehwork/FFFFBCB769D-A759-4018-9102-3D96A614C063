#include "config.h"
#include <shlobj.h>

namespace Config {

std::wstring GetIniPath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    wcscat(path, L"\\FocusLock");
    CreateDirectoryW(path, NULL);
    wcscat(path, L"\\config.ini");
    return path;
}

void Load(AppConfig& config) {
    std::wstring ini = GetIniPath();
    config.defaultMinutes = GetPrivateProfileIntW(L"General", L"DefaultMinutes", 45, ini.c_str());
    config.rememberPassword = GetPrivateProfileIntW(L"General", L"RememberPassword", 1, ini.c_str()) != 0;
    config.hotkeyVK = GetPrivateProfileIntW(L"General", L"HotkeyVK", 'L', ini.c_str());
    config.hotkeyMod = GetPrivateProfileIntW(L"General", L"HotkeyMod", MOD_CONTROL | MOD_SHIFT, ini.c_str());

    config.alpha = GetPrivateProfileIntW(L"Overlay", L"Alpha", 85, ini.c_str());
    config.bgColor = GetPrivateProfileIntW(L"Overlay", L"BackgroundColor", RGB(0,0,0), ini.c_str());
    config.showLockIcon = GetPrivateProfileIntW(L"Overlay", L"ShowLockIcon", 1, ini.c_str()) != 0;

    wchar_t buf[256];
    GetPrivateProfileStringW(L"Overlay", L"PromptText", L"专注时间，请勿打扰", buf, 256, ini.c_str());
    config.promptText = buf;

    config.maxAttempts = GetPrivateProfileIntW(L"Security", L"MaxAttempts", 3, ini.c_str());
    config.cooldownSeconds = GetPrivateProfileIntW(L"Security", L"CooldownSeconds", 30, ini.c_str());
    config.endAction = GetPrivateProfileIntW(L"Behavior", L"EndAction", 0, ini.c_str());
}

void Save(const AppConfig& config) {
    std::wstring ini = GetIniPath();
    auto WriteInt = [&](const wchar_t* section, const wchar_t* key, int val) {
        WritePrivateProfileStringW(section, key, std::to_wstring(val).c_str(), ini.c_str());
    };

    WriteInt(L"General", L"DefaultMinutes", config.defaultMinutes);
    WriteInt(L"General", L"RememberPassword", config.rememberPassword ? 1 : 0);
    WriteInt(L"General", L"HotkeyVK", config.hotkeyVK);
    WriteInt(L"General", L"HotkeyMod", config.hotkeyMod);

    WriteInt(L"Overlay", L"Alpha", config.alpha);
    WriteInt(L"Overlay", L"BackgroundColor", config.bgColor);
    WriteInt(L"Overlay", L"ShowLockIcon", config.showLockIcon ? 1 : 0);
    WritePrivateProfileStringW(L"Overlay", L"PromptText", config.promptText.c_str(), ini.c_str());

    WriteInt(L"Security", L"MaxAttempts", config.maxAttempts);
    WriteInt(L"Security", L"CooldownSeconds", config.cooldownSeconds);
    WriteInt(L"Behavior", L"EndAction", config.endAction);
}

}
