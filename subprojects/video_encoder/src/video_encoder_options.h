/**
 * @file        video_encoder_options.h
 * @brief       视频导出工具的命令行参数模型、解析器与 Qt Multimedia 枚举映射。
 * @details     本文件只回答两个问题：
 *
 *              1. 使用者可以通过命令行指定哪些内容（输入 epsx、输出视频、帧率、分辨率、
 *                 编码格式、质量档、相机位置与朝向、是否显示网格等）；
 *              2. 这些取值如何映射到 Qt Multimedia 的 QMediaFormat / QMediaRecorder 枚举，
 *                 以及环绕参数怎样换算成相机位置与注视点。
 *
 *              解析与映射本身不依赖渲染、编码与文件系统，因此可以被单独测试；
 *              真正的渲染与编码流程见 EpsxVideoEncoder。
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

#ifndef EGRET_VIDEO_ENCODER_VIDEO_ENCODER_OPTIONS_H
#define EGRET_VIDEO_ENCODER_VIDEO_ENCODER_OPTIONS_H

#include <QMediaFormat>
#include <QMediaRecorder>
#include <QString>
#include <QStringList>
#include <QVector3D>

namespace egret::video
{
    /** 视频编码格式（只保留本机 Qt 自带 FFmpeg 后端可能提供编码器的两种）。 */
    enum class VideoCodecChoice
    {
        /** H.264，Windows 上由 Media Foundation 编码器完成（硬件辅助编码）。 */
        H264,

        /** MPEG-4 Part 2，纯软件编码，作为兜底方案。 */
        Mpeg4,
    };

    /** 编码质量档位，直接映射到 QMediaRecorder::Quality。 */
    enum class QualityChoice
    {
        Low,
        Normal,
        High,
        VeryHigh,
    };

    /** 输出封装容器。 */
    enum class ContainerChoice
    {
        /** MP4（QMediaFormat::MPEG4）。 */
        Mp4,

        /** MOV / QuickTime。 */
        Mov,
    };

    /**
     * @brief 相机输入参数（两套输入方式可以任选其一）。
     * @details 坐标一律为物理坐标系（Z 轴向上，单位米），与播放器视口保持一致；
     *          渲染坐标系转换由 SimulationViewport 完成，本结构不做任何转换。
     */
    struct CameraOptions
    {
        /** 是否提供了「相机位置 + 注视点」。 */
        bool hasExplicitPose{false};

        /** 相机位置（物理坐标系，米）。 */
        QVector3D position{0.0F, 0.0F, 0.0F};

        /** 相机注视点（物理坐标系，米）。 */
        QVector3D target{0.0F, 0.0F, 0.0F};

        /** 是否提供了环绕参数（yaw / pitch / distance）。 */
        bool hasOrbit{false};

        /** 环绕偏航角（度）。 */
        float yaw{45.0F};

        /** 环绕俯仰角（度），有效范围 -90 ~ 90。 */
        float pitch{55.0F};

        /** 环绕半径（米）。 */
        float distance{900.0F};

        /** 环绕注视点（物理坐标系，米）。 */
        QVector3D orbitTarget{0.0F, 0.0F, 0.0F};

        /** 垂直视场角（度）。 */
        float fieldOfView{45.0F};

        /** 近裁剪面距离。 */
        float clipNear{1.0F};

        /** 远裁剪面距离。 */
        float clipFar{50000.0F};
    };

    /**
     * @brief 归一化之后的相机姿态。
     * @details 无论调用方给出的是位置 + 注视点，还是环绕参数，最终都换算成该结构；
     *          离屏渲染器只认识这一种表达。
     */
    struct CameraPose
    {
        /** 相机位置（物理坐标系，米）。 */
        QVector3D position{0.0F, 0.0F, 0.0F};

        /** 相机注视点（物理坐标系，米）。 */
        QVector3D target{0.0F, 0.0F, 0.0F};

        /** 垂直视场角（度）。 */
        float fieldOfView{45.0F};

        /** 近裁剪面距离。 */
        float clipNear{1.0F};

        /** 远裁剪面距离。 */
        float clipFar{50000.0F};
    };

    /** 一次视频导出的完整参数。 */
    struct Options
    {
        /** 输入 epsx 文件路径。 */
        QString inputPath{};

        /** 输出视频文件路径。 */
        QString outputPath{};

        /**
         * @brief 首帧预览 PNG 路径。
         * @details 非空时只渲染首帧并保存为 PNG，不进行视频编码，
         *          用于快速确认相机取景与画面内容。
         */
        QString previewPngPath{};

        /** 输出帧率；小于等于 0 表示跟随 epsx 采样率（逐帧 1:1）。 */
        double frameRate{0.0};

        /** 输出宽度（像素），必须为正偶数。 */
        int width{1920};

        /** 输出高度（像素），必须为正偶数。 */
        int height{1080};

        /** 视频编码格式。 */
        VideoCodecChoice codec{VideoCodecChoice::H264};

        /** 编码质量档。 */
        QualityChoice quality{QualityChoice::High};

        /** 封装容器。 */
        ContainerChoice container{ContainerChoice::Mp4};

        /** 相机参数。 */
        CameraOptions camera{};

        /** 是否渲染网格与坐标轴。 */
        bool showGrid{true};

        /** 是否只在标准输出打印最终结果（不打印逐帧进度）。 */
        bool quiet{false};
    };

    /** 命令行解析结果。 */
    struct ParseResult
    {
        /** 参数是否合法。 */
        bool ok{false};

        /** 是否请求打印帮助（--help）。 */
        bool helpRequested{false};

        /** 解析得到的参数（ok 为 true 时有效）。 */
        Options options{};

        /** 失败原因（ok 为 false 时有效）。 */
        QString errorMessage{};
    };

    /**
     * @brief  解析命令行参数。
     * @param  arguments 完整参数列表（含第 0 个可执行文件路径）。
     * @return 解析结果；失败时 errorMessage 给出可直接展示给用户的原因。
     */
    [[nodiscard]] ParseResult parseCommandLine(const QStringList& arguments);

    /**
     * @brief  返回完整帮助文本。
     * @return UTF-8 中文帮助文本。
     */
    [[nodiscard]] QString helpText();

    /**
     * @brief  把相机输入换算为统一的相机姿态。
     * @param  camera 相机输入参数。
     * @return 归一化后的相机姿态；未指定任何输入时使用与播放器视口一致的默认环绕参数。
     */
    [[nodiscard]] CameraPose resolveCameraPose(const CameraOptions& camera);

    /**
     * @brief  容器选择映射到 Qt 文件格式枚举。
     * @param  container 容器选择。
     * @return QMediaFormat 文件格式。
     */
    [[nodiscard]] QMediaFormat::FileFormat toFileFormat(ContainerChoice container);

    /**
     * @brief  编码格式选择映射到 Qt 视频编码枚举。
     * @param  codec 编码格式选择。
     * @return QMediaFormat 视频编码。
     */
    [[nodiscard]] QMediaFormat::VideoCodec toVideoCodec(VideoCodecChoice codec);

    /**
     * @brief  质量档映射到 Qt 录制质量枚举。
     * @param  quality 质量档。
     * @return QMediaRecorder 质量枚举。
     */
    [[nodiscard]] QMediaRecorder::Quality toQuality(QualityChoice quality);

    /**
     * @brief  编码格式的中文名称。
     * @param  codec 编码格式选择。
     * @return 中文名称，用于日志与错误信息。
     */
    [[nodiscard]] QString codecName(VideoCodecChoice codec);

    /**
     * @brief  容器的中文名称。
     * @param  container 容器选择。
     * @return 中文名称，用于日志与错误信息。
     */
    [[nodiscard]] QString containerName(ContainerChoice container);
}

#endif // EGRET_VIDEO_ENCODER_VIDEO_ENCODER_OPTIONS_H
