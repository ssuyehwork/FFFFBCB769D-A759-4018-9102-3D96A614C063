C++ Win32 倒计时锁屏软件 — 完整实现提示词

你是一名精通 Windows 底层开发的 C++ 专家。请使用纯 Win32 API（不使用 MFC/Qt/ATL）
实现一款名为 "FocusLock" 的倒计时锁屏软件。以下是完整规格说明，请严格按照每一条实现。

═══════════════════════════════════════════════
【一、工程结构】
═══════════════════════════════════════════════

单一 .cpp 源文件可拆分为以下逻辑模块（同一工程内多文件亦可）：

  main.cpp          — 入口、消息循环
  ui_setup.cpp      — 设置/密码窗口
  ui_overlay.cpp    — 遮罩锁屏窗口（核心）
  ui_unlock.cpp     — 解锁输入窗口
  overlay_engine.cpp— 遮罩强化逻辑（Hook、置顶、多显示器）
  security.cpp      — 密码 DPAPI 加密/解密/存储
  config.cpp        — INI 配置读写
  svg_icons.cpp     — 内嵌 SVG 渲染（GDI+ 或 Direct2D）
  timer_engine.cpp  — 倒计时核心逻辑
  tray.cpp          — 系统托盘

编译目标：Windows x64，编译器 MSVC 2022，链接库：
  user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib
  dwmapi.lib ole32.lib comctl32.lib winmm.lib

清单文件（app.manifest）中需声明：
  - requestedExecutionLevel: requireAdministrator（以便 Hook 全局生效）
  - 启用 Visual Styles（comctl32 v6）

═══════════════════════════════════════════════
【二、主窗口：设置 & 密码界面（SetupDialog）】
═══════════════════════════════════════════════

2.1 窗口属性
  - 居中弹出，尺寸 480×580，不可缩放
  - 标题栏文字："FocusLock"
  - 使用自定义非客户区绘制（WM_NCPAINT/DWM）实现圆角窗口（CornerRadius=12）
  - 背景色：#1E1E2E，文字色：#CDD6F4

2.2 控件布局（从上到下）

  ① SVG 锁图标（48×48）居中显示于顶部，内嵌如下 SVG 路径：
     Lock closed icon：M12 1C8.676 1 6 3.676 6 7v1H3v15h18V8h-3V7c0-3.324-2.676-6-6-6z
     M12 3c2.276 0 4 1.724 4 4v1H8V7c0-2.276 1.724-4 4-4z
     M13 14.723V17h-2v-2.277A2 2 0 1 1 13 14.723z
     颜色：#89B4FA，使用 GDI+ GraphicsPath 渲染

  ② 标题文字："设置专注时长" — 字体 Segoe UI 16pt Bold，色 #CDD6F4

  ③ 时间输入区
     - 标签："专注时长（分钟）"，Segoe UI 10pt，色 #A6ADC8
     - 单行 Edit 控件，默认值 "45"，宽 120px，仅允许输入数字（WM_CHAR 过滤）
     - Edit 左侧：SVG 时钟图标（16×16）：
       circle cx=12 cy=12 r=10 / path d="M12 6v6l4 2"（Feather icons 风格）
     - Edit 右侧：SVG 加/减按钮（UpDown 控件替换为自绘 +/- 按钮）
     - 范围限制：1 ~ 480 分钟

  ④ 密码区
     - 标签："解锁密码"
     - 密码 Edit（ES_PASSWORD），右侧有 SVG 眼睛图标按钮（切换明文/密文）
       Eye icon path：M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z
       circle cx=12 cy=12 r=3
     - 确认密码 Edit，同上
     - 复选框："记住密码"（勾选后将密码用 DPAPI 存储至 AppData/FocusLock/config.dat）

  ⑤ 快捷锁定键
     - 标签："快捷锁定键"
     - Hotkey 控件（HOTKEY_CLASS），默认 Ctrl+Shift+L
     - 程序启动后通过 RegisterHotKey() 注册全局热键

  ⑥ "设置" 按钮（齿轮 SVG 图标 + 文字）
     点击弹出子设置面板（见【三】）

  ⑦ "开始专注" 按钮
     - 圆角矩形按钮，背景 #89B4FA，文字 "开始专注"，宽 200px，高 44px
     - 点击后校验密码一致性，通过则启动遮罩并关闭设置窗口

2.3 倒计时前 20 秒 Tooltip 提示
  - 使用 CreateWindowEx(0, TOOLTIPS_CLASS...) 创建 Tooltip 窗口
  - 在系统托盘图标区域显示气泡 Tooltip："还有 XX 秒解锁"
  - 每秒更新一次，倒计时归零后自动销毁

═══════════════════════════════════════════════
【三、子设置面板（SettingsPanel）】
═══════════════════════════════════════════════

以模态对话框形式弹出，尺寸 400×420，同主窗口风格。

包含以下选项（均带 SVG 图标 + 标签 + 控件）：

  ① 遮罩透明度
     - SVG 图标：layers icon
     - 滑块（TrackBar），范围 10~95（%），默认 85
     - 实时预览：滑动时更新遮罩窗口 SetLayeredWindowAttributes alpha 值

  ② 遮罩背景色
     - SVG 调色板图标
     - 自定义颜色选择按钮，点击弹出 ChooseColor()
     - 默认 #000000

  ③ 显示背景图片
     - 复选框 + SVG 图片图标
     - 勾选后弹出 GetOpenFileName() 选择 PNG/JPG/BMP
     - 图片路径存储在 config.ini 中
     - 锁屏时将图片铺满（居中裁剪 / 拉伸，下方有单选按钮切换）

  ④ 显示锁图标
     - 复选框，默认勾选
     - 勾选后在遮罩中央显示大号锁 SVG（128×128）

  ⑤ 锁屏提示文字
     - Edit 控件，默认 "专注时间，请勿打扰"
     - 显示在遮罩中央锁图标下方，Segoe UI 18pt，色 #CDD6F4，带阴影

  ⑥ 倒计时结束行为
     - 单选按钮组：
       ○ 自动解锁（无需密码）
       ○ 保持锁定（需输入密码解锁）
       ○ 播放提示音（Windows Beep 或 .wav 文件）

  ⑦ 错误密码冷却
     - 标签："连续错误 N 次后冷却 M 秒"
     - 两个 Spinner（UpDown）：N 默认 3，M 默认 30
     - 冷却期间解锁按钮置灰，显示倒计时

  ⑧ "保存" 按钮 — 保存所有设置至 config.ini

═══════════════════════════════════════════════
【四、遮罩锁屏窗口（OverlayWindow）— 核心】
═══════════════════════════════════════════════

4.1 窗口创建参数（必须精确）

  CreateWindowEx(
    WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
    overlayClass,
    NULL,
    WS_POPUP,
    0, 0, virtualScreenWidth, virtualScreenHeight,  // 覆盖所有显示器
    NULL, NULL, hInstance, NULL
  );

  virtualScreenWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN)
  virtualScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN)
  起始坐标使用 SM_XVIRTUALSCREEN / SM_YVIRTUALSCREEN

4.2 遮罩强化策略（按优先级叠加实现）

  ① 持续置顶线程：
     启动独立线程，每 50ms 执行一次：
       SetWindowPos(hwndOverlay, HWND_TOPMOST, 0,0,0,0,
                    SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)
     确保任务管理器弹出后遮罩仍在最顶层

  ② Low-Level 键盘钩子（全局）：
     SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0)
     在回调中拦截并返回 1（吞噬）以下按键：
       - VK_LWIN / VK_RWIN（Windows 键）
       - VK_TAB + ALT（Alt+Tab）
       - VK_ESCAPE（保留，用于触发解锁界面，不吞噬）
       - VK_F4 + ALT（Alt+F4）
       - VK_DELETE + CTRL + ALT（Ctrl+Alt+Del 无法拦截，属系统级）
       - VK_ESCAPE + CTRL + SHIFT（任务管理器快捷键）
       - VK_F1~F12（可选，按需吞噬）
       - 所有字母/数字键（遮罩期间键盘完全失效）

  ③ Low-Level 鼠标钩子：
     SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, NULL, 0)
     拦截所有鼠标消息：WM_LBUTTONDOWN/UP/DBLCLK、WM_RBUTTONDOWN/UP、
     WM_MBUTTONDOWN、WM_MOUSEWHEEL、WM_MOUSEMOVE
     — 全部吞噬，仅当 Esc 被按下进入解锁态时临时解除鼠标拦截

  ④ 阻止系统休眠与屏保：
     SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)
     在锁屏期间持续调用

  ⑤ 进程优先级提升：
     SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)
     SetThreadPriority(hTopMostThread, THREAD_PRIORITY_TIME_CRITICAL)

  ⑥ 窗口焦点守护：
     处理 WM_ACTIVATE / WM_KILLFOCUS：
       若窗口失去焦点，立即 SetForegroundWindow(hwndOverlay)
     处理 WM_SYSCOMMAND：拦截 SC_MINIMIZE / SC_CLOSE / SC_MOVE

4.3 遮罩绘制（WM_PAINT）

  使用 GDI+ 在内存 DC 中绘制，然后 BitBlt 到屏幕：

  ① 背景层：
     - 若未设置背景图：填充 RGBA(0,0,0, alpha)（alpha 由透明度设置决定）
     - 若设置了背景图：加载图片（GDI+ Image），居中裁剪绘制，再叠加半透明黑色蒙版

  ② 中央锁图标（如配置启用）：
     128×128 SVG 路径，颜色 #89B4FA，带 drop-shadow 效果
     使用 GDI+ GraphicsPath 渲染

  ③ 提示文字：
     锁图标正下方 20px，居中对齐
     Segoe UI 18pt，色 #CDD6F4，GDI+ 文字阴影（偏移 2px，色 #000000 80%透明）

  ④ 倒计时显示：
     屏幕正中上方 1/3 处，Segoe UI 72pt Bold，色 #FFFFFF
     格式："MM:SS"，每秒重绘

  ⑤ 左下角"请勿操作"提示：
     当检测到用户有操作意图时（鼠标移动 / 按键被拦截）显示，3秒后淡出
     文字："请勿操作"，字体 Segoe UI 14pt，背景圆角矩形 #FF5555 60%透明
     SVG 警告图标（16×16）前缀：
       path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"
       line x1=12 y1=9 x2=12 y2=13 / line x1=12 y1=17 x2=12.01 y2=17

4.4 多显示器支持

  使用 EnumDisplayMonitors() 枚举所有显示器，
  为每个显示器创建一个独立的 OverlayWindow，坐标对齐各自 MONITORINFO.rcMonitor，
  所有遮罩窗口共享同一个倒计时状态。

═══════════════════════════════════════════════
【五、解锁界面（UnlockDialog）】
═══════════════════════════════════════════════

5.1 触发方式
  仅当 Esc 键被按下时触发（在键盘钩子中对 VK_ESCAPE 不吞噬，
  而是 PostMessage 给主线程创建解锁窗口）

5.2 窗口属性
  - 居中于主显示器，尺寸 360×260，圆角 16px
  - WS_EX_TOPMOST | WS_EX_LAYERED
  - 背景 #1E1E2E 85% 不透明（叠加在遮罩之上）
  - 出现时临时解除鼠标拦截（仅允许在该窗口范围内点击）

5.3 控件
  ① 顶部：SVG 锁图标（32×32，色 #89B4FA）+ 文字 "输入密码解锁"
  ② 密码 Edit（ES_PASSWORD），宽 280px，带边框 #45475A，焦点时 #89B4FA
  ③ 倒计时标签（若在冷却期）："冷却中，请等待 XX 秒"
  ④ "解锁" 按钮（SVG 解锁图标 + 文字，色 #A6E3A1）
  ⑤ 错误提示标签（初始隐藏）："密码错误，还剩 N 次机会"，色 #F38BA8

5.4 验证逻辑
  - 取 Edit 输入 → SHA-256 Hash → 与 DPAPI 解密后存储的 Hash 比对
  - 匹配：销毁所有遮罩窗口，卸载 Hook，恢复正常状态，显示托盘通知"专注完成！"
  - 不匹配：
    · 错误计数 +1，显示剩余次数
    · 密码 Edit 抖动动画（水平位移 ±8px，共 4 次，16ms 间隔，用 SetTimer 实现）
    · 达到错误上限：进入冷却，按钮置灰，冷却结束前禁止输入
  - 倒计时已归零且配置为"自动解锁"：跳过密码校验直接解锁

═══════════════════════════════════════════════
【六、系统托盘（TrayIcon）】
═══════════════════════════════════════════════

6.1 托盘图标
  - 使用 Shell_NotifyIcon() 注册托盘图标
  - 图标为 16×16 锁形 SVG 渲染为 HICON（GDI+ 渲染到 HBITMAP，转 HICON）
  - 锁屏状态下托盘图标变色（锁定态：#F38BA8，解锁态：#A6E3A1）

6.2 托盘右键菜单
  菜单项（带 SVG 小图标，使用 SetMenuItemBitmaps）：
  ┌──────────────────────────┐
  │ [锁图标]  剩余时间：MM:SS │（灰色不可点，仅展示）
  ├──────────────────────────┤
  │ [时钟图标] 立即锁定       │
  │ [暂停图标] 暂停倒计时     │（锁屏期间不可用）
  │ [齿轮图标] 设置           │（锁屏期间不可用）
  ├──────────────────────────┤
  │ [统计图标] 今日专注记录   │
  │ [退出图标] 退出程序       │（仅未锁屏时可用）
  └──────────────────────────┘

6.3 气泡通知
  倒计时结束时：Shell_NotifyIcon NIM_MODIFY，szInfoTitle="专注完成"，
                szInfo="你的专注时间已结束，继续保持！"，dwInfoFlags=NIIF_INFO

═══════════════════════════════════════════════
【七、密码安全模块（SecurityModule）】
═══════════════════════════════════════════════

7.1 密码 Hash 存储流程（设置时）
  ① 取用户输入明文密码（UTF-16）
  ② SHA-256 计算（使用 Windows CNG：BCryptOpenAlgorithmProvider + BCryptHash）
  ③ 将 32 字节 Hash 作为明文，调用 CryptProtectData()（DPAPI）加密
  ④ 将加密 blob 的 Base64 编码写入：
     %APPDATA%\FocusLock\credentials.dat

7.2 密码验证流程（解锁时）
  ① 从 credentials.dat 读取 Base64 → 解码 → 调用 CryptUnprotectData() 解密
  ② 得到存储的 SHA-256 Hash
  ③ 对用户输入计算 SHA-256，与存储 Hash 进行 constant-time 比较
     （使用 memcmp 替换为逐字节 XOR 累加，避免 timing attack）

7.3 配置存储
  所有非敏感配置存储在 %APPDATA%\FocusLock\config.ini，格式：
  [General]
  DefaultMinutes=45
  RememberPassword=1
  HotkeyVK=76
  HotkeyMod=6
  [Overlay]
  Alpha=85
  BackgroundColor=0x000000
  ShowLockIcon=1
  BackgroundImage=
  ImageFitMode=0
  PromptText=专注时间，请勿打扰
  [Security]
  MaxAttempts=3
  CooldownSeconds=30
  [Behavior]
  EndAction=0

═══════════════════════════════════════════════
【八、倒计时引擎（TimerEngine）】
═══════════════════════════════════════════════

  - 使用 timeSetEvent()（WinMM 多媒体定时器）代替 SetTimer，精度 1ms
  - 每 1000ms 触发回调，totalSecondsRemaining--，PostMessage 给遮罩窗口刷新显示
  - 倒计时归零时：PostMessage WM_TIMER_DONE 自定义消息
  - 支持暂停/恢复（timeKillEvent + timeSetEvent）
  - 锁屏期间持续记录"今日已专注 X 分钟"累计值，写入 config.ini [Stats] 节

═══════════════════════════════════════════════
【九、SVG 图标渲染工具函数】
═══════════════════════════════════════════════

实现以下辅助函数用于将 SVG 路径字符串渲染为 GDI+ 位图：

  // 将 SVG path d 属性字符串解析为 GDI+ GraphicsPath
  // 支持 M/L/C/Z/A 命令（实现最小子集覆盖本项目所需图标）
  Gdiplus::GraphicsPath* ParseSvgPath(const wchar_t* svgPathD);

  // 渲染 SVG 路径到指定大小的 HBITMAP（带 alpha 通道）
  HBITMAP RenderSvgIcon(const wchar_t* svgPathD, int width, int height,
                        Gdiplus::Color fillColor, Gdiplus::Color bgColor);

  // 将 HBITMAP 转为 HICON（用于托盘）
  HICON BitmapToIcon(HBITMAP hbmp);

所有内置 SVG 路径定义为全局 constexpr wchar_t* 常量，
命名规则：SVG_ICON_LOCK / SVG_ICON_CLOCK / SVG_ICON_EYE / 
         SVG_ICON_GEAR / SVG_ICON_WARN / SVG_ICON_EXIT 等

═══════════════════════════════════════════════
【十、今日专注统计界面（StatsWindow）】
═══════════════════════════════════════════════

从托盘菜单"今日专注记录"打开，小窗口 320×200：
  - 显示今日专注总时长（分钟）
  - 显示本次会话时长
  - 简单横向进度条（GDI 矩形）显示今日目标完成度
  - 目标默认 120 分钟，可在设置中修改

═══════════════════════════════════════════════
【十一、代码规范要求】
═══════════════════════════════════════════════

  1. 所有字符串使用 UTF-16（wchar_t / std::wstring），Win32 API 调用 W 后缀版本
  2. 资源释放：RAII 封装 HBITMAP/HICON/HFONT，析构时调用 DeleteObject
  3. 错误处理：关键 API 调用检查返回值，失败时 MessageBoxW 显示 GetLastError()
  4. 多线程：置顶线程 + 主 UI 线程 + 定时器线程，注意跨线程 PostMessage 而非 SendMessage
  5. 注释：所有函数头部有中文注释说明功能、参数、返回值
  6. 不使用 C++ 异常（noexcept）、不使用 STL 容器以外基础类型（允许 std::wstring/vector）
  7. 编译选项：/W4 /WX，消除所有警告

═══════════════════════════════════════════════
【十二、交付清单】
═══════════════════════════════════════════════

请依次输出以下内容：

  [1] 完整工程文件列表 + CMakeLists.txt（或 .vcxproj 描述）
  [2] 所有 .h 头文件（包含结构体定义、函数声明、常量）
  [3] 所有 .cpp 实现文件（完整可编译代码，不允许留 TODO/占位符）
  [4] app.manifest 文件
  [5] resource.rc 资源文件（仅包含版本信息，无外部图片资源，所有图标内嵌代码）
  [6] 编译与运行说明（Developer Command Prompt 命令行步骤）

核心约束：
  ★ 代码必须可在 MSVC 2022 + Windows 10/11 上直接编译运行，不允许伪代码
  ★ 遮罩效果必须在任务管理器弹出后仍然覆盖屏幕
  ★ 全程零 Emoji，所有视觉元素使用 SVG 路径或 GDI+ 几何图形
  ★ 单次响应若超长，请分 Part 1/2/3 输出，每部分结尾标注"续→Part N+1"
  
补充说明：
使用的编译器是“Qt Creator 18.0.2 (Community)”