//
// Created by jehor on 2026/5/29.
//

#include "enum_handler.h"
#include <QtQml/qqml.h>
#include <magic_enum.hpp>


namespace egret
{
    EnumHandler* EnumHandler::instance()
    {
        static EnumHandler inst;
        registerEnums();
        return &inst;
    }

    EnumHandler::ShapeIDQml EnumHandler::toQmlShapeId(ShapeType shapeID)
    {
        return static_cast<ShapeIDQml>(shapeID);
    }

    ShapeType EnumHandler::toShapeId(ShapeIDQml shapeID)
    {
        return static_cast<ShapeType>(shapeID);
    }

    QString EnumHandler::shapeToString(ShapeType shapeID)
    {
        const std::string_view view {magic_enum::enum_name<ShapeType>(shapeID)};
        return QString::fromUtf8(view.data(), view.size());
    }

    QString EnumHandler::shapeToString(ShapeIDQml shapeID)
    {
        return shapeToString(toShapeId(shapeID));
    }

    ShapeType EnumHandler::stringToShape(const QString& str)
    {
        auto result{magic_enum::enum_cast<ShapeType>(str.toStdString())};
        if (result.has_value()) {
            return result.value();
        }
        return ShapeType::Unknown;
    }

    EnumHandler::ShapeIDQml EnumHandler::stringToShapeQml(const QString& str)
    {
        return toQmlShapeId(stringToShape(str));
    }

    void EnumHandler::registerEnums()
    {
        qRegisterMetaType<ShapeType>("egret::ShapeID");
        qmlRegisterUncreatableType<EnumHandler>(
            "EnumHandler",
            1,
            0,
            "ShapeID",
            "Access to ShapeID enum only");
    }
} // egret