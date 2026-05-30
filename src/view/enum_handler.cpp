/**
 * @file        enum_handler.cpp
 * @brief       枚举类型处理器实现文件，定义物理形状等枚举类型。
 * @details     实现 EnumHandler 类的各项成员函数。
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

#include "enum_handler.h"
#include <QtQml/qqml.h>
#include <magic_enum.hpp>

namespace egret
{
    EnumHandler *EnumHandler::instance()
    {
        static EnumHandler inst;
        registerEnums();
        return &inst;
    }

    EnumHandler::ShapeTypeQml EnumHandler::toQmlShapeId(ShapeType shapeID)
    {
        return static_cast<ShapeTypeQml>(shapeID);
    }

    ShapeType EnumHandler::toShapeId(ShapeTypeQml shapeID)
    {
        return static_cast<ShapeType>(shapeID);
    }

    QString EnumHandler::shapeToString(ShapeType shapeID)
    {
        const std::string_view view{magic_enum::enum_name<ShapeType>(shapeID)};
        return QString::fromUtf8(view.data(), view.size());
    }

    QString EnumHandler::shapeToString(ShapeTypeQml shapeID)
    {
        return shapeToString(toShapeId(shapeID));
    }

    ShapeType EnumHandler::stringToShape(const QString &str)
    {
        auto result{magic_enum::enum_cast<ShapeType>(str.toStdString())};
        if (result.has_value())
        {
            return result.value();
        }
        return ShapeType::Unknown;
    }

    EnumHandler::ShapeTypeQml EnumHandler::stringToShapeQml(const QString &str)
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