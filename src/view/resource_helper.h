//
// Created by jehor on 2026/5/16.
//

#ifndef EGRET_PHYSICS_RESOURCE_HELPER_H
#define EGRET_PHYSICS_RESOURCE_HELPER_H
#include "basic_utils.h"
#include <QObject>

namespace egret
{
    class ResourceHelper : public QObject
    {
        Q_OBJECT

    public:
        explicit ResourceHelper(QObject* parent = nullptr);

        Q_INVOKABLE static QString getVersionString(const QString& prefix, const QString& suffix,
                                                    const QString& sep = " ");

        Q_INVOKABLE static int getMajorVersion();

        Q_INVOKABLE static int getMinorVersion();

        Q_INVOKABLE static int getPatchVersion();
    };
} // egret

#endif //EGRET_PHYSICS_RESOURCE_HELPER_H
