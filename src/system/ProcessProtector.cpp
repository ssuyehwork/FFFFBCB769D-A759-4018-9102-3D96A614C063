#include "ProcessProtector.h"

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

    // 拒绝权限：终止进程、修改内存、挂起/恢复、写入 DAC、修改所有者
    ea.grfAccessPermissions = PROCESS_TERMINATE | PROCESS_VM_WRITE | 
                              PROCESS_SUSPEND_RESUME | WRITE_DAC | WRITE_OWNER;
    ea.grfAccessMode = DENY_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

    // 获取 "Everyone" 组的 SID (S-1-1-0)
    PSID pEveryoneSid = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    if (AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
        ea.Trustee.ptstrName = (LPWSTR)pEveryoneSid;

        // 创建新的 ACL，将拒绝条目合并进去
        if (SetEntriesInAclW(1, &ea, pOldDACL, &pNewDACL) == ERROR_SUCCESS) {
            // 应用新的安全设置到当前进程
            SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                            NULL, NULL, pNewDACL, NULL);
        }

        if (pNewDACL) LocalFree(pNewDACL);
        FreeSid(pEveryoneSid);
    }

    if (pSD) LocalFree(pSD);
}

void ProcessProtector::unprotect() {
    HANDLE hProcess = GetCurrentProcess();
    
    // 恢复逻辑：将进程的 DACL 重置为允许当前用户完全访问
    // 实际上对于自身 suicide 来说，即便不恢复，ExitProcess 也能正常工作。
    // 为了稳妥，这里通过赋予空 DACL (注意：空 DACL 意味着允许所有人访问)
    // 或者重新获取默认安全描述符。
    // 在此处，我们简单地通过重新应用一个包含“允许所有人”条目的 ACL 来实现。
    
    PACL pNewDACL = NULL;
    EXPLICIT_ACCESSW ea;
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSW));

    ea.grfAccessPermissions = PROCESS_ALL_ACCESS;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

    PSID pEveryoneSid = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    if (AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
        ea.Trustee.ptstrName = (LPWSTR)pEveryoneSid;
        if (SetEntriesInAclW(1, &ea, NULL, &pNewDACL) == ERROR_SUCCESS) {
            SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                            NULL, NULL, pNewDACL, NULL);
        }
        if (pNewDACL) LocalFree(pNewDACL);
        FreeSid(pEveryoneSid);
    }
}
#else
void ProcessProtector::protect() {}
void ProcessProtector::unprotect() {}
#endif
