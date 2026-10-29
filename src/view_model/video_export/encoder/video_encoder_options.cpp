/**
 * @file        video_encoder_options.cpp
 * @brief       视频导出命令行参数的解析与枚举映射实现。
 * @details     解析器遵循「遇到第一个错误就返回」的策略：命令行工具不需要一次性罗列全部问题，
 *              尽早失败并给出可操作的提示更适合脚本调用。所有浮点参数都按 double 解析后
 *              再显式窄化为 float，避免隐式窄化。
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     VideoEncoder
 * @defgroup    VideoEncoder (EPSX 视频导出)
 */

#include "video_encoder_options.h"

#include <QFileInfo>

#include <cmath>

namespace egret::video
{
    namespace
    {
        /** 默认环绕偏航角（度），与 SimulationViewport 保持一致。 */
        constexpr float DEFAULT_ORBIT_YAW{45.0F};

        /** 默认环绕俯仰角（度），与 SimulationViewport 保持一致。 */
        constexpr float DEFAULT_ORBIT_PITCH{55.0F};

        /** 默认环绕半径（米），与 SimulationViewport 保持一致。 */
        constexpr float DEFAULT_ORBIT_DISTANCE{900.0F};

        /**
         * @brief  把 "x,y,z" 形式的字符串解析为三维向量。
         * @param  text 输入文本。
         * @param  out 解析结果输出，失败时不修改。
         * @return 解析成功返回 true。
         */
        bool parseVector3(const QString& text, QVector3D* out)
        {
            const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
            if (parts.size() != 3) {
                return false;
            }

            bool okX{false};
            bool okY{false};
            bool okZ{false};
            const float x{parts.at(0).trimmed().toFloat(&okX)};
            const float y{parts.at(1).trimmed().toFloat(&okY)};
            const float z{parts.at(2).trimmed().toFloat(&okZ)};
            if (!okX || !okY || !okZ) {
                return false;
            }

            if (out != nullptr) {
                *out = QVector3D{x, y, z};
            }
            return true;
        }

        /**
         * @brief  判断文本是否是有限浮点数。
         * @param  text 输入文本。
         * @param  out 解析结果输出，可为 nullptr。
         * @return 可解析且有限返回 true。
         */
        bool parseFiniteDouble(const QString& text, double* out)
        {
            bool ok{false};
            const double value{text.trimmed().toDouble(&ok)};
            if (!ok || !std::isfinite(value)) {
                return false;
            }
            if (out != nullptr) {
                *out = value;
            }
            return true;
        }

        /**
         * @brief  把失败原因写入解析结果并返回。
         * @param  result 解析结果。
         * @param  message 失败原因。
         * @return 始终返回传入的解析结果（便于 `return fail(...)` 写法）。
         */
        ParseResult fail(ParseResult& result, const QString& message)
        {
            result.ok = false;
            result.errorMessage = message;
            return result;
        }
    }

    ParseResult parseCommandLine(const QStringList& arguments)
    {
        ParseResult result{};
        Options options{};

        // 逐项消费参数：--key value 形式取值，--flag 形式置位。
        for (int index = 1; index < arguments.size(); ++index) {
            const QString& argument = arguments.at(index);

            const auto takeValue = [&arguments, &index](QString* value) -> bool
            {
                if (index + 1 >= arguments.size()) {
                    return false;
                }
                *value = arguments.at(++index);
                return true;
            };

            if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
                result.helpRequested = true;
                result.ok = true;
                return result;
            }
            if (argument == QStringLiteral("--input") || argument == QStringLiteral("-i")) {
                if (!takeValue(&options.inputPath)) {
                    return fail(result, QStringLiteral("--input 缺少取值"));
                }
                continue;
            }
            if (argument == QStringLiteral("--output") || argument == QStringLiteral("-o")) {
                if (!takeValue(&options.outputPath)) {
                    return fail(result, QStringLiteral("--output 缺少取值"));
                }
                continue;
            }
            if (argument == QStringLiteral("--preview")) {
                if (!takeValue(&options.previewPngPath)) {
                    return fail(result, QStringLiteral("--preview 缺少取值"));
                }
                continue;
            }
            if (argument == QStringLiteral("--fps")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value)) {
                    return fail(result, QStringLiteral("--fps 需要正数，例如 --fps 60"));
                }
                options.frameRate = value;
                continue;
            }
            if (argument == QStringLiteral("--width")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value) || value < 2.0) {
                    return fail(result, QStringLiteral("--width 需要不小于 2 的整数"));
                }
                options.width = static_cast<int>(std::llround(value));
                continue;
            }
            if (argument == QStringLiteral("--height")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value) || value < 2.0) {
                    return fail(result, QStringLiteral("--height 需要不小于 2 的整数"));
                }
                options.height = static_cast<int>(std::llround(value));
                continue;
            }
            if (argument == QStringLiteral("--codec")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--codec 缺少取值"));
                }
                if (text.compare(QStringLiteral("h264"), Qt::CaseInsensitive) == 0) {
                    options.codec = VideoCodecChoice::H264;
                }
                else if (text.compare(QStringLiteral("mpeg4"), Qt::CaseInsensitive) == 0 ||
                         text.compare(QStringLiteral("mpeg-4"), Qt::CaseInsensitive) == 0) {
                    options.codec = VideoCodecChoice::Mpeg4;
                }
                else {
                    return fail(result, QStringLiteral("--codec 只支持 h264 或 mpeg4"));
                }
                continue;
            }
            if (argument == QStringLiteral("--quality")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--quality 缺少取值"));
                }
                const QString normalized{text.trimmed().toLower()};
                if (normalized == QStringLiteral("low")) {
                    options.quality = QualityChoice::Low;
                }
                else if (normalized == QStringLiteral("normal")) {
                    options.quality = QualityChoice::Normal;
                }
                else if (normalized == QStringLiteral("high")) {
                    options.quality = QualityChoice::High;
                }
                else if (normalized == QStringLiteral("veryhigh") || normalized == QStringLiteral("very-high")) {
                    options.quality = QualityChoice::VeryHigh;
                }
                else {
                    return fail(result, QStringLiteral("--quality 只支持 low / normal / high / veryhigh"));
                }
                continue;
            }
            if (argument == QStringLiteral("--container")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--container 缺少取值"));
                }
                const QString normalized{text.trimmed().toLower()};
                if (normalized == QStringLiteral("mp4")) {
                    options.container = ContainerChoice::Mp4;
                }
                else if (normalized == QStringLiteral("mov")) {
                    options.container = ContainerChoice::Mov;
                }
                else {
                    return fail(result, QStringLiteral("--container 只支持 mp4 或 mov"));
                }
                continue;
            }
            if (argument == QStringLiteral("--camera-pos")) {
                QString text{};
                QVector3D value{};
                if (!takeValue(&text) || !parseVector3(text, &value)) {
                    return fail(result, QStringLiteral("--camera-pos 需要 x,y,z 形式，例如 --camera-pos 0,0,900"));
                }
                options.camera.position = value;
                options.camera.hasExplicitPose = true;
                continue;
            }
            if (argument == QStringLiteral("--camera-target")) {
                QString text{};
                QVector3D value{};
                if (!takeValue(&text) || !parseVector3(text, &value)) {
                    return fail(result, QStringLiteral("--camera-target 需要 x,y,z 形式，例如 --camera-target 0,0,0"));
                }
                options.camera.target = value;
                options.camera.orbitTarget = value;
                continue;
            }
            if (argument == QStringLiteral("--camera-yaw")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value)) {
                    return fail(result, QStringLiteral("--camera-yaw 需要角度值"));
                }
                options.camera.yaw = static_cast<float>(value);
                options.camera.hasOrbit = true;
                continue;
            }
            if (argument == QStringLiteral("--camera-pitch")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value)) {
                    return fail(result, QStringLiteral("--camera-pitch 需要角度值"));
                }
                options.camera.pitch = static_cast<float>(value);
                options.camera.hasOrbit = true;
                continue;
            }
            if (argument == QStringLiteral("--camera-distance")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value) || value <= 0.0) {
                    return fail(result, QStringLiteral("--camera-distance 需要正数"));
                }
                options.camera.distance = static_cast<float>(value);
                options.camera.hasOrbit = true;
                continue;
            }
            if (argument == QStringLiteral("--fov")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value) || value <= 1.0 || value >= 179.0) {
                    return fail(result, QStringLiteral("--fov 需要在 1 ~ 179 度之间"));
                }
                options.camera.fieldOfView = static_cast<float>(value);
                continue;
            }
            if (argument == QStringLiteral("--no-grid")) {
                options.showGrid = false;
                continue;
            }
            if (argument == QStringLiteral("--quiet")) {
                options.quiet = true;
                continue;
            }
            if (argument == QStringLiteral("--stats")) {
                options.printStats = true;
                continue;
            }
            if (argument == QStringLiteral("--backend")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--backend 缺少取值"));
                }
                const QString normalized{text.trimmed().toLower()};
                if (normalized == QStringLiteral("ffmpeg")) {
                    options.backend = EncoderBackend::Ffmpeg;
                }
                else if (normalized == QStringLiteral("qt")) {
                    options.backend = EncoderBackend::QtMultimedia;
                }
                else {
                    return fail(result, QStringLiteral("--backend 只支持 ffmpeg 或 qt"));
                }
                continue;
            }
            if (argument == QStringLiteral("--encoder")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--encoder 缺少取值"));
                }
                options.encoderName = text.trimmed();
                continue;
            }
            if (argument == QStringLiteral("--preset")) {
                QString text{};
                if (!takeValue(&text)) {
                    return fail(result, QStringLiteral("--preset 缺少取值"));
                }
                options.encoderPreset = text.trimmed();
                continue;
            }
            if (argument == QStringLiteral("--lanes")) {
                QString text{};
                double value{0.0};
                if (!takeValue(&text) || !parseFiniteDouble(text, &value) || value < 1.0 || value > 16.0) {
                    return fail(result, QStringLiteral("--lanes 需要 1 ~ 16 之间的整数"));
                }
                options.laneCount = static_cast<int>(std::llround(value));
                continue;
            }

            return fail(result, QStringLiteral("未知参数：%1（使用 --help 查看用法）").arg(argument));
        }

        // ---- 必填项与取值范围校验 ----
        if (options.inputPath.trimmed().isEmpty()) {
            return fail(result, QStringLiteral("缺少 --input（输入 .epsx 文件）"));
        }
        if (options.outputPath.trimmed().isEmpty() && options.previewPngPath.trimmed().isEmpty()) {
            return fail(result, QStringLiteral("缺少 --output（输出视频文件），或改用 --preview 只导出首帧"));
        }
        if (!QFileInfo::exists(options.inputPath)) {
            return fail(result, QStringLiteral("输入文件不存在：%1").arg(options.inputPath));
        }
        if (options.frameRate > 0.0 && (options.frameRate < 1.0 || options.frameRate > 480.0)) {
            return fail(result, QStringLiteral("--fps 需要落在 1 ~ 480 之间"));
        }
        if (options.width % 2 != 0 || options.height % 2 != 0) {
            return fail(result, QStringLiteral("--width / --height 必须为偶数（H.264 要求）"));
        }

        result.ok = true;
        result.options = options;
        return result;
    }

    QString helpText()
    {
        return QStringLiteral(
            "egret video_encoder —— 把 .epsx 仿真文件编码为 MP4 视频\n"
            "\n"
            "用法：\n"
            "  Egret_Physics --export-video --input <a.epsx> --output <a.mp4> [选项]\n"
            "\n"
            "必填：\n"
            "  -i, --input <path>            输入 .epsx 文件\n"
            "  -o, --output <path>           输出视频文件（建议 .mp4）\n"
            "\n"
            "输出参数：\n"
            "      --fps <n>                 输出帧率；缺省表示跟随 epsx 采样率（逐帧 1:1）\n"
            "      --width <w>               输出宽度，默认 1920（必须为偶数）\n"
            "      --height <h>              输出高度，默认 1080（必须为偶数）\n"
            "      --codec <h264|mpeg4>      视频编码，默认 h264\n"
            "      --quality <low|normal|high|veryhigh>\n"
            "                                编码质量档，默认 high\n"
            "      --container <mp4|mov>     封装容器，默认 mp4\n"
            "      --backend <ffmpeg|qt>     编码后端，默认 ffmpeg（自带 FFmpeg 9.0.2，支持 NVENC）\n"
            "      --encoder <name>          FFmpeg 编码器名，默认 h264_nvenc（失败自动回退）\n"
            "      --preset <name>           编码器预设（NVENC：p1~p7；x264：veryfast 等）\n"
            "\n"
            "相机参数（物理坐标系，Z 轴向上，单位米）：\n"
            "      --camera-pos <x,y,z>      相机位置（指定后使用「位置 + 注视点」模式）\n"
            "      --camera-target <x,y,z>   相机注视点，默认 0,0,0\n"
            "      --camera-yaw <deg>        环绕偏航角（默认 45）\n"
            "      --camera-pitch <deg>      环绕俯仰角（默认 55）\n"
            "      --camera-distance <d>     环绕半径（默认 900）\n"
            "      --fov <deg>               垂直视场角，默认 45\n"
            "\n"
            "其他：\n"
            "      --no-grid                 不渲染网格与坐标轴\n"
            "      --quiet                   不打印逐帧进度\n"
            "      --stats                   结束时打印逐阶段耗时统计\n"
            "      --lanes <n>               并行渲染通道数（默认按 CPU 核数自动决定，1 为单通道）\n"
            "      --preview <path.png>      只渲染首帧并保存为 PNG（用于检查相机取景），不编码视频\n"
            "  -h, --help                    显示本帮助\n"
            "\n"
            "时间轴语义：视频时长与 epsx 播放时长一致（帧数 × 采样间隔），不做裁剪与变速；\n"
            "            输出帧 n 对应仿真时刻 n / fps，源帧号取 floor(时刻 / 采样间隔)。\n"
            "\n"
            "示例：\n"
            "  Egret_Physics --export-video -i sim.epsx -o sim.mp4 --fps 60 --camera-pos 0,-900,600 --camera-target 0,0,0\n"
            "  Egret_Physics --export-video -i sim.epsx -o sim.mp4 --codec mpeg4 --width 1280 --height 720 --quiet\n");
    }

    CameraPose resolveCameraPose(const CameraOptions& camera)
    {
        CameraPose pose{};
        pose.fieldOfView = camera.fieldOfView;
        pose.clipNear = camera.clipNear;
        pose.clipFar = camera.clipFar;

        // 模式一：直接给出位置与注视点。
        if (camera.hasExplicitPose) {
            pose.position = camera.position;
            pose.target = camera.target;
            return pose;
        }

        // 模式二：环绕参数（默认值即播放器视口的初始视角）。
        const float yaw{camera.hasOrbit ? camera.yaw : DEFAULT_ORBIT_YAW};
        const float pitch{camera.hasOrbit ? camera.pitch : DEFAULT_ORBIT_PITCH};
        const float distance{camera.hasOrbit ? camera.distance : DEFAULT_ORBIT_DISTANCE};
        const QVector3D target{camera.orbitTarget};

        constexpr double DEGREES_TO_RADIANS{3.14159265358979323846 / 180.0};
        const double yawRadians{static_cast<double>(yaw) * DEGREES_TO_RADIANS};
        const double pitchRadians{static_cast<double>(pitch) * DEGREES_TO_RADIANS};
        const double cosPitch{std::cos(pitchRadians)};
        const double sinPitch{std::sin(pitchRadians)};
        const double cosYaw{std::cos(yawRadians)};
        const double sinYaw{std::sin(yawRadians)};

        pose.target = target;
        pose.position = target + QVector3D{static_cast<float>(distance * cosPitch * cosYaw),
                                           static_cast<float>(distance * cosPitch * sinYaw),
                                           static_cast<float>(distance * sinPitch)};
        return pose;
    }

    QMediaFormat::FileFormat toFileFormat(const ContainerChoice container)
    {
        switch (container) {
        case ContainerChoice::Mov:
            return QMediaFormat::QuickTime;
        case ContainerChoice::Mp4:
        default:
            return QMediaFormat::MPEG4;
        }
    }

    QMediaFormat::VideoCodec toVideoCodec(const VideoCodecChoice codec)
    {
        switch (codec) {
        case VideoCodecChoice::Mpeg4:
            return QMediaFormat::VideoCodec::MPEG4;
        case VideoCodecChoice::H264:
        default:
            return QMediaFormat::VideoCodec::H264;
        }
    }

    QMediaRecorder::Quality toQuality(const QualityChoice quality)
    {
        switch (quality) {
        case QualityChoice::Low:
            return QMediaRecorder::LowQuality;
        case QualityChoice::Normal:
            return QMediaRecorder::NormalQuality;
        case QualityChoice::VeryHigh:
            return QMediaRecorder::VeryHighQuality;
        case QualityChoice::High:
        default:
            return QMediaRecorder::HighQuality;
        }
    }

    double qualityToBitRateScale(const QualityChoice quality)
    {
        switch (quality) {
        case QualityChoice::Low:
            return 0.4;
        case QualityChoice::Normal:
            return 0.7;
        case QualityChoice::VeryHigh:
            return 1.7;
        case QualityChoice::High:
        default:
            return 1.0;
        }
    }

    QString codecName(const VideoCodecChoice codec)
    {
        switch (codec) {
        case VideoCodecChoice::Mpeg4:
            return QStringLiteral("MPEG-4");
        case VideoCodecChoice::H264:
        default:
            return QStringLiteral("H.264");
        }
    }

    QString containerName(const ContainerChoice container)
    {
        switch (container) {
        case ContainerChoice::Mov:
            return QStringLiteral("MOV");
        case ContainerChoice::Mp4:
        default:
            return QStringLiteral("MP4");
        }
    }
}
