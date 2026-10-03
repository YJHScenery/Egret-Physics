# TODO：待复核项

> **文件用途**：集中记录「当前不处理、但必须回头复核」的事项，避免它们散落在对话与注释里被遗忘。
>
> **维护约定**
> - 每条必须写明：编号、位置（文件:行）、当前取值、需要复核的理由、建议动作。
> - 复核完成后把 `[ ]` 改为 `[x]`，并在该条末尾追加 `→ 结论：…`。
> - 本文件只记录复核项与待办，不记录已完成的功能说明。

## 1. 求解步长改为 1 ms 后需要复核的数值阈值

**背景**：`src/view_model/simulator/scene_manager.h:455` 的 `m_fixedStepSeconds` 已由 `1.0 / 120.0`
（≈ 8.333 ms）改为 `1.0 / 1000.0`（1 ms）。凡是「以步长或步内位移为量纲」的阈值都需要回头复核——
步长缩小 8.33 倍后，这些阈值的相对大小全部变了。同步已把 `m_maxSubSteps` 由 5 提高到 32
（16 ms 周期 ÷ 1 ms 步长 = 16，取 2 倍余量），以恢复 1 倍实时的推进速度。

| 编号 | 位置 | 当前取值 | 复核理由 | 建议动作 |
|------|------|---------|---------|---------|
| T-01 | `src/model/solver/solver_types.h:214` `penetrationSlop` | `0.001` m | 1 ms 步长下静止接触的单步穿透量约 $g\Delta t^2 \approx 10^{-5}$ m，比 slop 小两个数量级，位置修正几乎不会触发 | [ ] 若观察到物体缓慢下沉或持续漂移，降到 `1e-4` 量级；若接触出现抖动则维持原值 |
| T-02 | `src/model/solver/solver_types.h:217` `positionCorrectionBeta` | `0.2` | 与 T-01 的 slop 联动，slop 改了它也要跟着看 | [ ] 与 T-01 一并复核 |
| T-03 | `src/model/solver/solver_types.h:224` `maxPositionCorrectionPerStep` | `0.2` m | 单步位置修正上限；步长变小后更难触发 | [ ] 确认不再出现位置跳变即可，一般无需修改 |
| T-04 | `src/model/solver/solver_types.h:227` `maxTranslationPerStep` | `10.0` m | 单步平移上限；1 ms 步长下几乎不可能触发 | [ ] 确认即可 |
| T-05 | `src/model/strategy/sleep_strategy/sleep_config.h:66` `speedSquaredThreshold` | `1e-4` | 阈值与速度量级相关，而积分得到的速度随步长变化 | [ ] 复核静止物体能否按预期进入休眠 |
| T-06 | `src/model/strategy/sleep_strategy/sleep_config.h:78` `wakeUpAbsoluteForceThreshold` | `1e-4` | 同上，影响唤醒判定 | [ ] 复核被外力击打后能否正常唤醒 |
| T-07 | `src/model/strategy/sleep_strategy/sleep_config.h:81` `frozenFrameCount` | `30`（单位是**渲染帧**，不是求解步） | 现在一个渲染帧要跑 16 个求解步，冻结判定所覆盖的仿真时长随之变化 | [ ] 确认「30 帧 ≈ 0.48 s」冻结延迟是否符合预期 |
| T-08 | `src/model/solver/solver.cpp` `kMaxCcdPenetration` | `1.0` m | CCD 穿透深度钳制值，与步长无关，但改制后仍值得确认 | [ ] 确认即可 |
| T-09 | `src/model/physics/collision/continuous_collision_detector.h` `kEpsilonDistance` / `kEpsilonGJK` / `kEpsilonEPA` | `1e-6` / `1e-8` / `1e-6` | 步长变小后单步位移变小，容差相对于位移变大，可能把接触误判为分离或反之 | [ ] 复核接触判定是否出现漏检或误判 |
| T-10 | `src/view_model/simulator/scene_manager.h:455` `m_fixedStepSeconds` + `:468` `m_maxSubSteps` | `1 ms` + `32` | 两者必须保持 `m_maxSubSteps ≥ 定时器周期 / 步长`，否则仿真时间会再次落后于真实时间 | [ ] 若后续调整任一项，重新按公式核算并同步两者 |

## 2. 需要实测标定的量

| 编号 | 事项 | 为什么要标定 | 建议动作 |
|------|------|-------------|---------|
| M-01 | 单步各阶段耗时（广相 / 窄相 CCD / 接触解算 / 积分） | 判断 0.5 ms 推进步长能否满足实时；当前只有模型估算（每步 65 µs–1.6 ms 的区间），缺少实测值 | [ ] 给 `SolverStats` 增加四个计时字段，跑 2000 步标定，并把结果写入 EPSX 的 `SCNE` 块 |
| M-02 | EPSX 文件 zstd 实际压缩率 | 草案 §8.8 的体积表目前是经验估计区间 | [ ] 实现写出流程后用真实场景标定并回填该表 |
| M-03 | 20 实体场景在 1 ms 步长下的实时余量 | 验证 `m_maxSubSteps = 32` 是否足够，以及渲染帧率是否稳定在 62.5 Hz | [ ] 观察界面 FPS 卡片与 `simTime` / 真实时间的比值 |

## 3. 需要联网核实

| 编号 | 事项 | 说明 |
|------|------|------|
| W-01 | `.epsx` 扩展名是否已被占用 | 本机注册表实测未被占用，但本地结果不能证明互联网上无人使用；见 EPSX 草案 §2.1 |
| W-02 | EPSX 草案 §14 的参考文献链接 | 当前环境无法联网，全部链接标记为【待核实】 |

## 4. 与 EPSX 草案相关的设计决策

明细见 `docs/EPSX 模拟数据文件格式规范（草案）.md` §13 与
`docs/EPSX 模拟数据文件编码格式规范.md`：

- [x] **曲线与凸体网格逐帧记录**：已决定「逐帧记录」。**实现待做**——需要在 `FRAM` 中
      新增 `CRVS` / `MESH` 定长段（几何本身仍只写在场景块一次，逐帧记录的是各实例的
      位姿/状态），并同步写入器、读取器、离线求解器采集与编码文档。
- [x] **默认精度一律 `float64`**：不再考虑 `float32`；文件头 `Float32Geometry` 位保留但永不置位。
- [x] **接受「一次模拟内实体集合固定」的 v1 简化**：帧段恒为定长，实体中途增删不在 v1 范围内。
- [ ] **`--parse` 的输出格式**：本轮决定**暂缓**，维持当前「单行 JSON」输出。
- [x] 求解器与播放器仍是**同一可执行文件**的不同模式（`--solve` / `--play`）；
      界面侧对应「离线仿真」与「播放器」两个独立工作区。
- [ ] 渲染目标 120 Hz 的落实方式（固定刷新定时器 / 跟随 vsync）——仍未定。
- [ ] 推进步长的最终取值（当前按 0.5 ms 撰写）。
- [ ] `--solve` 的输入来源（场景 JSON / 编辑器当前场景）——当前为场景 JSON 文件。

## 5. 遗留问题

- [ ] **平面平行接触的异常抖动**仍未解决（见 `ISSUES.md`，相关提交 `796a7c36`「抖动没解决，先不管了」）。
      下一步需要复现条件（场景、参数、运行时长）才能对照日志定位。
- [ ] 日志缺失：`cmake-build-release/logs/qdebug_output.log` 已被清空或删除（当前只剩 548 字节的
      `async_logger.log`）。后续复现任何问题之前，请先确认日志目录会被正常写入，
      否则无法按 AGENTS.md 的诊断流程取证。

## 6. EPSX 实现进度（未完成项）

已完成（截至 2026-10-03，均通过 `build-verify` 的 Release 构建）：

- 格式层（CRC-32C、小端读写器、格式常量与记录结构）、写入器、读取器、离线求解器；
- 命令行入口：`--solve` / `--parse epsx <n>` / `--play`（`-r` / `--replay`）；
- 播放后端：`SceneManagerViewModel` 的加载 / 播放 / 跳帧属性与方法；
- QML 界面：共享 3D 视口 `src/qml/components/viewport/`（SimulationViewport + 四个委托 +
  ViewportMath.js）、播放器工作区 `units/PlayerUnit.qml`、MainWindow 新增「播放器」路由；
- **格式更名为 EPSX**：扩展名 `.epsx`、命名空间 `egret::epsx`、`EpsxWriter` / `EpsxReader`、
  文件名 `epsx_*.{h,cpp}`、魔数 `89 45 50 53 58 0D 0A 1A`；旧 `.egps` 文件不再兼容（不做迁移）。
- **独立「离线仿真」工作区**：`units/OfflineSimulationUnit.qml`（视口 + 初始状态选择 +
  求解参数 + 进度 + 保存 / 删除 + **取消**），`SimulatorUnit` 的离线面板已迁出，
  实时仿真在 Combo 与界面中标记「不再维护」。
- **离线求解取消**：`OfflineSimulationRunner::Request::shouldCancel` + `Report::canceled`，
  每 100 步检查一次；取消后仍正常写索引块与尾块，已求解的帧可直接播放。

以下为尚未完成的部分：

- [ ] **实机运行验证**：全部 QML 只能由用户实机运行确认（语法用 `qmllint` 抽查、资源已进 qrc，
      但渲染与交互效果需要人工确认）。
- [x] **文件关联**：已实现，且**已迁移为独立子项目**。注册逻辑位于
      `subprojects/epsx_register/`（`CMakeLists.txt` + `src/main.cpp` + `src/file_association.{h,cpp}`
      + `resources/epsx_register_icon.qrc`），产出独立控制台程序 `epsx_register.exe`，只写
      `HKCU\Software\Classes`（含 `DefaultIcon` / `shell\open\command` / `OpenWithProgids`）；
      不带参数即注册、`--unregister` 注销、`-e <路径>` 指定播放器，缺省取同目录的 `Egret_Physics.exe`。
      主程序侧只保留 `--play`（`main.cpp` 不再提前退出，改为进入图形界面、切到「播放器」工作区、
      打开文件后调用 `playSimulation()` 自动播放，见 `MainWindow.qml` 的 `startupWorkspaceRoute` /
      `startupSimulationFile`）；`src/utils/os/windows/file_association.*` 与 `--register-epsx` /
      `--unregister-epsx` 已删除。文件图标取 `resources/assets/file_icon/simulation-file-blue.ico`
      （主程序经 `resources/global.qrc` 的 `/file_icon` 前缀引用，注册工具以内嵌别名引用，注册时释放到
      `%LOCALAPPDATA%\EgretPhysicsStudio\simulation-file-blue.epsx.ico`）。
      → 结论：本机注册表仍指向 `cmake-build-release\Egret_Physics.exe`（关联内容未变化，无需重写）。
      子项目已加入根 `CMakeLists.txt` 的 `add_subdirectory`、`add_dependencies(${PROJECT_NAME} epsx_register)`
      与 CPack 的 `install(TARGETS ...)`：带 `--target Egret_Physics` 构建时工具会一起编译并输出到同一目录。
      更换构建 / 部署位置后运行一次 `epsx_register.exe` 即可刷新指向。新工具的实机验证待用户确认。
      **2026-10-03 修复**：「注销 → 重新注册」报「无法写出文件图标」，实测该图标带
      `ReadOnly` 属性：`QFile::remove()` 对只读文件失败，随后 `QFile::copy()` 又因目标已存在
      而拒绝覆盖。现已改为「以写+截断方式直接覆盖字节、失败先清只读属性重试、内容一致则复用、
      仍失败则改用带 SHA-256 指纹的备用文件名」，并在写出后主动清除只读属性；注销时按通配清理
      全部图标副本。此修改尚未编译验证。
- [ ] **保留能力未实现**：文件头中的 `float32` 几何、zstd 压缩位已定义但读取器会明确拒绝，
      写入端也不会产生此类文件。
- [ ] **约束与场的帧记录不完整**：`CONS`/`FLDS` 段当前只记录 ID、槽位、类型、启用标志、
      参考长度与误差；锚点、场矢量与场强尚未采集（播放器改为从场景描述块重建，不受影响）。
- [x] ~~**离线求解的取消功能**~~：已实现（求解器回调 + 控制器 `cancel()` + 工作区「取消」按钮）。
- [ ] **求解期间不暂停实时仿真**：离线任务在线程池执行，仿真器仍在 GUI 线程推进，两者并存
      （结果不受影响，只是占用 CPU）。
- [ ] **曲线 / 凸体网格逐帧记录**：见 §4 第一条，格式扩展与采集尚未实现。
