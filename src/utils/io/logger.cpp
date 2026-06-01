/**
 * @file        logger.cpp
 * @brief       异步日志系统实现文件，提供线程安全的日志记录功能。
 * @details     实现 AsyncLogger 类的各项成员函数，包括日志写入、文件轮转等。
 *              使用 C++17 标准库实现，无需 Qt 依赖。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.1.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 */

#include "logger.h"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>

namespace egret
{
    // ============================================================
    //  单例
    // ============================================================

    AsyncLogger& AsyncLogger::instance()
    {
        static AsyncLogger instance;
        return instance;
    }

    // ============================================================
    //  构造 / 析构
    // ============================================================

    AsyncLogger::AsyncLogger()
        : m_logLevel(LogLevel::DEBUG)
          , m_logFilePath("app.log")
          , m_maxFileSize(10 * 1024 * 1024) // 10 MB
          , m_maxBackupFiles(5)
          , m_outputToConsole(true)
          , m_outputToFile(true)
          , m_currentFileSize(0)
          , m_worker(this)
    {
    }

    AsyncLogger::~AsyncLogger()
    {
        // 先刷新所有待处理日志
        flush();
        // 停止工作线程
        m_worker.stop();
        // 关闭文件
        if (m_fileStream.is_open()) {
            flushFile();
            m_fileStream.close();
        }
    }

    // ============================================================
    //  配置接口
    // ============================================================

    void AsyncLogger::setLogLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logLevel = level;
    }

    void AsyncLogger::setLogFilePath(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logFilePath = path;
    }

    void AsyncLogger::setMaxFileSize(std::int64_t bytes)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxFileSize = bytes;
    }

    void AsyncLogger::setMaxBackupFiles(int count)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxBackupFiles = count;
    }

    void AsyncLogger::setOutputToConsole(bool enable)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputToConsole = enable;
    }

    void AsyncLogger::setOutputToFile(bool enable)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputToFile = enable;
    }

    // ============================================================
    //  日志记录（非阻塞）
    // ============================================================

    void AsyncLogger::log(LogLevel level,
                          const std::string& file,
                          int line,
                          const std::string& function,
                          const std::string& message)
    {
        // 快速过滤低于全局级别的日志
        if (level < m_logLevel)
            return;

        LogMessage msg;
        msg.timestamp = std::chrono::system_clock::now();
        msg.level = level;
        msg.file = file;
        msg.line = line;
        msg.function = function;
        msg.message = message;

        // 放入队列，唤醒工作线程
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(msg));
        m_cond.notify_one();
    }

    // ============================================================
    //  刷新
    // ============================================================

    void AsyncLogger::flush()
    {
        // 等待队列清空
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this]() { return m_queue.empty(); });
    }

    // ============================================================
    //  后台工作线程
    // ============================================================

    AsyncLogger::WorkerThread::WorkerThread(AsyncLogger* logger)
        : m_logger(logger)
    {
        m_thread = std::thread(&WorkerThread::run, this);
    }

    AsyncLogger::WorkerThread::~WorkerThread()
    {
        stop();
        if (m_thread.joinable())
            m_thread.join();
    }

    void AsyncLogger::WorkerThread::stop()
    {
        m_stop.store(true, std::memory_order_release);
        // 唤醒线程使其退出
        {
            std::lock_guard<std::mutex> lock(m_logger->m_mutex);
            m_logger->m_cond.notify_one();
        }
        if (m_thread.joinable())
            m_thread.join();
    }

    void AsyncLogger::WorkerThread::run()
    {
        while (!m_stop.load(std::memory_order_acquire)) {
            m_logger->processQueue();
        }
    }

    // ============================================================
    //  队列处理（后台线程调用）
    // ============================================================

    void AsyncLogger::processQueue()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_queue.empty()) {
            // 没有日志，等待被唤醒（超时 1 秒是为了定期检查停止标志）
            m_cond.wait_for(lock, std::chrono::seconds(1));
            return;
        }

        // 取出所有待处理的消息
        while (!m_queue.empty()) {
            LogMessage msg = std::move(m_queue.front());
            m_queue.pop();

            // 释放锁，避免阻塞入队
            lock.unlock();

            // 写入文件（如果启用）
            if (m_outputToFile) {
                writeToFile(msg);
            }
            // 输出到控制台（如果启用）
            if (m_outputToConsole) {
                writeToConsole(msg);
            }

            lock.lock();
        }

        // 队列已空，通知 flush() 完成
        lock.unlock();
        m_cond.notify_all();
    }

    // ============================================================
    //  写入文件（后台线程调用）
    // ============================================================

    void AsyncLogger::writeToFile(const LogMessage& msg)
    {
        // 懒加载文件
        if (!m_fileStream.is_open()) {
            const std::string fullPath = m_logFilePath;
            const std::filesystem::path dir = std::filesystem::path(fullPath).parent_path();
            if (!dir.empty() && !std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }

            m_fileStream.open(fullPath, std::ios::out | std::ios::app);
            if (!m_fileStream.is_open()) {
                std::fprintf(stderr, "[AsyncLogger] Cannot open log file: %s\n", fullPath.c_str());
                std::fflush(stderr);
                return;
            }
            m_currentFileSize = static_cast<std::int64_t>(std::filesystem::file_size(fullPath));
        }

        // 检查是否触发滚动
        const std::string formatted = formatMessage(msg);
        const std::int64_t newSize = m_currentFileSize +
            static_cast<std::int64_t>(formatted.size()) + 1; // +1 for newline
        if (newSize > m_maxFileSize && m_maxFileSize > 0) {
            // 先关闭当前文件，执行滚动
            flushFile();
            m_fileStream.close();
            rollLogFile();

            // 重新打开文件
            const std::string fullPath = m_logFilePath;
            m_fileStream.open(fullPath, std::ios::out | std::ios::app);
            if (m_fileStream.is_open()) {
                m_currentFileSize = static_cast<std::int64_t>(std::filesystem::file_size(fullPath));
            }
            else {
                std::fprintf(stderr, "[AsyncLogger] Failed to reopen log file after rollover: %s\n",
                             fullPath.c_str());
                std::fflush(stderr);
                return;
            }
        }

        // 写入消息
        m_fileStream << formatted << std::endl;
        m_currentFileSize += static_cast<std::int64_t>(formatted.size()) + 1;
        m_fileStream.flush();
    }

    // ============================================================
    //  写入控制台（后台线程调用）
    // ============================================================

    void AsyncLogger::writeToConsole(const LogMessage& msg)
    {
        const std::string formatted = formatMessage(msg);
        std::fprintf(stderr, "%s\n", formatted.c_str());
        std::fflush(stderr);
    }

    // ============================================================
    //  日志滚动
    // ============================================================

    void AsyncLogger::rollLogFile()
    {
        if (m_maxBackupFiles <= 0)
            return;

        const std::string& basePath = m_logFilePath;

        // 删除最老的备份文件（如果存在）
        const std::string oldestPath = basePath + "." + std::to_string(m_maxBackupFiles);
        std::error_code ec;
        if (std::filesystem::exists(oldestPath, ec))
            std::filesystem::remove(oldestPath, ec);

        // 依次重命名： .4 -> .5, .3 -> .4, ..., .1 -> .2
        for (int i = m_maxBackupFiles - 1; i >= 1; --i) {
            const std::string oldPath = basePath + "." + std::to_string(i);
            const std::string newPath = basePath + "." + std::to_string(i + 1);
            if (std::filesystem::exists(oldPath, ec))
                std::filesystem::rename(oldPath, newPath, ec);
        }

        // 将当前日志文件重命名为 .1
        if (std::filesystem::exists(basePath, ec)) {
            const std::string backupPath = basePath + ".1";
            std::filesystem::rename(basePath, backupPath, ec);
        }
    }

    // ============================================================
    //  刷新文件缓冲区
    // ============================================================

    void AsyncLogger::flushFile()
    {
        if (m_fileStream.is_open())
            m_fileStream.flush();
    }

    // ============================================================
    //  格式化日志消息
    // ============================================================

    std::string AsyncLogger::formatMessage(const LogMessage& msg)
    {
        char levelChar = '?';
        switch (msg.level) {
        case LogLevel::DEBUG: levelChar = 'D';
            break;
        case LogLevel::INFO: levelChar = 'I';
            break;
        case LogLevel::WARN: levelChar = 'W';
            break;
        case LogLevel::ERROR: levelChar = 'E';
            break;
        case LogLevel::FATAL: levelChar = 'F';
            break;
        }

        // 将 time_point 格式化为 "yyyy-MM-dd HH:mm:ss.fff"
        const auto tt = std::chrono::system_clock::to_time_t(msg.timestamp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.timestamp.time_since_epoch()) % 1000;

        std::tm tm;
#if defined(_MSC_VER)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif

        std::ostringstream timestamp;
        timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();

        // 提取文件名（不含路径）
        const std::string fileName = std::filesystem::path(msg.file).filename().string();

        std::ostringstream oss;
        oss << "[" << timestamp.str() << "]"
            << " [" << levelChar << "]"
            << " [" << fileName << ":" << msg.line << "]"
            << " [" << msg.function << "]"
            << " " << msg.message;

        return oss.str();
    }
}
