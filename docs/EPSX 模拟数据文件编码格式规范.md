# EPSX（Egret Physics Simulation eXtended）模拟数据文件编码格式规范

> **状态：现行实现规范 v1.0（2026-10-03）**
> 本文档**逐字节**描述 `.epsx` 文件的编码方式，内容与实现一一对应：
> `src/view_model/simulation_serializer/epsx_format.h`（常量与记录布局）、
> `epsx_writer.cpp`（写出）、`epsx_reader.cpp`（读取）。
>
> 设计背景、扩展名决策与命令行约定见同目录《EPSX 模拟数据文件格式规范（草案）.md》；
> 两份文档若有不一致，**以实现与本文档为准**。

## 1. 总体布局

```
┌───────────────────────────┐
│ 文件头 File Header (64 B) │
├───────────────────────────┤
│ 场景描述块 SCNE (1 个)     │  ← 载荷为 UTF-8 的 JSON 文本
├───────────────────────────┤
│ 帧数据块 FRAM (N 个)       │  ← 每帧一个块，按帧号顺序写出
├───────────────────────────┤
│ 帧索引块 INDX (1 个)       │  ← 16 B/条，O(1) 定位任意帧
├───────────────────────────┤
│ 尾块 END! (1 个)          │  ← 写入正常收尾的标志
└───────────────────────────┘
```

- 所有多字节整数与浮点数**一律小端序（little-endian）**；文件头 `flags` 的 bit0 固定置位表示该约定。
- 所有数据块**从文件起始位置起 8 字节对齐**，块结束后补 0 至对齐边界；填充字节不计入 `payloadSize`，但计入块总长度。
- 逐字段落盘，**不直接写结构体内存布局**（避免对齐/填充差异）。
- 字符串（场景 JSON、生成者描述）统一 UTF-8 编码，不带 BOM、不带结尾 NUL。
- 校验算法统一为 **CRC-32C（Castagnoli）**：反射多项式 `0x82F63B78`，初值 `0xFFFFFFFF`，结果异或 `0xFFFFFFFF`（即 iSCSI 使用的标准 CRC-32C）。

## 2. 文件头（64 字节）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | bytes | `magic` | `89 45 50 53 58 0D 0A 1A`，即 `0x89 'E' 'P' 'S' 'X' CR LF SUB` |
| 8 | 2 | `u16` | `versionMajor` | 主版本号，当前 `1` |
| 10 | 2 | `u16` | `versionMinor` | 次版本号，当前 `0` |
| 12 | 4 | `u32` | `flags` | 见 §2.1 |
| 16 | 8 | `u64` | `frameCount` | 实际写入的帧数；打开文件时先写 0，`close()` 时回填 |
| 24 | 8 | `u64` | `durationNs` | 总时长（纳秒） |
| 32 | 4 | `u32` | `sampleIntervalNs` | 采样间隔（纳秒），只允许 1/2/5/10/20 ms |
| 36 | 4 | `u32` | `bodyCount` | 场景刚体数量（帧段校验用） |
| 40 | 8 | `u64` | `sceneChunkOffset` | SCNE 块起始偏移 |
| 48 | 8 | `u64` | `indexChunkOffset` | INDX 块起始偏移；未正常收尾时为 0 |
| 56 | 4 | `u32` | `frameStrideBytes` | 首帧块总长度；各帧长度不一致时为 0（变长） |
| 60 | 4 | `u32` | `headerCrc32c` | **前 60 字节**的 CRC-32C（该字段自身不参与） |

### 2.1 `flags` 位定义

| 位 | 名称 | 当前状态 |
|----|------|----------|
| 0 | `LittleEndian` | **固定置位**（小端序标记） |
| 1 | `Float32Geometry` | 保留，未实现；读取器遇到置位会明确报错 |
| 2 | `ZstdFrames` | 保留，未实现；同上 |
| 3 | `VariableStride` | 保留，未实现；同上 |
| 4 | `ZstdScene` | 保留，未实现；同上 |
| 5–31 | — | 保留，必须写 0；读取器视为不支持标志位 |

## 3. 数据块通用头（24 字节）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 4 | FourCC | `tag` | `SCNE` / `FRAM` / `INDX` / `END!` |
| 4 | 4 | `u32` | `chunkFlags` | 预留，当前写 0 |
| 8 | 8 | `u64` | `payloadSize` | 载荷字节数（不含块头与填充） |
| 16 | 4 | `u32` | `payloadCrc32c` | 载荷的 CRC-32C |
| 20 | 4 | `u32` | `reserved` | 0 |

块总长度 = `24 + payloadSize + padding`，其中 `padding` 使块总长度满足 8 字节对齐；填充字节写 0。
未知 `tag` 的块按 `payloadSize` 跳过（前向兼容）。

## 4. 场景描述块 `SCNE`

载荷即场景 JSON 原文（UTF-8）。当前由离线求解器生成的场景描述形如：

```json
{
  "format": "EPSX",
  "formatVersion": "1.0",
  "generator": "Egret Physics 1.0.0",
  "entities": [ { "id": 123, "slot": 0, "name": "ball", "mass": 1, "restitution": 1, "fixed": false } ],
  "models": [ /* 见 §4.1 */ ],
  "sampling": { "solverStepNs": 500000, "intervalNs": 10000000, "decimation": 20,
                "frameCount": 101, "durationNs": 1000000000 },
  "solver":  { "integrator": "velocity_verlet", "broadPhase": "octree",
               "contactResolver": "frictional", "velocityIterations": 8,
               "positionIterations": 2, "positionCorrectionBeta": 0.2,
               "penetrationSlop": 0.001 },
  "frameSections": [ { "type": "BODY", "strideBytes": 144, "recordVersion": 1, "count": 1 } ]
}
```

播放端**只依赖 `models` 数组**重建场景（`SceneManagerViewModel::createSceneFromJsonString`），
其余键供工具与自描述使用。

### 4.1 `models[]` 条目

公共键：

| 键 | 类型 | 说明 |
|----|------|------|
| `category` | int | `0` 刚体 / `1` 力场 / `2` 约束 |
| `type` | int | 形状类型、场类型或约束类型，取值见 §4.2 |
| `id` | **字符串** | 稳定 ID（十进制文本，64 位无符号；播放端按 `slot` 对齐，不依赖该值） |
| `name` | string | 实体名称（仅显示用） |
| `pos` | `[x,y,z]` | 初始位置（米） |

`category = 0`（刚体）另有：

| 键 | 类型 | 说明 |
|----|------|------|
| `initial_velocity` | `[x,y,z]` | 初速度（米/秒），缺失按 0 |
| `initial_angular_velocity` | `[x,y,z]` | 初角速度（弧度/秒），缺失按 0 |
| `rotation` | `[w,x,y,z]` | 初始姿态四元数 |
| `scale` | `[x,y,z]` | 尺度，缺失按 `[1,1,1]` |
| `mass` | double | 质量（kg）；`0` 表示静态体 |
| `restitution` | double | 恢复系数 |
| `box_size` | `[x,y,z]` | 仅 `type = 1`（Box） |
| `radius` | double | `type = 2/4/5/6/8/9`（圆柱/圆柱壳/圆盘/圆环/球/球壳） |
| `height` | double | `type = 2/4`（圆柱/圆柱壳） |
| `length` | double | `type = 7`（杆） |

`category = 1`（力场）另有 `field_data` 对象：

| 键 | 适用 `type` | 说明 |
|----|-------------|------|
| `gravity` | `1`（Gravity） | 重力加速度矢量 |
| `reference_point` | `1` | 零势能参考点（可省略，默认原点） |
| `speed` | `2`（Gravitation） | 场源速度 |
| `mass` | `2` | 场源质量 |
| `coupling` | `2` | 耦合系数（可省略，默认万有引力常量） |
| `fixed` | `2` | 场源是否固定 |

`category = 2`（约束）另有 `constraint_data` 与 `entity_ids` 数组，见草案文档 §6。

### 4.2 类型取值

| 类别 | 取值 |
|------|------|
| 形状 `ShapeType` | `1` Box、`2` Cylinder、`3` Capsule、`4` CylindricalShell、`5` Disk、`6` Ring、`7` Rod、`8` Sphere、`9` SphericalShell、`10` ConvexMesh |
| 力场 `FieldType` | `1` Gravity、`2` Gravitation |
| 约束 `ConstraintType` | `1` ConnectingLine、`2` SimplePendulum、`3` ConnectingRod、`5` Spring、`7` AirResistance |

> 当前加载器可重建的解析形状为 Box / Cylinder / CylindricalShell / Disk / Ring / Rod / Sphere / SphericalShell；
> `Capsule` 与 `ConvexMesh`（网格数据）暂无法从 JSON 还原，遇到时会被跳过并记录警告。

## 5. 帧数据块 `FRAM`

每帧一个 `FRAM` 块。载荷 = 帧头（24 B）+ 若干帧段（各 32 B 段头 + 元素数组）。
**空段不写出**，段顺序固定为 `BODY` → `CONS` → `FLDS`。

### 5.1 帧头（24 字节）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 4 | `u32` | `frameIndex` | 帧号，从 0 开始 |
| 4 | 4 | `u32` | `frameFlags` | 预留，0 |
| 8 | 8 | `u64` | `simTimeNs` | 该帧对应的仿真时刻（纳秒） |
| 16 | 8 | `u64` | `stepIndex` | 该帧对应的求解步号 |

第 0 帧为场景初始状态，`simTimeNs = 0`、`stepIndex = 0`。

### 5.2 帧段头（32 字节）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 4 | FourCC | `tag` | `BODY` / `CONS` / `FLDS` |
| 4 | 4 | `u32` | `elementCount` | 元素个数 |
| 8 | 4 | `u32` | `strideBytes` | 单元素字节数：144 / 128 / 88 |
| 12 | 4 | `u32` | `reserved` | 0 |
| 16 | 8 | `u64` | `payloadBytes` | 元素数组总字节数（= `elementCount × strideBytes`） |
| 24 | 4 | `u32` | `payloadCrc32c` | 元素数组的 CRC-32C |
| 28 | 4 | `u32` | `recordVersion` | 记录布局版本，当前 `1` |

### 5.3 `BODY` 刚体记录（144 字节/条）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | `u64` | `id` | 稳定实体 ID |
| 8 | 4 | `u32` | `slot` | 场景实体表槽位（播放端按此对齐） |
| 12 | 4 | `u32` | `stateFlags` | bit0 休眠、bit1 参与碰撞、bit2 参与积分、bit3 静态体 |
| 16 | 24 | 3×`f64` | `position` | 世界坐标 (x, y, z)，单位米 |
| 40 | 32 | 4×`f64` | `rotation` | 姿态四元数 **(x, y, z, w)** |
| 72 | 24 | 3×`f64` | `linearVelocity` | 线速度（米/秒） |
| 96 | 24 | 3×`f64` | `angularVelocity` | 角速度（弧度/秒） |
| 120 | 8 | `f64` | `mass` | 质量（kg），0 表示静态 |
| 128 | 8 | `f64` | `restitution` | 恢复系数 |
| 136 | 8 | `u64` | `reserved` | 0 |

### 5.4 `CONS` 约束记录（128 字节/条）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | `u64` | `id` | 约束 ID |
| 8 | 4 | `u32` | `slot` | 场景约束表槽位 |
| 12 | 4 | `u32` | `type` | 约束类型（§4.2） |
| 16 | 4 | `u32` | `flags` | bit0 启用 |
| 20 | 4 | `u32` | `reserved` | 0 |
| 24 | 8 | `u64` | `entityAId` | 关联实体 A 的 ID，0 表示无 |
| 32 | 8 | `u64` | `entityBId` | 关联实体 B 的 ID，0 表示无 |
| 40 | 24 | 3×`f64` | `anchorAWorld` | 实体 A 上的世界锚点 |
| 64 | 24 | 3×`f64` | `anchorBWorld` | 世界锚点 B（或固定锚点） |
| 88 | 8 | `f64` | `restLength` | 标称/目标长度 |
| 96 | 8 | `f64` | `currentLength` | 当前实际长度 |
| 104 | 8 | `f64` | `error` | 当前约束误差 |
| 112 | 8 | `f64` | `forceMagnitude` | 约束力/力矩大小 |
| 120 | 8 | `u64` | `reserved` | 0 |

### 5.5 `FLDS` 力场记录（88 字节/条）

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | `u64` | `id` | 力场 ID |
| 8 | 4 | `u32` | `slot` | 场景场表槽位 |
| 12 | 4 | `u32` | `type` | 场类型（§4.2） |
| 16 | 4 | `u32` | `flags` | bit0 启用 |
| 20 | 4 | `u32` | `reserved` | 0 |
| 24 | 24 | 3×`f64` | `vector` | 场方向 / 重力矢量 / 主轴 |
| 48 | 24 | 3×`f64` | `center` | 场中心（点场与质心场使用） |
| 72 | 8 | `f64` | `strength` | 场强或常量系数 |
| 80 | 8 | `u64` | `reserved` | 0 |

> **实现现状**：`CONS` / `FLDS` 目前只采集 ID、槽位、类型、启用标志与少量标量，
> 锚点、场矢量、场强等字段暂未从模型层采集，写出值为 0；播放端改由 `SCNE` 重建场景，
> 因此不影响回放。详见 `TODO.md` §6。

## 6. 帧索引块 `INDX`

载荷为定长条目数组，每条 16 字节，第 $k$ 条对应第 $k$ 帧：

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | `u64` | `frameOffset` | 该 `FRAM` 块起始偏移（含块头） |
| 8 | 4 | `u32` | `chunkLength` | 该块总长度（块头 + 载荷 + 填充） |
| 12 | 4 | `u32` | `payloadLength` | 该块载荷字节数 |

条目数由块头 `payloadSize / 16` 推出。索引块**总是写出**。

## 7. 尾块 `END!`

载荷 24 字节：

| 偏移 | 字节 | 类型 | 字段 | 说明 |
|------|------|------|------|------|
| 0 | 8 | `u64` | `frameCount` | 实际写入帧数 |
| 8 | 8 | `u64` | `indexChunkOffset` | INDX 块起始偏移 |
| 16 | 4 | `u32` | `fileLength` | 文件总长度的低 32 位 |
| 20 | 4 | `u32` | `reserved` | 0 |

收尾顺序：写 `INDX` → 写 `END!` → 回填文件头（`frameCount`、`indexChunkOffset`、`frameStrideBytes`、`headerCrc32c`）。

## 8. 采样、抽帧与帧号语义

- 求解器以固定推进步长 `solverStepNs`（默认 0.5 ms）积分，每 $k$ 步记录一帧：
  $k = \text{sampleIntervalNs} / \text{solverStepNs}$，必须整除。
- 第 $n$ 帧对应 `simTimeNs = n × sampleIntervalNs`、`stepIndex = n × k`；总帧数 = $\lfloor \text{durationNs} / \text{sampleIntervalNs} \rfloor + 1$。
- 帧号与**渲染帧率**无关：播放器按真实时间乘倍速累加播放时钟，再取 $\lfloor t / \text{sampleIntervalNs} \rfloor$ 定位帧。
- 采样间隔只允许 1 / 2 / 5 / 10 / 20 ms；默认 10 ms。

## 9. 版本、兼容与错误处理

| 情形 | 读取器行为 |
|------|------------|
| 文件长度 < 64 字节 | 报错「不是有效的 EPSX 文件」 |
| 魔数不匹配 | 报错「这不是 Egret Physics 模拟文件」（旧格式或扩展名被占用） |
| `headerCrc32c` 不匹配 | 报错，拒绝加载 |
| `versionMajor` 非 1 | 报错「不支持的 EPSX 主版本号」 |
| `flags` 含未实现的保留位（如 float32 / zstd） | 报错，不做静默降级 |
| 块载荷 CRC 不匹配 | 报错 |
| 缺 `INDX`（`indexChunkOffset = 0`） | 从 SCNE 之后**顺序扫描** `FRAM` 块重建索引，`hasTrailer()` 返回 false |
| 出现未知块 | 按 `payloadSize` 跳过 |

主版本号变化表示不兼容的布局变更；次版本号变化表示向后兼容的字段追加。

## 10. 最小合法文件自检清单

1. 第 0–7 字节 = `89 45 50 53 58 0D 0A 1A`；
2. 第 40 字节起 8 字节 = SCNE 块偏移（通常为 `0x40` = 64）；
3. 偏移 `sceneChunkOffset` 处 4 字节 = `SCNE`，其载荷是合法 UTF-8 JSON，且含 `models` 数组；
4. 每个 `FRAM` 块载荷以 24 字节帧头开始，`frameIndex` 单调递增；
5. `INDX` 条目数 = 文件头 `frameCount` = 实际 `FRAM` 块数；
6. 文件结尾为 `END!` 块；
7. 所有 CRC-32C 校验通过。

## 11. 参考

| 位置 | 内容 |
|------|------|
| `src/view_model/simulation_serializer/epsx_format.h` | 魔数、版本、各层定长头长度、FourCC、标志位、记录结构与字节步长 |
| `src/view_model/simulation_serializer/epsx_crc32c.h` | 编译期建表的 CRC-32C |
| `src/view_model/simulation_serializer/epsx_writer.{h,cpp}` | 流式写出：文件头 → SCNE → FRAM×N → INDX → END! → 回填文件头 |
| `src/view_model/simulation_serializer/epsx_reader.{h,cpp}` | 校验、场景解析、索引读取（含顺序扫描回退）、帧解码 |
| `docs/EPSX 模拟数据文件格式规范（草案）.md` | 设计目标、扩展名决策、命令行接口与待确认事项 |
