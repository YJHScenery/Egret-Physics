//
// Created by jehor on 2026/5/29.
//

#ifndef EGRET_PHYSICS_ENUM_HANDLER_H
#define EGRET_PHYSICS_ENUM_HANDLER_H
#include <QObject>
#include <QString>
#include "shape_base.h"
#include <QQmlComponent>

namespace egret
{
    class EnumHandler: public QObject
    {
        Q_OBJECT
        QML_ELEMENT

    public:
        enum class ShapeIDQml : int
        {
            Unknown = 0,

            Box, Cylinder, CylindricalShell, Disk, Ring, Rod, Sphere, SphericalShell,

            Abstract = 127,

            Other = 255
        };
        Q_ENUM(ShapeIDQml)

        explicit EnumHandler(QObject *parent = nullptr) : QObject(parent) {}

        static EnumHandler *instance();

        static ShapeIDQml toQmlShapeId(ShapeID shapeID);
        static ShapeID toShapeId(ShapeIDQml shapeID);

        Q_INVOKABLE static QString shapeToString(ShapeID shapeID);

        Q_INVOKABLE static QString shapeToString(ShapeIDQml shapeID);

        Q_INVOKABLE static ShapeID stringToShape(const QString& str);

        Q_INVOKABLE static ShapeIDQml stringToShapeQml(const QString& str);

        static void registerEnums();
    };

} // egret

#endif //EGRET_PHYSICS_ENUM_HANDLER_H
