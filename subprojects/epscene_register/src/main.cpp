/**
 * @file        main.cpp
 * @brief       epscene_register 命令行入口：注册 / 注销 .epscene 文件关联。
 * @details     本工具只做一件事——把 .epscene 扩展名关联到 Egret Physics 场景编辑器，
 *              或删除这一关联。注册内容全部写在 `HKCU\Software\Classes`，
 *              因此不需要管理员权限，也不会影响其他用户或系统级关联。
 *
 *              典型用法：
 *              ```
 *              epscene_register.exe                                   # 注册：编辑器取同目录的 Egret_Physics.exe
 *              epscene_register.exe -e "D:\Egret\Egret_Physics.exe"    # 注册：显式指定编辑器
 *              epscene_register.exe --unregister                      # 注销
 *              ```
 *
 * @author      jehor
 * @date        2026-10-06
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     EpsceneRegister
 * @defgroup    Main (入口)
 */

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <cstdio>

#include "file_association.h"

#if defined(_WIN32)

#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <Windows.h>

namespace
{
    /** 把控制台输出代码页切到 UTF-8，保证中文提示与帮助文本不乱码。 */
    void setConsoleUtf8()
    {
        ::SetConsoleOutputCP(CP_UTF8);
    }
}

#else

namespace
{
    /** 非 Windows 平台无需调整控制台代码页。 */
    void setConsoleUtf8()
    {
    }
}

#endif

namespace
{
    /** 进程退出码：执行成功。 */
    constexpr int kExitOk{0};

    /** 进程退出码：运行失败（例如注册表写入被拒绝）。 */
    constexpr int kExitFailure{1};

    /** 进程退出码：命令行参数错误。 */
    constexpr int kExitUsageError{2};

    /**
     * @brief  打印用法说明。
     * @param  out 输出流（stdout 或 stderr）。
     */
    void printUsage(QTextStream& out)
    {
        out << "epscene_register —— Egret Physics 的 .epscene 场景文件关联工具\n"
               "\n"
               "用法：epscene_register [选项]\n"
               "\n"
               "选项：\n"
               "  --register              注册 .epscene 关联（不带任何选项时的默认动作）\n"
               "  --unregister            删除 .epscene 关联，并清理图标副本\n"
               "  -e, --executable <路径> 指定场景编辑器可执行文件；\n"
               "                          缺省取本工具同目录下的 Egret_Physics.exe\n"
               "  -h, --help              显示本帮助\n"
               "\n"
               "说明：关联只写入 HKCU\\Software\\Classes（当前用户），无需管理员权限；\n"
               "      注册后资源管理器双击 .epscene 会执行 \"<编辑器>\" --edit \"%1\"，\n"
               "      并在场景编辑器工作区载入该场景。\n";
        out.flush();
    }
}

/**
 * @brief  程序入口。
 * @param  argc 参数个数。
 * @param  argv 参数数组。
 * @return 进程退出码：0 成功，1 运行失败，2 参数错误。
 */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    setConsoleUtf8();

    QTextStream out(stdout);
    QTextStream err(stderr);

    bool unregister{false};
    QString executablePath{};

    const QStringList arguments = app.arguments();
    for (int index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments.at(index);

        if (argument == QStringLiteral("--unregister") || argument == QStringLiteral("-u")) {
            unregister = true;
        }
        else if (argument == QStringLiteral("--register")) {
            unregister = false;
        }
        else if (argument == QStringLiteral("--executable") || argument == QStringLiteral("-e")) {
            if (index + 1 >= arguments.size()) {
                err << "缺少 --executable 的取值。\n\n";
                printUsage(err);
                return kExitUsageError;
            }
            executablePath = arguments.at(++index);
        }
        else if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
            printUsage(out);
            return kExitOk;
        }
        else {
            err << "无法识别的参数：" << argument << "\n\n";
            printUsage(err);
            return kExitUsageError;
        }
    }

    const egret::epscene_register::FileAssociationResult result =
        unregister
            ? egret::epscene_register::unregisterEpsceneFileAssociation()
            : egret::epscene_register::registerEpsceneFileAssociation(executablePath);

    QTextStream& stream = result.succeeded ? out : err;
    stream << result.message << "\n";
    stream.flush();
    return result.succeeded ? kExitOk : kExitFailure;
}
