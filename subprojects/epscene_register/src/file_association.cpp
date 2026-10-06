/**
 * @file        file_association.cpp
 * @brief       Windows 文件关联实现：直接调用 Win32 注册表接口写入 HKCU\Software\Classes。
 * @details     不使用 QSettings：需要精确控制「默认值」「REG_NONE 空值」与整棵子树的删除，
 *              直接调用 RegCreateKeyExW / RegSetValueExW / RegDeleteTreeW 语义最明确，
 *              也与 .epsx 关联工具的写法保持一致。
 *
 *              写入内容（可执行文件取参数，默认是本工具同目录的 Egret_Physics.exe）：
 *              ```
 *              HKCU\Software\Classes\.epscene                                 (默认) = EgretPhysics.Scene
 *              HKCU\Software\Classes\.epscene\OpenWithProgids                 EgretPhysics.Scene = (空)
 *              HKCU\Software\Classes\EgretPhysics.Scene                       (默认) = Egret Physics Scene (EPSCENE)
 *              HKCU\Software\Classes\EgretPhysics.Scene\DefaultIcon           (默认) = <图标文件>
 *              HKCU\Software\Classes\EgretPhysics.Scene\shell                 (默认) = open
 *              HKCU\Software\Classes\EgretPhysics.Scene\shell\open\command
 *                                                                             (默认) = "<exe>" --edit "%1"
 *              ```
 *
 *              图标写出策略与 .epsx 工具一致：直接以「写 + 截断」方式覆盖目标文件
 *              （必要时先清除只读属性），目标被占用且内容已一致时直接复用，内容不同
 *              又写不进去时退回到带内容指纹的备用文件名。因此「注销 → 重新注册」
 *              不会被残留的只读 / 被占用的图标挡住。
 *
 * @author      jehor
 * @date        2026-10-06
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     EpsceneRegister
 * @defgroup    FileAssociation (场景文件关联)
 */

#include "file_association.h"

#if defined(_WIN32)

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
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
    constexpr wchar_t kEpsceneExtension[] = L".epscene";

    /** 关联使用的 ProgID。 */
    constexpr wchar_t kEpsceneProgId[] = L"EgretPhysics.Scene";

    /** 注册表 Classes 根路径（位于 HKCU 之下）。 */
    constexpr wchar_t kClassesRootKey[] = L"Software\\Classes\\";

    /** 关联在「打开方式」中显示的名称。 */
    constexpr wchar_t kEpsceneFriendlyName[] = L"Egret Physics Scene (EPSCENE)";

    /** 未显式指定时使用的编辑器可执行文件名（与本工具同目录）。 */
    constexpr const char* kDefaultEditorExecutable = "Egret_Physics.exe";

    /** 本工具内嵌的场景文件图标（qrc 资源路径）。 */
    constexpr const char* kEpsceneIconResource = ":/epscene_register/serialization-scene-json-file.ico";

    /** 图标落盘目录（位于用户数据目录之下），与 .epsx 工具共用同一目录。 */
    constexpr const char* kEpsceneIconDirName = "EgretPhysicsStudio";

    /** 图标文件名主干与后缀：完整名形如 `serialization-scene-json-file.epscene.ico`。 */
    constexpr const char* kEpsceneIconBaseName = "serialization-scene-json-file";
    constexpr const char* kEpsceneIconSuffix = ".epscene.ico";

    /**
     * @brief  拼接 HKCU\Software\Classes 下的完整子键路径。
     * @param  relative 相对 Classes 的路径，例如 `.epscene` 或 `EgretPhysics.Scene`。
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
     * @return 形如「注册表操作失败（错误码 5）：Software\Classes\.epscene」的说明。
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
     * @brief  图标所在的用户数据目录。
     * @details 放在 `%LOCALAPPDATA%/<应用名>/` 下：既不依赖安装目录是否可写，
     *          也不会被重新部署覆盖。与 .epsx 工具共用该目录，只是文件名不同。
     * @return 绝对路径。
     */
    QString epsceneIconDirectory()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QLatin1Char('/') + QString::fromLatin1(kEpsceneIconDirName);
    }

    /**
     * @brief  图标在磁盘上的首选路径（`<目录>/serialization-scene-json-file.epscene.ico`）。
     * @return 绝对路径。
     */
    QString epsceneIconPath()
    {
        return epsceneIconDirectory() + QLatin1Char('/') + QString::fromLatin1(kEpsceneIconBaseName)
            + QString::fromLatin1(kEpsceneIconSuffix);
    }

    /**
     * @brief  图标在磁盘上的备用路径，文件名携带内容指纹。
     * @details 首选路径被别的进程占用（例如资源管理器正把它当作 .epscene 的图标）且
     *          内容又需要更新时，写到这里就能绕开占用，同时不影响正在使用旧图标的进程。
     * @param  contentTag 内容指纹（短十六进制串）。
     * @return 绝对路径。
     */
    QString epsceneIconPathWithTag(const QString& contentTag)
    {
        return epsceneIconDirectory() + QLatin1Char('/') + QString::fromLatin1(kEpsceneIconBaseName)
            + QLatin1Char('-') + contentTag + QString::fromLatin1(kEpsceneIconSuffix);
    }

    /**
     * @brief  读取内嵌的 .epscene 图标资源。
     * @param  error 失败时接收原因。
     * @return 图标字节；失败时为空。
     */
    QByteArray loadIconData(QString* error)
    {
        const QString resourcePath = QString::fromLatin1(kEpsceneIconResource);
        QFile resource{resourcePath};
        if (!resource.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("程序内嵌的图标资源无法读取：%1").arg(resourcePath);
            return {};
        }

        const QByteArray data = resource.readAll();
        if (data.isEmpty()) {
            *error = QStringLiteral("程序内嵌的图标资源为空：%1").arg(resourcePath);
            return {};
        }
        return data;
    }

    /**
     * @brief  把字节写入目标文件（覆盖已有内容）。
     * @details 目标可能是只读文件——典型场景是文件被资源管理器登记为 .epscene 的图标，
     *          既带只读属性又可能被占用。这里先直接覆盖，失败后清掉只读属性再试一次，
     *          避免「卸载后重新注册」这种最常见的操作被一个只读属性挡住。
     * @param  path 目标路径。
     * @param  data 要写入的字节。
     * @param  error 失败时接收原因。
     * @return 成功返回 true。
     */
    bool writeFileBytes(const QString& path, const QByteArray& data, QString* error)
    {
        QFile file{path};
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            // 目标可能是只读文件（例如已被资源管理器登记为 .epscene 图标）：
            // 清掉只读属性后再试一次。
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                            | QFileDevice::ReadUser | QFileDevice::WriteUser
                                            | QFileDevice::ReadOther);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                *error = QStringLiteral("无法写入文件：%1（%2；已尝试清除只读属性后重试）")
                             .arg(path, file.errorString());
                return false;
            }
        }

        const qint64 written = file.write(data);
        file.close();

        if (written != data.size()) {
            *error = QStringLiteral("写入文件不完整：%1（预期 %2 字节，实际 %3 字节）")
                         .arg(path).arg(data.size()).arg(written);
            return false;
        }

        // 保证写出的图标是可覆盖的普通文件：某些复制路径会把文件写成只读，
        // 那会导致下一次注册失败。
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                        | QFileDevice::ReadUser | QFileDevice::WriteUser
                                        | QFileDevice::ReadOther);
        return true;
    }

    /**
     * @brief  确保图标已落到磁盘，并返回实际可用的图标路径。
     * @details 顺序为「首选路径直接覆盖写入」→「若内容已一致则直接复用」→
     *          「退回带内容指纹的备用文件名」。三步都失败才报错。
     * @param  error 失败时接收原因。
     * @return 图标文件的绝对路径；失败时为空。
     */
    QString ensureIconFileOnDisk(QString* error)
    {
        const QByteArray iconData = loadIconData(error);
        if (iconData.isEmpty()) {
            return {};
        }

        const QString directory = epsceneIconDirectory();
        if (!QDir().mkpath(directory)) {
            *error = QStringLiteral("无法创建图标目录：%1").arg(directory);
            return {};
        }

        const QString preferred = epsceneIconPath();
        if (writeFileBytes(preferred, iconData, error)) {
            return preferred;
        }

        // 首选路径写不进去：若现有内容与内嵌图标完全一致，直接复用它。
        QFile existing{preferred};
        if (existing.open(QIODevice::ReadOnly)) {
            const bool identical = (existing.readAll() == iconData);
            existing.close();
            if (identical) {
                error->clear();
                return preferred;
            }
        }

        const QString contentTag =
            QString::fromLatin1(QCryptographicHash::hash(iconData, QCryptographicHash::Sha256)
                                    .toHex()
                                    .left(8));
        const QString tagged = epsceneIconPathWithTag(contentTag);
        if (writeFileBytes(tagged, iconData, error)) {
            return tagged;
        }
        return {};
    }

    /**
     * @brief  清理本工具写出的全部图标副本（含带指纹的备用文件）。
     * @details 删除失败（文件被占用或只读）不算错误：注销的关键是注册表键已被删除，
     *          残留的图标文件不影响功能。
     */
    void removeIconFiles()
    {
        const QDir directory{epsceneIconDirectory()};
        const QStringList leftovers = directory.entryList(
            {QString::fromLatin1(kEpsceneIconBaseName) + QStringLiteral("*")
             + QString::fromLatin1(kEpsceneIconSuffix)},
            QDir::Files);

        for (const QString& name : leftovers) {
            const QString path = directory.absoluteFilePath(name);
            if (!QFile::remove(path)) {
                QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                | QFileDevice::ReadUser | QFileDevice::WriteUser
                                                | QFileDevice::ReadOther);
                QFile::remove(path);
            }
        }
    }

    /**
     * @brief  解析编辑器可执行文件路径。
     * @param  overridePath 显式指定的路径；为空时取本工具同目录下的 Egret_Physics.exe。
     * @param  error 失败时接收原因。
     * @return 存在的绝对路径（已转为 Windows 分隔符）；失败返回空串。
     */
    QString resolveEditorExecutable(const QString& overridePath, QString* error)
    {
        const QString candidate = overridePath.isEmpty()
            ? QDir(QCoreApplication::applicationDirPath())
                  .absoluteFilePath(QString::fromLatin1(kDefaultEditorExecutable))
            : QFileInfo(overridePath).absoluteFilePath();

        if (!QFileInfo::exists(candidate)) {
            *error = QStringLiteral("找不到编辑器可执行文件：%1\n"
                                    "请用 -e / --executable <路径> 显式指定。")
                         .arg(QDir::toNativeSeparators(candidate));
            return {};
        }
        return QDir::toNativeSeparators(candidate);
    }
}

namespace egret::epscene_register
{
    FileAssociationResult registerEpsceneFileAssociation(const QString& executablePath)
    {
        // 1. 解析编辑器路径：注册表里的命令行必须指向真正的主程序，而不是本工具。
        QString error;
        const QString executable = resolveEditorExecutable(executablePath, &error);
        if (executable.isEmpty()) {
            return {false, error};
        }

        // 2. 把内嵌的图标释放到用户数据目录，供注册表以文件路径引用。
        const QString iconPath = ensureIconFileOnDisk(&error);
        if (iconPath.isEmpty()) {
            return {false, error};
        }

        // 3. 组装 shell\open\command：双击时以编辑器模式打开被点击的场景。
        //    第二个占位符是 Windows 的「被点击文件路径」，固定为字面量 %1。
        const QString openCommand =
            QStringLiteral("\"%1\" --edit \"%2\"").arg(executable, QStringLiteral("%1"));

        const QString extensionKey = classesKeyPath(QString::fromWCharArray(kEpsceneExtension));
        const QString progIdKey = classesKeyPath(QString::fromWCharArray(kEpsceneProgId));
        const QString progId = QString::fromWCharArray(kEpsceneProgId);

        const bool writeOk =
            writeStringValue(extensionKey, nullptr, progId.toStdWString(), &error) &&
            writeEmptyValue(extensionKey + QStringLiteral("\\OpenWithProgids"), kEpsceneProgId, &error) &&
            writeStringValue(progIdKey, nullptr, std::wstring{kEpsceneFriendlyName}, &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\DefaultIcon"), nullptr,
                             iconPath.toStdWString(), &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\shell"), nullptr, L"open", &error) &&
            writeStringValue(progIdKey + QStringLiteral("\\shell\\open\\command"), nullptr,
                             openCommand.toStdWString(), &error);

        if (!writeOk) {
            return {false, error};
        }

        // 4. 通知资源管理器：扩展名与图标缓存已变化。
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return {
            true,
            QStringLiteral("已注册 .epscene 场景文件关联（当前用户）。\n"
                           "  编辑器：%1\n"
                           "  文件图标：%2\n"
                           "  双击命令：%3\n"
                           "现在双击任意 .epscene 文件将以场景编辑器打开并载入场景。")
                .arg(executable, iconPath, openCommand)
        };
    }

    FileAssociationResult unregisterEpsceneFileAssociation()
    {
        QString error;
        const bool deleteOk =
            deleteKeyTree(classesKeyPath(QString::fromWCharArray(kEpsceneProgId)), &error) &&
            deleteKeyTree(classesKeyPath(QString::fromWCharArray(kEpsceneExtension)), &error);

        if (!deleteOk) {
            return {false, error};
        }

        removeIconFiles(); // 图标删除失败不影响注销结果（例如文件正被资源管理器占用）
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return {true, QStringLiteral("已删除 .epscene 场景文件关联（当前用户），并清理了图标副本。")};
    }
}

#else

namespace egret::epscene_register
{
    FileAssociationResult registerEpsceneFileAssociation(const QString& executablePath)
    {
        Q_UNUSED(executablePath)
        return {false, QStringLiteral("文件关联目前只支持 Windows。")};
    }

    FileAssociationResult unregisterEpsceneFileAssociation()
    {
        return {false, QStringLiteral("文件关联目前只支持 Windows。")};
    }
}

#endif
