//
// Created by jehor on 2026/4/23.
//

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

// 日志级别枚举
enum class LogLevel : int {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    FATAL = 4
};

// 日志消息结构（前端提交的数据）
struct LogMessage {
    QDateTime   timestamp;
    LogLevel    level;
    QString     file;
    int         line;
    QString     function;
    QString     message;
};

// 异步日志系统（单例）
class AsyncLogger
{
public:
    // 获取单例实例
    static AsyncLogger& instance();

    // 禁止拷贝和移动
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    // 配置接口
    void setLogLevel(LogLevel level);                 // 设置全局最低日志级别
    void setLogFilePath(const QString& path);         // 设置日志文件路径（不含滚动后缀）
    void setMaxFileSize(qint64 bytes);                // 单个日志文件最大字节数，默认 10MB
    void setMaxBackupFiles(int count);                // 最大滚动文件数量，默认 5
    void setOutputToConsole(bool enable);             // 是否同时输出到控制台，默认 true
    void setOutputToFile(bool enable);                // 是否写入文件，默认 true

    // 记录日志（非阻塞）
    void log(LogLevel level,
             const QString& file,
             int line,
             const QString& function,
             const QString& message);

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
        WorkerThread(AsyncLogger* logger);
        ~WorkerThread() override;
        void stop();
    protected:
        void run() override;
    private:
        AsyncLogger* m_logger;
        volatile bool m_stop;
    };

    friend class WorkerThread;

    // 配置参数
    LogLevel    m_logLevel;          // 最低记录级别
    QString     m_logFilePath;       // 基础日志文件路径
    qint64      m_maxFileSize;       // 滚动大小阈值
    int         m_maxBackupFiles;    // 最大备份数量
    bool        m_outputToConsole;
    bool        m_outputToFile;

    // 队列与线程同步
    QQueue<LogMessage> m_queue;
    QMutex             m_mutex;
    QWaitCondition     m_cond;
    WorkerThread       m_worker;

    // 文件输出相关（只能在后台线程使用）
    QFile*      m_file;
    QTextStream m_fileStream;
    qint64      m_currentFileSize;

    // 内部写入函数（后台线程调用）
    void processQueue();
    void writeToFile(const LogMessage& msg);
    static void writeToConsole(const LogMessage& msg);
    void rollLogFile();
    void flushFile();

    // 辅助函数：格式化消息为字符串
    static QString formatMessage(const LogMessage& msg);
};

}

// ----- 便捷宏（自动捕获文件名、行号、函数名）-----
#define LOG_DEBUG(msg) \
AsyncLogger::instance().log(LogLevel::DEBUG, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_INFO(msg) \
AsyncLogger::instance().log(LogLevel::INFO, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_WARN(msg) \
AsyncLogger::instance().log(LogLevel::WARN, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_ERROR(msg) \
AsyncLogger::instance().log(LogLevel::ERROR, __FILE__, __LINE__, Q_FUNC_INFO, msg)

#define LOG_FATAL(msg) \
AsyncLogger::instance().log(LogLevel::FATAL, __FILE__, __LINE__, Q_FUNC_INFO, msg)

// 支持 QString 拼接版本的宏（使用 QStringLiteral 避免构造临时变量）
#define LOG_DEBUG_STR(str)   LOG_DEBUG(QString::fromUtf8(str))
#define LOG_INFO_STR(str)    LOG_INFO(QString::fromUtf8(str))
#define LOG_WARN_STR(str)    LOG_WARN(QString::fromUtf8(str))
#define LOG_ERROR_STR(str)   LOG_ERROR(QString::fromUtf8(str))
#define LOG_FATAL_STR(str)   LOG_FATAL(QString::fromUtf8(str))


#endif //EGRET_PHYSICS_LOGGER_H
