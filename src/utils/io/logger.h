/**
 * @file        logger.h
 * @brief       异步日志系统头文件，提供线程安全的日志记录功能。
 * @details     定义 AsyncLogger 类，实现异步日志写入，支持多线程安全操作，
 *              可配置日志文件路径和最大文件大小。
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

#ifndef EGRET_PHYSICS_LOGGER_H
#define EGRET_PHYSICS_LOGGER_H

#include <QtGlobal>
#include <QString>
#include <QDateTime>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QFile>
#include <QTextStream>

namespace egret
{

    /**
     * @brief       日志级别枚举，定义日志的严重程度。
     * @details     LogLevel 是日志级别枚举，定义日志的严重程度。
     *              包含 DEBUG、INFO、WARN、ERROR、FATAL 五个级别。
     *              用于控制日志输出和过滤。
     *
     * @see         AsyncLogger, LogMessage
     */
    enum class LogLevel : int
    {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        FATAL = 4
    };

    /**
     * @brief       日志消息结构，包含日志的所有信息。
     * @details     LogMessage 是日志消息结构，包含日志的所有信息。
     *              包含时间戳、日志级别、文件名、行号、函数名、消息内容。
     *              用于异步日志系统的消息传递。
     *
     * @invariant   timestamp 是有效的时间戳
     * @invariant   level 是有效的日志级别
     * @see         AsyncLogger, LogLevel
     */
    struct LogMessage
    {
        QDateTime timestamp;
        LogLevel level;
        QString file;
        int line;
        QString function;
        QString message;
    };

    /**
     * @brief       异步日志系统类，实现线程安全的日志记录功能。
     * @details     AsyncLogger 是异步日志系统类，实现异步日志写入，
     *              支持多线程安全操作，可配置日志文件路径和最大文件大小。
     *              采用单例模式，全局唯一实例。
     *              使用后台线程异步写入日志，避免阻塞主线程。
     *              支持日志滚动（Log Rotation）和备份文件管理。
     *              支持同时输出到控制台和文件。
     *              提供便捷宏（LOG_DEBUG、LOG_INFO 等）自动捕获文件名、行号、函数名。
     *
     * @invariant   instance() 返回全局唯一实例
     * @invariant   m_logLevel 是最低记录级别
     * @invariant   m_maxFileSize > 0，日志文件最大字节数
     * @invariant   m_maxBackupFiles >= 0，最大备份文件数量
     * @remark      AsyncLogger 是单例，全局唯一实例，线程安全
     * @see         LogLevel, LogMessage, WorkerThread
     */
    class AsyncLogger
    {
    public:
        // 获取单例实例
        static AsyncLogger &instance();

        // 禁止拷贝和移动
        AsyncLogger(const AsyncLogger &) = delete;
        AsyncLogger &operator=(const AsyncLogger &) = delete;
        AsyncLogger(AsyncLogger &&) = delete;
        AsyncLogger &operator=(AsyncLogger &&) = delete;

        // 配置接口
        void setLogLevel(LogLevel level);         // 设置全局最低日志级别
        void setLogFilePath(const QString &path); // 设置日志文件路径（不含滚动后缀）
        void setMaxFileSize(qint64 bytes);        // 单个日志文件最大字节数，默认 10MB
        void setMaxBackupFiles(int count);        // 最大滚动文件数量，默认 5
        void setOutputToConsole(bool enable);     // 是否同时输出到控制台，默认 true
        void setOutputToFile(bool enable);        // 是否写入文件，默认 true

        // 记录日志（非阻塞）
        void log(LogLevel level,
                 const QString &file,
                 int line,
                 const QString &function,
                 const QString &message);

        // 强制刷新剩余日志（通常程序退出前调用）
        void flush();

    protected:
        AsyncLogger();
        ~AsyncLogger();

    private:
        // 后台工作线程
        class WorkerThread : public QThread
        {
        public:
            explicit WorkerThread(AsyncLogger *logger);
            ~WorkerThread() override;
            void stop();

        protected:
            void run() override;

        private:
            AsyncLogger *m_logger;
            volatile bool m_stop;
        };

        friend class WorkerThread;

        // 配置参数
        LogLevel m_logLevel;   // 最低记录级别
        QString m_logFilePath; // 基础日志文件路径
        qint64 m_maxFileSize;  // 滚动大小阈值
        int m_maxBackupFiles;  // 最大备份数量
        bool m_outputToConsole;
        bool m_outputToFile;

        // 队列与线程同步
        QQueue<LogMessage> m_queue;
        QMutex m_mutex;
        QWaitCondition m_cond;
        WorkerThread m_worker;

        // 文件输出相关（只能在后台线程使用）
        QFile *m_file;
        QTextStream m_fileStream;
        qint64 m_currentFileSize;

        // 内部写入函数（后台线程调用）
        void processQueue();
        void writeToFile(const LogMessage &msg);
        static void writeToConsole(const LogMessage &msg);
        void rollLogFile();
        void flushFile();

        // 辅助函数：格式化消息为字符串
        static QString formatMessage(const LogMessage &msg);
    };

}

// ----- 便捷宏（自动捕获文件名、行号、函数名）-----
#define LOG_DEBUG(msg) \
    ::egret::AsyncLogger::instance().log(::egret::LogLevel::DEBUG, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_INFO(msg) \
    ::egret::AsyncLogger::instance().log(::egret::LogLevel::INFO, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_WARN(msg) \
    ::egret::AsyncLogger::instance().log(::egret::LogLevel::WARN, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_ERROR(msg) \
    ::egret::AsyncLogger::instance().log(::egret::LogLevel::ERROR, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_FATAL(msg) \
    ::egret::AsyncLogger::instance().log(::egret::LogLevel::FATAL, __FILE__, __LINE__, Q_FUNC_INFO, msg)

// 支持 QString 拼接版本的宏（使用 QStringLiteral 避免构造临时变量）
#define LOG_DEBUG_STR(str) LOG_DEBUG(QString::fromUtf8(str))
#define LOG_INFO_STR(str) LOG_INFO(QString::fromUtf8(str))
#define LOG_WARN_STR(str) LOG_WARN(QString::fromUtf8(str))
#define LOG_ERROR_STR(str) LOG_ERROR(QString::fromUtf8(str))
#define LOG_FATAL_STR(str) LOG_FATAL(QString::fromUtf8(str))

#define LOG_DEBUG_LITERAL(str) LOG_DEBUG(QStringLiteral(str))
#define LOG_INFO_LITERAL(str) LOG_INFO(QStringLiteral(str))
#define LOG_WARN_LITERAL(str) LOG_WARN(QStringLiteral(str))
#define LOG_ERROR_LITERAL(str) LOG_ERROR(QStringLiteral(str))
#define LOG_FATAL_LITERAL(str) LOG_FATAL(QStringLiteral(str))

#endif // EGRET_PHYSICS_LOGGER_H
