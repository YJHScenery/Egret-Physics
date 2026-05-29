//
// Created by jehor on 2026/5/16.
//

#include "resource_helper.h"
#include <QString>

#include "constants.h"
#include "shape_base.h"
#include <QList>
#include <QVariant>
#include <QVariantMap>

namespace egret
{
    const QMap<std::uint32_t, QString> ResourceHelper::StaticGeneralTypeSourceMap = {
        {static_cast<std::uint32_t>(ShapeID::Box), "#Cube"},
        {static_cast<std::uint32_t>(ShapeID::Cylinder), "#Cylinder"},
        {
            static_cast<std::uint32_t>(ShapeID::CylindricalShell),
            "qrc:/model_3d/assets/model_3d/cylinder_side/cylinder_side.mesh"
        },
        {static_cast<std::uint32_t>(ShapeID::Disk), "#Cylinder"},
        {static_cast<std::uint32_t>(ShapeID::Rod), "#Cylinder"},
        {static_cast<std::uint32_t>(ShapeID::Ring), "qrc:/model_3d/assets/model_3d/torus/mesh/torus_R1.mesh"},
        {static_cast<std::uint32_t>(ShapeID::Sphere), "#Sphere"},
        {static_cast<std::uint32_t>(ShapeID::SphericalShell), "#Sphere"},
    };

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

    QString ResourceHelper::getSourceByShape(quint32 type)
    {
        if (StaticGeneralTypeSourceMap.contains(static_cast<std::uint32_t>(type))) {
            return StaticGeneralTypeSourceMap.value(static_cast<std::uint32_t>(type));
        }
        return {};
    }
} // egret
