#ifndef PROCESSPROTECTOR_H
#define PROCESSPROTECTOR_H

class ProcessProtector {
public:
    // 开启进程保护：通过修改 ACL 拒绝外部终止、挂起及内存写入权限
    static void protect();
    
    // 解除进程保护：恢复正常的访问权限以便安全退出
    static void unprotect();

    // 设置进程为“系统关键进程”，被终止时将触发蓝屏
    static void setCritical(bool enable);
};

#endif // PROCESSPROTECTOR_H
