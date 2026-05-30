/**
 * @file        resource_helper.cpp
 * @brief       QML 资源助手实现文件，提供版本信息和资源路径管理功能。
 * @details     实现 ResourceHelper 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

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
        {static_cast<std::uint32_t>(ShapeType::Box), "#Cube"},
        {static_cast<std::uint32_t>(ShapeType::Cylinder), "#Cylinder"},
        {static_cast<std::uint32_t>(ShapeType::CylindricalShell),
         "qrc:/model_3d/assets/model_3d/cylinder_side/cylinder_side.mesh"},
        {static_cast<std::uint32_t>(ShapeType::Disk), "#Cylinder"},
        {static_cast<std::uint32_t>(ShapeType::Rod), "#Cylinder"},
        {static_cast<std::uint32_t>(ShapeType::Ring), "qrc:/model_3d/assets/model_3d/torus/mesh/torus_R1.mesh"},
        {static_cast<std::uint32_t>(ShapeType::Sphere), "#Sphere"},
        {static_cast<std::uint32_t>(ShapeType::SphericalShell), "#Sphere"},
    };

    ResourceHelper::ResourceHelper(QObject *parent) : QObject(parent)
    {
    }

    QString ResourceHelper::getVersionString(const QString &prefix, const QString &suffix, const QString &sep)
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
        if (StaticGeneralTypeSourceMap.contains(static_cast<std::uint32_t>(type)))
        {
            return StaticGeneralTypeSourceMap.value(static_cast<std::uint32_t>(type));
        }
        return {};
    }
} // egret
