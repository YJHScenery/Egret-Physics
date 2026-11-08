<!-- /** @file interpreted_scene_control.md @brief 三后端统一控制框架、受限 Lua 使用与验证说明。 */ -->

# 统一控制框架与受限 Lua

谓词规则、受限 C/Wasm、受限 Lua 均实现 `egret::control::Program`，共用 `Frame` 与 `CommandBuffer`。世界只负责快照、整批校验和提交，不依赖源码语言。谓词保留原规则配置入口，源码程序通过 `prepareProgram` 按 `backend` 分派。

## 使用

在“场景 → 控制程序”中选择“添加 Lua”。绑定 `body`，设置数值参数 `force`，填写源码并检查草稿、保存程序和场景。

```lua
-- 场景控制入口：一秒施力，三秒撤销。
function on_step()
    local body = scene_entity("body")
    local time = simulation_time()
    if time >= 1 and not state_bool("started") then
        entity_set_force(body, vector3(parameter_number("force"), 0, 0))
        state_set_bool("started", true)
    end
    if time >= 3 and not state_bool("ended") then
        entity_set_force(body, vector3(0, 0, 0))
        state_set_bool("ended", true)
    end
end
```

程序结束时间应大于 3 秒；起止区间为左闭右开。持久力必须显式撤销。示例见 `demo/lua_body_control.epscene`。

## 配置与归档

`scene_settings.programs[]` 新增 `backend`：`c_wasm` 或 `lua`。字段缺失时按原有 C/Wasm 解析，版本仍为 1。未知后端明确拒绝。`name`、`enabled`、绑定、参数、周期、起止时间和 `fuel` 共用。谓词仍保存在已有规则配置中。

`.epscene` 内嵌源码；`.epscenex` 分别保存 `control/program_N.c` 与 `control/program_N.lua`。EPSX 保存原场景和程序来源，回放不执行控制器。旧程序不会被自动改写为 Lua。

## 共享接口与语义

Lua 开放现有 `egret_control.h` 的 46 个 SDK 函数（包括用户定义入口）。名称、参数顺序、单位与领域语义一致：实体、模板、实体集合与数值序列使用带类型的不可伪造 userdata；向量使用 `x/y/z` 表，姿态另含 `w`，材质使用 `staticFriction/dynamicFriction/restitution` 表。布尔参数必须传 Lua 布尔值，数值必须有限。

C 与 Lua 共用 `sdk/egret_runtime.c`：构建时将可信运行库生成为实例私有成员，避免复制领域算法或共享全局状态。Wasm 继续使用原 ABI；Lua 在宿主内部使用同样的固定容量数据布局。修改 SDK 后 CMake 自动重新生成适配代码。

三个后端读取同轮快照，按稳定顺序输出动作，后写设置覆盖前写设置。谓词规则保留原有顺序，源码程序按列表顺序运行。任一失败由世界丢弃整轮动作并停止求解；程序私有状态不回滚，重跑须重建世界并 reset。

Lua 顶层可初始化局部变量、数值序列与具名状态；场景访问放在 `on_step` 中。各实例的全局变量、具名状态、容器和动态身份互相隔离；reset 重新加载源码。控制周期和 `control_delta` 沿用 C 后端语义。Lua 语言本身的数值运算规则不等于 C 规则。

## 限制

采用现成 Lua 5.4.8，静态链接，源码仅按文本加载。不开启标准库，不提供 `io`、`os`、`debug`、`package`、`require`、`load`、`dofile`、协程或 `pcall/xpcall`。数学操作使用 `math_sqrt/math_abs` 等已有 SDK。语言内建的表、循环、函数和运算仍可用。

Lua 堆总量上限 8 MiB，另有固定容量宿主 SDK 状态；每 100 条解释器指令检查 fuel 和取消。fuel 与 Wasm 指令预算不等价，不能直接按预算值比较性能。Lua 自带调用栈限制；宿主 SDK 操作受固定容量约束。没有开放可绕过预算的用户回调、元表设置或任意宿主能力。

源代码最大 256 KiB；动作最多 1024 条，绑定和具名状态各 256 项，数值序列 32 个、每个 256 项。错误包含程序名、后端、仿真时间、步序号，Lua 错误保留源码行号。Lua 解释器不是进程隔离，不能将其描述为与 Wasm 相同的内存隔离边界。

## 验证

新增 `EgretLuaControl` 覆盖状态隔离与重置、调度、参数、集合与序列、动态创建配置销毁、三后端动作等价、C/Lua 混合执行、语法错误、伪造句柄、非有限值、动作超限、死循环、递归、内存耗尽和取消。`EgretSceneArchiveTests` 增加 C/Lua 混合源码的归档往返。

Windows 受限环境运行 C 编译测试时，将 TEMP/TMP 指向工作区内临时目录，避免 Clang 临时文件改名权限问题。GUI 的显示与实际编辑交互仍需用户实机验收。

本轮验证结果：Release 完整程序及相关测试目标构建成功；原有谓词控制、编辑器、C/Wasm 核心、可编程场景和归档求解回归通过，新增 Lua 核心与 Lua 场景测试通过，共八项相关测试。Lua 正式求解示例运行 5 秒，验证最终 X 速度约 4 m/s、位移约 12 m，并验证死循环按预算中止。PowerShell 测试脚本使用 UTF-8 BOM 兼容 Windows PowerShell 5.1。
