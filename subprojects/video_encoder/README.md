# video_encoder —— .epsx → 视频导出工具

把离线仿真产生的 `.epsx` 文件离屏渲染成逐帧画面，并编码为 MP4 / MOV 视频文件。
本工具是独立子项目，不依赖主界面，可用于脚本批量导出。

## 构建

```powershell
# 在仓库根目录配置（与主程序同一个构建目录）
cmake -S . -B build

# 只构建本工具（主程序构建不会因为本目标变慢）
cmake --build build --config Release --target video_encoder
```

产物位于 `<build>/subprojects/video_encoder/Release/video_encoder.exe`，Qt 运行时与
QML 模块会在构建后自动部署到同一目录，可直接运行。
本目标复用主工程源码与 QML 资源，保证「场景重建 + 视口渲染」与播放器完全一致，
代价是首次编译需要额外编译一份公共源码。

## 用法

```
video_encoder --input <a.epsx> --output <a.mp4> [选项]
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

相机参数（物理坐标系，Z 轴向上，单位米；两套输入方式任选其一）：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--camera-pos <x,y,z>` | — | 相机位置，指定后进入「位置 + 注视点」模式 |
| `--camera-target <x,y,z>` | `0,0,0` | 相机注视点 |
| `--camera-yaw <deg>` | 45 | 环绕偏航角 |
| `--camera-pitch <deg>` | 55 | 环绕俯仰角 |
| `--camera-distance <d>` | 900 | 环绕半径 |
| `--fov <deg>` | 45 | 垂直视场角 |

其他：`--no-grid`（不渲染网格与坐标轴）、`--quiet`（不打印逐帧进度）、`-h/--help`。

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
| 回读 | QRhi `readBackTexture`（离屏帧不流水线，`endFrame()` 后即可取数据） |
| 编码 | `QVideoFrameInput` → `QMediaCaptureSession` → `QMediaRecorder`（拉模式逐帧送帧） |
| 线程 | 渲染在主线程（Quick3D 场景对象必须在 GUI 线程）；像素组装与编码在独立工作线程 |
| 后端 | 启动时设置 `QT_MEDIA_BACKEND=ffmpeg`（`QVideoFrameInput` 仅该后端实现） |

## 已知限制

1. `QVideoFrameInput` 需要 FFmpeg 多媒体后端，本工具在启动时强制指定；
2. 编码器可用性由运行期探测（`QMediaFormat::supportedVideoCodecs(Encode)`）决定，
   本机 Qt 自带 FFmpeg 通常提供 H.264（Media Foundation）与 MPEG-4，
   H.265 在 Windows 上被 Qt 主动剔除；
3. 渲染仍在主线程：Quick3D 的 QML 场景对象必须属于 GUI 线程，
   后续接入界面时会把 `sync/render` 通过 `QQuickRenderControl::prepareThread()` 迁移到导出线程；
4. 暂不支持导出时间区间、变速与相机动画。
