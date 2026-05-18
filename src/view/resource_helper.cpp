//
// Created by jehor on 2026/5/16.
//

#include "resource_helper.h"
#include <QString>

#include "constants.h"

namespace egret
{
    ResourceHelper::ResourceHelper(QObject* parent) : QObject(parent)
    {
    }

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

    double ResourceHelper::getG()
    {
        return G;
    }

    double ResourceHelper::getStandardGravity()
    {
        return STANDARD_GRAVITY;
    }

    double ResourceHelper::getSpeedOfLight()
    {
        return SPEED_OF_LIGHT;
    }

    double ResourceHelper::getStandardAtmosphere()
    {
        return STANDARD_ATMOSPHERE;
    }

    double ResourceHelper::getEarthMass()
    {
        return EARTH_MASS;
    }

    double ResourceHelper::getEarthMeanRadius()
    {
        return EARTH_MEAN_RADIUS;
    }

    double ResourceHelper::getMoonMass()
    {
        return MOON_MASS;
    }

    double ResourceHelper::getMoonOrbitRadius()
    {
        return MOON_ORBIT_RADIUS;
    }

    double ResourceHelper::getSunMass()
    {
        return SUN_MASS;
    }

    double ResourceHelper::getAstronomicalUnit()
    {
        return ASTRONOMICAL_UNIT;
    }
} // egret