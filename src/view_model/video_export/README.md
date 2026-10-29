# 视频导出 —— .epsx → 视频

主程序通过命令行离屏渲染仿真文件并编码为 MP4 / MOV。

## 构建

```powershell
cmake --build build --config Release --target Egret_Physics
```

## 用法

```
Egret_Physics --export-video --input <a.epsx> --output <a.mp4> [选项]
```

必填：

| 参数 | 说明 |
|------|------|
| `-i, --input <path>` | 输入 `.epsx` 文件 |
| `-o, --output <path>` | 输出视频文件（建议 `.mp4`） |

输出参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--fps <n>` | 跟随采样率 | 输出帧率；缺省表示逐帧 1:1（例如采样间隔 10 ms → 100 fps） |
| `--width <w>` / `--height <h>` | 1920 / 1080 | 输出分辨率，必须为偶数 |
| `--codec <h264\|mpeg4>` | `h264` | 视频编码；H.264 在 Windows 上由 Media Foundation 硬件编码器完成 |
| `--quality <low\|normal\|high\|veryhigh>` | `high` | 质量档 |
| `--container <mp4\|mov>` | `mp4` | 封装容器 |
| `--backend <ffmpeg\|qt>` | `ffmpeg` | 编码后端：自带 FFmpeg 9.0.2（可用 NVENC）/ Qt Multimedia |
| `--encoder <name>` | `h264_nvenc` | FFmpeg 编码器名；打开失败会自动回退到 amf → qsv → mf → libx264 → h264 |
| `--preset <name>` | 编码器默认 | NVENC 用 `p1`~`p7`，x264 用 `veryfast` 等 |

相机参数（物理坐标系，Z 轴向上，单位米；两套输入方式任选其一）：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--camera-pos <x,y,z>` | — | 相机位置，指定后进入「位置 + 注视点」模式 |
| `--camera-target <x,y,z>` | `0,0,0` | 相机注视点 |
| `--camera-yaw <deg>` | 45 | 环绕偏航角 |
| `--camera-pitch <deg>` | 55 | 环绕俯仰角 |
| `--camera-distance <d>` | 900 | 环绕半径 |
| `--fov <deg>` | 45 | 垂直视场角 |

其他：

| 参数 | 说明 |
|------|------|
| `--no-grid` | 不渲染网格与坐标轴 |
| `--lanes <n>` | 并行渲染通道数（默认 1；渲染成为瓶颈时可尝试 2~4） |
| `--stats` | 结束时打印逐阶段耗时统计 |
| `--quiet` | 不打印逐帧进度 |
| `-h, --help` | 显示帮助 |

## 时间轴语义

**epsx 怎么播放，mp4 就怎么输出**：

- 视频时长 = 源帧数 × 采样间隔，与播放器 `simulationDurationSeconds` 相同，不做裁剪与变速；
- 输出帧 `n` 对应仿真时刻 `n / fps`；
- 源帧号取 `floor(时刻 / 采样间隔)`，与 `SceneManagerViewModel::advancePlayback()` 同一条规则；
- 缺省帧率等于 `1 / 采样间隔`，此时逐帧 1:1 输出。

## 实现要点

| 环节 | 做法 |
|------|------|
| 场景重建 | 独立 `SceneManagerViewModel` 打开 epsx（与播放器同一条数据链路） |
| 渲染 | `QQuickRenderControl` + 不显示的 `QQuickWindow`，复用 `SimulationViewport` |
| 回读 | QRhi `readBackTexture`，回读缓冲被 `QVideoFrame` 直接引用（零拷贝，必要时只做原地行交换） |
| 编码（默认） | dependency 自带 **FFmpeg 9.0.2**：RGBA → swscale 转 NV12 → **h264_nvenc** → MP4 封装 |
| 编码（备选） | Qt Multimedia：`QVideoFrameInput` → `QMediaCaptureSession` → `QMediaRecorder` |
| 线程 | 渲染在主线程（Quick3D 场景对象必须在 GUI 线程）；像素组装与编码在独立工作线程 |
| 依赖 | FFmpeg 运行库位于 `dependency/bin/ffmpeg`，构建后自动复制到输出目录 |

## 性能（本机实测：RTX 级独显）

1920×1080、demo/test.epsx（6001 帧源数据）：

| 后端 | 速度 | 说明 |
|------|------|------|
| FFmpeg / **h264_nvenc**（默认） | **≈320 帧/秒** | 1800 帧 5.5 s；导出期间 `nvidia-smi` 显示 **NVENC 引擎占用 93~100%** |
| Qt Multimedia（`--backend qt`） | ≈215 帧/秒 | 编码走 Media Foundation 软件编码器，CPU 占用高 |
| FFmpeg / libx264（`--encoder libx264`） | 640×360 下 858 帧/秒 | CUDA 不可用时的软件回退 |

每帧耗时构成（1080p，`--stats` 输出）：

| 阶段 | 耗时 |
|------|------|
| 套用源帧状态 | 0.24 ms |
| 同步 + 渲染 + 回读 | 3.44 ms（同步 0.02｜渲染 0.39｜GPU 回读等待 1.51） |
| 编码器周转（投递 → 下次请求） | 2.85 ms（RGBA→NV12 转换 + NVENC + 封装） |
| 合计 | 3.12 ms/帧 |

结论：

1. 换成自带 FFmpeg + NVENC 后，1080p 从 ≈215 帧/秒 提升到 **≈320 帧/秒**，
   编码不再是瓶颈（编码 2.85 ms/帧 vs 渲染 3.44 ms/帧，两者流水线重叠）；
2. 渲染侧现在是限制项，其中 **GPU 回读等待 1.5 ms/帧** 与 RGBA→NV12 的 CPU 转换是主要开销；
   进一步提速需要「渲染纹理 → 编码器」的 GPU 零拷贝直通（D3D11 共享纹理 + NVENC 硬件帧）；
3. 并行渲染通道（`--lanes`）在 1080p 下仍无明显收益（多设备竞争 GPU），默认 1 通道；
4. 质量档对速度影响很小，主要影响文件大小。

> 修复记录：早期版本在收尾时白等 15 秒（流结束标志在编码队列未清空时被丢弃，
> 导致 autoStop 不触发），同样的 1080p 导出需要 18.9 s；修复后为 2.9 s，约 6 倍提升。

## 已知限制

1. 默认后端依赖 `dependency/bin/ffmpeg` 中的 FFmpeg 9.0.2 运行库（构建后自动复制到输出目录）；
   若运行环境缺少该目录，可用 `--backend qt` 退回 Qt Multimedia；
2. Qt Multimedia 后端会强制 `QT_MEDIA_BACKEND=ffmpeg`（`QVideoFrameInput` 仅该后端实现），
   其编码器由 `QMediaFormat::supportedVideoCodecs(Encode)` 运行期探测，H.265 在 Windows 上被 Qt 剔除；
3. FFmpeg 后端目前仅暴露 H.264（`--codec mpeg4` 需配合 `--backend qt`），
   若要 HEVC/AV1 只需新增编码器名映射（`hevc_nvenc` / `av1_nvenc` 在自带 FFmpeg 中已存在）；
4. 渲染仍在主线程：Quick3D 的 QML 场景对象必须属于 GUI 线程，
   后续接入界面时会把 `sync/render` 通过 `QQuickRenderControl::prepareThread()` 迁移到导出线程；
5. 暂不支持导出时间区间、变速与相机动画。
