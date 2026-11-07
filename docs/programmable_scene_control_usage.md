<!-- /** @file programmable_scene_control_usage.md @brief 场景 C 控制的使用方式、执行语义、资源上限与结果回放。 */ -->

# 场景 C 控制使用说明

在编辑器的“时间轴与条件控制”面板点击“C 代码控制”。可以添加多段程序、调整顺序、启停、选择物体并绑定英文别名、设置数值参数、编辑代码、配置控制周期和起止时间。点击“保存全部程序”后，源码及配置随 epscene 保存；保存支持撤销、重做及多文档隔离。“检查当前草稿”在后台编译捕获的草稿，不必先保存。正式求解独立校验捕获的场景版本，不依赖提前点过检查。

每段代码定义 `void on_step(void)`。默认已包含公开 SDK，不必包含头文件。仅允许显式包含 `egret_control.h`。旧场景加载后补一段合法空入口，保存时写入；没有动作的空入口完成准备后不逐步执行。

```c
/** @brief 在一秒到三秒之间施加指定拉力。 */
void on_step(void)
{
    Entity board = scene_entity("board");
    if (simulation_time() >= 1.0 && !state_bool("started")) {
        entity_set_force(board, vector3(parameter_number("force"), 0, 0));
        state_set_bool("started", true);
    }
    if (simulation_time() >= 3.0 && !state_bool("ended")) {
        entity_set_force(board, vector3(0, 0, 0));
        state_set_bool("ended", true);
    }
}
```

上例需绑定 `board` 并设置数值参数 `force`。参数作为可信编译期常量注入，改变后重新编译。别名持久绑定稳定 ID，重命名物体不改变绑定；删除物体后，启用程序的无效绑定在求解前报错。对象 ID 使用十进制字符串保存，不经 JavaScript 浮点转换。

可以使用整数、double、bool、局部及全局状态、数值结构体、分支、循环和自定义函数。禁止递归、用户指针、地址运算、数组下标、联合体、强制转换、函数指针、任意外部头文件及系统调用。限制使用 Clang AST 白名单检查；实际预处理文件也经过头文件白名单核验。可信 SDK 的数组和地址操作不属于用户代码。

完整公开函数以 `src/control_core/sdk/egret_control.h` 为唯一能力清单：校验器从该头文件生成允许调用列表，接口注释同时给出语义。支持位置、速度、角速度、姿态、质量、材质、转动自由度、活动状态和接触查询；支持设置位置、速度、角速度、姿态、质量、材质、活动状态、持久力及持久力矩。姿态使用 `rotation(x,y,z,w)` 四元数，提交时归一化；材质使用 `material(静摩擦,动摩擦,恢复系数)`。数值单位为 SI，角速度为 rad/s，力矩为 N·m。数学接口目前为平方根及绝对值。

```c
/** @brief 创建模板替身，在同次回调配置并保存其句柄。 */
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

模板是绑定物体在场景初始时的独立快照，保留形状、质量、材质与渲染配置。原物体后来移动、修改或被销毁，不改变模板。初版模板入口复用场景物体选择器；同时作为实体和力源的复合对象不能克隆或单独销毁。永久销毁遇到关联约束时明确拒绝并终止任务，不隐式级联删除。

`scene_spawn` 返回本批次临时身份，可继续设置属性及保存在具名状态。新实体尚未参加本轮接触和快照枚举；读取新实体返回模板和本批次已配置初值。原有实体始终读取同轮快照，写入下一轮可见。销毁后同批次继续写入会报错。动态身份在一次运行中单调分配，先检查初始身份区间，跨程序和跨世界状态隔离，不复用已销毁身份。

```c
/** @brief 不使用地址或下标，遍历同轮实体。 */
void on_step(void)
{
    EntityCollection entities = scene_entities();
    Entity body = collection_next(entities);
    while (entity_exists(body)) {
        Vector3 velocity = entity_velocity(body);
        if (velocity.x > 20) entity_set_velocity(body, vector3(20, velocity.y, velocity.z));
        body = collection_next(entities);
    }
}
```

具名状态在编译准备阶段映射为内部槽位，执行不查字符串。`state_number` 与 `state_bool` 使用同一数值状态；`state_entity` 保存实体值。全局 C 变量也跨调用保留。`NumberList` 提供固定资源内的动态序列对象池，支持创建、销毁、push、pop、count；释放后句柄代次失效。集合游标只在当前回调有效。每次新求解与 reset 都恢复模块初始状态；重跑物理初态需要重新加载场景，而非只清空程序变量。

控制周期零表示每个物理步调用，正数表示按仿真时间采样。`control_delta()` 是距上次调用的仿真间隔，首次为零；`simulation_step()` 是物理步长，二者不能混用。调度在构造快照之前判断；默认仅采集声明绑定与程序所创建的实体，调用 `scene_entities()` 时订阅全部。各程序独立累计接触，累计区间为自上次回调以来曾进入真实接触求解的实体对，不是精确接触开始或结束事件。

起止时间采用 `[start,end)`。停止回调不会撤销持久力，结束清理应在结束时间之前显式执行或使用另一程序。定时动作沿用物理步边界，不切分物理步；检测延迟由实际物理步长和控制周期决定。播放器不执行控制程序。

旧规则先执行，随后按照源码列表顺序执行 C 程序。所有程序读取同一快照，共享动作总预算，完整验证动作后才提交。位置、速度和本控制层的力采用后写覆盖；多个程序的 `entity_set_force` 不自动叠加。连续弹簧或阻尼的力函数仍使用原有物理组件，不能把每步采样后保持的力等同于积分器各阶段的连续求值。

| 资源 | 当前上限 |
|---|---|
| 源码程序 | 32 段，每段 256 KiB |
| 单模块线性内存 | 固定 2 MiB，初始与最大均 32 页 |
| 场景控制实体／模板 | 4096 |
| 全部程序单批次动作 | 1024 |
| 单程序具名状态／绑定／参数 | 各 256 |
| 累计接触对 | 8192，超过则终止，不静默丢弃 |
| 回调内实体集合游标 | 32 |
| 数值序列池 | 32 个序列，每个最多 256 个 double |
| 单回调 fuel | 默认 1,000,000，可持久化配置 100～100,000,000 |
| 编译子进程 | 单阶段 30 秒、1 GiB 内存、32 MiB 输出 |

越界、死循环耗尽 fuel、失效句柄、非法数值或资源超限会丢弃未提交动作并停止本次求解，显示程序名、步号、仿真时间及编译／执行诊断。失败后必须重置重跑。创建前先准备克隆与容量，普通无效动作不会留下半批修改；不能承诺宿主分配或求解器缓存异常具备完整事务回滚。

EPSX 1.1 保留原 BODY 数据，并加入 `DYNA` 当前动态身份与初始模板映射、`CTRL` 实际动作记录及 `MTRL` 逐帧摩擦系数，包含创建、销毁及属性修改的纳秒仿真时间。BODY 的逐帧集合表示当前存活物体，支持数量变化及零实体帧。播放器按稳定身份和模板恢复对象，可倒放及随机跳转，不需要编译器；旧 EPSX 继续可读。旧版播放器不保证理解新的动态生命周期。

EPSX 的场景描述保存源码、配置、ABI／SDK 版本、缓存摘要和各阶段准备耗时。命令行 `--parse epsx` 同时输出 `dynamicBodies`、`controlEvents` 及避免精度丢失的 `stableId`。命令行与 GUI 离线求解共用正式请求入口。

只缓存由源码重建的 Wasm，不反序列化本机机器码缓存。缓存键覆盖源代码、可信降级结果（含绑定和参数）、SDK／ABI、Clang 版本及固定编译设置。每次请求仍重新执行预处理及 AST 检查，Wasmtime 每次建立独立引擎、模块与状态。清空缓存后自动重新构建；默认缓存位于用户临时目录。可用 `EGRET_CONTROL_CLANG`、`EGRET_CONTROL_SDK`、`EGRET_CONTROL_CACHE` 为独立宿主指定工具路径。

运行部署必须带 `control/bin`、`control/sdk`、`control/control_abi.h`、`control/licenses` 和 `wasmtime.dll`。CMake 构建后自动复制；依赖准备见 [依赖清单](control_dependencies.md)。无 Qt 验证宿主可单独配置 `cmake -S src/control_core -B build/control-core -G "Visual Studio 18 2026" -A x64`。

按本轮用户确认，公开接口聚焦刚体控制，力场与约束保留原有 UI，未接入智能体。独立模板库和纯函数力回调未开放；完整物理世界仍由原有 Qt 适配路径驱动，不能将无 Qt 控制核心等同于整个物理求解器已解耦。可直接打开 `demo/programmable_body_control.epscene` 查看两段源码的施力、创建及销毁示例。
