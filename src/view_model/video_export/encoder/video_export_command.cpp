/**
 * @file        main.cpp
 * @brief       视频导出命令行工具的入口。
 * @details     本工具把 .epsx 仿真文件离屏渲染成逐帧画面并编码为 MP4 / MOV：
 *
 *              1. 强制使用 FFmpeg 多媒体后端（QVideoFrameInput 只有该后端实现）；
 *              2. 复用主工程的场景重建与 QML 视口组件，保证画面与播放器一致；
 *              3. 渲染在主线程（Quick3D 的场景对象必须留在 GUI 线程），
 *                 像素组装与编码在工作线程完成；
 *              4. 退出码约定：0 成功、1 运行期失败、2 参数错误。
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

#include <QCoreApplication>
#include <QGuiApplication>
#include <QApplication>
#include <QString>
#include <QTextStream>
#include <QtQml/qqml.h>

#include "async_logger.h"
#include "curve_geometry.h"
#include "epsx_video_encoder.h"
#include "os/windows/windows_config.h"
#include "qdebug_handler.h"
#include "spring_geometry.h"
#include "view/geometry/bezier_curve_geometry.h"
#include "video_encoder_options.h"

#include <cstdio>

namespace
{
    /** 参数错误退出码。 */
    constexpr int EXIT_CODE_ARGUMENT_ERROR{2};

    /** 运行期失败退出码。 */
    constexpr int EXIT_CODE_RUNTIME_ERROR{1};

    /**
     * @brief 输出导出结束后的结果摘要。
     * @param output 标准输出流。
     * @param report 导出统计。
     */
    void printReport(QTextStream& output, const egret::video::EpsxVideoEncoder::Report& report)
    {
        constexpr double BYTES_PER_MEGABYTE{1024.0 * 1024.0};
        const double fileSizeMb{static_cast<double>(report.fileBytes) / BYTES_PER_MEGABYTE};

        output << QStringLiteral("导出完成\n")
               << QStringLiteral("  输出文件：%1\n").arg(report.outputPath)
               << QStringLiteral("  分辨率　：%1x%2\n").arg(report.resolution.width()).arg(report.resolution.height())
               << QStringLiteral("  源帧数　：%1　输出帧数：%2\n").arg(report.sourceFrames).arg(report.outputFrames)
               << QStringLiteral("  时长　　：%1 s　帧率：%2 fps\n")
                      .arg(QString::number(report.durationSeconds, 'f', 3),
                           QString::number(report.frameRate, 'f', 3))
               << QStringLiteral("  文件大小：%1 MB　耗时：%2 s\n")
                      .arg(QString::number(fileSizeMb, 'f', 2),
                           QString::number(report.wallClockSeconds, 'f', 2));
        output.flush();
    }
}

/**
 * @brief  程序入口：解析命令行、启动导出任务并等待结束。
 * @param  app 应用实例。
 * @param  arguments 导出参数列表。
 * @return 进程退出码：0 成功、1 运行期失败、2 参数错误。
 */
int runVideoExport(QApplication& app, const QStringList& arguments)
{
    setTerminalUtf8();
    qmlRegisterType<egret::SpringGeometry>("EgretPhysics", 1, 0, "SpringGeometry");
    qmlRegisterType<egret::BezierCurveGeometry>("EgretPhysics", 1, 0, "BezierCurveGeometry");
    qmlRegisterType<egret::CurveGeometry>("EgretPhysics", 1, 0, "CurveGeometry");

    QTextStream standardOutput{stdout};
    QTextStream standardError{stderr};

    const egret::video::ParseResult parsed{egret::video::parseCommandLine(arguments)};
    if (parsed.helpRequested) {
        standardOutput << egret::video::helpText();
        standardOutput.flush();
        return 0;
    }
    if (!parsed.ok) {
        standardError << QStringLiteral("参数错误：%1\n\n").arg(parsed.errorMessage);
        standardError << egret::video::helpText();
        standardError.flush();
        return EXIT_CODE_ARGUMENT_ERROR;
    }

    const egret::video::Options& options{parsed.options};
    standardOutput << QStringLiteral("开始导出视频\n")
                   << QStringLiteral("  输入文件：%1\n").arg(options.inputPath)
                   << QStringLiteral("  输出文件：%1\n")
                          .arg(options.previewPngPath.isEmpty() ? options.outputPath : options.previewPngPath)
                   << QStringLiteral("  分辨率　：%1x%2\n").arg(options.width).arg(options.height)
                   << QStringLiteral("  帧率　　：%1\n")
                          .arg(options.frameRate > 0.0 ? QStringLiteral("%1 fps").arg(options.frameRate)
                                                       : QStringLiteral("跟随 epsx 采样率（逐帧 1:1）"))
                   << QStringLiteral("  编码格式：%1 / %2\n")
                          .arg(egret::video::containerName(options.container), egret::video::codecName(options.codec))
                   << QStringLiteral("  编码后端：%1\n")
                          .arg(options.backend == egret::video::EncoderBackend::Ffmpeg
                                   ? QStringLiteral("FFmpeg 9.0.2（%1）").arg(options.encoderName)
                                   : QStringLiteral("Qt Multimedia"));
    standardOutput.flush();

    egret::video::EpsxVideoEncoder encoder{};

    QObject::connect(&encoder, &egret::video::EpsxVideoEncoder::progress,
                     [&standardOutput, quiet = options.quiet](const quint64 rendered, const quint64 total)
                     {
                         if (quiet || total == 0) {
                             return;
                         }
                         const double percent{static_cast<double>(rendered) * 100.0 / static_cast<double>(total)};
                         standardOutput << QStringLiteral("\r  编码进度：%1 / %2（%3%）")
                                               .arg(rendered)
                                               .arg(total)
                                               .arg(QString::number(percent, 'f', 1));
                         standardOutput.flush();
                     });

    QObject::connect(&encoder, &egret::video::EpsxVideoEncoder::finished,
                     [&app, &encoder, &standardOutput, &standardError,
                      quiet = options.quiet, printStats = options.printStats](
                         const bool success, const QString& message)
                     {
                         if (!quiet) {
                             standardOutput << QLatin1Char('\n');
                         }

                         if (!success) {
                             standardError << QStringLiteral("导出失败：%1\n").arg(message);
                             standardError.flush();
                             app.exit(EXIT_CODE_RUNTIME_ERROR);
                             return;
                         }

                         if (encoder.report().previewOnly) {
                             standardOutput << QStringLiteral("首帧预览已保存：%1\n").arg(encoder.report().outputPath);
                             standardOutput.flush();
                             app.exit(0);
                             return;
                         }

                         printReport(standardOutput, encoder.report());
                         if (printStats) {
                             standardOutput << encoder.timingSummary();
                             standardOutput.flush();
                         }
                         app.exit(0);
                     });

    QString startError{};
    if (!encoder.start(options, &startError)) {
        standardError << QStringLiteral("启动导出失败：%1\n").arg(startError);
        standardError.flush();
        return EXIT_CODE_RUNTIME_ERROR;
    }

    return app.exec();
}
