/**
 * @file        enum_handler.h
 * @brief       枚举类型处理器头文件，定义物理形状等枚举类型。
 * @details     定义 EnumHandler 类和 ShapeIDQml 等枚举类型，供 QML 和 C++ 共享使用。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_ENUM_HANDLER_H
#define EGRET_PHYSICS_ENUM_HANDLER_H
#include <QObject>
#include <QString>
#include "shape_base.h"
#include <QQmlComponent>

namespace egret
{
    /**
     * @brief       枚举类型处理器类，定义物理形状等枚举类型。
     * @details     EnumHandler 继承自 QObject，是枚举类型处理器类，
     *              定义物理形状等枚举类型，供 QML 和 C++ 共享使用。
     *              提供 ShapeIDQml 枚举，与 ShapeType 枚举对应。
     *              提供枚举类型转换函数（字符串转枚举、枚举转字符串）。
     *              支持 QML 调用和静态方法调用。
     *              采用单例模式，全局唯一实例。
     *
     * @invariant   instance() 返回全局唯一实例
     * @invariant   ShapeIDQml 与 ShapeType 枚举值一一对应
     * @remark      EnumHandler 是 QObject，支持 QML 调用
     * @see         QObject, ShapeType, ShapeIDQml
     */
    class EnumHandler : public QObject
    {
        Q_OBJECT
        QML_ELEMENT

    public:
        enum class ShapeTypeQml : int
        {
            Unknown = 0,

            Box,
            Cylinder,
            CylindricalShell,
            Disk,
            Ring,
            Rod,
            Sphere,
            SphericalShell,

            Abstract = 127,

            Other = 255
        };
        Q_ENUM(ShapeTypeQml)

        explicit EnumHandler(QObject *parent = nullptr) : QObject(parent) {}

        static EnumHandler *instance();

        static ShapeTypeQml toQmlShapeId(ShapeType shapeID);
        static ShapeType toShapeId(ShapeTypeQml shapeID);

        Q_INVOKABLE static QString shapeToString(ShapeType shapeID);

        Q_INVOKABLE static QString shapeToString(ShapeTypeQml shapeID);

        Q_INVOKABLE static ShapeType stringToShape(const QString &str);

        Q_INVOKABLE static ShapeTypeQml stringToShapeQml(const QString &str);

        static void registerEnums();
    };

} // egret

#endif // EGRET_PHYSICS_ENUM_HANDLER_H
