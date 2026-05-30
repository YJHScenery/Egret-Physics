/**
 * @file        file_handler.cpp
 * @brief       文件处理器实现文件，提供文件读写功能。
 * @details     实现 FileHandler 类的各项成员函数。
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

#include "file_handler.h"

namespace egret
{
    FileHandler::FileHandler(QObject *parent) : QObject(parent)
    {
    }

    bool FileHandler::saveTextToFile(const QString &url, const QString &textContent)
    {
        qDebug() << "Saving text to file:" << textContent;
        qDebug() << "Saving text to file:" << url;

        if (url.isEmpty())
        {
            emit errorOccurred("File URL is empty");
            return false;
        }

        // 将 QML 的 file:// URL 转换为本地路径
        QString filePath = url;
        if (filePath.startsWith("file://"))
        {
            filePath = QUrl(filePath).toLocalFile();
        }

        qDebug() << "Created directory:" << filePath;

        // 检查文件路径有效性
        if (filePath.isEmpty())
        {
            emit errorOccurred("Invalid file path");
            return false;
        }

        // 创建目录（如果不存在）
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists())
        {
            if (!dir.mkpath("."))
            {
                QString error = QString("Failed to create directory: %1").arg(dir.absolutePath());
                emit errorOccurred(error);
                return false;
            }
            qDebug() << "Created directory:" << dir.absolutePath();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QString error = QString("Failed to open file: %1").arg(file.errorString());
            emit errorOccurred(error);
            return false;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << textContent;
        stream.flush();

        file.close();

        if (file.error() != QFile::NoError)
        {
            QString error = QString("Error writing to file: %1").arg(file.errorString());
            emit errorOccurred(error);
            return false;
        }

        qDebug() << "Text file saved successfully:" << filePath;
        emit fileSaved(filePath);
        return true;
    }

    bool FileHandler::saveBinaryToFile(const QString &url, const QByteArray &binaryContent)
    {
        if (url.isEmpty())
        {
            emit errorOccurred("File URL is empty");
            return false;
        }

        if (binaryContent.isEmpty())
        {
            emit errorOccurred("Binary content is empty");
            return false;
        }

        // 将 QML 的 file:// URL 转换为本地路径
        QString filePath = url;
        if (filePath.startsWith("file://"))
        {
            filePath = QUrl(filePath).toLocalFile();
        }

        // 检查文件路径有效性
        if (filePath.isEmpty())
        {
            emit errorOccurred("Invalid file path");
            return false;
        }

        // 创建目录（如果不存在）
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists())
        {
            if (!dir.mkpath("."))
            {
                QString error = QString("Failed to create directory: %1").arg(dir.absolutePath());
                emit errorOccurred(error);
                return false;
            }
            qDebug() << "Created directory:" << dir.absolutePath();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            QString error = QString("Failed to open file: %1").arg(file.errorString());
            emit errorOccurred(error);
            return false;
        }

        qint64 bytesWritten = file.write(binaryContent);
        file.close();

        if (bytesWritten != binaryContent.size())
        {
            QString error = QString("Failed to write all data. Written: %1, Expected: %2")
                                .arg(bytesWritten)
                                .arg(binaryContent.size());
            emit errorOccurred(error);
            return false;
        }

        if (file.error() != QFile::NoError)
        {
            QString error = QString("Error writing to file: %1").arg(file.errorString());
            emit errorOccurred(error);
            return false;
        }

        qDebug() << "Binary file saved successfully:" << filePath
                 << "Size:" << bytesWritten << "bytes";
        emit fileSaved(filePath);
        return true;
    }
} // egret
