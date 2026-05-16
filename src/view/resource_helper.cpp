//
// Created by jehor on 2026/5/16.
//

#include "resource_helper.h"
#include <QString>

namespace egret
{
    QString ResourceHelper::getVersionString(const QString& prefix, const QString& suffix, const QString& sep)
    {
        QString result{};
        result += prefix;
        result += sep;
        result += EGRET_PHYSICS_VERSION_STRING;
        result += sep;
        result += suffix;
        return result;
    }

     int ResourceHelper::getMajorVersion()
    {
        return EGRET_PHYSICS_VERSION_MAJOR;
    }

     int ResourceHelper::getMinorVersion()
    {
        return EGRET_PHYSICS_VERSION_MINOR;
    }

     int ResourceHelper::getPatchVersion()
    {
        return EGRET_PHYSICS_VERSION_PATCH;
    }
} // egret