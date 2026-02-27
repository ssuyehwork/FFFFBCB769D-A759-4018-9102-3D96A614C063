## 最终确认总结

| 决策点 | 方案 |
|---|---|
| 平台 | Windows Only，充分使用 Win32 API |
| 密码存储 | SHA-256 哈希，QSettings 本地持久化 |
| 息屏/睡眠 | 用户可勾选，`SetThreadExecutionState` 控制 |
| 遮罩风格 | DWM Acrylic 毛玻璃 + 自定义透明度 |

---

以下是完整实现提示词：

---

# Prompt

## 项目概述

使用 **C++ + Qt 6**（编译器环境：Qt Creator 18.0.2 Community，编译套件 MSVC 或 MinGW for Windows）开发一款 **Windows 专用倒计时锁屏桌面应用**。应用核心目标：在倒计时结束后，以极强的系统级遮罩完全封锁用户对电脑的一切操作，直到倒计时自然结束或用户输入正确密码解锁。全程禁止使用 Emoji，所有图标必须使用内联 SVG。

---

## 技术栈与依赖

```
Qt 6.x (Widgets + Gui + Core)
Win32 API (windows.h, dwmapi.h, winuser.h)
链接库：dwmapi.lib, user32.lib, kernel32.lib
构建系统：CMake 或 qmake 均可，推荐 CMake
```

---

## 架构分层（严格按此分层实现，每个类单独 .h/.cpp 文件）

```
src/
├── main.cpp
├── core/
│   ├── ConfigManager        # QSettings封装，密码SHA256哈希，所有配置读写
│   ├── CountdownEngine      # QTimer状态机，管理倒计时生命周期，发射信号
│   └── SessionLogger        # 本地日志：记录每次锁屏起止时间、被触碰次数
├── system/
│   ├── SystemHookManager    # Win32低级键鼠钩子，拦截危险组合键
│   ├── TopMostGuard         # 50ms定时器周期强制置顶所有遮罩窗口
│   └── PowerManager         # SetThreadExecutionState控制息屏/睡眠
├── ui/
│   ├── SetupDialog          # 启动配置界面
│   ├── SettingsPanel        # "设置"子面板（透明度/图片/图标/息屏/日志）
│   ├── LockScreenWindow     # 遮罩主窗口（每个屏幕一个实例）
│   ├── UnlockDialog         # ESC触发解锁弹层
│   └── TrayManager          # 系统托盘
└── utils/
    └── SvgIcon              # 统一SVG图标注册与渲染工具类
```

---

## 一、ConfigManager

**职责**：所有配置的读写中枢。

```cpp
// 需实现的配置项：
struct AppConfig {
    int     countdownMinutes;      // 倒计时分钟数，默认45
    bool    rememberPassword;      // 是否记住密码
    QString passwordHash;          // SHA-256哈希后的密码（空字符串=未设置）
    int     overlayOpacity;        // 遮罩不透明度 0~100，默认80
    QString backgroundImagePath;   // 背景图路径，空=纯色
    bool    showLockIcon;          // 是否显示锁SVG图标
    QString customMessage;         // 自定义锁屏文字，默认"专注中，请勿打扰"
    bool    preventSleep;          // 阻止系统息屏/睡眠
    bool    autoRestartAfterUnlock;// 解锁后自动重启下一轮
    bool    launchOnStartup;       // 开机自启
    int     maxPasswordAttempts;   // 最大密码错误次数，默认5
    int     lockoutDurationSecs;   // 超过错误次数后的惩罚锁定秒数，默认30
    QStringList presetTemplates;   // 预设时间模板名称列表
    QList<int>  presetMinutes;     // 预设时间模板对应分钟数
};

// 密码处理：
QString hashPassword(const QString& raw); // QCryptographicHash::Sha256
bool    verifyPassword(const QString& raw, const QString& hash);
```

---

## 二、CountdownEngine（状态机）

状态定义：
```
Idle → Counting → LastWarning（最后20秒）→ Locking → Locked → Unlocking → Idle
```

关键信号：
```cpp
signals:
    void stateChanged(State newState);
    void tickSecond(int remainingSeconds);   // 每秒发射
    void warningPhaseStarted();              // 进入最后20秒
    void lockActivated();                    // 触发遮罩
    void unlockSucceeded();
    void unlockFailed(int attemptsLeft);
    void lockedOut(int lockoutSeconds);      // 密码错误次数超限
```

---

## 三、SystemHookManager（核心安全层）

使用 `SetWindowsHookEx(WH_KEYBOARD_LL, ...)` 和 `SetWindowsHookEx(WH_MOUSE_LL, ...)`。

**必须拦截的按键（返回1阻断，不传递）**：
```
Win键（VK_LWIN / VK_RWIN）
Alt + Tab
Alt + F4
Ctrl + Esc
Ctrl + Alt + Delete  ← 此组合受系统保护，钩子无法完全拦截，
                        但可通过持续抢焦点+置顶对抗任务管理器
Ctrl + Shift + Esc  （任务管理器）
PrintScreen
```

**鼠标钩子**：锁屏期间，捕获所有鼠标移动/点击事件，不传递，并通过信号通知 LockScreenWindow 在左下角短暂显示"请勿操作"警告，同时通知 SessionLogger 计数。

**任务管理器对抗**：
```cpp
// 在TopMostGuard的定时器回调中：
// 1. 枚举顶层窗口，若发现 taskmgr.exe 的窗口句柄位于最顶层，
//    立即对所有 LockScreenWindow 调用 SetWindowPos(HWND_TOPMOST)
// 2. 同时调用 SetForegroundWindow 强制抢回焦点
```

---

## 四、PowerManager

```cpp
void preventSleepAndScreenOff();   // SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)
void allowSleepAndScreenOff();     // SetThreadExecutionState(ES_CONTINUOUS)
```
根据 `AppConfig::preventSleep` 在锁屏激活/解锁时调用。

---

## 五、LockScreenWindow（遮罩主窗口）

**窗口标志**（缺一不可）：
```cpp
setWindowFlags(
    Qt::Window |
    Qt::FramelessWindowHint |
    Qt::WindowStaysOnTopHint |
    Qt::Tool                    // 不在任务栏显示
);
setAttribute(Qt::WA_TranslucentBackground);
setAttribute(Qt::WA_DeleteOnClose, false);
```

**Win32 置顶**（在 `showEvent` 及 TopMostGuard 中调用）：
```cpp
HWND hwnd = reinterpret_cast<HWND>(winId());
SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
SetForegroundWindow(hwnd);
```

**Acrylic 毛玻璃效果**（DWM）：
```cpp
// 使用 DwmSetWindowAttribute + ACCENT_POLICY 实现 Acrylic
struct ACCENT_POLICY {
    int AccentState;    // ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
    int AccentFlags;
    int GradientColor;  // AABBGGRR，AA控制透明度
    int AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;         // WCA_ACCENT_POLICY = 19
    PVOID pvData;
    SIZE_T cbData;
};
// 动态加载 user32.dll 的 SetWindowCompositionAttribute
```
透明度由 `AppConfig::overlayOpacity` 映射到 `GradientColor` 的 Alpha 通道。

**遮罩内容渲染（paintEvent）**：
```
图层顺序（从下到上）：
1. Acrylic 毛玻璃背景（由DWM提供）
2. 若设置了背景图：居中/铺满绘制，叠加透明度
3. 若开启锁SVG图标：居中绘制 lock.svg（尺寸 120x120，白色）
4. 自定义文字：居中偏下，白色，字体 20px
5. 当前真实时钟：右下角，白色，字体 16px（每秒刷新）
6. "请勿操作"警告：左下角，显示2秒后淡出，SVG警告图标+文字
7. 最后20秒倒计时：顶部居中，大字体60px红色，QToolTip同步显示
8. 密码错误/锁定提示：居中弹出，动画显示后自动消失
```

**多屏支持**：
```cpp
// 在 LockScreenWindow 激活时：
for (QScreen* screen : QGuiApplication::screens()) {
    LockScreenWindow* w = new LockScreenWindow(screen->geometry(), config);
    w->show();
    guardList.append(w);
}
```
主屏窗口接收焦点，副屏窗口仅作遮挡。

---

## 六、UnlockDialog

- 仅在用户按下 `Esc` 键时，由主屏 `LockScreenWindow` 创建并显示
- 本身也是置顶窗口，`Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint`
- 包含：SVG锁图标、密码输入框（带显示/隐藏密码切换按钮，用 SVG eye/eye-off 图标）
- 密码错误：输入框红色抖动动画（`QPropertyAnimation` 水平位移）
- 错误次数达到上限：显示"已锁定 X 秒"倒计时，禁用输入框
- 正确解锁：发射信号给 CountdownEngine，关闭所有遮罩

---

## 七、SetupDialog（启动界面）

布局要求：
```
┌─────────────────────────────────┐
│  [SVG应用图标]  倒计时锁屏       │
├─────────────────────────────────┤
│  倒计时时长：[__45__] 分钟       │
│  预设模板：[番茄25] [课堂45] [自定义60] │
│                                  │
│  密码：[__________] [👁SVG]      │
│  确认：[__________]              │
│  ☑ 记住密码                      │
│                                  │
│  [设置 ⚙SVG]           [开始锁屏]│
└─────────────────────────────────┘
```

- 若 `rememberPassword=true` 且已有密码哈希，密码栏显示"已保存"占位符，用户可直接点"开始锁屏"或重新输入新密码覆盖
- 时间输入框：`QSpinBox`，范围 1~480 分钟
- 点击预设模板按钮自动填充 SpinBox
- "开始锁屏"前验证：密码不能为空，两次输入必须一致

---

## 八、SettingsPanel（设置子面板，从SetupDialog弹出）

```
遮罩外观
  透明度：[====|====] 80%  （QSlider 0~100）
  背景图：[选择图片] [清除]  当前：xxx.jpg
  ☑ 显示锁图标（SVG）
  自定义文字：[专注中，请勿打扰_____]

行为设置
  ☑ 阻止系统息屏/睡眠
  ☑ 解锁后自动开始下一轮
  ☑ 开机自启动

安全设置
  最大密码错误次数：[5] 次
  错误超限后锁定：[30] 秒

历史记录
  [查看锁屏日志]  （弹出简单列表，显示日期/时长/触碰次数）
```

---

## 九、TrayManager（系统托盘）

托盘图标：SVG锁图标渲染为 16x16 QPixmap。

右键菜单：
```
剩余时间：XX分XX秒  （灰色，不可点击）
──────────────
紧急解锁...        （弹出UnlockDialog）
暂停/继续倒计时
──────────────
退出               （需先解锁或未处于锁屏状态）
```

倒计时进行时，托盘图标上叠加小红点角标（`QPainter` 绘制）。

---

## 十、SessionLogger

```cpp
struct SessionRecord {
    QDateTime startTime;
    QDateTime endTime;
    int       touchCount;    // 被触碰次数
    bool      manualUnlock;  // true=手动解锁，false=倒计时结束
};
// 以 JSON Lines 格式追加写入 AppData/Local/CountdownLock/sessions.jsonl
```

---

## 十一、SvgIcon 工具类

**所有图标均以内联 SVG 字符串定义在此类中，禁止外部图片文件依赖**：

```cpp
class SvgIcon {
public:
    // 返回指定颜色和尺寸的QPixmap
    static QPixmap get(IconType type, QSize size, QColor color = Qt::white);

    enum IconType {
        Lock,           // 锁
        Unlock,         // 解锁
        Eye,            // 显示密码
        EyeOff,         // 隐藏密码
        Warning,        // 警告（请勿操作）
        Settings,       // 设置齿轮
        Clock,          // 时钟
        Shield,         // 应用主图标
        Tray,           // 托盘图标
        Log,            // 日志
        Close,          // 关闭
        Play,           // 开始/继续
        Pause,          // 暂停
    };

private:
    static const QMap<IconType, QString> svgTemplates; // SVG模板字符串，含{color}占位符
};
```

每个 SVG 模板需使用标准 Material Design 或 Feather Icons 风格的路径数据，颜色使用 `fill="{color}"` 或 `stroke="{color}"` 占位符，由 `get()` 方法动态替换后通过 `QSvgRenderer` 渲染为 `QPixmap`。

---

## 十二、关键细节与约束清单

```
[安全]
- SetWindowsHookEx 钩子必须在独立线程的消息循环中运行（GetMessage循环），
  不能在Qt主线程，避免阻塞UI
- 钩子线程与Qt线程通过 PostMessage 或 QMetaObject::invokeMethod 通信
- 程序退出时必须 UnhookWindowsHookEx，否则系统钩子泄漏

[稳定性]  
- LockScreenWindow 的 closeEvent 必须被 override 并 ignore()，
  防止任何外部关闭请求
- TopMostGuard 定时器间隔50ms，在锁屏状态下运行，解锁后停止

[Acrylic兼容性]
- Acrylic 效果仅在 Windows 10 1803+ 可用
- 需在运行时检测 Windows 版本，低版本降级为半透明纯色背景

[最后20秒警告]
- QToolTip::showText() 显示在遮罩窗口上方，文字格式：
  "还剩 X 秒，即将结束锁定"
- 同时在遮罩中央顶部以60px红色粗体数字显示倒计秒数

[密码]
- 密码最短4位，最长32位
- 哈希存储：QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex()

[开机自启]
- 写入注册表 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
- 值名称：CountdownLock，值：程序完整路径

[多屏]
- 主屏（QGuiApplication::primaryScreen()）的 LockScreenWindow 负责
  接收键盘事件（Esc解锁、钩子通信）
- 所有屏幕的窗口在 TopMostGuard 中统一置顶

[日志查看]
- 简单 QDialog + QTableWidget 展示 sessions.jsonl 内容
- 列：日期、开始时间、结束时间、持续时长、触碰次数、解锁方式

[构建配置 CMakeLists.txt]
- WIN32 应用（无控制台窗口）
- 链接：Qt6::Widgets Qt6::Svg dwmapi user32 kernel32
- MSVC: /utf-8 编译选项确保中文字符正确
```

---

## 交付要求

1. 输出完整可编译的所有 `.h` 和 `.cpp` 文件内容
2. 输出完整 `CMakeLists.txt`
3. 每个文件顶部注明文件路径注释
4. 代码内关键逻辑必须有中文行内注释
5. 不使用第三方库，仅 Qt 6 + Win32 API
6. 不使用 Emoji，SVG 图标全部内联
7. 编译后为单一 `.exe`，无需安装，配置文件写入 `%APPDATA%\CountdownLock\`