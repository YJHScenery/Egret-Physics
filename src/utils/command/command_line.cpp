/**
 * @file        command_line.cpp
 * @brief       基于 CLI11 的命令行解析与分发实现。
 * @details     命令行语法由 CLI11 声明与校验，本文件只做三件事：
 *              1. 把 CLI11 的选项值翻译成 Request；
 *              2. 把 CLI11 的 ParseError / 帮助文本转成 Qt 类型；
 *              3. 按模式分发到 solve_command 与 parse_command。
 *
 *              支持的语法：
 *              - `--solve <scene.epscene> [--duration <秒>] [--sample <毫秒>] [--step <毫秒>] [-o <文件>]`
 *              - `--parse EPSX <帧号> [文件]`（缩写 `-p`）
 *              - `--play <文件>`（缩写 `-r`，进入图形界面的播放器工作区并自动播放）
 *              - `--edit <文件>`（进入图形界面的场景编辑器工作区并载入场景）
 *              - `--help`（缩写 `-h`）
 *
 *              文件关联的注册 / 注销不在本程序中，由独立子项目负责：
 *              `subprojects/epsx_register`（.epsx，播放器）与
 *              `subprojects/epscene_register`（.epscene，场景编辑器）。
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

#include "command_line.h"

#include <CLI11/CLI11.hpp>

#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>
#include <string>
#include <vector>

#include "basic_utils.h"
#include "egret_version.h"
#include "parse_command.h"
#include "solve_command.h"
#include "os/windows/windows_config.h"

namespace egret::command
{
    namespace
    {
        /** CLI11 选项直接绑定的原始值集合。 */
        struct CliOptions
        {
            /** 是否给出 --solve。 */
            bool solve{false};

            /** 位置参数：场景 JSON 路径。 */
            std::string sceneFile{};

            /** 仿真时长（秒）。 */
            double durationSeconds{60.0};

            /** 采样间隔（毫秒）。 */
            double sampleMs{10.0};

            /** 求解推进步长（毫秒）。 */
            double solverStepMs{0.5};

            /** 输出文件路径。 */
            std::string outputFile{};

            /** --parse 的取值：对象类型、帧号、可选的输入文件。 */
            std::vector<std::string> parseArgs{};

            /** --play 的输入文件路径。 */
            std::string playFile{};

            /** --edit 的输入场景文件路径。 */
            std::string editFile{};
        };

        /**
         * @brief  向 CLI11 应用注入全部选项声明。
         * @details 解析与帮助输出共用同一份声明，避免两处描述不一致。
         *          CLI11 的 App 不可拷贝，因此只能在调用方就地构造后注入。
         * @param  app 目标应用。
         * @param  options 选项绑定目标。
         */
        void addOptions(CLI::App& app, CliOptions& options)
        {
            app.set_help_flag("-h,--help", "显示帮助信息");
            app.set_version_flag("--version", std::string{EGRET_VERSION_STRING});

            app.add_flag("--export-video", "导出视频，使用 --export-video --help 查看选项");
            app.add_flag("--solve", options.solve, "离线求解场景并写出仿真文件");
            app.add_option("scene", options.sceneFile, "场景文件路径（.epscene，与 --solve 搭配）");
            app.add_option("--duration", options.durationSeconds, "仿真时长，单位秒（默认 60）");
            app.add_option("--sample", options.sampleMs,
                           "文件采样间隔，单位毫秒：1 / 2 / 5 / 10 / 20（默认 10）");
            app.add_option("--step", options.solverStepMs, "求解推进步长，单位毫秒（默认 0.5）");
            app.add_option("-o,--output", options.outputFile, "输出文件路径（默认写入应用数据目录）");
            app.add_option("-p,--parse", options.parseArgs, "解析仿真文件：--parse EPSX <帧号> [文件]")
                ->expected(2, 3);
            app.add_option("-r,--play,--replay", options.playFile,
                           "以播放器模式打开仿真文件并自动播放（图形界面）");
            app.add_option("-e,--edit", options.editFile,
                           "以场景编辑器打开场景文件并载入（图形界面）");
        }
    }

    void printUsage()
    {
        CliOptions options{};
        CLI::App app{"Egret Physics 物理引擎命令行接口"};
        addOptions(app, options);

        QTextStream out(stdout);
        out << QString::fromStdString(app.help());
        out.flush();
    }

    Request parse(const QStringList& arguments)
    {
        Request request{};
        if (arguments.size() <= 1) {
            return request;
        }

        CliOptions options{};
        CLI::App app{"Egret Physics 物理引擎命令行接口"};
        addOptions(app, options);

        // CLI11 的 std::vector 重载要求「逆序、且不含程序名」的实参列表
        // （与内部 parse(argc, argv) 的构造方式一致）；若按正序连程序名一起传入，
        // 选项取值会整体错位，表现为 “Could not convert: --duration = --solve”。
        std::vector<std::string> argv{};
        argv.reserve(static_cast<std::size_t>(arguments.size()));
        for (auto iterator = arguments.crbegin(); iterator != arguments.crend(); ++iterator) {
            argv.push_back(iterator->toStdString());
        }
        if (!argv.empty()) {
            argv.pop_back(); // 反转后位于末尾的元素即原 argv[0]（程序名）
        }

        try {
            app.parse(argv);
        }
        catch (const CLI::ParseError& error) {
            // 退出码 0 表示这是 --help / --version 之类的正常"中断"，不是错误。
            if (error.get_exit_code() == 0) {
                request.mode = Mode::Help;
                request.helpText = QString::fromStdString(app.help());
                return request;
            }
            request.mode = Mode::Invalid;
            request.errorMessage = QString::fromStdString(error.what());
            return request;
        }

        // --parse / --play / --edit 优先于 --solve：它们互斥，谁先在语义上更具体就归谁。
        if (!options.parseArgs.empty()) {
            request.mode = Mode::Parse;
            request.parseTarget = QString::fromStdString(options.parseArgs[0]);

            bool ok{false};
            request.parseFrame = QString::fromStdString(options.parseArgs[1]).toLongLong(&ok);
            if (!ok) {
                request.mode = Mode::Invalid;
                request.errorMessage =
                    QStringLiteral("帧号不是合法整数：%1").arg(QString::fromStdString(options.parseArgs[1]));
                return request;
            }
            if (options.parseArgs.size() >= 3) {
                request.parseFile = QString::fromStdString(options.parseArgs[2]);
            }
            return request;
        }

        if (!options.playFile.empty()) {
            request.mode = Mode::Play;
            request.playFile = QString::fromStdString(options.playFile);
            return request;
        }

        if (!options.editFile.empty()) {
            request.mode = Mode::Edit;
            request.editFile = QString::fromStdString(options.editFile);
            return request;
        }

        if (options.solve) {
            request.mode = Mode::Solve;
            request.sceneFile = QString::fromStdString(options.sceneFile);
            request.outputFile = QString::fromStdString(options.outputFile);
            request.durationSeconds = options.durationSeconds;
            request.sampleMs = options.sampleMs;
            request.solverStepMs = options.solverStepMs;
            return request;
        }

        // 只给了参数却没给功能开关：按图形界面处理。
        return request;
    }

    int execute(const Request& request)
    {
        setTerminalUtf8();

        // 默认缓存路径依赖 QStandardPaths，需要应用身份信息；缺省时补上与图形界面一致的值。
        if (QCoreApplication::applicationName().isEmpty()) {
            QCoreApplication::setApplicationName(QStringLiteral(EGRET_PHYSICS_APPLICATION_NAME));
        }
        if (QCoreApplication::applicationVersion().isEmpty()) {
            QCoreApplication::setApplicationVersion(egret::EGRET_VERSION_STRING.data());
        }

        switch (request.mode)
        {
        case Mode::Help:
        {
            QTextStream out(stdout);
            if (request.helpText.isEmpty()) {
                printUsage();
            }
            else {
                out << request.helpText;
                out.flush();
            }
            return 0;
        }
        case Mode::Solve:
            return runSolve(request);
        case Mode::Parse:
            return runParse(request);
        case Mode::Play:
        {
            // 正常情况下 main.cpp 已在图形界面中处理该模式；此处只是兜底，
            // 避免任何调用路径静默返回成功。
            QTextStream err(stderr);
            err << "播放器模式需要在图形界面中启动（请由 main.cpp 处理 --play）。\n";
            err.flush();
            return 2;
        }
        case Mode::Edit:
        {
            // 同上：--edit 由 main.cpp 在图形界面里处理。
            QTextStream err(stderr);
            err << "场景编辑器模式需要在图形界面中启动（请由 main.cpp 处理 --edit）。\n";
            err.flush();
            return 2;
        }
        case Mode::Invalid:
        {
            QTextStream err(stderr);
            err << "参数错误：" << request.errorMessage << "\n";
            err.flush();
            printUsage();
            return 2;
        }
        default:
            return 0;
        }
    }
}
