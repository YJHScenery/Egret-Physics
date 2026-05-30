//
// Created by jehor on 2026/5/30.
//
#include "qdebug_handler.h"

QString CustomMessageHandler::m_outputFileName = "debug_log.txt"; // 默认文件名
QMutex CustomMessageHandler::logMutex;

void CustomMessageHandler::setOutputTarget(const QString& outputFileName)
{
    m_outputFileName = outputFileName;
}

void CustomMessageHandler::message(const QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    // 1. 准备格式化日志字符串
    QString logEntry = QString("[%1] ")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));

    // 确定日志级别
    switch (type) {
    case QtDebugMsg:
        logEntry += "[DEBUG] ";
        break;
    case QtInfoMsg:
        logEntry += "[INFO] ";
        break;
    case QtWarningMsg:
        logEntry += "[WARN] ";
        break;
    case QtCriticalMsg:
        logEntry += "[CRITICAL] ";
        break;
    case QtFatalMsg:
        logEntry += "[FATAL] ";
        break;
    }

    logEntry += msg;

    // 如果可用，附加源文件名和行号
    if (context.file) {
        logEntry += QString(" (File: %1, Line: %2, Function: %3)")
                    .arg(context.file)
                    .arg(context.line)
                    .arg(context.function);
    }
    logEntry += "\n";

    // 2. 输出到控制台 (标准错误流)
    fprintf(stderr, "%s", logEntry.toLocal8Bit().constData());
    fflush(stderr); // 确保立即刷新

    // 3. 输出到专用的调试日志文件
    QMutexLocker locker(&logMutex); // 线程安全地加锁
    QFile debugLogFile(m_outputFileName); // 替换为你想要的文件路径
    if (debugLogFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream out(&debugLogFile);
        out << logEntry;
        debugLogFile.close();
    }

    // 对于致命错误，终止程序
    if (type == QtFatalMsg) {
        abort();
    }
}

void CustomMessageHandler::install()
{
    qInstallMessageHandler(message);
}

void CustomMessageHandler::uninstall()
{
    qInstallMessageHandler(nullptr);
}
