<!-- /** @file restricted_c_control_injection.md @brief Egret Physics 受限 C 语言控制注入的完整使用、执行契约及维护说明。 */ -->

# Egret Physics 受限 C 语言控制注入文档

适用日期：2026-11-08。本文描述当前工作区已实现的能力，面向控制代码编写者、场景作者和维护者。“注入”指将用户控制源码编译为受约束的 Wasm 模块，在物理步边界读取快照并提交动作。

## 1. 文档依据与适用范围

本文结合近期工作记录，并用当前源码核对最终行为：

| 近期工作记录 | 本文采用的结果 |
| --- | --- |
| 完善用户控制逻辑方案 | 领域接口、快照与动作缓冲、Clang → Wasm → Wasmtime 路线 |
| 实现 epscene 内嵌 C 控制代码 | 多程序、对象绑定、数值参数、调度、刚体属性、创建销毁和动态回放；本轮只开放刚体控制 |
| 同步 C 界面并添加预设 | 深蓝主题、C 高亮、JetBrains Mono、字号设置、四空格自动缩进与可叠加预设 |
| 升级 epscene 文件格式 | V1 `.epscene` JSON 兼容；V2 `.epscenex` ZIP 独立保存 C 源码；EPSX 回放辅助锚点与实体生命周期修复 |

这些记录中的早期建议不全部等于当前接口。例如，当前允许用户全局变量；公开接口使用 `Entity` 等领域值；尚未实现的智能体、力场和约束编程接口不属于现有功能。后续变更以实际 SDK 和校验器为准。

主要核对入口：

- [公开 SDK](../src/control_core/sdk/egret_control.h)：允许用户调用的函数及类型。
- [编译与运行核心](../src/control_core/control_program.cpp)：语法限制、可信降级、缓存、调度和运行预算。
- [可信运行库](../src/control_core/sdk/egret_runtime.c)：集合、状态、动作和数值序列实现。
- [交换 ABI](../src/control_core/control_abi.h)：宿主与模块的数据边界。
- [世界控制接口](../src/model/control/scene_control.h)：快照、动作及后端抽象。
- [世界管理器](../src/model/scene/world_scene_manager.cpp)：动作验证、物理提交和动态生命周期。

## 2. 能力边界

已提供：读取刚体状态，修改位置、速度、角速度、质量、姿态、材质和活动状态，施加持久力及力矩，从初始物体模板创建实体，永久销毁符合条件的实体，按时间或条件执行用户逻辑，并保存与回放结果。

当前未开放：任意系统库、文件与网络访问、线程、用户指针、任意外部调用、力场及约束的编程增删改查、独立模板库、积分器内部的纯函数力回调、智能体自动生成或执行流程。重力、弹簧和约束仍通过原有场景 UI 配置。

`Egret_Control_Core` 不链接 Qt，可由独立宿主使用；完整物理世界仍由现有适配路径驱动，这不表示整个求解器已经脱离 Qt。

## 3. 从界面开始

1. 新建或打开场景，添加需要控制的物体。可使用“添加预设”加入重力、地平面或测试场景。
2. 从“场景”菜单的 C 控制程序入口，或“时间轴与条件控制”面板的“C 代码控制”进入控制窗口。
3. 添加程序，设置名称和启用开关；有多段程序时调整列表顺序。
4. 选择场景物体，绑定英文别名，例如 `board`。源码通过别名访问物体。
5. 添加需要的数值参数，例如 `force = 26`。
6. 编写 `void on_step(void)`，设置控制周期、开始时间和结束时间。
7. 使用“检查当前草稿”查看编译诊断，再点击“保存全部程序”，并保存场景文件。
8. 发起正式求解，完成后使用 EPSX 播放器观察结果。

“检查当前草稿”后台编译当时捕获的草稿，不会代替保存。正式求解重新校验捕获的场景版本，不要求事先点击检查，也不会自动采用未保存的草稿。

程序保存支持撤销、重做及多文档隔离。字体与字号是本机配置，不写入场景。编辑器默认使用 JetBrains Mono、14 px，支持语法高亮、行号、Tab／Shift+Tab 和四空格自动缩进；当前没有代码补全或语言服务器，彩色高亮不代表语义检查通过。

预设可以叠加，会重新分配数字 ID、重映射约束引用并处理重名；整组支持撤销。重复加入包含重力的预设会累计重力，需检查最终场景。

## 4. 最小程序及配置

无需自行包含头文件，宿主已强制包含公开 SDK：

```c
/** @brief 在一秒后施加拉力，三秒后撤销。 */
void on_step(void)
{
    Entity board = scene_entity("board");
    double time = simulation_time();
    if (time >= 1.0 && !state_bool("started")) {
        entity_set_force(board, vector3(parameter_number("force"), 0, 0));
        state_set_bool("started", true);
    }
    if (time >= 3.0 && !state_bool("ended")) {
        entity_set_force(board, vector3(0, 0, 0));
        state_set_bool("ended", true);
    }
}
```

需绑定 `board` 并设置参数 `force`。程序结束时间应大于 3 秒，求解也应运行到撤销动作发生之后。若结束时间恰为 3 秒，左闭右开区间会阻止撤销分支执行。

以下是 V1 场景 `scene_settings.programs` 中一个程序的配置示意；它不是完整场景：

```json
{
    "version": 1,
    "name": "定时拉力",
    "enabled": true,
    "source": "/** @brief 场景控制入口。 */\nvoid on_step(void)\n{\n}\n",
    "period": 0,
    "start": 0,
    "end": 10,
    "fuel": 1000000,
    "bindings": {"board": "1"},
    "parameters": {"force": 26}
}
```

| 字段 | 规则与默认值 |
| --- | --- |
| `version` | 控制配置版本，目前为整数 `1` |
| `name` | 默认“控制程序”，解析上限为 256 字节 |
| `source` | 非空 C 源码，最多 256 KiB；V2 归档中此位置改存包内源码相对路径 |
| `enabled` | 默认 `true`，控制程序是否启用 |
| `period` | 有限非负秒数，默认 `0`；零为每个物理步，正数为采样周期 |
| `start`、`end` | 有限秒数，`start >= 0`，`end > start`；默认 `0` 和 `1e100` |
| `fuel` | 单次回调执行预算，默认 1,000,000，允许 100～100,000,000 |
| `bindings` | 最多 256 个别名到初始场景实体 ID 的映射；ID 必须为非零十进制字符串 |
| `parameters` | 最多 256 个具名有限数值 |

别名及参数名匹配 `[A-Za-z][A-Za-z0-9_]{0,62}`，即以英文字母开头，后续可用字母、数字、下划线，总长 1～63。别名持久绑定稳定 ID，物体重命名不影响绑定；删除物体后，启用程序中的无效绑定会在求解准备阶段报错。ID 不应经过 JavaScript 浮点数转换。

`parameter_number("force")` 在准备阶段替换为可信数值常量，参数改变需要重新准备编译。实体别名也会降级为领域值，具名状态映射到固定槽位；执行阶段无需逐步解析这些名称。

## 5. 受限 C 语言规范

编译基于 C11，目标为 `wasm32-unknown-unknown`，采用严格诊断及 AST 白名单。它是面向控制计算的 C 子集。

| 类别 | 当前允许 |
| --- | --- |
| 数值 | 整数、浮点、`double`、`bool`、常量、普通算术和比较 |
| 控制流 | `if/else`、`switch`、`for`、`while`、`do/while`、`break`、`continue`、`return` |
| 组织 | 有实现的自定义函数、数值结构体、枚举、局部及全局变量 |
| 领域值 | SDK 提供的实体、模板、向量、姿态、材质、集合和序列 |
| 头文件 | 可省略；显式包含只接受 `#include "egret_control.h"` |

禁止用户声明指针和数组、使用数组下标、取地址和解引用、函数指针或间接调用、直接或间接递归、显式强制转换、联合体、`goto`、汇编、可变参数访问、复合字面量等未列入白名单的语法。不得手动调用 `on_step`，不得覆盖 SDK 名称或使用保留的 `egret_`、`__` 顶层名称。

不支持自定义预处理指令，包括 `#define` 和条件编译；不能包含 `stdio.h`、`math.h`、`stdlib.h` 等头文件，也不能调用 `printf`、`malloc`、`sin`、`system` 等未公开函数。即使系统安装了这些库也不构成可用能力。当前数学函数只有 `math_sqrt` 和 `math_abs`。

实体、模板、状态和参数接口的名称必须是源码中的字符串常量，不能在运行中拼接或传入名称变量。状态键应使用简短 ASCII 字符串（1～63 字符），便于稳定校验与维护。

`Entity`、`EntityTemplate`、`EntityCollection`、`NumberList` 为不透明领域值：可以接收、复制并传给匹配接口，不能访问内部字段、伪造初始化或转换成整数。不要因为 SDK 头文件展示了内部存储就依赖其布局。数值结构体的公开字段可以直接访问；构造 SDK 数值使用 `vector3`、`rotation` 和 `material`。

用户全局变量会跨回调保留，适合保存计数或 `NumberList`；不得依赖未初始化局部变量、整数溢出等未定义行为。循环仍需合理有界，fuel 是执行上限，不是毫秒计时器。

## 6. 公开类型、单位与完整函数表

函数表对应当前 SDK；所有用户能力以 [egret_control.h](../src/control_core/sdk/egret_control.h) 为准。

| 类型 | 含义 |
| --- | --- |
| `Entity` | 实体句柄，可能已失效；用 `entity_exists` 检查 |
| `EntityTemplate` | 初始场景物体的不可变克隆模板 |
| `Vector3` | `double x, y, z`，位置 m、速度 m/s、角速度 rad/s、力 N、力矩 N·m |
| `Rotation` | `double x, y, z, w`，四元数；单位姿态为 `(0,0,0,1)` |
| `Material` | `double staticFriction, dynamicFriction, restitution` |
| `EntityCollection` | 当前回调有效的快照遍历游标 |
| `NumberList` | 固定对象池中的有界数值序列 |

质量使用 kg，时间使用 s；材质系数无量纲。场景显示缩放不改变这些物理单位。重力预设沿 Z 负方向，示例采用 Z 轴向上。

### 6.1 数值构造与实体访问

| 签名 | 语义 |
| --- | --- |
| `Vector3 vector3(double x, double y, double z)` | 构造向量 |
| `Rotation rotation(double x, double y, double z, double w)` | 构造姿态 |
| `Material material(double staticFriction, double dynamicFriction, double restitution)` | 构造材质 |
| `Entity scene_entity(const char *alias)` | 取得已绑定实体；原实体销毁后返回的句柄仍指向原身份 |
| `EntityTemplate scene_template(const char *alias)` | 取得同一绑定物体在初始化时的模板 |
| `EntityCollection scene_entities(void)` | 创建当前快照的集合游标，并使程序订阅全场景实体 |
| `Entity collection_next(EntityCollection collection)` | 推进游标，结束时返回不存在的实体值 |
| `bool entity_exists(Entity entity)` | 检查快照实体或本轮待创建实体是否存在 |

表中 `const char *` 是可信 SDK 的命名参数接口，用户直接传字符串字面量；它不开放用户指针变量。

### 6.2 状态查询

| 签名 | 返回内容 |
| --- | --- |
| `Vector3 entity_position(Entity entity)` | 位置 |
| `Vector3 entity_velocity(Entity entity)` | 线速度 |
| `Vector3 entity_angular_velocity(Entity entity)` | 角速度 |
| `double entity_mass(Entity entity)` | 质量 |
| `bool entity_rotatable(Entity entity)` | 是否具有物理转动自由度 |
| `Rotation entity_rotation(Entity entity)` | 姿态 |
| `Material entity_material(Entity entity)` | 摩擦及恢复系数 |
| `bool entity_active(Entity entity)` | 活动状态 |
| `bool entities_touching(Entity first, Entity second)` | 自本程序上次回调以来累计的真实接触对是否包含二者 |

属性读取要求实体有效；可能已销毁的句柄应先检查存在性。存在性与活动状态不同：停用实体仍然存在。

### 6.3 修改与生命周期

| 签名 | 提交规则 |
| --- | --- |
| `void entity_set_position(Entity entity, Vector3 value)` | 设置位置 |
| `void entity_set_velocity(Entity entity, Vector3 value)` | 设置线速度 |
| `void entity_set_angular_velocity(Entity entity, Vector3 value)` | 设置角速度；要求转动自由度 |
| `void entity_set_mass(Entity entity, double value)` | 有限质量，范围为 `[0,1e15]` kg，原生接口同步维护惯量；质量零沿用项目固定物体语义 |
| `void entity_set_rotation(Entity entity, Rotation value)` | 四元数模长须有限且不小于 `1e-12`，提交时归一化 |
| `void entity_set_material(Entity entity, Material value)` | 摩擦系数非负，恢复系数在 `[0,1]` |
| `void entity_set_active(Entity entity, bool value)` | 可恢复地启用或停用 |
| `void entity_set_force(Entity entity, Vector3 value)` | 替换本控制层的持久力，零向量取消 |
| `void entity_set_torque(Entity entity, Vector3 value)` | 替换持久力矩，零向量取消；要求转动自由度 |
| `Entity scene_spawn(EntityTemplate model, Vector3 position)` | 从模板创建实体，立即返回本批次身份 |
| `void entity_destroy(Entity entity)` | 永久销毁；关联约束或共享力源等不允许情形会使求解失败 |

所有动作参数需满足有限性和物理约束；通过语言检查不代表物理动作一定有效。

### 6.4 时间、参数与持久状态

| 签名 | 语义 |
| --- | --- |
| `double simulation_time(void)` | 当前物理步边界的仿真时间 |
| `double simulation_step(void)` | 当前物理步长 |
| `unsigned long long simulation_step_index(void)` | 当前物理步序号 |
| `double control_delta(void)` | 距本程序上次调用的实际仿真间隔，首次为零 |
| `double parameter_number(const char *key)` | 读取已配置的有限数值参数 |
| `double state_number(const char *key)` | 读取具名数值状态，初始零 |
| `void state_set_number(const char *key, double value)` | 保存具名数值状态 |
| `bool state_bool(const char *key)` | 读取布尔状态，初始假 |
| `void state_set_bool(const char *key, bool value)` | 保存布尔状态 |
| `Entity state_entity(const char *key)` | 读取实体状态，初始为无效句柄 |
| `void state_set_entity(const char *key, Entity value)` | 保存实体句柄，包括本回调新建实体 |

数值与布尔状态共享同一数值存储，不要用同名键表达互不相关的状态。不同程序之间不共享具名状态和全局变量。

### 6.5 有界序列、数学与入口

| 签名 | 语义 |
| --- | --- |
| `NumberList numbers_create(void)` | 从固定池创建空序列 |
| `void numbers_destroy(NumberList list)` | 释放序列，旧句柄失效 |
| `void numbers_push(NumberList list, double value)` | 尾部追加数值 |
| `double numbers_pop(NumberList list)` | 取出尾部数值；空序列报错 |
| `unsigned numbers_count(NumberList list)` | 当前元素数 |
| `double math_sqrt(double value)` | 平方根，负输入报错 |
| `double math_abs(double value)` | 绝对值 |
| `void on_step(void)` | 用户必须定义的唯一控制入口，由宿主调度 |

`NumberList` 当前没有随机索引或元素修改接口，不是通用 C 数组。跨步使用应将句柄保存在全局变量，释放前检查自己的生命周期状态。

## 7. 一轮控制如何执行

```text
正式请求捕获场景与配置
    → 校验绑定、预处理与 AST
    → 可信接口降级 → 编译或命中 Wasm 缓存
    → Wasmtime 编译本机代码并创建独立实例
    → 在到期物理步采样只读快照
    → 原有规则先执行，随后按列表顺序调用 C 程序
    → 汇总动作，完整校验并准备创建资源
    → 世界统一提交动作、刷新缓存并推进物理
    → 写入 EPSX 状态和控制事件
```

### 7.1 同轮快照与动作顺序

所有程序读取同轮快照。对原有实体执行 `entity_set_velocity` 后，再读取速度仍得到该轮快照中的旧值。后面的程序同样读不到前一程序刚提交的意图。

修改通过动作缓冲输出，完整验证后统一提交。同一属性后写覆盖前写；两个程序的 `entity_set_force` 不自动求和。需要合力时，应在一个程序中计算合向量，再提交一次。控制力与重力等既有物理力源叠加。

普通无效动作不会留下部分已提交的批次，但用户模块的全局变量、触发状态等不承诺回滚；宿主内存分配或求解器缓存异常也没有完整事务回滚保证。失败后修正原因并重置重跑。

### 7.2 调度和采样

有效时段为 `[start,end)`。周期零表示每个有效物理步调用；正周期按仿真时间判断到期，回调落在实际步边界上，不切分物理步，也不保证命中每个精确时间点。步长大于周期时，不应假设会补齐期间所有采样点。

使用 `control_delta()` 累计控制积分，使用 `simulation_step()` 理解物理推进。首次 `control_delta()` 为零，要避免用它作除数。时间判断推荐 `>=` 配合一次性状态，避免 `==` 漏过阈值。

开始和结束时间只控制回调，不撤销此前设置的持久力或力矩。撤销逻辑必须有机会在结束之前执行。控制是采样后保持的行为，不能把它等同于 Verlet 等积分器各阶段重新求值的连续弹簧或阻尼函数。

默认仅订阅声明绑定及本程序创建的实体；使用 `scene_entities()` 时订阅全部实体。各程序独立累计接触；接触表示上次回调后曾进入真实接触求解的实体对，包括 CCD 子步冲击及持续支撑，不是 AABB 重叠，也不是精确的接触开始／结束事件。首次物理推进之前没有接触结果。

### 7.3 停用、创建与销毁

停用保留身份和运动状态，但退出积分、碰撞及可见渲染，并影响依赖该端点的约束；重新启用仍遵守既有加载时序。永久销毁则释放实体，旧句柄不能恢复，也不会复用为另一个动态实体。

模板在初始化时独立保存形状、质量、材质和渲染配置；原物体后续移动、修改或销毁不改变模板。当前复用初始物体绑定作为模板入口，同时兼任实体与力源的复合对象不能克隆或单独销毁。有关联约束时销毁明确失败，不会隐式级联删除约束。

`scene_spawn` 返回的实体可在同次回调继续设置属性，并可保存到具名实体状态。它在本轮读取时使用模板及已配置初值，但不加入本轮快照枚举与接触；后续快照才会包含它。销毁后同批次继续写入会报错。

动态身份在本次运行中单调分配，分配前考虑初始场景身份范围，并隔离各程序及世界。不要把某次求解产生的句柄带到另一次求解。

### 7.4 重置与重新求解

每次新求解建立独立实例；程序 reset 恢复初始全局变量、具名状态、序列池和调度历史。集合游标只能用于当前回调，不能跨回调保存复用。

仅重置程序不能恢复已经改变的位置、速度、材质或动态生命周期。要从物理初态重跑，应重新加载初始场景并重新安装控制程序；调整时间数值不等于倒带世界。

## 8. 可直接改用的示例

下列每个代码块均为独立程序，不能把多个 `on_step` 原样拼在同一程序中。需要组合时合并入口，或使用多段程序并检查动作覆盖顺序。

### 8.1 速度反馈控制

绑定 `body`；配置 `targetSpeed`（m/s）和 `gain`（N·s/m）。控制周期可先用 0.01 s，再根据物理步长和数值稳定性调整。

```c
/** @brief 按当前 X 速度误差计算持久控制力。 */
void on_step(void)
{
    Entity body = scene_entity("body");
    if (!entity_exists(body) || !entity_active(body)) {
        return;
    }
    Vector3 velocity = entity_velocity(body);
    double error = parameter_number("targetSpeed") - velocity.x;
    double force = parameter_number("gain") * error;
    entity_set_force(body, vector3(force, 0, 0));
}
```

这是离散反馈示例，没有自动限幅或稳定性保证；增益、采样周期及质量需要与场景匹配。

### 8.2 从模板创建并销毁

绑定 `block`。原体不得为不支持克隆的复合力源；动态替身不应有关联约束。

```c
/** @brief 创建一次替身，并在越过 X 阈值后永久销毁。 */
void on_step(void)
{
    if (!state_bool("created")) {
        Entity replacement = scene_spawn(scene_template("block"), vector3(0, 0, 1));
        entity_set_velocity(replacement, vector3(3, 0, 0));
        state_set_entity("replacement", replacement);
        state_set_bool("created", true);
    }
    Entity replacement = state_entity("replacement");
    if (entity_exists(replacement) && entity_position(replacement).x > 10) {
        entity_destroy(replacement);
    }
}
```

### 8.3 无数组遍历实体

```c
/** @brief 遍历同轮实体，将超过阈值的 X 速度设置为上限。 */
void on_step(void)
{
    EntityCollection entities = scene_entities();
    Entity body = collection_next(entities);
    while (entity_exists(body)) {
        Vector3 velocity = entity_velocity(body);
        if (velocity.x > 20) {
            entity_set_velocity(body, vector3(20, velocity.y, velocity.z));
        }
        body = collection_next(entities);
    }
}
```

上例可能对大量实体产生动作；1024 条共享预算仍然生效。集合遍历能力不意味着每轮都能修改全部 4096 个实体。

### 8.4 接触后只触发一次

绑定 `body` 和 `ground`。

```c
/** @brief 首次检测到累计接触时撤销控制力并记录时刻。 */
void on_step(void)
{
    Entity body = scene_entity("body");
    Entity ground = scene_entity("ground");
    if (entity_exists(body) && entity_exists(ground)
        && !state_bool("handled") && entities_touching(body, ground)) {
        entity_set_force(body, vector3(0, 0, 0));
        state_set_number("contactTime", simulation_time());
        state_set_bool("handled", true);
    }
}
```

`contactTime` 是控制检测时刻，不是精确碰撞发生时刻；首次真实接触需要先经过物理求解。

## 9. 沙箱与资源限制

限制分层执行：源码预处理白名单 → Clang AST 校验及调用图递归检查 → 可信接口降级 → 无外部导入的 Wasm → 固定内存和 fuel → 宿主重新验证动作。

用户模块不获得 C++ 对象地址、Qt 对象、Eigen 对象、虚表或宿主容器。输入是数值快照和稳定身份，输出是动作数据。Wasm 经 Wasmtime 编译为本机代码后仍受其内存模型约束；当前没有 WASI、宿主函数导入或系统调用入口。编译器独立子进程的资源限制与执行期 Wasm 限制分别负责不同阶段。

| 资源 | 当前限制 |
| --- | --- |
| 场景源码程序 | 最多 32 段，每段 256 KiB |
| 单模块线性内存 | 固定 2 MiB，初始和最大均 32 个 Wasm 页 |
| 模块链接栈配置 | 256 KiB，与交换区、运行库等共同受固定内存限制 |
| 场景控制实体／模板 | 容量 4096 |
| 所有程序单轮动作 | 合计最多 1024 |
| 每程序绑定、参数、具名状态 | 各最多 256 |
| 累计接触对 | 最多 8192；超限终止，不静默丢弃 |
| 当前回调集合游标 | 最多 32 |
| 数值序列池 | 最多 32 个序列，每个最多 256 个 double |
| 单次回调 fuel | 默认 1,000,000；可配置 100～100,000,000 |
| 编译子进程 | 单阶段 30 s、1 GiB 内存、32 MiB 输出 |

死循环、失效句柄、越界、非法动作数值或资源耗尽会使当前求解失败，并丢弃未提交动作。诊断包含程序名以及执行阶段的步号、仿真时间等信息；部分底层运行库错误以 Wasm trap 表现，不能保证每一种错误都有单独的中文业务提示。

上述限制不构成严格实时保证；fuel 不直接对应固定耗时，编译器和运行时也仍是需要维护的依赖。

## 10. 场景保存与结果回放

### 10.1 三种扩展名

| 扩展名 | 用途 | 控制源码 |
| --- | --- | --- |
| `.epscene` | V1.0 UTF-8 JSON 场景，可再次编辑求解 | `scene_settings.programs[*].source` 为源码字符串 |
| `.epscenex` | V2.0 ZIP 场景，可再次编辑求解 | `control/program_N.c`，元数据原 `source` 位置存包内相对路径 |
| `.epsx` | 仿真结果及播放数据 | 内嵌场景描述保留源码与配置，播放按已记录结果进行 |

新建场景默认保存 V2；打开旧 `.epscene` 后直接保存仍为 V1，另存为 `.epscenex` 才转换。读取器按内容识别格式；编辑器、模拟器与命令行求解使用同一读取路径。

V2 结构如下，完整规范见 [场景格式](epscenex_format.md)：

```text
metadata.json                 根 magic="EPSCENE"，version="2.0"
entities/general.json         一般实体数组
entities/fields.json          场数组
entities/constraints.json     约束数组
control/program_1.c           第一段程序源码
control/program_2.c           第二段程序源码
config/                       预留目录
```

`metadata.json` 的 `scene` 保存除 `models` 外的内部场景字段，其中内部 JSON 的 `version: "1.0"` 与归档版本独立。`entity_order` 恢复原跨类别实体顺序。源码路径位于 `scene.scene_settings.programs[*].source`，相对于 ZIP 根目录。

读取后源码恢复为字符串，内部 JSON、撤销历史和 EPSX 内嵌场景不依赖外部 `.c` 文件。归档不解包到磁盘；缺文件、错误魔数或版本、非法顺序或源码路径会导致读取失败，不覆盖输出场景。

### 10.2 EPSX 动态结果

EPSX 1.1 在既有 BODY 数据之外记录：`DYNA` 动态身份与初始模板映射、`CTRL` 实际控制动作、`MTRL` 逐帧摩擦信息。事件包含纳秒仿真时间；BODY 帧中的集合表示当前存活物体，支持数量变化及零实体帧。

播放器按稳定身份、模板和已记录状态重建画面，支持倒放和随机跳帧，不编译或执行用户 C 代码，不要求部署控制编译器。旧 EPSX 继续可读，旧播放器则不保证理解新的动态生命周期。

近期回放修复还保留了不在 BODY 帧中的必要辅助锚点，并保留暂时缺席的原场景对象以维持约束引用；动态实体按生命周期移除和恢复。这些属于回放实现，不能当作用户可在求解中任意销毁约束端点的许可。

EPSX 场景描述保存源码、配置、ABI／SDK 版本、缓存摘要和准备耗时。`--parse epsx` 的解析输出包含 `dynamicBodies`、`controlEvents` 和避免数字精度丢失的 `stableId`。

## 11. 编译、缓存与部署

项目固定控制依赖为 LLVM/Clang/LLD 21.1.8、Wasmtime C API 38.0.3、nlohmann/json 3.12.0。准备流程为预处理检查、AST 校验、可信降级、C→Wasm、Wasmtime 本机编译、实例准备。

缓存只保存 Wasm，不反序列化本机机器码缓存。键覆盖源码、可信降级结果（包括绑定和参数）、SDK／ABI、Clang 版本及固定编译设置；每次请求仍进行预处理和 AST 检查。命中缓存省去 C→Wasm 编译，但仍创建独立引擎、模块和状态。缓存通过同卷重命名发布，避免同时检查与求解时读取未写完的文件。

默认缓存位于用户临时目录。独立宿主可设置 `EGRET_CONTROL_CLANG`、`EGRET_CONTROL_SDK`、`EGRET_CONTROL_CACHE` 指定路径；SDK 路径对应含 `egret_control.h` 和 `egret_runtime.c` 的目录，其父目录应提供 `control_abi.h`。

运行部署需包含：

```text
Egret_Physics.exe
wasmtime.dll
control/bin/clang.exe
control/bin/wasm-ld.exe
control/sdk/egret_control.h
control/sdk/egret_runtime.c
control/control_abi.h
control/licenses/...
```

主工程构建后自动复制相应文件；仍需满足主程序的 Qt 和 Windows/MSVC 运行库依赖。依赖来源、固定哈希及许可证清单见 [控制依赖说明](control_dependencies.md)。该清单不能替代整个项目的分发审核，也不改变项目现有许可。

以下命令均在项目根目录的 PowerShell 中执行。已有 build 配置可直接构建：

```powershell
cmake --build build --config Release --target Egret_Physics
./build/Release/Egret_Physics.exe --solve ./scene.epscenex --duration 5 -o ./result.epsx
```

独立控制核心可单独配置与验证：

```powershell
cmake -S src/control_core -B build/control-core -G "Visual Studio 18 2026" -A x64
cmake --build build/control-core --config Release --target Egret_Programmable_Control_Tests
ctest --test-dir build/control-core -C Release --output-on-failure
```

独立核心测试不代表完整 GUI 和物理世界已经验证。

## 12. 常见问题与诊断流程

| 现象 | 首先核对 |
| --- | --- |
| 检查通过但求解没有新行为 | 是否保存全部程序、启用程序、运行了当前场景版本，以及是否位于有效时间段 |
| 编译提示未绑定别名／缺参数 | 字符串字面量拼写与绑定、参数表是否完全一致 |
| 某个物体改名后仍能控制 | 正常，绑定依据稳定 ID；重命名不改变身份 |
| 删除物体后无法开始求解 | 启用程序仍有指向旧初始实体的绑定 |
| 设置速度后立即读到旧值 | 原有实体读取同轮快照；这是执行契约 |
| 到结束时间后力还在 | 结束只停止回调；需提前发出零力或零力矩动作 |
| 多段施力结果不等于力之和 | 同控制层后写覆盖，需要自行计算合力 |
| 接触检测有延迟或连续为真 | 累计真实接触及控制采样语义，不是精确边沿事件 |
| 创建成功但本轮遍历没有它 | 新实体后续快照才参与枚举 |
| 销毁失败 | 是否存在关联约束或共享力源等不支持情形 |
| Wasm trap／fuel 耗尽 | 无效实体、失效游标或序列、空 pop、负平方根、超预算或循环过长 |
| 临时文件创建或改名失败 | 缓存和 TEMP／TMP 的实际目录权限及安全软件限制 |
| 重跑仍不是初始状态 | 是否只重置程序而未重新加载物理初态 |

遇到崩溃、错误或异常，严格按项目 [AGENTS.md](../AGENTS.md) 执行：

1. 定位当前运行程序的主日志 `logs/qdebug_output.log`，提取最后 50 行，并搜索 `DEBUG/ERROR/FATAL/WARN/CRITICAL`。常见目录为根 `logs/`、`build/Release/logs/` 或用户构建目录的 `logs/`。
2. 记录场景文件、程序路径、源码及参数、控制周期、物理步长、求解时长、触发步骤，确认偶发或必现；GUI 输入与复现条件由用户补充。
3. 根据日志文件名、行号和程序诊断定位源码。
4. 分析变量、初始化、调度、绑定和生命周期根因；主日志不足时再查 `async_logger.log`。
5. 先输出具体修复方案，得到确认后修改代码。
6. 构建并执行适用回归，最终由用户确认 GUI 行为恢复且日志没有新错误。

读取日志示例，路径应换成实际运行目录：

```powershell
$controlLogPath = './logs/qdebug_output.log'
Get-Content -LiteralPath $controlLogPath -Tail 50
Select-String -LiteralPath $controlLogPath -Pattern 'DEBUG|ERROR|FATAL|WARN|CRITICAL' | Select-Object -Last 20
```

## 13. 验证与维护约定

本次交付为文档整理，不声称重新运行了所有历史验收。2026-11-07 的控制验证记录包含真实 C→Wasm 执行、状态隔离与重置、调度、数值结构、语法拒绝、fuel、动作预算、动态创建销毁及回放；之后的工作记录还包含编辑器、V2 格式及 EPSX 回放生命周期回归。具体历史验证条件见 [控制验证记录](programmable_control_validation.md)。

维护时按改动选择测试：

| 测试 | 主要覆盖 |
| --- | --- |
| `EgretProgrammableControl` | 无 Qt 核心、语言限制、真实编译执行、资源预算和状态隔离 |
| `EgretProgrammableSceneTests` | 正式请求、多程序、动态实体、动作及结果记录 |
| `EgretSceneSettingsTests` | 场景控制配置、规则与绑定 |
| `EgretEditorTests` | 编辑器、保存加载、预设及源码编辑相关行为 |
| `EgretContactTests` | 真实物理世界控制、接触与回放生命周期集成 |
| `EgretLoadTimeTests` | 原有实体、场和约束的加载时序 |
| `EgretEpscenexSolveTests` | V2 场景读取及命令行求解 |

已有完整构建时，可按需执行：

```powershell
cmake --build build --config Release --target Egret_Physics Egret_Programmable_Control_Tests Egret_Editor_Tests Egret_Contact_Tests
ctest --test-dir build -C Release -R 'Egret(Programmable|Editor|SceneSettings|LoadTime|Contact|Epscenex)' --output-on-failure
```

增加公开能力时，应同步审查公开头文件、AST 与可信降级、运行库、ABI、宿主动作解码、世界验证提交、场景序列化、EPSX 记录与回放，以及上述测试和本文。内部 C++ 新增一个方法不意味着用户 C 自动获得该能力。

旧 [场景控制接口说明](scene_control_api.md) 记录较早的谓词后端阶段，其中“尚未提供编译器”“尚无创建销毁”等阶段描述不能用于判断当前功能；早期 [设计计划](programmable_scene_control_design.md) 中的未来能力也不能当作现有 API。日常使用以本文、公开 SDK 及当前实现为准。
