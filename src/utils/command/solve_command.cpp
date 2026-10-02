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

#include <QFile>
#include <QTextStream>

#include <cstdio>

#include "offline_simulation_runner.h"
#include "scene_manager.h"

namespace egret::command
{
    namespace
    {
        /**
         * @brief  读取整个文本文件。
         * @param  path 文件路径。
         * @param  out 输出文件内容。
         * @param  errorMessage 失败时写入错误描述。
         * @return 成功返回 true。
         */
        bool readTextFile(const QString& path, QString* out, QString* errorMessage)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                *errorMessage = QStringLiteral("无法打开文件：%1").arg(path);
                return false;
            }
            *out = QString::fromUtf8(file.readAll());
            return true;
        }
    }

    int runSolve(const Request& request)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        QString sceneJson;
        QString error;

        if (request.sceneFile.isEmpty()) {
            // 未指定场景文件：使用内置预设场景，与图形界面「离线仿真」面板保持一致。
            const SceneManagerViewModel presetScene{};
            sceneJson = presetScene.currentSceneJson();
            if (sceneJson.isEmpty()) {
                err << "无法生成内置预设场景快照。\n";
                err.flush();
                return 1;
            }
            out << "未指定场景文件，使用内置预设场景。\n";
            out.flush();
        }
        else if (!readTextFile(request.sceneFile, &sceneJson, &error)) {
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
        solveRequest.solverStepNs = eps::secondsToNanoseconds(request.solverStepMs / 1000.0);
        solveRequest.sampleIntervalNs = eps::secondsToNanoseconds(request.sampleMs / 1000.0);

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
            << "  耗时：" << QString::number(report.wallClockSeconds, 'f', 2)
            << " 秒，文件大小：" << (report.fileBytes / 1024ull) << " KiB\n";
        out.flush();
        return 0;
    }
}
