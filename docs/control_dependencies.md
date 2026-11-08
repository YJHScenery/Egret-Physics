<!-- /** @file control_dependencies.md @brief 可编程控制实际使用的版本、来源与部署清单。 */ -->

# 控制依赖清单

本次固定 Windows x64：LLVM 21.1.8、Wasmtime C API 38.0.3、nlohmann/json 3.12.0、Lua 5.4.8。核心目标 `Egret_Control_Core` 使用标准 C++20、Windows 原生进程 API、系统 BCrypt，不链接 Qt，也不引入 GPL 系列工具或库。

| 构件 | 来源和主许可 | 实际用途及分发文件 |
|---|---|---|
| LLVM/Clang/LLD 21.1.8 | LLVM 官方发行，Apache-2.0 WITH LLVM-exception | 求解期间编译，部署 `control/bin/clang.exe` 与 `control/bin/wasm-ld.exe`；来自同包的 lld 可按名称作为 Wasm 链接器运行 |
| Wasmtime C API 38.0.3 | Bytecode Alliance 官方发行，Apache-2.0 WITH LLVM-exception | MSVC 导入库仅用于构建，运行时部署 `wasmtime.dll` |
| nlohmann/json 3.12.0 | 官方单头文件，MIT | 编译进核心，用于配置及 Clang AST JSON；保留 MIT 许可 |
| Lua 5.4.8 | Lua 官方源码，MIT | 以 C++ 异常展开方式静态编译；仅注册领域 SDK，不打开标准库；分发 `control/licenses/Lua-LICENSE.html` |
| Windows/MSVC 运行库 | 微软系统组件与再分发许可 | Wasmtime 依赖 VCRUNTIME140、UCRT 等，沿用应用现有运行库部署要求 |

LLVM 安装包 SHA-256：`7a5386c26497db1691f320121e5b113364dd0274b98e55f15f4dbc00c0450113`。

Wasmtime 压缩包 SHA-256：`6427a2eefeeb9da7abe717b313ce4d75db9b910bc763d6d3c8b284c28812d7a3`。

本机下载值与 GitHub 官方发行资产的 digest 相符。`dependency/control/` 内保留下载包许可证；部署应同时携带 `control/licenses/`。LLVM 仅解出所需可执行文件，没有运行安装程序、修改注册表或安装 GCC。解包使用开发机已有 7-Zip，解包器不属于运行或分发链路。

已检查 Wasmtime 和 Clang 的 PE 动态导入，未发现额外非系统 DLL。此检查不等同于审计 Rust 静态链接依赖；官方预构建包未附完整传递依赖 SBOM，本轮不能宣称全部传递依赖已完成商业清理。商业发行前仍须对固定发行包的内含代码、版权、NOTICE 与 MSVC 再分发条件完成审核。未改动本项目原有授权。

复现依赖准备：在 PowerShell 运行 `tools/setup_control_dependencies.ps1 -ArchiveExtractor <已有7z.exe绝对路径>`。该脚本只写项目依赖和下载目录，不自动安装解包器。

官方来源：[LLVM 21.1.8](https://github.com/llvm/llvm-project/releases/tag/llvmorg-21.1.8)、[LLVM 许可证](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/llvm/LICENSE.TXT)、[Wasmtime 38.0.3](https://github.com/bytecodealliance/wasmtime/releases/tag/v38.0.3)、[JSON 3.12.0 MIT](https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT)。

Lua 官方源码包 SHA-256：`4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`。此值为本轮官方 HTTPS 下载后计算并固定的摘要。

## 增量构建与共享结构一致性

控制程序配置、快照与动作结构跨越主程序、控制核心和测试宿主。`cmake/ControlDependencies.cmake` 为这些项目 C++ 源文件添加显式 `OBJECT_DEPENDS`，涵盖 `control_program.h`、`control_abi.h`、`scene_control.h`、`predicate_program.h` 与 `scene_settings.h`，避免 Ninja 包含扫描漏报时混用结构布局。

本机曾出现编译器 `/showIncludes` 输出为 UTF-8、Ninja 缓存的中文提示前缀却为 GBK 的情况，导致依赖数量为零，旧对象文件没有随头文件变化而更新。新增 Lua 后，旧版配置构造函数与新版解析函数混合链接，打开场景时在字符串赋值中发生访问冲突。显式对象依赖不依赖上述本地化输出。Visual Studio 生成器仍使用自身的头文件跟踪。

升级控制契约后首次采用本修复时，应对现有构建目录执行 clean 并完整重建主程序和相关测试；仅重新链接无法修复已经存在的旧对象。不要用旧版本的目标文件或静态库覆盖新构建产物。

2026-11-08 验证：`build/Release` 与 `cmake-build-release` 均完成 clean 后的完整重建；两个目录各自的八项相关回归均通过。已检查 Ninja 的离线求解对象依赖边，包含控制配置、快照与场景设置头文件。窗口自动操作由用户按 Esc 停止，最终双击及菜单交互效果交由用户复测。
