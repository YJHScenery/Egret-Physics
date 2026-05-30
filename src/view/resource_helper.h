/**
 * @file        resource_helper.h
 * @brief       QML 资源助手头文件，提供版本信息和资源路径管理功能。
 * @details     定义 ResourceHelper 类，提供获取版本字符串等工具函数，供 QML 调用。
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
#ifndef EGRET_PHYSICS_RESOURCE_HELPER_H
#define EGRET_PHYSICS_RESOURCE_HELPER_H
#include "basic_utils.h"
#include <QObject>
#include <QVariantMap>
#include <QAbstractListModel>

namespace egret
{
    /**
     * @brief       QML 资源助手类，提供版本信息和资源路径管理功能。
     * @details     ResourceHelper 继承自 QObject，是 QML 资源助手类，
     *              提供获取版本字符串等工具函数，供 QML 调用。
     *              提供物理常量查询函数（重力加速度、光速、地球质量等）。
     *              提供形状类型到资源路径的映射。
     *              支持 QML 调用和静态方法调用。
     * 
     * @invariant   getVersionString() 返回有效的版本字符串
     * @invariant   StaticGeneralTypeSourceMap 包含所有形状类型的资源路径
     * @remark      ResourceHelper 是 QObject，支持 QML 调用
     * @see         QObject, ShapeType, constants.h
     */
    class ResourceHelper : public QObject
    {
        Q_OBJECT

    public:
        explicit ResourceHelper(QObject *parent = nullptr);

        Q_INVOKABLE static QString getWholeVersionString();

        Q_INVOKABLE static int getMajorVersion();

        Q_INVOKABLE static int getMinorVersion();

        Q_INVOKABLE static int getPatchVersion();

        Q_INVOKABLE static double getG();

        Q_INVOKABLE static double getStandardGravity();

        Q_INVOKABLE static double getSpeedOfLight();

        Q_INVOKABLE static double getStandardAtmosphere();

        Q_INVOKABLE static double getEarthMass();

        Q_INVOKABLE static double getEarthMeanRadius();

        Q_INVOKABLE static double getMoonMass();

        Q_INVOKABLE static double getMoonOrbitRadius();

        Q_INVOKABLE static double getSunMass();

        Q_INVOKABLE static double getAstronomicalUnit();

        Q_INVOKABLE static QString getSourceByShape(quint32 type);

        static const QMap<std::uint32_t, QString> StaticGeneralTypeSourceMap;
    };

} // egret

#endif // EGRET_PHYSICS_RESOURCE_HELPER_H
