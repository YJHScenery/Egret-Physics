/**
 * @file        solve_command.cpp
 * @brief       --solve 功能的实现。
 * @details     读取场景 JSON、配置离线求解请求、把进度回调转发到标准输出，
 *              最后打印求解统计。求解本身全部由 OfflineSimulationRunner 完成。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    Command (命令行接口)
 */

#include "solve_command.h"

#include <QJsonDocument>
#include "serialize/epscene_archive.h"
#include <QTextStream>

#include <cstdio>

#include "offline_simulation_runner.h"

namespace egret::command
{
    namespace
    {
        /**
         * @brief  兼容读取场景 JSON 或 ZIP 归档。
         * @param  path 文件路径。
         * @param  out 输出文件内容。
         * @param  errorMessage 失败时写入错误描述。
         * @return 成功返回 true。
         */
        bool readTextFile(const QString& path, QString* out, QString* errorMessage)
        {
            QJsonObject scene;
            if (!EpsceneArchive::read(path, scene, *errorMessage)) return false;
            *out = QString::fromUtf8(QJsonDocument(scene).toJson(QJsonDocument::Compact));
            return true;
        }
    }

    int runSolve(const Request& request)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        if (request.sceneFile.isEmpty()) {
            err << "请通过 --solve <scene.epscenex> 指定场景文件。\n";
            err.flush();
            return 2;
        }

        QString sceneJson;
        QString error;
        if (!readTextFile(request.sceneFile, &sceneJson, &error)) {
            err << error << "\n";
            err.flush();
            return 1;
        }

        OfflineSimulationRunner::Request solveRequest{};
        solveRequest.sceneJson = sceneJson;
        solveRequest.outputPath = request.outputFile.isEmpty()
                                      ? OfflineSimulationRunner::defaultSimulationFilePath()
                                      : request.outputFile;
        solveRequest.durationSeconds = request.durationSeconds;
        solveRequest.solverStepNs = epsx::secondsToNanoseconds(request.solverStepMs / 1000.0);
        solveRequest.sampleIntervalNs = epsx::secondsToNanoseconds(request.sampleMs / 1000.0);

        int lastPercent{-1};
        solveRequest.progressCallback =
            [&out, &lastPercent](const std::uint64_t done, const std::uint64_t total)
            {
                if (total == 0) {
                    return;
                }
                const int percent = static_cast<int>(done * 100ull / total);
                if (percent == lastPercent) {
                    return;
                }
                lastPercent = percent;
                out << "\r求解进度 " << percent << "%";
                out.flush();
            };

        OfflineSimulationRunner::Report report{};
        if (!OfflineSimulationRunner::run(solveRequest, &report, &error)) {
            out << "\n";
            out.flush();
            err << "求解失败：" << error << "\n";
            err.flush();
            return 1;
        }

        out << "\n求解完成。\n"
            << "  输出文件：" << solveRequest.outputPath << "\n"
            << "  实体 / 约束 / 场：" << report.bodyCount << " / " << report.constraintCount
            << " / " << report.fieldCount << "\n"
            << "  求解步数：" << report.stepCount << "，写入帧数：" << report.frameCount << "\n"
            << "  控制准备：" << QString::number(report.preparationSeconds,'f',3) << " 秒，缓存命中 " << report.controlCacheHits << "/" << report.controlProgramCount << "\n"
            << "  推进与控制：" << QString::number(report.physicsSeconds,'f',3) << " 秒\n"
            << "  总任务耗时：" << QString::number(report.wallClockSeconds, 'f', 2)
            << " 秒，文件大小：" << (report.fileBytes / 1024ull) << " KiB\n";
        out.flush();
        return 0;
    }
}
