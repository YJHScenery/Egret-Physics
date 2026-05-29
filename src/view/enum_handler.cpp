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
        return &inst;
    }

    EnumHandler::ShapeIDQml EnumHandler::toQmlShapeId(ShapeID shapeID)
    {
        return static_cast<ShapeIDQml>(shapeID);
    }

    ShapeID EnumHandler::toShapeId(ShapeIDQml shapeID)
    {
        return static_cast<ShapeID>(shapeID);
    }

    QString EnumHandler::shapeToString(ShapeID shapeID)
    {
        const std::string_view view {magic_enum::enum_name<ShapeID>(shapeID)};
        return QString::fromUtf8(view.data(), view.size());
    }

    QString EnumHandler::shapeToString(ShapeIDQml shapeID)
    {
        return shapeToString(toShapeId(shapeID));
    }

    ShapeID EnumHandler::stringToShape(const QString& str)
    {
        auto result{magic_enum::enum_cast<ShapeID>(str.toStdString())};
        if (result.has_value()) {
            return result.value();
        }
        return ShapeID::Unknown;
    }

    EnumHandler::ShapeIDQml EnumHandler::stringToShapeQml(const QString& str)
    {
        return toQmlShapeId(stringToShape(str));
    }

    void EnumHandler::registerEnums()
    {
        qRegisterMetaType<ShapeID>("egret::ShapeID");
        qmlRegisterUncreatableType<EnumHandler>(
            "EnumHandler",
            1,
            0,
            "ShapeID",
            "Access to ShapeID enum only");
    }
} // egret