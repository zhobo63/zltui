# ZLTUI — 未完成项目 (TODO)

> 基于 `include/zltui.h` / `src/zltui.cpp` 代码审查，按模块与优先级整理。

---

## 🔴 高优先级（核心功能缺失）

### 1. Linux/macOS 事件线程未实现
- **位置**: `Terminal::event_thread()` (Unix, L727)
- **问题**: Unix 平台的 `event_thread()` 函数体为空，无法接收键盘/鼠标输入。
- **影响**: 在 Linux/macOS 上 TUI 完全不可交互。
- **建议**: 使用 `read(STDIN_FILENO)` + ANSI escape sequence 解析（鼠标 SGR 模式、Bracketed Paste），或使用 `libuv` / `poll`。

### 2. Windows 键盘事件未处理
- **位置**: `Terminal::event_thread()` (Windows, L642)
- **问题**: `KEY_EVENT` 分支仅有空判断，没有将按键转换为 `Event` 并推入队列。
- **影响**: Windows 上无法接收键盘输入。
- **建议**: 解析 `KeyEvent.wVirtualKeyCode` / `uChar.UnicodeChar`，构造 `EventType_Key` 事件。

### 3. Edit 控件未实现
- **位置**: `struct Edit : Slider` (L415)
- **问题**: `Edit` 继承自 `Slider` 但没有任何自定义方法（无文本编辑、光标管理、输入处理）。
- **影响**: 无法创建可编辑的文本框/多行编辑器。
- **建议**:
  - 实现单行/多行文本缓冲
  - 光标位置与闪烁
  - 键盘事件处理（方向键、退格、删除等）
  - `Paint()` 方法渲染文本与光标

### 4. Ctrl+C / 异常退出清理不完整
- **位置**: `Terminal::EnableRawMode()` (Windows, L581)
- **问题**: `SetConsoleCtrlHandler` 被注释掉，程序异常退出时终端可能残留原始模式。
- **建议**: 启用控制台信号处理或使用 RAII 确保 `DisableRawMode()` 在退出时被调用。

---

## 🟢 低优先级（改进与增强）

### 11. utf8_char_width() — Emoji/组合字符处理不完整
- **位置**: `utf8_char_width()` (L13)
- **问题**:
  - 缺少对更多 Unicode 范围的覆盖（如 Arabic、Devanagari 等可能为宽字符的脚本）
  - 不处理 ZWJ 序列（👨‍👩‍👧‍👦 等多码位 Emoji）
  - 不处理组合变音符号（é = e + ◌́）
- **建议**: 考虑使用 `wcwidth()` (POSIX) 或集成 Unicode 宽度表。

### 14. Terminal::Render() — 差异渲染优化空间
- **位置**: `Terminal::Render()`
- **问题**:
  - 双缓冲对比机制已实现，但样式切换时可能产生冗余的 ANSI 序列
  - 没有批量合并同一行的相邻单元格更新
- **建议**: 按行分组渲染，减少光标移动次数。

### 15. Terminal — 缺少屏幕刷新率控制
- **位置**: `Mgr::Update()` / `Terminal::Render()`
- **问题**: 每次循环都调用 `Render()`，即使没有变化（虽然 `is_dirty` 标志存在，但外部循环未展示）。
- **建议**: 在示例/主循环中确保仅在 `is_dirty` 时渲染；考虑添加帧率限制。

### 16. Win — 缺少键盘焦点导航
- **位置**: `Mgr::Update()` (L1335)
- **问题**: 仅处理鼠标事件，没有 Tab / Shift+Tab 在控件间切换焦点的逻辑。
- **建议**: 实现键盘焦点管理（`Win::is_focused`），支持 Tab 导航和 Enter/Space 激活。

### 17. Win — 缺少 `on_hover` / `on_leave` 回调
- **位置**: `struct Win` (L317)
- **问题**: 仅有 `on_click`，没有鼠标悬停/离开事件。
- **建议**: 添加 `std::function<void()> on_hover;` 和 `on_leave`。

### 23. Mgr::Create() — Edit 类型注册但无功能
- **位置**: `Mgr::Create()` (L1304)
- **问题**: DSL 中可以创建 `Edit`，但由于 `Edit` 未实现（见 #3），实际不可用。

### 24. 缺少 CMakeLists.txt / 构建系统文档
- **位置**: 项目根目录
- **问题**: 没有发现构建配置文件或编译说明。
- **建议**: 添加 `CMakeLists.txt` 和 `README.md` 中的编译指南。

### 25. 缺少单元测试
- **位置**: —
- **问题**: 核心功能（UTF-8 解析、颜色解析、布局计算等）没有测试覆盖。
- **建议**: 使用 Google Test 或 Catch2 添加基础单元测试。

---

## 📋 按模块汇总

| 模块 | 高优 | 中优 | 低优 |
|------|------|------|------|
| Terminal (跨平台) | #1, #2, #4 | — | #15 |
| DrawBuffer | — | — | #12, #13, #14 |
| Win / 布局系统 | — | #5, #9, #10 | #16, #17 |
| Label / Button / Check | — | — | #18, #19, #20 |
| Slider | — | #6 | #21, #22 |
| Edit | #3 | — | — |
| Mgr / DSL 解析 | — | #7 | #23 |
| 项目基础设施 | — | — | #24, #25 |

---

*最后更新: 基于代码审查自动生成，建议定期回顾并调整优先级。*
