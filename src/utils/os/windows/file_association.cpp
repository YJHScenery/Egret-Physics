/**
 * @file        file_association.cpp
 * @brief       Windows 文件关联实现：直接调用 Win32 注册表接口写入 HKCU\Software\Classes。
 * @details     不使用 QSettings：需要精确控制「默认值」「REG_NONE 空值」与整棵子树的删除，
 *              直接调用 RegCreateKeyExW / RegSetValueExW / RegDeleteTreeW 语义最明确，
 *              也与文档 §2.2 给出的 reg add / reg delete 命令一一对应。
 *
 *              写入内容（%CD% 代指可执行文件所在目录，实际取 applicationFilePath）：
 *              ```
 *              HKCU\Software\Classes\.epsx                                  (默认) = EgretPhysics.Simulation
 *              HKCU\Software\Classes\.epsx\OpenWithProgids                  EgretPhysics.Simulation = (空)
 *              HKCU\Software\Classes\EgretPhysics.Simulation                (默认) = Egret Physics Simulation (EPSX)
 *              HKCU\Software\Classes\EgretPhysics.Simulation\DefaultIcon    (默认) = <图标文件>
 *              HKCU\Software\Classes\EgretPhysics.Simulation\shell          (默认) = open
 *              HKCU\Software\Classes\EgretPhysics.Simulation\shell\open\command
 *                                                                           (默认) = "<exe>" --play "%1"
 *              ```
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    FileAssociation (文件关联)
 */

#include "file_association.h"

#if defined(_WIN32)

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <string>

// Qt 头文件先于 Windows.h 引入：Qt 会以 NOMINMAX 等保护宏引入 Windows API，
// 反过来先引入 Windows.h 则可能把 min / max 宏带到 Qt 头文件中。
#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <shlobj.h>

namespace
{
    /** 目标扩展名（小写，含点）。 */
    constexpr wchar_t kEpsxExtension[] = L".epsx";

    /** 关联使用的 ProgID。 */
    constexpr wchar_t kEpsxProgId[] = L"EgretPhysics.Simulation";

    /** 注册表 Classes 根路径（位于 HKCU 之下）。 */
    constexpr wchar_t kClassesRootKey[] = L"Software\\Classes\\";

    /** 关联在「打开方式」中显示的名称。 */
    constexpr wchar_t kEpsxFriendlyName[] = L"Egret Physics Simulation (EPSX)";

    /** 随程序分发的文件图标（qrc 资源路径）。 */
    constexpr const char* kEpsxIconResource = ":/file_icon/assets/file_icon/simulation-file-blue.ico";

    /** 图标落盘后的文件名。 */
    constexpr const char* kEpsxIconFileName = "simulation-file-blue.epsx.ico";

    /**
     * @brief  拼接 HKCU\Software\Classes 下的完整子键路径。
     * @param  relative 相对 Classes 的路径，例如 `.epsx` 或 `EgretPhysics.Simulation`。
     * @return 可直接交给 Win32 注册表接口的路径。
     */
    QString classesKeyPath(const QString& relative)
    {
        return QString::fromWCharArray(kClassesRootKey) + relative;
    }

    /**
     * @brief  把 Win32 错误码整理成中文可读文本。
     * @param  code 注册表接口返回的错误码。
     * @param  keyPath 出错时正在操作的子键路径。
     * @return 形如「写入失败（错误码 5）：Software\Classes\.epsx」的说明。
     */
    QString registryErrorText(const LONG code, const QString& keyPath)
    {
        return QStringLiteral("注册表操作失败（错误码 %1）：%2").arg(code).arg(keyPath);
    }

    /**
     * @brief  在指定子键下写入一个字符串值。
     * @param  subKey 相对 HKCU 的子键路径。
     * @param  valueName 值名；`nullptr` 表示写入键的默认值。
     * @param  value 字符串内容（UTF-16）。
     * @param  error 失败时接收原因。
     * @return 成功返回 true。
     */
    bool writeStringValue(const QString& subKey, const wchar_t* valueName, const std::wstring& value,
                          QString* error)
    {
        HKEY key{nullptr};
        const LONG createResult = ::RegCreateKeyExW(HKEY_CURRENT_USER,
                                                    reinterpret_cast<const wchar_t*>(subKey.utf16()),
                                                    0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                                    nullptr, &key, nullptr);
        if (createResult != ERROR_SUCCESS) {
            *error = registryErrorText(createResult, subKey);
            return false;
        }

        const auto bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        const LONG setResult = ::RegSetValueExW(key, valueName, 0, REG_SZ,
                                                reinterpret_cast<const BYTE*>(value.c_str()), bytes);
        ::RegCloseKey(key);

        if (setResult != ERROR_SUCCESS) {
            *error = registryErrorText(setResult, subKey);
            return false;
        }
        return true;
    }

    /**
     * @brief  在指定子键下写入一个空值（REG_NONE）。
     * @details 用于 `OpenWithProgids`：只需要「值存在」，不携带数据。
     * @param  subKey 相对 HKCU 的子键路径。
     * @param  valueName 值名。
     * @param  error 失败时接收原因。
     * @return 成功返回 true。
     */
    bool writeEmptyValue(const QString& subKey, const wchar_t* valueName, QString* error)
    {
        HKEY key{nullptr};
        const LONG createResult = ::RegCreateKeyExW(HKEY_CURRENT_USER,
                                                    reinterpret_cast<const wchar_t*>(subKey.utf16()),
                                                    0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                                    nullptr, &key, nullptr);
        if (createResult != ERROR_SUCCESS) {
            *error = registryErrorText(createResult, subKey);
            return false;
        }

        const LONG setResult = ::RegSetValueExW(key, valueName, 0, REG_NONE, nullptr, 0);
        ::RegCloseKey(key);

        if (setResult != ERROR_SUCCESS) {
            *error = registryErrorText(setResult, subKey);
            return false;
        }
        return true;
    }

    /**
     * @brief  删除一棵注册表子树。
     * @param  subKey 相对 HKCU 的子键路径。
     * @param  error 失败时接收原因。
     * @return 成功（或键本来就不存在）返回 true。
     */
    bool deleteKeyTree(const QString& subKey, QString* error)
    {
        const LONG result = ::RegDeleteTreeW(HKEY_CURRENT_USER,
                                             reinterpret_cast<const wchar_t*>(subKey.utf16()));
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        *error = registryErrorText(result, subKey);
        return false;
    }

    /**
     * @brief  图标文件在磁盘上的目标路径。
     * @details 放在 `%LOCALAPPDATA%/<应用名>/` 下：既不依赖安装目录是否可写，
     *          也不会被重新部署覆盖。
     * @return 绝对路径。
     */
    QString epsxIconPath()
    {
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/EgretPhysicsStudio");
        return baseDir + QStringLiteral("/") + QString::fromLatin1(kEpsxIconFileName);
    }
}

namespace egret
{
    FileAssociationResult registerEpsxFileAssociation()
    {
        // 1. 把内嵌资源中的图标释放到用户数据目录，供注册表以文件路径引用。
        const QString iconPath = epsxIconPath();
        const QString iconDir = QFileInfo(iconPath).absolutePath();
        if (!QDir().mkpath(iconDir)) {
            return {false, QStringLiteral("无法创建图标目录：%1").arg(iconDir)};
        }

        QFile::remove(iconPath); // 先删除旧副本，保证覆盖为本次构建内嵌的图标
        if (!QFile::copy(QString::fromStdString(kEpsxIconResource), iconPath)) {
            return {false, QStringLiteral("无法写出文件图标：%1").arg(iconPath)};
        }

        // 2. 组装 shell\open\command：双击时以播放器模式打开被点击的文件。
        //    第二个占位符是 Windows 的「被点击文件路径」，固定为字面量 %1。
        const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString openCommand =
            QStringLiteral("\"%1\" --play \"%2\"").arg(executable, QStringLiteral("%1"));

        QString error;
        const QString extensionKey = classesKeyPath(QString::fromWCharArray(kEpsxExtension));
        const QString progIdKey = classesKeyPath(QString::fromWCharArray(kEpsxProgId));
        const QString progId = QString::fromWCharArray(kEpsxProgId);

        const bool writeOk =
            writeStringValue(extensionKey, nullptr, progId.toStdWString(), &error) &&
            writeEmptyValue(extensionKey + QStringLiteral("\\OpenWithProgids"), kEpsxProgId, &error) &&
            writeStringValue(progIdKey, nullptr, std::wstring{kEpsxFriendlyName}, &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\DefaultIcon"), nullptr,
                             iconPath.toStdWString(), &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\shell"), nullptr, L"open", &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\shell\\open\\command"), nullptr,
                             openCommand.toStdWString(), &error);

        if (!writeOk) {
            return {false, error};
        }

        // 3. 通知资源管理器：扩展名与图标缓存已变化。
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return {
            true,
            QStringLiteral("已注册 .epsx 文件关联（当前用户）。\n"
                           "  可执行文件：%1\n"
                           "  文件图标：%2\n"
                           "  双击命令：%3\n"
                           "现在双击任意 .epsx 文件将以播放器模式打开并自动播放。")
                .arg(executable, iconPath, openCommand)
        };
    }

    FileAssociationResult unregisterEpsxFileAssociation()
    {
        QString error;
        const bool deleteOk =
            deleteKeyTree(classesKeyPath(QString::fromWCharArray(kEpsxProgId)), &error) &&
            deleteKeyTree(classesKeyPath(QString::fromWCharArray(kEpsxExtension)), &error);

        if (!deleteOk) {
            return {false, error};
        }

        QFile::remove(epsxIconPath()); // 图标删除失败不影响注销结果
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return {true, QStringLiteral("已删除 .epsx 文件关联（当前用户），并清理了图标副本。")};
    }
}

#else

namespace egret
{
    FileAssociationResult registerEpsxFileAssociation()
    {
        return {false, QStringLiteral("文件关联目前只支持 Windows。")};
    }

    FileAssociationResult unregisterEpsxFileAssociation()
    {
        return {false, QStringLiteral("文件关联目前只支持 Windows。")};
    }
}

#endif
