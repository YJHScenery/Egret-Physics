# EPSX（Egret Physics Simulation eXtended）模拟数据文件格式规范

> **状态：草案（Draft v0.5）**
> 本文档定义 Egret Physics Simulation 二进制容器格式，扩展名定为 `.epsx`（见 §2.1）；
> **逐字节的编码规范见同目录《EPSX 模拟数据文件编码格式规范.md》**，本文档保留设计目标、决策与接口约定。
> v0.2 相对 v0.1 的变更：采样间隔确定为「抽帧」语义、求解步长确定为 0.5 ms、帧记录范围扩大到全部实体类型、
> 补充命令行缩写评估、扩展名由 `.eps` 改为 `.egps`。
> v0.3 相对 v0.2 的变更：新增 §4.3，明确区分「求解推进步长 / 采样间隔 / 渲染帧率」三个时间量，
> 并说明当前离线实现中渲染与求解步是耦合的。
> v0.4 相对 v0.3 的变更：采样间隔的**默认值改为 10 ms**；五档取值与 `sampleIntervalNs` 参数本身全部保留。
> v0.5 相对 v0.4 的变更：扩展名由 `.egps` 改为 **`.epsx`**；格式名、命名空间、类名与文件名统一为 EPSX
> （`egret::epsx`、`EpsxReader`、`EpsxWriter`、`epsx_format.h` 等）；文件魔数改为
> `89 45 50 53 58 0D 0A 1A`（仍为 8 字节）；`--parse` 的对象类型关键字改为 `epsx`；新增编码格式文档。
> 文中标注 `【待确认】` 的条目需要项目作者决策，`【待核实】` 的条目需要在联网环境下复核。

## 1. 背景与设计目标

本项目的模拟器当前以「实时」方式运行：`QTimer` 以 16 ms 触发、累加真实时间、
按固定步长推进物理世界，并直接把世界状态喂给 `View3D` 渲染。
该方式有三个固有缺陷：渲染帧率与物理解算相互耦合；解算快于 / 慢于实时时会丢帧或追帧；
运行结果无法回放、无法逐帧比对，因而难以定位抖动一类的数值问题。

EPSX 文件用于把一次模拟的**全部逐帧状态**固化下来，使解算与播放解耦。设计目标：

| 目标 | 具体要求 |
|------|---------|
| 可校验 | 文件头带魔数，数据块带校验和；损坏时能明确报错而不是静默给出错误物理结果 |
| 可随机访问 | 播放器拖动进度条时，能 $O(1)$ 定位到任意第 $k$ 帧，不必从头顺序读 |
| 可流式写入 | 求解器边算边写，中途崩溃时已写入的帧仍可被读取（文件尾回填总帧数与索引） |
| 可自描述 | 场景（实体表、形状、质量、约束、力场、求解器配置）写在文件内，播放器无需外部场景文件 |
| 可扩展 | 未知数据块按长度跳过而不报错；版本号允许向后兼容地增加字段 |
| 体积可控 | 提供单精度与 zstd 压缩选项；在保证播放精度的前提下压缩典型场景 |
| 与实时解耦 | 求解步长固定 0.5 ms，采样间隔只决定抽帧比例，二者都与渲染帧率无关 |

## 2. 命名、扩展名与系统关联

格式名为 **EPSX**（Egret Physics Simulation eXtended，前身为 EPS / EGPS），文档与命令行里的 `epsx` 一律指格式名；
文件扩展名定为 **`.epsx`**，理由见 §2.1。

### 2.1 扩展名：避开 `.eps`，最终定为 `.epsx`

`.eps` 同时也是被广泛使用的 **Adobe Encapsulated PostScript** 扩展名。本机注册表实测结果：

| 注册表位置 | 实测结果 |
|-----------|---------|
| `HKCR\.eps` | 存在，`Content Type = application/postscript`，但**没有** `(默认)` ProgID 值 |
| `HKCU\...\FileExts\.eps\UserChoice` | **不存在**，即本机当前未给 `.eps` 指定默认打开程序 |

结论：本机把 `.eps` 关联到播放器不会与现有软件冲突，但在安装了 Adobe 系软件或 PostScript 查看器的机器上，
用户极可能在资源管理器里双击到真正的 PostScript 文件——两份互相不兼容的数据共用同一个扩展名，
只能靠文件内容区分，代价过高。因此**不采用 `.eps`**。

【已决策】扩展名定为 **`.epsx`**（Egret Physics Simulation eXtended）。本机注册表实测候选占用情况：

| 候选扩展名 | `HKCR\<ext>` | `HKCU\...\FileExts\<ext>` | 说明 |
|-----------|-------------|---------------------------|------|
| `.epsx` | 未注册 | 无记录 | **采用**：与格式名 EPSX 同名，4 字符，本机无冲突 |
| `.egps` | 未注册 | 无记录 | 早期候选（EGret Physics Simulation），v0.5 起被 `.epsx` 取代 |
| `.epsim` | 未注册 | 无记录 | 语义最直白（Egret Physics SIMulation），但与 `eps` 形近，容易再次混淆 |
| `.egret` | 未注册 | 无记录 | 与白鹭引擎（Egret Engine）同名，品牌上易撞 |
| `.egp` / `.epsm` / `.egsy` | 未注册 | 无记录 | 均无本机冲突，缩写可读性不如 `.epsx` |

【待核实】本地注册表只能证明「本机没有已安装的软件占用该扩展名」，不能证明它在互联网上无人使用。
正式定名前需要联网核对通用扩展名索引，结论回填到本小节。

无论最终采用哪个扩展名，实现上都保留两条强制措施：

1. 关联只写 **`HKCU\Software\Classes`**（当前用户），不要求管理员权限，也不覆盖系统级关联；
2. 播放器打开文件时**必须先校验 8 字节魔数**，魔数不符则提示「这不是 Egret Physics 模拟文件」
   并正常退出，禁止按本格式强行解析。

### 2.2 关联注册内容（Windows）

```
HKCU\Software\Classes\EgretPhysics.Simulation          (默认) = "Egret Physics Simulation"
HKCU\Software\Classes\EgretPhysics.Simulation\DefaultIcon (默认) = "<安装目录>\Egret_Physics.exe,0"
HKCU\Software\Classes\EgretPhysics.Simulation\shell\open\command
                                                        (默认) = "\"<安装目录>\Egret_Physics.exe\" --play \"%1\""
HKCU\Software\Classes\.epsx                             (默认) = "EgretPhysics.Simulation"
```

```
# 注册（当前用户，无需管理员）
reg add "HKCU\Software\Classes\EgretPhysics.Simulation\shell\open\command" /ve /d "\"%CD%\Egret_Physics.exe\" --play \"%1\"" /f
reg add "HKCU\Software\Classes\.epsx" /ve /d "EgretPhysics.Simulation" /f

# 注销
reg delete "HKCU\Software\Classes\.epsx" /f
reg delete "HKCU\Software\Classes\EgretPhysics.Simulation" /f
```

## 3. 总体结构

文件由一个固定长度的文件头、若干数据块（Chunk）和可选的尾块组成。
所有多字节整数与浮点数**统一为小端序（little-endian）**，该约定在文件头中显式声明。
所有数据块按 8 字节对齐，块尾填充字节填充 `0x00`，填充长度不参与校验和。

```
┌──────────────────────────────────────────────────────────────┐
│ File Header (64 B, 固定)                                      │
├──────────────────────────────────────────────────────────────┤
│ Chunk "SCNE"  场景描述（JSON，UTF-8，可压缩）                  │
│ Chunk "META"  可选：作者、描述、标签等键值元数据               │
│ Chunk "FRAM"  第 0 帧                                          │
│ Chunk "FRAM"  第 1 帧                                          │
│ ...                                                           │
│ Chunk "INDX"  帧索引：帧号 → 块偏移（变长 / 压缩时必须存在）    │
│ Chunk "END!"  尾块：总帧数、文件级校验和、索引位置             │
└──────────────────────────────────────────────────────────────┘
```

不认识的块类型按「FourCC + 长度」跳过，这是 PNG、RIFF、EBML 等格式通用的向前兼容策略。

## 4. 文件头（64 字节）

| 偏移 | 长度 | 类型 | 字段 | 说明 |
|-----:|-----:|------|------|------|
| 0 | 8 | bytes | `magic` | `89 45 50 53 0D 0A 1A 0A`，即 `\x89` `'E'` `'P'` `'S'` `CR` `LF` `SUB` `LF` |
| 8 | 2 | uint16 | `versionMajor` | 主版本号，当前 `1` |
| 10 | 2 | uint16 | `versionMinor` | 次版本号，当前 `0` |
| 12 | 4 | uint32 | `flags` | 见 §4.1 |
| 16 | 8 | uint64 | `frameCount` | 帧总数；流式写入时为 `0`，收尾时回填 |
| 24 | 8 | uint64 | `durationNs` | 声明总时长（纳秒）；未知时为 `0` |
| 32 | 4 | uint32 | `sampleIntervalNs` | 采样间隔（纳秒），默认 10 ms = `10000000`，取值见 §4.2 |
| 36 | 4 | uint32 | `entityCount` | 帧记录中的实体槽位数 $N$ |
| 40 | 8 | uint64 | `sceneChunkOffset` | `SCNE` 块起始偏移 |
| 48 | 8 | uint64 | `indexChunkOffset` | `INDX` 块起始偏移；无索引时为 `0` |
| 56 | 4 | uint32 | `frameStrideBytes` | 定长帧块字节数；变长或压缩时为 `0` |
| 60 | 4 | uint32 | `headerCrc32c` | 前 60 字节的 CRC-32C 校验和 |

魔数沿用 PNG 的思路：首字节为 >0x7F 的非 ASCII 值，随后是一对 CR/LF。
这样任何「文本模式」传输或换行转换都会立刻破坏魔数，从而被识别为损坏文件而不是被误解为文本。

### 4.1 `flags` 位定义

| 位 | 名称 | 含义 |
|---:|------|------|
| 0 | `EPSF_LITTLE_ENDIAN` | 固定为 `1`；为 `0` 表示需要字节序转换（保留） |
| 1 | `EPSF_FLOAT32_GEOMETRY` | 位姿与速度用 `float32` 存储；默认 `0`（`float64`） |
| 2 | `EPSF_ZSTD_FRAMES` | 帧载荷经 zstd 压缩；默认 `0` |
| 3 | `EPSF_VARIABLE_STRIDE` | 帧块长度可变，必须依赖 `INDX` 定位；默认 `0` |
| 4 | `EPSF_ZSTD_SCENE` | 场景块经 zstd 压缩 |
| 5–31 | 保留 | 必须为 `0`；读取端忽略未知位但仍应警告 |

### 4.2 采样间隔与求解步长（已确定：抽帧）

【已决策】采用「抽帧」语义：**求解步长固定为 $h = 0.5\ \text{ms}$，与采样间隔无关**；
采样间隔只决定「每多少步记录一帧」：

$$
k = \frac{\Delta t_{\text{rec}}}{h}, \qquad \Delta t_{\text{rec}} \in \{1, 2, 5, 10, 20\}\ \text{ms}
$$

因为 $1, 2, 5, 10, 20$ 都是 $0.5$ 的整数倍，五档采样率都能整除，不会出现累积漂移。

【已决策】**默认采样间隔为 10 ms**（下表中以「默认」标出）。采样间隔仍然是一个可配置参数：
`sampleIntervalNs` 继续作为文件头的必备字段写入（§4），五档取值全部保留，可在求解开始前指定，
因此「默认 10 ms」只影响未显式指定时的取值，不改变参数本身的存在与否。

| 采样率 | 抽帧比 $k$ | 10 s 模拟的求解步数 | 记录帧数 |
|-------|----------:|------------------:|--------:|
| 1 ms | 2 | 20 000 | 10 001 |
| 2 ms | 4 | 20 000 | 5 001 |
| 5 ms | 10 | 20 000 | 2 001 |
| **10 ms（默认）** | 20 | 20 000 | 1 001 |
| 20 ms | 40 | 20 000 | 501 |

帧数按 $F = \lfloor T / \Delta t_{\text{rec}} \rfloor + 1$ 计算，第 0 帧为初始状态（$t = 0$，步号 0），
第 $j$ 帧对应求解器第 $j \cdot k$ 步与时刻 $j \cdot \Delta t_{\text{rec}}$。

由于帧号与步号不再一一对应，文件头与帧头同时记录两者：`frameIndex` 供播放器寻址，
`stepIndex` 供调试时把某一帧对回求解器的第几步。

【已核实】项目**当前**的实际求解步长不是 0.5 ms，证据如下：

| 证据 | 内容 |
|------|------|
| `src/view_model/simulator/scene_manager.h:455` | `double m_fixedStepSeconds{1.0 / 120.0};`，即默认 ≈ 8.333 ms（120 Hz） |
| 全局检索 `setFixedStepMs` | 除定义与 QML 只读展示（`SimulatorUnit.qml:1122`）外，**没有任何调用点** |
| `git log -S"1.0 / 2000"` | 全历史无匹配，历史上从未使用过 2 kHz 步长 |

因此 0.5 ms 需要在实现离线求解器时**新建**为求解器侧常量（例如 `SolverTimeConfig::solverStepSeconds = 5e-4`）。

实现时**不要**顺手把现有实时路径的默认步长改成 0.5 ms：`SceneManagerViewModel` 每 16 ms 最多补偿
5 个子步（`m_maxSubSteps{5}`），步长 0.5 ms 时每帧只能推进 2.5 ms 仿真时间，实时预览会退化成
约 15% 速度的慢动作，同时把 CPU 消耗抬高一个数量级。0.5 ms 属于新的离线求解器；
旧的实时路径在「求解器 / 播放器拆分」时整体退役。

### 4.3 三种「节拍」必须分清

系统里同时存在三个互不相同的时间量。把它们混为一谈，既会误判现有实现，也会把离线方案设计错：

| 名称 | 含义 | 当前实现 | 离线 / 播放方案中的取值 |
|------|------|---------|----------------------|
| **求解推进步长** $h$ | 求解器一次 `step()` 推进的物理时间 | `m_fixedStepSeconds = 1/120 s` ≈ 8.333 ms | 0.5 ms，固定不变 |
| **采样间隔** $\Delta t_{\text{rec}}$ | 每隔多久把状态写入一帧 | 不存在（当前没有任何记录机制） | 用户在 1 / 2 / 5 / 10 / 20 ms 中选择 |
| **渲染帧率** | 把状态画到屏幕上的频率 | 由 16 ms 的 `QTimer` 触发；`View3D` 的实际呈现跟随显示器 vsync | 播放器自行决定（目标 120 Hz，见下方待确认项） |

需要特别澄清的一点：**当前代码里并没有独立的渲染节拍**。
`SceneManagerViewModel::advanceFixedStep()` 每完成一次求解步，就同步调用
`refreshBodyModel / refreshConstraintModel / refreshCurveModel / refreshConvexMeshModel`
并发出变更信号（`scene_manager.cpp:1291`–`1298`），QML 拿到的就是这份刚刷新的数据。
换句话说，**渲染内容跟着求解步走**，两者是耦合的——这正是需求 2 要解决的核心问题。

由此也可确认：代码里唯一写死的「120」是**求解推进步长**（`1/120 s`），不是渲染帧率；
而「渲染按 120 Hz 进行」目前只是一句设计意图，代码里没有对应的定时器或刷新策略，
真正限制刷新节奏的是 16 ms 的 `QTimer` 与显示器的 vsync。

EPSX 记录的是**采样帧**，抽自求解推进步，与渲染帧无关。播放器显示第 $n$ 个显示帧时，
按当前播放时间 $t$ 取最近的采样帧号：

$$
j = \operatorname{clamp}\left( \left\lfloor \frac{t}{\Delta t_{\text{rec}}} \right\rfloor,\ 0,\ F - 1 \right)
$$

显示帧率与采样率互不约束：20 ms 采样（50 Hz）在 120 Hz 屏幕上会连续显示约 2.4 个显示帧同一内容，
这属于回放的正常表现，不是丢帧。

【待确认】渲染目标 120 Hz 的落实方式：

- 方案 ①：播放器用固定 120 Hz 的刷新定时器驱动，节奏与显示器无关，在 60 Hz 屏上也能按 120 Hz 计算进度（但实际呈现仍受 vsync 限制）；
- 方案 ②：直接跟随显示器 vsync，在 120 Hz 屏上即 120 Hz，在 60 Hz 屏上降为 60 Hz。

另外提示一个数字关系：120 Hz 显示帧为 8.333 ms，与 0.5 ms 求解步之比是 16.67，**不是整数**，
因此「每个显示帧恰好包含整数个求解步」并不成立。二者本来无需对齐；
若你要求严格对齐，应把显示帧率定为 240 Hz（每帧 16 步）。

## 5. 数据块（Chunk）通用结构

每个块由 24 字节定长块头 + 载荷 + 填充组成。

| 偏移 | 长度 | 类型 | 字段 | 说明 |
|-----:|-----:|------|------|------|
| 0 | 4 | FourCC | `tag` | ASCII 块类型，如 `SCNE` |
| 4 | 4 | uint32 | `chunkFlags` | 块级标志，当前保留为 `0` |
| 8 | 8 | uint64 | `payloadLength` | 载荷字节数，不含块头与填充 |
| 16 | 4 | uint32 | `payloadCrc32c` | 载荷的 CRC-32C |
| 20 | 4 | uint32 | `reserved` | 必须为 `0` |

已定义块类型：

| `tag` | 名称 | 必需性 | 说明 |
|-------|------|--------|------|
| `SCNE` | 场景描述块 | 必需 | §6 |
| `META` | 元数据块 | 可选 | 自由的键值对（JSON） |
| `FRAM` | 帧数据块 | 必需 | §8 |
| `INDX` | 帧索引块 | 必需 | §7 |
| `END!` | 尾块 | 必需 | §9 |

## 6. 场景描述块 `SCNE`

载荷为 UTF-8 编码的 JSON 文本（`EPSF_ZSTD_SCENE` 置位时先 zstd 压缩）。
它必须自包含到「有了它就能重建初始状态」的程度，避免播放器依赖外部场景文件。

载荷除「重建初始状态」所需信息外，还声明**帧段布局**（§8），使读取端不必内置版本判断即可跳过节段。

```jsonc
{
  "format": "EPSX",
  "formatVersion": "1.0",
  "generator": { "app": "Egret Physics", "version": "1.0.0", "gitCommit": "7908ad9b" },
  "createdAtUtc": "2026-09-26T12:00:00Z",
  "units": { "length": "m", "mass": "kg", "time": "s", "angle": "rad" },
  "coordinateSystem": { "handedness": "right", "upAxis": "Y", "note": "与 Qt Quick 3D 渲染坐标系的转换在视图层完成" },
  "sampling": {
    "solverStepNs": 500000,      // 求解步长 0.5 ms
    "intervalNs": 10000000,      // 采样间隔 10 ms（默认）
    "decimation": 20,            // 抽帧比 k = intervalNs / solverStepNs
    "frameCount": 1001,
    "durationNs": 10000000000
  },
  "solver": {
    "integrator": "semi_implicit_euler",
    "broadPhase": "bvh",
    "contactResolver": "frictionless",
    "config": { "velocityIterations": 8, "positionIterations": 2, "penetrationSlop": 0.001,
                "positionCorrectionBeta": 0.2, "maxPositionCorrectionPerStep": 0.2 }
  },
  "frameSections": [
    { "type": "BODY", "recordVersion": 1, "strideBytes": 144, "count": 3 },
    { "type": "CONS", "recordVersion": 1, "strideBytes": 128, "count": 1 },
    { "type": "FLDS", "recordVersion": 1, "strideBytes": 88,  "count": 1 }
  ],
  "entities": [
    { "slot": 0, "id": 1, "name": "Ground", "kind": "rigid_body", "shape": "box",
      "shapeParams": { "size": [10000, 10000, 130] }, "mass": 0, "restitution": 0.2,
      "initialTransform": { "position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] } }
  ],
  "constraints": [ { "slot": 0, "id": 9, "name": "Pendulum", "type": "simple_pendulum",
                     "params": { "length": 3.0, "anchor": [0, 5, 0], "entityId": 1 } } ],
  "fields": [ { "slot": 0, "id": 2, "name": "Gravity", "type": "gravity",
                "params": { "g": [0, -9.81, 0] } } ],
  "curves": [ { "id": 11, "name": "Rail", "type": "bezier", "params": { "controlPoints": [] } } ],
  "convexMeshes": [ { "id": 12, "name": "Rock", "vertices": [], "indices": [], "triangles": [] } ]
}
```

说明：

- `sampling.decimation` 即 §4.2 的抽帧比 $k$，由求解器写入，播放器读它来把帧号换算成时间；
- `frameSections` 按帧内出现顺序声明段，`strideBytes` 与 `count` 给出该段的元素布局与元素个数；
- `entities` / `constraints` / `fields` 的 `slot` 决定该对象在对应帧段中的位置，
  `slot` 与对象 `id` 分离，便于将来支持动态增删（见 §8.7）；
- `curves` / `convexMeshes` 是**静态几何**，只在 `SCNE` 中记录一次（理由见 §8.6）；
- 读取端对 `frameSections` 中的未知 `type` 按段头长度直接跳过，不报错。

## 7. 帧索引块 `INDX`

索引块**总是存在**，载荷为 `frameCount` 个定长条目：

| 长度 | 类型 | 字段 |
|-----:|------|------|
| 8 | uint64 | `frameOffset`（`FRAM` 块头起始偏移） |
| 4 | uint32 | `frameChunkLength`（含块头与填充的总长度） |
| 4 | uint32 | `framePayloadLength`（压缩后载荷长度） |

索引块放在文件尾部（类似 ZIP 的中央目录）：写入端流式追加完所有帧之后再一次性写出，
既不预留空间也不移动数据；读取端则只有一条寻址路径，不必区分定长 / 变长 / 压缩三种情况，
代价只是每帧 16 字节的索引开销。

## 8. 帧数据块 `FRAM`

一帧 = 24 字节帧头 + 若干帧段。帧段的种类、顺序与元素个数由 `SCNE.frameSections` 声明，逐帧一致。

### 8.1 帧头（24 字节）

| 偏移 | 长度 | 类型 | 字段 | 说明 |
|-----:|-----:|------|------|------|
| 0 | 4 | uint32 | `frameIndex` | 从 0 开始 |
| 4 | 4 | uint32 | `frameFlags` | 帧级标志，当前保留为 `0` |
| 8 | 8 | uint64 | `simTimeNs` | 第 $j$ 帧为 $j \times \texttt{intervalNs}$ |
| 16 | 8 | uint64 | `stepIndex` | 第 $j$ 帧对应求解器第 $j \times k$ 步 |

### 8.2 帧段（Section）

每个帧段 = 32 字节段头 + 元素数组，元素采用定长 AoS 布局。

| 偏移 | 长度 | 类型 | 字段 | 说明 |
|-----:|-----:|------|------|------|
| 0 | 4 | FourCC | `sectionType` | `BODY` / `CONS` / `FLDS` / `CRVS` / `MESH` / `USER` |
| 4 | 4 | uint32 | `elementCount` | 元素个数 |
| 8 | 4 | uint32 | `strideBytes` | 单个元素的字节数（定长，含对齐填充） |
| 12 | 4 | uint32 | `sectionFlags` | 当前保留为 `0` |
| 16 | 8 | uint64 | `payloadLength` | 元素数组总字节数，应等于 `elementCount × strideBytes` |
| 24 | 4 | uint32 | `payloadCrc32c` | 元素数组的 CRC-32C |
| 28 | 4 | uint32 | `recordVersion` | 元素记录的版本号，用于同段内字段扩展 |

第 $i$ 个元素从段头偏移 $32 + i \times \texttt{strideBytes}$ 处开始，定长，
因此「取某一帧里某个实体 / 约束 / 场的数据」是 $O(1)$，也允许将来把单个段独立压缩或惰性解码。

**为什么由 v0.1 的 SoA 改为 AoS**：v0.1 假定帧内只有刚体一种记录，SoA 更利于压缩与 SIMD；
而记录范围现已扩大为全部对象类型，帧内出现的是若干**异构段**，每段元素短小、类型各异。
AoS + `strideBytes` 让所有段共用同一套「定位 / 跳过」逻辑，无需为每种类型单独定义字段偏移表。
压缩率的损失在整帧 zstd 之下可以忽略。

下表的「偏移」列按 f64 模式给出；f32 模式保持字段顺序不变，
每个浮点字段长度减半，故其偏移需按字段顺序重新累加（各段末尾给出 f32 的 `strideBytes`）。

### 8.3 `BODY` 刚体段

| 偏移 | f64 / f32 长度 | 类型 | 字段 | 说明 |
|-----:|---------------|------|------|------|
| 0 | 8 / 8 | uint64 | `entityId` | 稳定实体 ID |
| 8 | 4 / 4 | uint32 | `slot` | 在 `SCNE.entities` 中的槽位 |
| 12 | 4 / 4 | uint32 | `stateFlags` | 位 0 休眠、位 1 参与碰撞、位 2 参与积分、位 3 静态体 |
| 16 | 24 / 12 | f64[3] / f32[3] | `position` | 世界坐标位置，米 |
| 40 | 32 / 16 | f64[4] / f32[4] | `rotation` | 四元数 $x, y, z, w$，已归一化 |
| 72 | 24 / 12 | 同上 | `linearVelocity` | 质心线速度，米/秒 |
| 96 | 24 / 12 | 同上 | `angularVelocity` | 角速度，弧度/秒 |
| 120 | 8 / 4 | f64 / f32 | `mass` | 质量；0 表示静态体 |
| 128 | 8 / 4 | f64 / f32 | `restitution` | 恢复系数 |
| 136 | 8 / 4 | 保留 | `reserved` | 必须为 0 |

`strideBytes`：f64 模式 **144**，f32 模式 **80**（含 4 字节对齐填充）。

### 8.4 `CONS` 约束段

| 偏移 | f64 / f32 长度 | 类型 | 字段 | 说明 |
|-----:|---------------|------|------|------|
| 0 | 8 / 8 | uint64 | `constraintId` | 稳定约束 ID |
| 8 | 4 / 4 | uint32 | `slot` | 在 `SCNE.constraints` 中的槽位 |
| 12 | 4 / 4 | uint32 | `constraintType` | 与 `SCNE.constraints[].type` 的枚举取值一致 |
| 16 | 4 / 4 | uint32 | `flags` | 位 0 启用 |
| 20 | 4 / 4 | 保留 | `reserved` | 必须为 0 |
| 24 | 8 / 8 | uint64 | `entityAId` | 0 表示无 |
| 32 | 8 / 8 | uint64 | `entityBId` | 0 表示无 |
| 40 | 24 / 12 | vec3 | `anchorAWorld` | 约束在实体 A 上的世界锚点 |
| 64 | 24 / 12 | vec3 | `anchorBWorld` | 世界锚点 B（或固定锚点） |
| 88 | 8 / 4 | f64 / f32 | `restLength` | 约束标称长度 / 目标长度 |
| 96 | 8 / 4 | f64 / f32 | `currentLength` | 当前实际长度 |
| 104 | 8 / 4 | f64 / f32 | `error` | 当前约束误差 |
| 112 | 8 / 4 | f64 / f32 | `forceMagnitude` | 当前约束力或力矩大小 |
| 120 | 8 / 4 | 保留 | `reserved` | 必须为 0 |

`strideBytes`：f64 模式 **128**，f32 模式 **88**。

约束的**静态参数**（目标长度、弹簧劲度、滑轨方向等）写在 `SCNE.constraints[].params` 里，
帧段只承载逐帧变化的量。若某类约束有额外的逐帧状态（例如阻尼累计、内部迭代缓存），
在同类文件里追加一个 `USER` 段承载，保持 `CONS` 段的布局固定。

### 8.5 `FLDS` 力场段

| 偏移 | f64 / f32 长度 | 类型 | 字段 | 说明 |
|-----:|---------------|------|------|------|
| 0 | 8 / 8 | uint64 | `fieldId` | 稳定场 ID |
| 8 | 4 / 4 | uint32 | `slot` | 在 `SCNE.fields` 中的槽位 |
| 12 | 4 / 4 | uint32 | `fieldType` | 与 `SCNE.fields[].type` 的枚举取值一致 |
| 16 | 4 / 4 | uint32 | `flags` | 位 0 启用 |
| 20 | 4 / 4 | 保留 | `reserved` | 必须为 0 |
| 24 | 24 / 12 | vec3 | `vector` | 场方向 / 重力矢量 / 主轴 |
| 48 | 24 / 12 | vec3 | `center` | 场中心，点场与质心场使用 |
| 72 | 8 / 4 | f64 / f32 | `strength` | 场强 / 常量系数 |
| 80 | 8 / 4 | 保留 | `reserved` | 必须为 0 |

`strideBytes`：f64 模式 **88**，f32 模式 **56**。

重力场这类参数恒定的场，逐帧记录看似冗余，但代价只有 88 字节/帧，
换来的是「任何一帧都能独立复现施加在实体上的场」这一自证性质，值得保留。

### 8.6 静态几何：`CRVS` 与 `MESH`

曲线（`CurveBase` 派生：贝塞尔、圆锥曲线、螺旋线）与凸体网格（`ConvexMesh`）在模拟过程中不发生变化——
它们由场景定义、不参与积分，因此只在 `SCNE` 中记录一次，不在每帧重复。
判断标准是「该数据是否会随求解步变化」，而不是「它属于哪一类对象」。

`CRVS` / `MESH` 段类型已预留；将来若出现可动画的曲线或网格，把它们加进 `frameSections` 即可，
旧读取端会按段头长度自动跳过。

【待确认】如果你要求严格「每帧都记录曲线与网格的全部数据」，需把这两类也声明进 `frameSections`。
代价是每帧重复存一份静态几何（顶点数 × 帧数），文件体积可能膨胀一到两个数量级；
若确实需要，建议改用「首帧存全量、后续帧存引用」的变体，而不是每帧全量重存。

### 8.7 精度模式与实体生命周期

精度由文件头 `EPSF_FLOAT32_GEOMETRY` 决定：

- 位为 0（默认）：全部浮点字段用 `float64`。调试抖动的关键是位置差的量级，可能只有 $10^{-6}$ m；
  `float32` 约 7 位十进制有效数字，在坐标本身较大时会直接吃掉这些差异。
- 位为 1：全部浮点字段用 `float32`，体积减半，适合只做可视化回放的场合。

【待确认】v1 采用最简规则：**一次模拟内实体集合固定**，槽位在 `SCNE` 中一次性定义，所有帧元素数相同。
模拟过程中新增 / 删除实体（含约束创建时附带生成的实体）应在开始记录之前完成。
若确实需要动态增删，v2 引入「场景版本号 + 增量实体表段」，此处不展开。

### 8.8 压缩与单帧体积

`EPSF_ZSTD_FRAMES` 置位时，整个 `FRAM` 载荷（帧头 + 全部帧段）作为**一个 zstd 帧**压缩，
此时帧长度可变，文件头的 `frameStrideBytes` 置 0，寻址一律走 `INDX`。

体积以 `BODY` 段为主：$N \times 144$ 字节（f64）。$N = 100$ 时约 14.1 KiB/帧：

| 采样间隔 | 帧数（10 s） | 未压缩 | zstd 后（估计） |
|---------|------------:|-------:|---------------:|
| 1 ms | 10 001 | ≈ 141 MiB | 7–28 MiB |
| 2 ms | 5 001 | ≈ 70 MiB | 3.5–14 MiB |
| 5 ms | 2 001 | ≈ 28 MiB | 1.4–5.6 MiB |
| **10 ms（默认）** | 1 001 | ≈ 14 MiB | 0.7–2.8 MiB |
| 20 ms | 501 | ≈ 7 MiB | 0.4–1.5 MiB |

压缩率为经验估计，需在实现后用真实场景标定，标定结果回填本表。

## 9. 尾块 `END!`

| 偏移 | 长度 | 类型 | 字段 | 说明 |
|-----:|-----:|------|------|------|
| 0 | 8 | uint64 | `frameCount` | 与文件头一致，用于交叉校验 |
| 8 | 8 | uint64 | `indexChunkOffset` | 与文件头一致 |
| 16 | 4 | uint32 | `totalFileLength`（低 32 位） | 便于流式截断检测 |
| 20 | 4 | uint32 | `reserved` | 必须为 `0` |

尾块存在即说明本次写入正常收尾；缺失尾块说明写入被打断，
读取端应给出「文件可能不完整」的警告，并允许播放已完整写入的帧。

## 10. 读写流程

### 10.1 求解器（写入）

```text
1. 建立场景，固定实体 / 约束 / 场集合，按类型分配槽位 slot
2. 写文件头（frameCount = 0，indexChunkOffset = 0 占位）
3. 写 SCNE 块（实体表、约束与场参数、求解器配置、solverStepNs、intervalNs、decimation）
4. 记录第 0 帧（t = 0，stepIndex = 0，frameIndex = 0）
5. 求解循环 step = 1 .. floor(T / h)：
   5.1 调用 solver.step(0.5 ms)
   5.2 若 step 能被 k 整除：采集本步的 BODY / CONS / FLDS 段，
       写 FRAM 块（frameIndex = step / k，stepIndex = step）
6. 写 INDX 块（记录每帧偏移，总是写出）
7. 写 END! 块
8. 回填文件头的 frameCount、indexChunkOffset、frameStrideBytes、headerCrc32c
```

第 5 步可以边算边写：内存中不保留整段历史，仅保留「当前帧」与写文件缓冲。
这正好满足「存储到内存或文件」两种模式——两者共用同一套帧编码，内存模式把块依次压入
`QByteArray`/`std::vector<std::byte>`，文件模式直接落到 `QFile`。

### 10.2 播放器（读取）

```text
1. 读 64 字节文件头，校验魔数与 headerCrc32c
2. 校验 versionMajor 是否可读；flags 中是否有未知位
3. 定位 SCNE，解析实体表，建立 slot → 渲染项 的映射
4. 定位 INDX，建立帧偏移表
5. 播放：
   - 顺序播放：按采样间隔推进帧号（可选倍速）
   - 拖动 / 输入时间：frame = clamp(round(t / interval), 0, frameCount-1)，
     由偏移表 $O(1)$ 定位并解码该帧
6. 每一帧只刷新渲染项（实体位姿、约束几何、场面可视化），不触发任何物理解算
```

由于播放不再依赖实时计时器，帧率不足时表现为播放变慢（或跳帧），
而不会像当前实时循环那样出现「死亡螺旋」与追帧。

## 11. 命令行接口

主程序可执行文件记为 `name`（当前即 `Egret_Physics.exe`）。

| 命令 | 含义 |
|------|------|
| `name --parse epsx <integer>` / `name -p epsx <integer>` | 解析指定帧，把该帧全部实体数据以 JSON 打印到标准输出 |
| `name --play <file.epsx>` / `name --replay <file.epsx>` / `name -r <file.epsx>` | 以播放器模式打开文件（Windows 双击关联使用此入口） |
| `name --solve <scene.json> --duration <seconds> --sample <1\|2\|5\|10\|20> -o <file.epsx>` | 求解器模式，离线求解并写出模拟文件 |

`--solve` 的 `--sample` 省略时取**默认值 10 ms**；五档取值全部保留，也可在界面侧改为下拉选择。

### 11.1 `--parse` 的行为约定

- 参数顺序固定为「对象类型 + 整数」。`text` 目前只接受 `epsx` 一个取值
  （指**格式名** EPSX，与扩展名 `.epsx` 无关），其他取值报错并返回非零退出码；
- 帧号从 0 开始，越界时返回非零退出码与明确错误信息；
- 输出为**单行 JSON**（便于管道与脚本处理），写入标准输出；错误信息写入标准错误；
- 输出内容至少包含帧级字段 `frameIndex`、`stepIndex`、`simTimeNs`、`intervalNs`、`solverStepNs`，
  以及每个实体的 `id`、`name`、`position`、`rotation`、`linearVelocity`、`angularVelocity`、
  `stateFlags`，并包含每个约束与每个力场在该帧的数据（字段与 §8.4、§8.5 的段布局一一对应）；
- 退出码 `0` 表示成功，非 `0` 表示参数错误、文件损坏或帧号越界。

### 11.2 `--play` 的缩写选择

`-p` 已按你的要求固定给 `--parse`，`--play` 需要另选一个字母。候选与判断：

| 候选 | 语义 | 判断 |
|------|------|------|
| `-r` | replay（回放） | **推荐**：与「回放已算好的模拟文件」语义最贴合，与 `-p` 不冲突 |
| `-l` | load（载入） | 可用，但本程序里「载入」更常用于载入场景 JSON，易与 `--solve` 的输入混淆 |
| `-v` | view（查看） | 排除：`-v` 惯例留给 `--version` |
| `-o` | open（打开） | 排除：`-o` 惯例留给 `--output`，与 `--solve -o` 直接冲突 |
| `-P` | play（大写） | 排除：仅靠大小写区分两个命令，在脚本与人工输入中极易出错 |

结论：短选项定为 **`-r`**，长选项同时接受 `--play` 与 `--replay`。
Windows 文件关联中使用长选项 `--play "%1"`，可读性更好。

## 12. 与成熟二进制格式的设计对照

本格式的每一处选择都对应一个已被广泛使用的先例，而非凭空设计：

| 本格式的设计 | 参照格式 | 借鉴点 |
|-------------|---------|--------|
| 8 字节魔数含非 ASCII 字节与 CRLF | PNG | 检测文本模式传输导致的损坏 |
| FourCC + 长度 + CRC 的数据块 | PNG、RIFF | 可跳过未知块，逐块校验 |
| 自描述 JSON 头块 | glTF（`.glb` 的 JSON chunk）、3D Tiles | 用可读文本描述结构，二进制只承载大数组 |
| 尾部中央索引 | ZIP 中央目录、MCAP 索引 | 流式追加写完后一次性写索引，支持 $O(1)$ 随机访问 |
| 元素化、可扩展的二进制流 | EBML（Matroska/WebM）、ROS `.bag` | 版本兼容与流式解析 |
| CRC-32C + 分块校验 | iSCSI、ext4 元数据校验、zstd 帧 | 比 CRC-32 更易用硬件指令加速，逐块定位损坏 |
| zstd 无损压缩 | 现代日志/列存格式 | 高压缩比与高解压速度，支持流式解码 |
| 定长步长 + 逐帧状态快照 | 游戏 demo 录制（Quake `.dem`、SourceTV）、飞行数据记录 | 回放与逐帧比对的标准做法 |

本项目已有的追踪导出（`SceneManagerViewModel::exportTrackingAsync` → QXlsx）面向人工查看，
帧率低、字段少、文件体积大，无法承担「全实体 × 高采样率 × 可随机访问」的回放需求，
因此 EPSX 作为**二进制回放格式**与 XLSX 导出并存，不相互替代。

## 13. 待确认问题清单

本轮已确定的议题：

| 议题 | 结论 | 出处 |
|------|------|------|
| 采样间隔语义 | 抽帧 | §4.2 |
| 求解步长 | 0.5 ms，需在离线求解器中新建 | §4.2 |
| 帧记录范围 | 全部对象类型；曲线与网格按静态几何记录一次 | §8.3–§8.6 |
| 扩展名 | `.epsx` | §2.1 |
| `--play` 缩写 | `-r` / `--replay` | §11.2 |
| 采样间隔默认值 | **10 ms**；`sampleIntervalNs` 参数与五档取值全部保留 | §4.2 |

仍需确认：

1. 曲线与凸体网格是否**确实**要逐帧记录（§8.6）？当前默认按静态几何处理，只在 `SCNE` 记录一次。
2. 默认精度用 `float64`（推荐，便于调试抖动）还是 `float32`？
3. 是否接受「一次模拟内实体集合固定」的 v1 简化（§8.7）？
4. `--parse` 的输出格式用单行 JSON（推荐）还是 CSV / 自定义文本？
5. 求解器与播放器是同一可执行文件的不同模式，还是拆成两个可执行文件？
   需求中提到「双击文件直接启动播放器」，同一可执行文件 + `--play` 即可满足；
   拆分成两个可执行文件则双击关联更直观，但需要额外处理 Qt 部署与共享代码。
6. `--solve` 的输入用场景 JSON（`WorldSceneManager::loadSceneFromJsonFile` 的格式），
   还是直接沿用编辑器当前场景？
7. 渲染目标 120 Hz 的落实方式：固定 120 Hz 刷新定时器，还是跟随显示器 vsync（§4.3）？
8. 「非常低的推进步长」具体取多少？当前草案按 **0.5 ms** 写。若你指的是更小的值
   （例如 0.1 ms），抽帧比、步数与文件体积估算都要相应调整。

## 14. 参考文献

以下条目为设计依据，链接【待核实】（当前环境无法联网）：

1. PNG (Portable Network Graphics) Specification, Version 1.2 — 魔数与 chunk 结构。
2. Microsoft RIFF (Resource Interchange File Format) 规范 — FourCC + 长度 + 4 字节对齐。
3. Khronos Group. *glTF 2.0 Specification*（含 `.glb` 二进制容器）— JSON 头块 + 二进制数据块。
4. Matroska / EBML 规范 — 元素化、可扩展的二进制容器。
5. MCAP File Format Specification（Foxglove）— 高吞吐带索引的时序数据容器。
6. PKWARE. *ZIP APPNOTE*，§4.3.6 中央目录 — 尾部索引。
7. IETF RFC 3720 (iSCSI)，附录 B — CRC-32C（Castagnoli）多项式与实现。
8. Facebook / Meta. *Zstandard Compression* 格式规范。
9. Erin Catto. *Box2D* 源码与文档 — 位置修正上限 `b2_maxLinearCorrection`、Baumgarte 稳定化与穿透容差 `slop`。
10. Sutherland et al. 关于时域数据记录与回放格式的讨论（游戏 demo 录制）— 逐帧状态快照 + 时间戳索引。
