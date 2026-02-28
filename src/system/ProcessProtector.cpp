#include "ProcessProtector.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

void ProcessProtector::protect() {
    HANDLE hProcess = GetCurrentProcess();
    PACL pOldDACL = NULL;
    PACL pNewDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;

    // 获取进程当前的 DACL
    if (GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        NULL, NULL, &pOldDACL, NULL, &pSD) != ERROR_SUCCESS) {
        return;
    }

    EXPLICIT_ACCESSW ea;
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSW));

    // 拒绝权限：终止进程、修改内存、挂起/恢复、写入 DAC、修改所有者、查询信息
    // 增加 PROCESS_QUERY_INFORMATION 拒绝可以防止 taskkill 通过名称匹配到进程
    ea.grfAccessPermissions = PROCESS_TERMINATE | PROCESS_VM_WRITE | 
                              PROCESS_SUSPEND_RESUME | WRITE_DAC | WRITE_OWNER |
                              PROCESS_QUERY_INFORMATION;
    ea.grfAccessMode = DENY_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;

    // 定义需要拒绝的组及其 SID
    struct GroupSid {
        SID_IDENTIFIER_AUTHORITY authority;
        BYTE count;
        DWORD subAuthorities[8];
    };

    GroupSid targets[3] = {
        { SECURITY_WORLD_SID_AUTHORITY, 1, { SECURITY_WORLD_RID } },                       // Everyone (S-1-1-0)
        { SECURITY_NT_AUTHORITY, 2, { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS } }, // Administrators (S-1-5-32-544)
        { SECURITY_NT_AUTHORITY, 1, { SECURITY_LOCAL_SYSTEM_RID } }                        // SYSTEM (S-1-5-18)
    };

    PACL pCurrentACL = pOldDACL;

    for (int i = 0; i < 3; ++i) {
        PSID pSid = NULL;
        if (AllocateAndInitializeSid(&targets[i].authority, targets[i].count,
                                     targets[i].subAuthorities[0], targets[i].subAuthorities[1],
                                     targets[i].subAuthorities[2], targets[i].subAuthorities[3],
                                     targets[i].subAuthorities[4], targets[i].subAuthorities[5],
                                     targets[i].subAuthorities[6], targets[i].subAuthorities[7],
                                     &pSid)) {
            ea.Trustee.ptstrName = (LPWSTR)pSid;
            PACL pNextACL = NULL;
            if (SetEntriesInAclW(1, &ea, pCurrentACL, &pNextACL) == ERROR_SUCCESS) {
                if (pCurrentACL != pOldDACL) LocalFree(pCurrentACL);
                pCurrentACL = pNextACL;
            }
            FreeSid(pSid);
        }
    }

    if (pCurrentACL != pOldDACL) {
        SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        NULL, NULL, pCurrentACL, NULL);
        LocalFree(pCurrentACL);
    }

    if (pSD) LocalFree(pSD);
}

void ProcessProtector::unprotect() {
    HANDLE hProcess = GetCurrentProcess();
    
    // 恢复逻辑：将进程的 DACL 重置为允许所有人完全访问，确保能被正常关闭
    // unprotect 必须显式移除拒绝条目，这里赋予一个新的、只有“允许”条目的 ACL。
    
    PACL pNewDACL = NULL;
    EXPLICIT_ACCESSW ea[3];
    ZeroMemory(&ea, sizeof(ea));

    SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID pSids[3] = { NULL, NULL, NULL };

    AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pSids[0]);
    AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSids[1]);
    AllocateAndInitializeSid(&ntAuth, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSids[2]);

    int validCount = 0;
    for (int i = 0; i < 3; ++i) {
        if (pSids[i]) {
            ea[validCount].grfAccessPermissions = PROCESS_ALL_ACCESS;
            ea[validCount].grfAccessMode = SET_ACCESS;
            ea[validCount].grfInheritance = NO_INHERITANCE;
            ea[validCount].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea[validCount].Trustee.ptstrName = (LPWSTR)pSids[i];
            validCount++;
        }
    }

    if (validCount > 0 && SetEntriesInAclW(validCount, ea, NULL, &pNewDACL) == ERROR_SUCCESS) {
        SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        NULL, NULL, pNewDACL, NULL);
    }

    if (pNewDACL) LocalFree(pNewDACL);
    for (int i = 0; i < 3; ++i) {
        if (pSids[i]) FreeSid(pSids[i]);
    }
}

typedef NTSTATUS(NTAPI* pRtlSetProcessIsCritical)(BOOLEAN, PBOOLEAN, BOOLEAN);

void ProcessProtector::setCritical(bool enable) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto RtlSetProcessIsCritical = (pRtlSetProcessIsCritical)GetProcAddress(hNtdll, "RtlSetProcessIsCritical");
        if (RtlSetProcessIsCritical) {
            // 提权：需要 SeShutdownPrivilege 才能设置关键进程（某些 Windows 版本）
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                TOKEN_PRIVILEGES tp;
                LUID luid;
                if (LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid)) {
                    tp.PrivilegeCount = 1;
                    tp.Privileges[0].Luid = luid;
                    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
                }
                CloseHandle(hToken);
            }

            RtlSetProcessIsCritical(enable ? TRUE : FALSE, NULL, FALSE);
        }
    }
}

void ProcessProtector::setSystemPolicies(bool enable) {
    HKEY hKeySystem;
    HKEY hKeyExplorer;
    LPCWSTR systemPolicyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
    LPCWSTR explorerPolicyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";

    DWORD value = enable ? 1 : 0;

    // 1. 系统级封锁 (针对 Ctrl+Alt+Del 界面)
    if (RegCreateKeyExW(HKEY_CURRENT_USER, systemPolicyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKeySystem, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKeySystem, L"DisableTaskMgr", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegSetValueExW(hKeySystem, L"DisableLockWorkstation", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegSetValueExW(hKeySystem, L"DisableChangePassword", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKeySystem);
    }

    // 2. 资源管理器封锁 (针对注销)
    if (RegCreateKeyExW(HKEY_CURRENT_USER, explorerPolicyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKeyExplorer, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKeyExplorer, L"NoLogoff", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKeyExplorer);
    }
}
#else
void ProcessProtector::protect() {}
void ProcessProtector::unprotect() {}
void ProcessProtector::setCritical(bool) {}
#endif
