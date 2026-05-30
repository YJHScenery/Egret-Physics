/**
 * @file        logger.cpp
 * @brief       异步日志系统实现文件，提供线程安全的日志记录功能。
 * @details     实现 AsyncLogger 类的各项成员函数，包括日志写入、文件轮转等。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QThreadStorage>
#include <iostream>
namespace egret
{
    // 单例实例 (C++11 线程安全)
    AsyncLogger &AsyncLogger::instance()
    {
        static AsyncLogger instance;
        return instance;
    }

    AsyncLogger::AsyncLogger()
        : m_logLevel(LogLevel::DEBUG), m_logFilePath("app.log"), m_maxFileSize(10 * 1024 * 1024) // 10 MB
          ,
          m_maxBackupFiles(5), m_outputToConsole(true), m_outputToFile(true), m_file(nullptr), m_currentFileSize(0), m_worker(this)
    {
        // 启动后台工作线程
        m_worker.start();
    }

    AsyncLogger::~AsyncLogger()
    {
        // 先刷新所有待处理日志
        flush();
        // 停止工作线程
        m_worker.stop();
        m_worker.wait();
        // 关闭文件
        if (m_file)
        {
            flushFile();
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
    }

    void AsyncLogger::setLogLevel(LogLevel level)
    {
        QMutexLocker locker(&m_mutex);
        m_logLevel = level;
    }

    void AsyncLogger::setLogFilePath(const QString &path)
    {
        QMutexLocker locker(&m_mutex);
        m_logFilePath = path;
    }

    void AsyncLogger::setMaxFileSize(qint64 bytes)
    {
        QMutexLocker locker(&m_mutex);
        m_maxFileSize = bytes;
    }

    void AsyncLogger::setMaxBackupFiles(int count)
    {
        QMutexLocker locker(&m_mutex);
        m_maxBackupFiles = count;
    }

    void AsyncLogger::setOutputToConsole(bool enable)
    {
        QMutexLocker locker(&m_mutex);
        m_outputToConsole = enable;
    }

    void AsyncLogger::setOutputToFile(bool enable)
    {
        QMutexLocker locker(&m_mutex);
        m_outputToFile = enable;
    }

    void AsyncLogger::log(LogLevel level,
                          const QString &file,
                          int line,
                          const QString &function,
                          const QString &message)
    {
        // 快速过滤低于全局级别的日志
        if (level < m_logLevel)
            return;

        LogMessage msg;
        msg.timestamp = QDateTime::currentDateTime();
        msg.level = level;
        msg.file = file;
        msg.line = line;
        msg.function = function;
        msg.message = message;

        // 放入队列，唤醒工作线程
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue(msg);
        m_cond.wakeOne();
    }

    void AsyncLogger::flush()
    {
        // 等待队列清空
        QMutexLocker locker(&m_mutex);
        while (!m_queue.isEmpty())
        {
            m_cond.wait(&m_mutex);
        }
    }

    // ----- 后台工作线程 -----
    AsyncLogger::WorkerThread::WorkerThread(AsyncLogger *logger)
        : m_logger(logger), m_stop(false)
    {
    }

    AsyncLogger::WorkerThread::~WorkerThread()
    {
        stop();
    }

    void AsyncLogger::WorkerThread::stop()
    {
        m_stop = true;
        // 唤醒线程使其退出
        QMutexLocker locker(&m_logger->m_mutex);
        m_logger->m_cond.wakeOne();
    }

    void AsyncLogger::WorkerThread::run()
    {
        while (!m_stop)
        {
            m_logger->processQueue();
            // 短暂休眠避免 CPU 空转（队列空时由条件变量唤醒，不会空转）
        }
    }

    // 后台线程主循环：处理队列中的消息
    void AsyncLogger::processQueue()
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty())
        {
            // 没有日志，等待被唤醒（超时 1 秒是为了定期检查停止标志）
            m_cond.wait(&m_mutex, 1000);
            return;
        }

        // 取出所有待处理的消息（批量处理可提高吞吐量，此处一次取一条）
        while (!m_queue.isEmpty())
        {
            LogMessage msg = m_queue.dequeue();

            // 释放锁，避免阻塞入队（短时间释放锁后重新锁住）
            locker.unlock();

            // 写入文件（如果启用）
            if (m_outputToFile)
            {
                writeToFile(msg);
            }
            // 输出到控制台（如果启用）
            if (m_outputToConsole)
            {
                writeToConsole(msg);
            }

            locker.relock();
        }
        // 队列已空，发送条件变量以通知 flush() 完成
        m_cond.wakeAll();
    }

    void AsyncLogger::writeToFile(const LogMessage &msg)
    {
        // 懒加载文件
        if (!m_file)
        {
            const QString fullPath = m_logFilePath;
            const QDir dir = QFileInfo(fullPath).absoluteDir();
            if (!dir.exists())
                std::ignore = dir.mkpath(".");
            m_file = new QFile(fullPath);
            if (!m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qWarning() << "Cannot open log file:" << fullPath;
                return;
            }
            m_fileStream.setDevice(m_file);
            // m_fileStream.setCodec("UTF-8");
            m_currentFileSize = m_file->size();
        }

        // 检查是否触发滚动
        QString formatted = formatMessage(msg);
        qint64 newSize = m_currentFileSize + formatted.size() + 1; // +1 for newline
        if (newSize > m_maxFileSize && m_maxFileSize > 0)
        {
            // 先关闭当前文件，执行滚动
            flushFile();
            m_file->close();
            delete m_file;
            m_file = nullptr;
            rollLogFile();

            // 重新打开文件
            QString fullPath = m_logFilePath;
            m_file = new QFile(fullPath);
            if (m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                m_fileStream.setDevice(m_file);
                // m_fileStream.setCodec("UTF-8");
                m_currentFileSize = m_file->size();
            }
            else
            {
                qWarning() << "Failed to reopen log file after rollover:" << fullPath;
                return;
            }
        }

        // 写入消息
        m_fileStream << formatted << Qt::endl;
        m_currentFileSize += formatted.size() + 1;
        // 每写入一条日志就刷新一次，确保崩溃时不丢失重要数据（可配置）
        m_fileStream.flush();
    }

    void AsyncLogger::writeToConsole(const LogMessage &msg)
    {
        const QString formatted = formatMessage(msg);
        // 输出到 stderr
        const QByteArray utf8 = formatted.toUtf8();
        fprintf(stderr, "%s\n", utf8.constData());
        fflush(stderr);
    }

    void AsyncLogger::rollLogFile()
    {
        if (m_maxBackupFiles <= 0)
            return;

        QString basePath = m_logFilePath;
        // 删除最老的备份文件（如果存在）
        QString oldestPath = basePath + "." + QString::number(m_maxBackupFiles);
        if (QFile::exists(oldestPath))
            QFile::remove(oldestPath);

        // 依次重命名： .4 -> .5, .3 -> .4, ..., .1 -> .2, 原文件 -> .1
        for (int i = m_maxBackupFiles - 1; i >= 1; --i)
        {
            QString oldPath = basePath + "." + QString::number(i);
            QString newPath = basePath + "." + QString::number(i + 1);
            if (QFile::exists(oldPath))
                QFile::rename(oldPath, newPath);
        }
        // 将当前日志文件重命名为 .1
        if (QFile::exists(basePath))
        {
            QString backupPath = basePath + ".1";
            QFile::rename(basePath, backupPath);
        }
    }

    void AsyncLogger::flushFile()
    {
        if (m_file)
            m_fileStream.flush();
    }

    QString AsyncLogger::formatMessage(const LogMessage &msg)
    {
        char levelChar = '?';
        switch (msg.level)
        {
        case LogLevel::DEBUG:
            levelChar = 'D';
            break;
        case LogLevel::INFO:
            levelChar = 'I';
            break;
        case LogLevel::WARN:
            levelChar = 'W';
            break;
        case LogLevel::ERROR:
            levelChar = 'E';
            break;
        case LogLevel::FATAL:
            levelChar = 'F';
            break;
        }

        QString timestamp = msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
        // 提取文件名（不含路径）
        QString fileName = QFileInfo(msg.file).fileName();

        return QString("[%1] [%2] [%3:%4] [%5] %6")
            .arg(timestamp, levelChar, fileName)
            .arg(msg.line)
            .arg(msg.function, msg.message);
    }
}