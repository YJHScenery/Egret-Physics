/**
 * @file        file_handler.h
 * @brief       文件处理器头文件，提供文件读写功能。
 * @details     定义 FileHandler 类，提供保存文本和二进制文件到本地存储的功能。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_FILE_HANDLER_H
#define EGRET_PHYSICS_FILE_HANDLER_H
#include <QObject>
#include <QFile>
#include <QDir>
#include <QUrl>

namespace egret
{
    /**
     * @brief       文件处理器类，提供文件读写功能。
     * @details     FileHandler 继承自 QObject，是文件处理器类，
     *              提供保存文本和二进制文件到本地存储的功能。
     *              支持 QML 调用和信号通知。
     *              提供错误处理和完成通知信号。
     *              用于 QML 文件保存操作。
     *
     * @invariant   saveTextToFile() 返回操作是否成功
     * @invariant   saveBinaryToFile() 返回操作是否成功
     * @remark      FileHandler 是 QObject，支持 QML 调用
     * @see         QObject, QFile, QDir
     */
    class FileHandler : public QObject
    {
        Q_OBJECT

    public:
        explicit FileHandler(QObject *parent = nullptr);

        Q_INVOKABLE bool saveTextToFile(const QString &url, const QString &textContent);

        Q_INVOKABLE bool saveBinaryToFile(const QString &url, const QByteArray &binaryContent);

    signals:
        void errorOccurred(const QString &errorMessage);
        void fileSaved(const QString &url);

    private:
    };
} // egret

#endif // EGRET_PHYSICS_FILE_HANDLER_H
