/**
 * @file        command_line.h
 * @brief       Egret Physics 命令行入口的解析与分发。
 * @details     解析工作交给 CLI11（dependency/include/CLI11/CLI11.hpp）完成，
 *              本模块只负责：把 CLI11 的解析结果翻译为 Request、按模式分发到
 *              solve_command 与 parse_command，以及把 CLI11 的报错与帮助文本
 *              转成 Qt 的类型以便统一输出。
 *
 *              main.cpp 只需在创建 QApplication 后调用 parse() 与 execute()，
 *              不包含任何命令行业务逻辑。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.1.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    Command (命令行接口)
 */

#ifndef EGRET_PHYSICS_COMMAND_LINE_H
#define EGRET_PHYSICS_COMMAND_LINE_H

#include <QString>
#include <QStringList>

namespace egret::command
{
    /**
     * @brief 命令行模式。
     * @details Gui 表示未提供任何功能参数，调用方应继续走图形界面流程。
     */
    enum class Mode
    {
        /** 无功能参数，进入图形界面。 */
        Gui,

        /** --solve：离线求解并写出仿真文件。 */
        Solve,

        /** --parse / -p：解析仿真文件的指定帧。 */
        Parse,

        /** --play / -r：以播放器模式打开文件（尚未实现）。 */
        Play,

        /** --help / -h：打印帮助。 */
        Help,

        /** 参数错误。 */
        Invalid,
    };

    /**
     * @brief       解析后的命令行请求。
     * @details     时长与间隔一律以用户可见单位（秒、毫秒）保存，
     *              换算为纳秒由各自的命令实现完成。
     */
    struct Request
    {
        /** 命令行模式。 */
        Mode mode{Mode::Gui};

        /** --solve 的输入场景 JSON 路径。 */
        QString sceneFile{};

        /** --solve 的输出文件路径；为空时使用默认缓存路径。 */
        QString outputFile{};

        /** 仿真时长（秒）。 */
        double durationSeconds{60.0};

        /** 文件采样间隔（毫秒）。 */
        double sampleMs{10.0};

        /** 求解推进步长（毫秒）。 */
        double solverStepMs{0.5};

        /** --parse 的对象类型，当前只接受 "epsx"。 */
        QString parseTarget{};

        /** --parse 的帧号。 */
        qint64 parseFrame{0};

        /** --parse 的输入文件路径；为空时使用默认缓存路径。 */
        QString parseFile{};

        /** --play 的输入文件路径。 */
        QString playFile{};

        /** 参数错误时的描述信息。 */
        QString errorMessage{};

        /** --help 时由 CLI11 生成的帮助文本。 */
        QString helpText{};
    };

    /**
     * @brief  解析命令行参数。
     * @param  arguments 完整参数列表（含程序名，通常来自 QCoreApplication::arguments()）。
     * @return 解析结果；参数列表只有程序名时返回 Mode::Gui。
     */
    [[nodiscard]] Request parse(const QStringList& arguments);

    /** 打印命令行帮助到标准输出。 */
    void printUsage();

    /**
     * @brief  执行命令行请求。
     * @details 会先把终端设为 UTF-8 输出，并在应用身份信息缺失时补上默认值
     *          （默认缓存路径依赖 QStandardPaths，需要应用名）。
     * @param  request 解析后的请求。
     * @return 进程退出码：0 成功，1 运行失败，2 参数错误或功能未实现。
     */
    [[nodiscard]] int execute(const Request& request);
}

#endif // EGRET_PHYSICS_COMMAND_LINE_H
