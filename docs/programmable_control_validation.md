<!-- /** @file programmable_control_validation.md @brief 本轮 Windows Release 构建、自动验收和实际 UI 检查记录。 */ -->

# 验证记录

日期：2026-11-07。环境：Windows x64、Visual Studio 2026、MSVC 19.51、CMake、Qt 6.9.3；控制编译器 LLVM 21.1.8，Wasmtime 38.0.3。

完整应用已构建为 `build/Release/Egret_Physics.exe`。`Egret_Control_Core` 的独立测试宿主已从 `src/control_core` 单独配置、构建并运行；PE 导入检查仅列出 Wasmtime、BCrypt 和 Windows/MSVC 运行库，没有 Qt DLL。

已通过的自动验证：

| 测试 | 内容 |
|---|---|
| EgretProgrammableControl | 真实 C→Wasm→本机执行，状态隔离、重置、调度、参数精度、数值结构、无地址集合、有界容器、头文件／AST 限制、递归拒绝、死循环及动作预算、同批创建配置、并发缓存原子发布、准备阶段取消 |
| EgretProgrammableSceneTests | 不先点击编译即可求解，多段源码独立状态，创建后立即配置，原体及新体永久销毁，零实体帧，DYNA／CTRL／MTRL 读取，动态质量、姿态、持久力矩与材质，编译失败不创建新结果文件 |
| EgretSceneSettingsTests | 原有规则恒力启停、阈值停用、显示尺度不改变物理结果、设置保存及无效绑定拒绝 |
| EgretEditorTests | 保存与加载、源码默认入口、撤销重做、多文档、变换和复制粘贴回归 |
| EgretLoadTimeTests | 原有实体、约束和场的加载时序 |
| EgretContactTests | 原有接触与真实世界控制集成回归 |

全套相关六项测试通过后，针对后续核心诊断、缓存及 UI 修订重复运行相应四项回归，均通过。Windows 受限执行环境中曾出现 Clang 临时对象改名权限失败，测试时将 TEMP／TMP 设为工作区内 `build/control-core/temp` 后通过；实际桌面 GUI 使用用户正常临时目录也完成了后台编译。

```powershell
cmake --build build --config Release --target Egret_Physics Egret_Programmable_Control_Tests Egret_Editor_Tests Egret_Contact_Tests
$env:TEMP = Join-Path (Get-Location) 'build/control-core/temp'
$env:TMP = $env:TEMP
ctest --test-dir build -C Release -R 'Egret(Programmable|Editor|SceneSettings|LoadTime|Contact)' --output-on-failure
```

实际 UI 验证了示例载入、“C 代码控制”窗口、两段代码切换、别名和参数显示、后台检查通过、深色面板与诊断。已修正初次窗口加载发现的新增 QML 语法问题。动态回放验证时，将 `EGRET_CONTROL_CLANG` 指向不存在的文件，仍可加载 EPSX 并从第 40 帧（零实体）回退到第 11 帧（两个实体），再跳到第 21 帧（一个实体）；适应场景后两个实体均可见，证明动态身份与倒放恢复不依赖 C 编译器。

采样基准采用 Release、单测试线程、200 次调用，包含 C++ 输入转换／复制、Wasm 执行及动作解码。一次实测 10／100／1000 个实体平均约 29／30／31 微秒，稳态 C++ `operator new` 计数均为零。另以 50 次调用测试 10／100／1000 个 C 条件分支，平均约 32／31／57 微秒。源码和运行环境变化会影响结果，这些数据不包含整个求解器、不覆盖 Rust 内部分配，也不构成严格实时保证。

编译缓存只存 Wasm；通过同卷重命名发布完整文件，避免草稿检查和正式求解同时准备时读到未写完的缓存。任务统计分别记录校验、C 编译、Wasm 本机编译、实例准备、物理推进和总耗时，来源与准备统计写入 EPSX 场景描述。

商业分发仍以依赖清单中的发布审计为前置事项。独立模板库、纯函数力接口、力场／约束编程接口及智能体不属于本轮用户确认的刚体交付范围。
