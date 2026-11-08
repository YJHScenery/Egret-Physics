# 受限 C 与受限 Lua 编程控制注入基准对比报告

> 生成日期：2026-11-08
> 场景文件：`bench_control/control_arena_c.epscenex`、`bench_control/control_arena_lua.epscenex`
> 说明：本报告只做基准测试，**未修改仓库内任何源码**；所有新增文件都位于 `bench_control/` 目录。

## 一、资料来源

1. **Codex 工作记录**：`C:\Users\YJH\.codex\sessions\2026\11\08\` 下的 rollout `*.jsonl`（如 `rollout-2026-11-08T14-31-59-...jsonl`、`rollout-2026-11-08T14-24-38-...jsonl`）记录了“统一控制框架 / 受限 C (Wasm) 与受限 Lua 双后端 / 统一 Program 快照接口”的设计与实现过程，并大量检索了 `src/control_core/`、`docs/restricted_c_control_injection.md`、`docs/programmable_scene_control_usage.md` 等文件。`C:\Users\YJH\Documents\ChatGPT\`、`C:\Users\YJH\Documents\Codex\` 下是图标/绘图类产物，与本功能无关。
2. **仓库文档**：`docs/epscenex_format.md`（归档格式）、`docs/interpreted_scene_control.md`（受限 Lua 语义）、`docs/restricted_c_control_injection.md`（受限 C 注入契约）、`docs/programmable_scene_control_usage.md`、`docs/control_dependencies.md`。
3. **仓库源码**（仅阅读）：`src/view_model/serialize/epscene_archive.cpp`（场景包读写与校验）、`src/control_core/control_program.cpp`（C 前端与 Wasm 执行）、`src/control_core/lua_program.cpp`（Lua 后端）、`src/control_core/egret_runtime.c`（可信运行库）、`src/utils/command/solve_command.cpp`（离线求解与计时输出）。

## 二、.epscenex 场景包格式

`.epscenex` 是 **V2.0 标准 ZIP 容器**（`PK` 魔数），内部条目：

| 条目 | 内容 |
| --- | --- |
| `metadata.json` | 根对象：`magic="EPSCENE"`、`version="2.0"`、`scene`（除 `models` 外的完整场景 JSON，内部 `version` 仍为 `"1.0"`）、`entity_order`（按原顺序记录每个实体的类别 0/1/2） |
| `entities/general.json` | 类别 0 实体（刚体）数组 |
| `entities/fields.json` | 类别 1 场（如重力场）数组 |
| `entities/constraints.json` | 类别 2 约束数组 |
| `control/program_N.c` | 第 N 段受限 C 程序源码（UTF-8 文本） |
| `control/program_N.lua` | 第 N 段受限 Lua 程序源码（仅 `backend=="lua"` 时） |
| `config/` | 可选配置目录（可省略） |

读取端（`epscene_archive.cpp`）的硬性校验：条目路径必须安全（非空、不以 `/` 开头、不含 `\` 与 `:`、无 `.`/`..` 段）、不允许符号链接、不允许重复条目、单文件 ≤64 MiB、总计 ≤256 MiB；`metadata.scene` 必须是对象、`entity_order` 必须是数组且完整覆盖三个类别；实体文件必须是数组且每项 `category` 与所在文件一致；程序源码路径必须以 `control/` 开头，并以 `.c` 结尾（`backend=="lua"` 时以 `.lua` 结尾）。注意：`Expand-Archive` 不识别 `.epscenex` 扩展名，检查包内容时需先复制为 `.zip`。

`scene.scene_settings.programs[i]` 的字段（两后端共用）：`name`、`enabled`、`source`（包内为相对路径）、`period`（控制周期，0 表示每个物理步）、`start`、`end`（可省略，默认 1e100）、`fuel`、`bindings`（别名→实体 ID）、`parameters`（编译期常量）、`version`（必须为 1）、`backend`（`c_wasm` 默认 / `lua`）。

## 三、两个基准场景（除控制程序语言外完全相同）

### 3.1 世界

自由空间（无重力场、无场、无约束、**无接触**）中的 6 体协同编队，刻意规避碰撞求解噪声：

| id | 名称 | 形状/尺寸 | 质量 | 初始位置 | 备注 |
| --- | --- | --- | --- | --- | --- |
| 1 | 中心体 | box `[1.2,1.2,1.2]` | 2.0 | (0,0,3.4) | 可旋转 |
| 2–5 | 卫星甲/乙/丙/丁 | box `[0.4,0.4,0.4]` | 1.0 | 半径 1.4 的 XY 环上 | 可旋转 |
| 6 | 自由飞行球 | sphere r=0.35 | 1.5 | (2.5,0,5.2)，初速 (-0.8,0,0.5) | 高速飞行，用于全场速度扫描 |

### 3.2 控制程序（单个程序，`period=0` 每物理步执行，`fuel=1000000`）

绑定：`hub→1, sat1→2, sat2→3, sat3→4, sat4→5, ball→6`

参数：`kp=25, kd=7, maxf=25, radius=1.4, gain=3, satkp=8, satkd=3, satmax=12, speedcap=100, iters=8000, torque=0.5`

每步依次执行的控制逻辑（C 与 Lua 逐行同构）：

1. **三角波轨迹跟踪**：`cycle = step % 1200`，`phase = cycle/600`，期望位置在 Z 轴上以 ±1 m/s 往返。
2. **自适应 PD**：用 `0.94/0.06` 指数均值估计速度，再执行 **8000 次迭代**的在线滤波自整定（`g_f = 0.5*g_f + 0.25*sqrt(0.25+|g_f|*0.01)`），把自整定结果按 `gain` 折算进增益，输出力做 `maxf` 限幅。
3. **四卫星编队**：对每个卫星做“径向弹簧 + 速度阻尼 + 切向涡旋”，力限幅 `satmax`，并把 `-0.35` 倍力反作用回中心体。
4. **全场枚举**：`scene_entities()` + `collection_next()` 统计 body 数、总质量、最大速度，对超速体施加 `-0.7v` 阻尼（`speedcap` 阈值）。
5. **接触统计**：`entities_touching()` 维护接触计数。
6. **19 个具名状态**：`state_number/state_set_number`、`state_bool/state_set_bool`、`state_entity/state_set_entity` 全部用到。
7. **数值序列环形缓冲**（`numbers_create/push/pop/count/destroy`）：每 24 步 push 一次历史误差，序列满 256 后 pop 最旧项。
8. **力矩**：对中心体施加限幅力矩并重写 `material`（摩擦/恢复系数）。
9. **实体生命周期**：`step==2000` 时用 `scene_template("sat1")` 与 `scene_spawn()` 生成第 7 个实体并设置速度/质量/材质；`step==6000` 时 `entity_destroy()` 销毁它（4 s × 0.5 ms = 8000 步内完整走完生灭）。

C 版使用 6 个自定义函数与全局持久状态，Lua 版用闭包 upvalue 表达相同的持久状态，两者控制行为逐位一致（见第四节）。

## 四、功能等同性验证

分别求解两个场景（时长 4 s、步长 0.5 ms、采样 10 ms），再用 `Egret_Physics.exe -p epsx <帧号> <文件>` 解析两份 EPSX 输出，逐帧比对每个实体的 `position / linearVelocity / rotation / angularVelocity` 以及 `controlEvents`：

| 帧号 | 实体数 | 差异项 |
| --- | --- | --- |
| 0 / 20 / 60 / 100 | 6 | 0 |
| 150 / 200 / 250 / 300 | 7 | 0 |
| 350 / 399 | 6 | 0 |

**总计差异项 0**：第 150 帧（1.5 s）起为 7 体（spawn 生效），第 350 帧（3.5 s）回到 6 体（destroy 生效），两个场景的轨迹、姿态与全部控制事件完全一致。

> 两份 EPSX 文件的整体字节并不相同，原因是文件头记录了 `controlPreparation`（C 后端的编译/准备耗时与源码摘要），属于元数据差异，不代表仿真结果差异。

## 五、计时结果

测试环境：Windows，`build/Release/Egret_Physics.exe`（2026-11-08 构建），每条命令使用**完全相同的参数**：

```
Egret_Physics.exe --solve <场景.epscenex> --duration 4 --step 0.5 --sample 10 -o <输出.epsx>
```

关注命令行输出中的 **“推进与控制”** 秒数（即仿真推进 + 控制注入的时间，不含控制准备/编译），同时记录“控制准备”和“总任务耗时”。

### 5.1 三次重复测量

| 后端 | 推进与控制 (s) | 均值 | 控制准备 (s) | 总任务耗时 (s) |
| --- | --- | --- | --- | --- |
| C (Wasm) | 2.405 / 2.655 / 2.407 | **2.489** | 0.181–0.193 | 2.64 / 2.90 / 2.65 |
| Lua | 6.715 / 6.485 / 6.533 | **6.578** | 0.001–0.002 | 6.77 / 6.54 / 6.58 |

- 推进与控制：**Lua / C = 6.578 / 2.489 ≈ 2.64×**，绝对差约 **4.09 s**。
- 同世界纯物理基线（无控制程序）：**0.689 s**（两次 0.661 / 0.717），即 8000 步共 0.689 s。
- 扣除物理基线后的控制注入开销：C 约 **1.80 s**、Lua 约 **5.89 s**，比值约 **3.3×**；折合每步约 **224 µs vs 735 µs**。
- 控制准备：C 首次约 0.40 s，后续 0.18–0.19 s（求解器每次仍要执行源码预处理与缓存校验，日志显示缓存命中 0/1）；Lua 仅 0.001–0.002 s（直接解析文本，无编译）。若把准备时间计入总耗时，差距为 2.64 s vs 6.63 s。

### 5.2 开销分解（同一世界，两次重复）

| 对照场景 | 控制程序内容 | 推进与控制 (s) | 相对纯物理增量 |
| --- | --- | --- | --- |
| 纯物理 | 无程序 | 0.689 | — |
| 最小回调 C | 仅 `state_set_number("dist", simulation_time())` | 0.977 | +0.29 s（≈36 µs/步） |
| 最小回调 Lua | 同上（Lua 写法） | 0.750 | +0.06 s（≈7.5 µs/步） |
| 纯循环 C | 每步 8000 次迭代的滤波循环 | 1.688 | +0.71 s（≈89 µs/步，11 ns/迭代） |
| 纯循环 Lua | 同上（Lua 写法） | 5.503 | +4.75 s（≈594 µs/步，74 ns/迭代） |

可见：

- **固定注入开销**（快照序列化 + 跨边界调用）：C 后端约 36 µs/步，Lua 后端约 7.5 µs/步（Lua 的宿主调用路径更短，但它每步多一次脚本解释的固定成本）。
- **计算型负载**：受限 C 经 Clang `-O2` 编译为 Wasm 后，循环约 11 ns/迭代；Lua 解释执行约 74 ns/迭代，相差约 **6.7×**。
- 完整基准场景中，两个后端都还要承担大量 SDK 调用（枚举、状态读写、序列操作、材质/力矩设置），因此最终差距（2.64×）小于纯循环差距（6.7×），但仍稳定显著。

## 六、结论

1. 两个 `.epscenex` 的场景数据、绑定、参数、周期、燃料完全相同，控制逻辑逐行同构；EPSX 输出逐帧逐位一致，可认定**功能完全相同**。
2. 相同配置下，**受限 Lua 的仿真耗时约为受限 C 的 2.6–2.7 倍**（2.489 s vs 6.578 s，4 s 仿真 / 8000 物理步），差距主要来自解释执行的循环/表达式开销（约 6.7×）以及每步脚本调用的固定成本。
3. C 后端的优势是热路径快，代价是**首次编译约 0.4 s**（之后仍需 0.18–0.19 s 的前端校验）；Lua 后端准备几乎免费（约 1–2 ms），适合迭代调试与轻量控制。
4. 场景设计刻意采用“无重力、无场、无接触”的自由空间编队，避免接触求解主导物理耗时、掩盖控制注入差异；若改为有接触的场景，物理本身会吃掉大部分时间，后端差异会被稀释（早期带承载平台的实验中，2 s 仿真的 C/Lua 分别为 2.24 s / 2.29 s，几乎无法区分）。

## 七、注意事项与局限

- 计时为单机单次会话的实测值，受 CPU 频率、系统负载影响；`纯物理` 与 `纯循环 Lua` 存在约 ±5% 波动，但 2.64× 的结论远超噪声。
- `period=0` 表示每个物理步都调用控制器（8000 次调用）；若把 `period` 设为 0.01，调用次数降到约 400 次，循环开销会大幅下降，C 与 Lua 的差距也会缩小到主要由固定注入开销决定。
- `fuel=1000000` 对两个后端都足够（本场景未触发预算耗尽）；Lua 每 100 条解释器指令检查一次预算与取消。
- 本基准只测离线求解（`--solve`）；GUI 交互、UI 线程与渲染开销不在比较范围内。
- 两个场景的 EPSX 整文件字节不同（文件头含 C 编译准备信息），比对内容时应逐帧解析而非直接比较文件哈希。

## 八、复现步骤

```powershell
cd 'D:\Programing\Egret Physics'
python .\bench_control\build_scenes.py          # 重新生成两个 .epscenex
python .\bench_control\decompose.py             # 生成纯物理/最小回调/纯循环对照场景

# 正式测量（两者配置完全相同）
.\build\Release\Egret_Physics.exe --solve .\bench_control\control_arena_c.epscenex   --duration 4 --step 0.5 --sample 10 -o .\bench_control\final_c.epsx
.\build\Release\Egret_Physics.exe --solve .\bench_control\control_arena_lua.epscenex --duration 4 --step 0.5 --sample 10 -o .\bench_control\final_lua.epsx

# 功能等同性验证
python .\bench_control\verify_equivalence.py
```

## 九、文件清单（均在 `bench_control/` 下）

| 文件 | 说明 |
| --- | --- |
| `control_arena_c.epscenex` | 受限 C 基准场景（交付物） |
| `control_arena_lua.epscenex` | 受限 Lua 基准场景（交付物） |
| `scene_control.c` / `scene_control.lua` | 两个逐行同构的控制器源码 |
| `build_scenes.py` | 由世界定义生成两个场景包 |
| `decompose.py` | 生成纯物理/最小回调/纯循环对照场景 |
| `diagnose.py`、`variants.py`、`exp.py`、`calib.py` | 调参与诊断脚本（保留历史实验） |
| `verify_equivalence.py` | 逐帧比对两份 EPSX 输出 |
| `probe_codex.py` | 从 Codex 会话记录中检索控制注入相关片段 |
| `final_c.epsx` / `final_lua.epsx`、`run_*.epsx` | 求解输出（EPSX 1.1 轨迹记录） |
