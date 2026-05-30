# AGENTS.md

## 通用规则

- **环境**：本项目运行在 **Windows** 上。所有命令必须使用 **PowerShell** 执行。
- **回答与注释语言**：所有回答、解释、代码注释均使用 **中文**。
- **代码命名**：类、函数、对象、文件的命名使用 **英文**。
- **文档注释**：每个新创建的类、函数、文件 **必须** 包含 Doxygen 注释，注释内容用中文书写。

## 构建与运行

**工具链**：Visual Studio 2026 + CMake

```
# 配置（仅首次执行，在 cmd 中运行），操作目录应为 CMakeLists.txt 所在文件夹

cmd

%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"

cmake -S . -B build -G "Visual Studio 18 2026" -A x64

# 构建（PowerShell 中执行）
cmake --build build --config Release --target Egret_Physics

# 运行
./build/Release/Egret_Physics.exe
```

**日志文件位置**：
- AI 构建时：`build/Release/logs/` 或 项目根目录下的 `logs/`（与 `CMakeLists.txt` 同级）
- 用户构建时：`cmake-release-build/logs`（大多数）或 `cmake-debug-build/logs`（较少数）

## 目录职责（简表）

| 目录 | 用途 |
|------|------|
| `src/model/physics/rigid/` | 刚体动力学、质量、惯性、碰撞检测 |
| `src/model/physics/field/` | 力场（重力、电磁、自定义力场） |
| `src/model/physics/constraints/` | 关节、弹簧、刚体之间的约束 |
| `src/model/solver/` | 数值积分器 |
| `src/model/scene/` | 场景图、对象管理、空间划分 |
| `src/model/strategy/` | 可插拔的模拟策略 |
| `src/view_model/` | 连接物理模拟与 UI 的数据模型 |
| `src/view/` | QML 视图、UI 组件、可视化器 |
| `src/utils/` | 日志、数学辅助、内存工具、平台辅助 |
| `tests/` | 所有核心组件的单元测试 |


## 日志系统

- **主日志（优先查看）**：`logs/qdebug_output.log`
  - 包含 C++ 中 `qDebug()` 和 QML 中 `console.log()` 的输出。
- **辅助日志**：`logs/async_logger.log`
  - 自定义异步日志系统的输出，仅在主日志信息不足时查阅。

## 问题诊断流程（必须严格遵守）

遇到任何崩溃、错误或行为异常时，**按以下顺序执行，不得跳步**：

1. **定位并阅读主日志**  
   打开 `logs/qdebug_output.log`，搜索 `DEBUG`、`ERROR`、`FATAL`、`WARN`、`CRITICAL` 等关键词。  
   提取**最后 50 行**日志（不要全部粘贴）。

2. **确认复现条件**  
   记录触发错误时的输入参数、操作步骤、运行时长。 
   由于本项目是 GUI 项目，因此输入参数一般将由我来提供。
   判断问题是偶发还是必现。

3. **定位到代码**  
   根据日志中的文件名和行号（例如 `rigid_body.cpp:142`），在对应目录中找到源文件。

4. **分析根因**  
   追溯变量状态、初始化顺序、逻辑错误等深层原因。

5. **输出修复方案**  
   先描述修改思路，等待确认后再动手改代码。

6. **验证修复**  
   一般情况下，你无法直接获取此项目的运行状态，最终将会由我来确认日志中不再出现新错误，且原问题已解决。

**禁止行为**：
- 在我未曾明确指定的情况下构建项目：这会浪费太多的时间和上下文，这是 GUI 项目，由你来构建运行本质上没有任何意义。
- 跳过日志直接猜测问题原因：优先调取日志，但有些问题无法通过日志反映，这需要我来向你描述。
- 不分析根因就随意修改代码。 

## 限制

- **除非我明确指定，否则禁止执行任何 Git 命令**（`commit`、`push`、`branch` 等），查询操作除外。
- **除非我明确指定，禁止修改 `.git/` 目录下的任何文件**。
