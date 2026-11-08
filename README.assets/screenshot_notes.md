<!-- /** @file screenshot_notes.md @brief README 静态截图的采集条件、来源和验证范围。 */ -->

# README 截图记录

采集日期：2026-11-08。当前截图来自 Windows 上运行的 `build/Release/Egret_Physics.exe`，画面分辨率约为 1443 × 931，保留实际程序界面，没有拼接虚构数据或生成替代界面。文件名中的 `v2_` 表示当前 v2.0 界面采集，不表示图中所有功能均首次加入 v2.0。

## 当前界面截图

| 文件 | 实际输入与操作 | 观察点 |
| --- | --- | --- |
| [v2_editor.jpg](v2_editor.jpg) | 使用 `--edit demo/programmable_body_control.epscene` 打开示例 | 多文档、实体管理、三维视口、显示参考、属性和控制入口 |
| [v2_presets.jpg](v2_presets.jpg) | 同一场景，打开“添加预设”列表 | 七项预设说明；本次截图没有向示例文件写入预设 |
| [v2_condition_rule.jpg](v2_condition_rule.jpg) | 打开 `demo/sliding_block_board.epscene`，新增临时规则草稿，选择时间大于等于 1 秒、首次成立一次、对木板设置 `(26,0,0) N` | 中文条件及动作表单；截图后取消，未保存规则和场景 |
| [v2_control_code.jpg](v2_control_code.jpg) | 可编程方块示例第一段“定时拉力”，周期 0.01 s，绑定 `body`，参数 `force=2`，点击“检查当前草稿” | C 高亮、字体、别名、参数、时序和检查通过诊断 |
| [v2_control_lifecycle.jpg](v2_control_lifecycle.jpg) | 同一示例第二段“动态替身”，执行草稿检查 | 3.2 秒创建替身，4 秒销毁原体 |
| [v2_playback_spawn.jpg](v2_playback_spawn.jpg) | 新求解的 `build/readme_control.epsx`，跳至 3.48 秒并适应场景 | 原体和新实体共存 |
| [v2_playback_destroy.jpg](v2_playback_destroy.jpg) | 同一结果，跳至 4.48 秒 | 原体已销毁，替身继续存在 |
| [v2_analysis_time.jpg](v2_analysis_time.jpg) | 打开历史 `demo/test.epsx`，切到数据分析；选择 `1123`、位置 Z、抽帧比 1，抽取后选择该序列 | 6001 个样本，10 ms 采样间隔，时域图及表格／拟合入口 |
| [v2_analysis_spectrum.jpg](v2_analysis_spectrum.jpg) | 对同一历史序列切到频域，勾选“显示相位谱” | 当前矩形窗单边幅度谱与相位谱，采样率为 100 Hz |
| [v2_video_export.jpg](v2_video_export.jpg) | 在播放器打开历史 `demo/test.epsx`，暂停并打开导出窗口 | 30 fps、1920 × 1080 设置；本次仅展示设置，没有新导出视频 |

截图采集使用原始示例和临时草稿，未保存对示例文件的修改。控制回放结果是本轮重新求解生成；分析和视频设置截图使用的 `demo/test.epsx` 是原有历史数据，文件元数据中的生成器版本为 1.0.0，不是本次新求解结果。

## 控制演示的重新求解

在项目根目录的 PowerShell 中执行：

```powershell
./build/Release/Egret_Physics.exe --solve ./demo/programmable_body_control.epscene --duration 5 --step 0.5 --sample 10 -o ./build/readme_control.epsx
./build/Release/Egret_Physics.exe --play ./build/readme_control.epsx
```

本轮实际结果：求解成功，10000 个物理步，501 帧；控制准备缓存命中 2/2。截图选取 348 和 448 帧，分别对应 3.48 和 4.48 秒。`--step`、`--sample` 单位为毫秒，`--duration` 为秒。

受限执行环境中，Clang 曾因系统临时目录内的临时对象改名权限而检查失败；改用工作区内 `build/readme-temp` 作为当前演示进程的 `TEMP`／`TMP` 后，草稿检查和正式求解完成。该处理仅调整演示子进程环境，未修改项目源代码或系统设置；这与已有控制验证文档中记录的临时目录限制一致。

## 历史图片

`program_screenshot_v1.0-Simulator.png`、`program_screenshot_v1.0-Editor.png`、`program_screenshot_v1.0-Editor_2.png` 原样保留自旧 README，用于展示 v1.0 原始状态，本轮未重新采集。

`PhysicsModel_InheritanceChain.png` 也是原有模型继承图，不代表当前新增控制核心与全部架构的完整关系。

## 验证范围

本轮实际操作了示例加载、C 草稿检查、命令行求解、动态回放跳帧、历史数据抽取、时域与频域显示，并查看预设、规则和视频导出设置。未重新构建程序，未运行全部自动回归，未把截图当作数值精度或性能验证。

动态实体结果在本次分析操作中出现抽取到样本但没有生成序列的现象，因此分析截图使用固定实体集合的历史结果；动态序列分析需要独立复现与诊断。本轮仅整理文档，没有针对该现象修改代码。

历史数据的曲线和频谱只演示界面与处理流程，不证明当前求解器的准确性。接触与教学题的数值验证应阅读相应专项文档，并使用其场景及测量条件重新运行。
