# Prompt

## 项目概述

使用 **C++ + Qt 6**（编译器环境：Qt Creator 18.0.2 Community，Windows 专用，编译套件 MSVC 或 MinGW）开发一款**倒计时锁屏桌面应用**。核心目标：倒计时结束后以极强的系统级遮罩完全封锁用户对电脑的一切操作，直到倒计时自然结束或输入正确密码解锁。**全程禁止使用 Emoji，所有图标必须使用内联 SVG 字符串。**

---

## 架构分层（每个类单独 .h/.cpp 文件）

```
src/
├── main.cpp
├── core/
│   ├── ConfigManager        # QSettings封装，SHA-256密码哈希，所有配置读写
│   ├── CountdownEngine      # QTimer状态机，管理倒计时完整生命周期
│   └── SessionLogger        # 本地JSON日志：锁屏起止时间、触碰次数
├── system/
│   ├── SystemHookManager    # Win32低级键鼠钩子，拦截危险组合键，独立线程
│   ├── TopMostGuard         # 50ms定时器周期强制置顶所有遮罩窗口
│   └── PowerManager         # SetThreadExecutionState控制息屏/睡眠
├── ui/
│   ├── SetupDialog          # 启动配置界面
│   ├── SettingsPanel        # 设置子面板
│   ├── PreLockNotification  # 锁屏前最后20秒通知浮窗（屏幕中心偏上）
│   ├── LockScreenWindow     # 遮罩主窗口（每个屏幕一个实例）
│   ├── UnlockDialog         # ESC触发解锁弹层
│   └── TrayManager          # 系统托盘
└── utils/
    └── SvgIcon              # 统一SVG图标注册与渲染工具类
```

---

## 一、完整时间线与状态机

**这是整个应用最核心的逻辑，所有模块必须严格遵循此时间线，不得错位：**

```
[用户点击"开始锁屏"]
        ↓
[State: Counting] — 倒计时正常运行，托盘显示剩余时间
        ↓
[剩余时间 == 20秒] — CountdownEngine 发射 warningPhaseStarted() 信号
        ↓
[State: PreLockWarning]
  → PreLockNotification 窗口从屏幕上方淡入，显示在屏幕中心偏上位置
  → 每秒更新倒计秒数（20→1），用户仍可自由操作电脑
  → 通知窗口不可被用户关闭，不拦截鼠标事件
        ↓
[倒计秒数归零] — PreLockNotification 淡出并销毁
        ↓
[State: Locking] — LockScreenWindow 在所有屏幕实例化并激活
  → SystemHookManager 启动键鼠钩子
  → TopMostGuard 启动50ms置顶守护
  → PowerManager 根据配置阻止息屏
        ↓
[State: Locked] — 遮罩完全激活，用户无法操作
        ↓
[用户按下 Esc] → UnlockDialog 弹出
        ↓
[密码正确] → [State: Unlocked] → 所有遮罩销毁，钩子卸载
[密码错误] → 错误次数+1，抖动动画，超限则进入 LockedOut 状态
```

**CountdownEngine 状态枚举：**
```cpp
enum class State {
    Idle,
    Counting,
    PreLockWarning,   // 最后20秒，PreLockNotification显示中
    Locking,          // 遮罩激活过渡帧
    Locked,           // 遮罩完全激活
    Unlocking,        // 解锁验证中
    LockedOut,        // 密码错误超限，临时封锁解锁入口
    Unlocked          // 解锁成功，回到Idle前的瞬态
};

signals:
    void stateChanged(State newState);
    void tickSecond(int remainingSeconds);     // 每秒发射，Counting阶段
    void warningPhaseStarted();                // 剩余20秒时发射，触发PreLockNotification
    void warningTick(int remainingSeconds);    // PreLockWarning阶段每秒发射（20→1）
    void lockActivated();                      // 触发遮罩
    void unlockSucceeded();
    void unlockFailed(int attemptsLeft);
    void lockedOut(int lockoutSeconds);
```

---

## 二、ConfigManager

```cpp
struct AppConfig {
    // 倒计时
    int     countdownMinutes;        // 默认45
    // 密码
    bool    rememberPassword;
    QString passwordHash;            // SHA-256哈希，空=未设置
    int     maxPasswordAttempts;     // 默认5
    int     lockoutDurationSecs;     // 超限惩罚秒数，默认30
    // 遮罩外观
    int     overlayOpacity;          // 0~100，默认80，映射到Acrylic Alpha通道
    QString backgroundImagePath;     // 空=纯色
    bool    showLockIcon;            // 是否显示锁SVG图标
    QString customMessage;           // 默认"专注中，请勿打扰"
    // 行为
    bool    preventSleep;            // 阻止系统息屏/睡眠
    bool    autoRestartAfterUnlock;  // 解锁后自动开始下一轮
    bool    launchOnStartup;         // 开机自启（注册表）
    // 预设模板
    QStringList presetNames;
    QList<int>  presetMinutes;
};

// 密码处理
QString hashPassword(const QString& raw);
// → QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).toHex()

bool verifyPassword(const QString& raw, const QString& storedHash);

// 配置存储路径：%APPDATA%\CountdownLock\config.ini
// 日志存储路径：%APPDATA%\CountdownLock\sessions.jsonl
```

---

## 三、PreLockNotification（锁屏前20秒通知窗口）

**这是独立的无边框置顶窗口，与 LockScreenWindow 完全无关，在遮罩激活之前存在，归零后销毁。**

**窗口属性：**
```cpp
setWindowFlags(
    Qt::Window |
    Qt::FramelessWindowHint |
    Qt::WindowStaysOnTopHint |
    Qt::Tool                     // 不在任务栏显示
);
setAttribute(Qt::WA_TranslucentBackground);
setAttribute(Qt::WA_TransparentForMouseEvents, false); 
// 注意：不拦截底层窗口鼠标事件，但通知窗口自身不提供任何可交互控件
```

**位置：** 屏幕中心水平居中，垂直位置在屏幕高度的 25% 处（`screen.height() * 0.25 - widgetHeight/2`）

**外观（paintEvent 绘制）：**
```
┌─────────────────────────────────┐
│  [锁SVG 24x24]    即将锁屏       │  ← 顶部标题栏，深色半透明
├─────────────────────────────────┤
│                                 │
│          还剩  12  秒            │  ← 数字48px粗体，12秒以上橙色，以下红色
│                                 │
│  ████████████████░░░░░░░░░░░░  │  ← 进度条，从满到空，颜色同步数字
└─────────────────────────────────┘
  宽度约 340px，高度约 130px，圆角12px
  背景：Acrylic毛玻璃（与LockScreenWindow同样的DWM实现方式）
```

**动画：**
- 出现：从屏幕上方 -130px 位置以 `QPropertyAnimation` 在 250ms 内滑入到目标位置，同时 opacity 从 0 渐变到 1
- 消失：opacity 从 1 渐变到 0，持续 150ms，动画结束后 `deleteLater()`，紧接着 CountdownEngine 发射 `lockActivated()`

**数据更新：**
- 连接 CountdownEngine 的 `warningTick(int)` 信号
- 每秒更新数字和进度条，数字颜色在剩余 10 秒时从橙色切换为红色，触发 `update()`

---

## 四、SystemHookManager（独立线程运行）

**必须在独立线程中创建并运行 Win32 消息循环，禁止在 Qt 主线程安装钩子：**

```cpp
class HookThread : public QThread {
    void run() override {
        // 安装两个钩子
        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
        mouseHook    = SetWindowsHookEx(WH_MOUSE_LL,    LowLevelMouseProc,    NULL, 0);
        // Win32消息循环，钩子依赖此循环分发
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UnhookWindowsHookEx(keyboardHook);
        UnhookWindowsHookEx(mouseHook);
    }
};
```

**键盘钩子必须拦截并返回1阻断（不调用CallNextHookEx）的按键：**
```
VK_LWIN / VK_RWIN          → Win键
Alt + Tab                  → 切换窗口
Alt + F4                   → 关闭窗口
Ctrl + Esc                 → 开始菜单
Ctrl + Shift + Esc         → 任务管理器
PrintScreen / VK_SNAPSHOT  → 截图
```

**关于 Ctrl+Alt+Delete：**
此组合由 Windows 内核在硬件层拦截（SAS序列），任何用户态钩子均无法阻断。对抗策略在 TopMostGuard 中实现。

**鼠标钩子：**
- 锁屏状态下捕获所有 `WM_MOUSEMOVE`、`WM_LBUTTONDOWN`、`WM_RBUTTONDOWN` 等事件
- 阻断事件传递（返回1）
- 通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 通知主线程，触发 LockScreenWindow 左下角"请勿操作"警告显示，并通知 SessionLogger 计数

---

## 五、TopMostGuard

```cpp
// 50ms 定时器，锁屏激活时启动，解锁后停止
void TopMostGuard::onTimer() {
    for (LockScreenWindow* w : lockWindows) {
        HWND hwnd = reinterpret_cast<HWND>(w->winId());
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd);
    }

    // 任务管理器对抗：枚举顶层窗口
    // 若发现 taskmgr.exe 窗口存在且位于前台，立即抢回焦点
    HWND fgWnd = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fgWnd, &pid);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        char exeName[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        QueryFullProcessImageNameA(hProc, 0, exeName, &size);
        CloseHandle(hProc);
        if (strstr(exeName, "Taskmgr.exe") || strstr(exeName, "taskmgr.exe")) {
            // 强制将主屏遮罩窗口拉回前台
            SetForegroundWindow(reinterpret_cast<HWND>(primaryLockWindow->winId()));
        }
    }
}
```

---

## 六、PowerManager

```cpp
void preventSleepAndScreenOff() {
    SetThreadExecutionState(
        ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
    );
}

void allowSleepAndScreenOff() {
    SetThreadExecutionState(ES_CONTINUOUS);
}
// 根据 AppConfig::preventSleep，在 lockActivated() 时调用前者，unlockSucceeded() 时调用后者
```

---

## 七、LockScreenWindow（遮罩主窗口）

**窗口标志：**
```cpp
setWindowFlags(
    Qt::Window |
    Qt::FramelessWindowHint |
    Qt::WindowStaysOnTopHint |
    Qt::Tool
);
setAttribute(Qt::WA_TranslucentBackground);
setAttribute(Qt::WA_DeleteOnClose, false);

// 覆盖整个对应屏幕的几何区域
setGeometry(targetScreen->geometry());
```

**Acrylic 毛玻璃实现（DWM，运行时动态加载）：**
```cpp
// 仅 Windows 10 1803 (build 17134) 以上支持
// 运行时检测版本，低版本降级为半透明纯色

struct ACCENT_POLICY {
    DWORD AccentState;   // 4 = ACCENT_ENABLE_ACRYLICBLURBEHIND
    DWORD AccentFlags;   // 2 = 绘制边框
    DWORD GradientColor; // AABBGGRR，AA由overlayOpacity映射
    DWORD AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD  Attrib;   // 19 = WCA_ACCENT_POLICY
    PVOID  pvData;
    SIZE_T cbData;
};
// 动态加载：
typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
pSetWindowCompositionAttribute SetWCA = (pSetWindowCompositionAttribute)
    GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
```

**closeEvent 必须 override 并 ignore：**
```cpp
void LockScreenWindow::closeEvent(QCloseEvent* event) {
    event->ignore(); // 禁止任何外部关闭请求
}
```

**paintEvent 图层顺序（从下到上）：**
```
1. Acrylic 毛玻璃背景（DWM提供）
2. 若有背景图：居中/铺满，叠加 overlayOpacity 透明度
3. 若 showLockIcon=true：lock.svg 居中，尺寸 120x120，白色，轻微发光阴影
4. 自定义文字（customMessage）：锁图标正下方，白色，20px
5. 当前真实时钟（HH:MM:SS）：右下角，白色半透明，16px，每秒刷新
6. "请勿操作"警告：左下角，SVG警告图标 + 白色文字，显示后2秒淡出
7. 触碰计数：左下角警告消失后，小字显示"已记录 X 次操作"
```

**"请勿操作"警告动画：**
```cpp
// 由 SystemHookManager 通过 invokeMethod 触发
void LockScreenWindow::showWarning() {
    warningVisible = true;
    warningOpacity = 1.0;
    warningTimer->start(2000); // 2秒后开始淡出
    update();
}
// 淡出用 QPropertyAnimation 将 warningOpacity 从1.0→0.0，持续400ms
```

**多屏支持：**
```cpp
QList<LockScreenWindow*> windows;
for (QScreen* screen : QGuiApplication::screens()) {
    auto* w = new LockScreenWindow(screen, config);
    w->show();
    windows.append(w);
}
// 主屏窗口负责接收 Esc 键触发 UnlockDialog
// 所有窗口统一由 TopMostGuard 管理置顶
```

---

## 八、UnlockDialog

```cpp
setWindowFlags(
    Qt::Dialog |
    Qt::FramelessWindowHint |
    Qt::WindowStaysOnTopHint
);
// 显示在主屏幕中心
```

**界面布局：**
```
┌──────────────────────────────┐
│  [lock SVG 32x32]            │
│       输入密码以解锁          │
│  ┌────────────────────┐[👁]  │  ← 密码框+显示/隐藏SVG按钮
│  └────────────────────┘      │
│  [错误提示文字，红色]         │
│                              │
│  [取消(ESC)]     [解锁]      │
└──────────────────────────────┘
```

**密码错误动画：**
```cpp
// QPropertyAnimation 对输入框做水平位移抖动
// x偏移序列：0 → +8 → -8 → +6 → -6 → +4 → -4 → 0，总时长300ms
```

**超限锁定状态：**
- 输入框和解锁按钮全部 `setEnabled(false)`
- 显示"已锁定，请等待 XX 秒"，每秒倒计更新
- 倒计结束后恢复可输入状态，错误次数重置

---

## 九、SetupDialog（启动界面）

```
┌──────────────────────────────────────────┐
│  [shield SVG]  倒计时锁屏                  │
├──────────────────────────────────────────┤
│                                          │
│  倒计时时长：  [▼ 45 ▲] 分钟             │  ← QSpinBox，1~480
│                                          │
│  预设：[番茄 25min] [课堂 45min] [自定义 60min] │  ← 点击自动填充SpinBox
│                                          │
│  密码：   [________________] [eye SVG]   │
│  确认密码：[________________] [eye SVG]   │
│  ☐ 记住密码                              │
│                                          │
├──────────────────────────────────────────┤
│  [设置 settings SVG]       [开始锁屏 →]  │
└──────────────────────────────────────────┘
```

**逻辑细节：**
- `rememberPassword=true` 且已存密码哈希时：密码栏显示 placeholder "已保存密码"，用户可直接点"开始锁屏"（跳过重新输入），也可输入新密码覆盖
- 开始前校验：密码不能为空，两次输入必须一致，长度4~32位
- 点击"设置"按钮弹出 SettingsPanel（`QDialog::exec()`）

---

## 十、SettingsPanel

```
遮罩外观
  透明度：  [━━━━━━|━━━━━━] 80%   ← QSlider 0~100，实时预览
  背景图：  [选择图片(folder SVG)] [清除]  当前：无
  ☑ 显示锁图标
  自定义文字：[专注中，请勿打扰_________]

行为设置
  ☑ 阻止系统息屏/睡眠
  ☑ 解锁后自动开始下一轮
  ☐ 开机自启动

安全设置
  密码错误上限：[▼ 5 ▲] 次
  超限锁定时长：[▼ 30 ▲] 秒

数据
  [查看锁屏记录 (log SVG)]
  [清空所有记录]

              [取消]  [保存]
```

---

## 十一、TrayManager

**托盘图标**：`SvgIcon::get(Lock, {16,16}, Qt::white)` 渲染为 QPixmap，锁屏状态下叠加红色小圆点角标。

**右键菜单：**
```
剩余时间：43分12秒        ← QAction，禁用状态（灰色，仅显示）
─────────────────────
紧急解锁...               ← 弹出UnlockDialog
暂停倒计时 / 继续倒计时
─────────────────────
退出                      ← 锁屏激活中时此项置灰禁用
```

**锁屏激活时双击托盘图标**：无响应（屏蔽双击事件）

---

## 十二、SessionLogger

```cpp
struct SessionRecord {
    QDateTime startTime;
    QDateTime endTime;
    int       durationSeconds;
    int       touchCount;      // 用户触碰次数（鼠标/键盘）
    bool      manualUnlock;    // true=手动密码解锁，false=倒计时自然结束
};

// 存储格式：JSON Lines，每条记录占一行
// 路径：%APPDATA%\CountdownLock\sessions.jsonl
// 示例：
// {"start":"2025-03-01T09:00:00","end":"2025-03-01T09:45:00","duration":2700,"touches":3,"manual":false}

void append(const SessionRecord& record);
QList<SessionRecord> readAll();
void clearAll();
```

**日志查看对话框**（从 SettingsPanel 打开）：
```
QDialog + QTableWidget，列：
日期 | 开始时间 | 结束时间 | 持续时长 | 触碰次数 | 解锁方式
```

---

## 十三、SvgIcon 工具类

**所有图标内联定义，禁止任何外部图片文件依赖：**

```cpp
class SvgIcon {
public:
    enum IconType {
        Lock, Unlock, Eye, EyeOff,
        Warning, Settings, Clock,
        Shield, Log, Close, Play, Pause,
        Folder, Trash
    };

    // 动态替换颜色后用 QSvgRenderer 渲染为 QPixmap
    static QPixmap get(IconType type, QSize size, QColor color = Qt::white);

private:
    // SVG模板字符串，颜色占位符统一用 {{COLOR}}
    // 风格：Feather Icons（stroke风格，strokeWidth=2，无fill）
    static const QHash<IconType, QString>& templates();
};

// 实现示例（Lock图标）：
// <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"
//      fill="none" stroke="{{COLOR}}" stroke-width="2"
//      stroke-linecap="round" stroke-linejoin="round">
//   <rect x="3" y="11" width="18" height="11" rx="2" ry="2"/>
//   <path d="M7 11V7a5 5 0 0 1 10 0v4"/>
// </svg>
```

每个 IconType 都需提供对应完整 SVG path 数据（参考 Feather Icons 规范）。

---

## 十四、关键约束与细节

```
[线程安全]
- SystemHookManager 的钩子回调在 HookThread 中执行
- 所有跨线程通信必须使用 QMetaObject::invokeMethod(..., Qt::QueuedConnection)
  或 Qt::BlockingQueuedConnection，禁止直接调用 UI 方法

[稳定性]
- LockScreenWindow::closeEvent 必须 override 并 ignore()
- 程序退出前必须 UnhookWindowsHookEx，防止系统钩子泄漏
- TopMostGuard 仅在 State::Locked 时运行

[Acrylic 兼容]
- 运行时检测 Windows 版本（VerifyVersionInfo 或 RtlGetVersion）
- Windows 10 1803 (build 17134) 以上：启用 Acrylic
- 低版本：降级为 QColor(0,0,0, overlayOpacity * 2.55) 半透明纯色背景

[密码]
- 最短4位，最长32位，校验在 SetupDialog 的"开始锁屏"按钮点击时执行
- 哈希：QCryptographicHash::hash(pwd.toUtf8(), QCryptographicHash::Sha256).toHex()

[开机自启]
- 写入 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
- 键名：CountdownLock，值：QCoreApplication::applicationFilePath()（带引号）

[构建]
- CMakeLists.txt：WIN32 入口（无控制台），链接 Qt6::Widgets Qt6::Svg dwmapi user32 kernel32
- MSVC 编译选项加 /utf-8，确保中文字符编译正确
- 最终产物为单一 .exe，无安装程序

[配置与日志路径]
- 统一使用 QStandardPaths::AppDataLocation 获取路径
- 首次运行自动创建目录
```

---

## 交付要求

1. 输出所有 `.h` 和 `.cpp` 文件的完整可编译代码
2. 输出完整 `CMakeLists.txt`
3. 每个文件顶部注明完整相对路径注释（如 `// src/core/ConfigManager.h`）
4. 所有关键逻辑段落附有中文行内注释
5. 不引入任何第三方库，仅使用 Qt 6 + Win32 API
6. 全程无 Emoji，SVG 图标全部以内联字符串形式定义在 SvgIcon 类中