//
// Created by jehor on 2026/5/30.
//

#ifndef EGRET_PHYSICS_QDEBUG_HANDLER_H
#define EGRET_PHYSICS_QDEBUG_HANDLER_H

#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <iostream>
#include <cstdio>
#include <QMutexLocker>

class CustomMessageHandler
{
public:
    static void setOutputTarget(const QString& outputFileName);

    static void message(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    static void install();

    static void uninstall();

private:
    CustomMessageHandler() = default;

    static QString m_outputFileName;

    static QMutex logMutex; // 用于多线程安全
};


#endif //EGRET_PHYSICS_QDEBUG_HANDLER_H
