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

    const int groupCount = 3;
    WELL_KNOWN_SID_TYPE groups[groupCount] = {
        WinWorldSid,             // Everyone
        WinBuiltinAdminsSid,     // Administrators
        WinLocalSystemSid        // SYSTEM
    };

    PACL pCurrentACL = pOldDACL;

    for (int i = 0; i < groupCount; ++i) {
        PSID pSid = NULL;
        DWORD sidSize = SECURITY_MAX_SID_SIZE;
        pSid = LocalAlloc(LPTR, sidSize);
        if (CreateWellKnownSid(groups[i], NULL, pSid, &sidSize)) {
            ea.Trustee.ptstrName = (LPWSTR)pSid;
            PACL pNextACL = NULL;
            if (SetEntriesInAclW(1, &ea, pCurrentACL, &pNextACL) == ERROR_SUCCESS) {
                if (pCurrentACL != pOldDACL) LocalFree(pCurrentACL);
                pCurrentACL = pNextACL;
            }
            LocalFree(pSid);
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
    // 注意：在 protect() 中我们使用了 DENY_ACCESS，它在 ACL 中的优先级高于 ALLOW_ACCESS。
    // 因此，unprotect 必须显式移除这些拒绝条目，最简单的做法是赋予一个新的、只有“允许”条目的 ACL。
    
    PACL pNewDACL = NULL;
    EXPLICIT_ACCESSW ea[3];
    ZeroMemory(&ea, sizeof(ea));

    WELL_KNOWN_SID_TYPE groups[3] = { WinWorldSid, WinBuiltinAdminsSid, WinLocalSystemSid };
    PSID pSids[3] = { NULL, NULL, NULL };

    for (int i = 0; i < 3; ++i) {
        DWORD sidSize = SECURITY_MAX_SID_SIZE;
        pSids[i] = LocalAlloc(LPTR, sidSize);
        if (CreateWellKnownSid(groups[i], NULL, pSids[i], &sidSize)) {
            ea[i].grfAccessPermissions = PROCESS_ALL_ACCESS;
            ea[i].grfAccessMode = SET_ACCESS;
            ea[i].grfInheritance = NO_INHERITANCE;
            ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea[i].Trustee.ptstrName = (LPWSTR)pSids[i];
        }
    }

    if (SetEntriesInAclW(3, ea, NULL, &pNewDACL) == ERROR_SUCCESS) {
        SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        NULL, NULL, pNewDACL, NULL);
    }

    if (pNewDACL) LocalFree(pNewDACL);
    for (int i = 0; i < 3; ++i) {
        if (pSids[i]) LocalFree(pSids[i]);
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
#else
void ProcessProtector::protect() {}
void ProcessProtector::unprotect() {}
void ProcessProtector::setCritical(bool) {}
#endif
