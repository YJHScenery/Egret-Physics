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

    // EnumHandler::ShapeTypeQml EnumHandler::toQmlShapeId(ShapeType shapeID)
    // {
    //     return static_cast<ShapeTypeQml>(shapeID);
    // }
    //
    // ShapeType EnumHandler::toShapeId(ShapeTypeQml shapeID)
    // {
    //     return static_cast<ShapeType>(shapeID);
    // }

    // QString EnumHandler::shapeToString(ShapeType shapeID)
    // {
    //     const std::string_view view{magic_enum::enum_name<ShapeType>(shapeID)};
    //     return QString::fromUtf8(view.data(), view.size());
    // }


    //
    // ShapeType EnumHandler::stringToShape(const QString &str)
    // {
    //     auto result{magic_enum::enum_cast<ShapeType>(str.toStdString())};
    //     if (result.has_value())
    //     {
    //         return result.value();
    //     }
    //     return ShapeType::Unknown;
    // }
    //
    // EnumHandler::ShapeTypeQml EnumHandler::stringToShapeQml(const QString &str)
    // {
    //     return toQmlShapeId(stringToShape(str));
    // }

    QString EnumHandler::shapeTypeToString(int shapeID)
    {
        const auto st = static_cast<ShapeType>(shapeID);
        const std::string_view view{magic_enum::enum_name<ShapeType>(st)};
        return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size()));
    }

    QString EnumHandler::fieldTypeToString(int typeId)
    {
        const auto ft = static_cast<FieldType>(typeId);
        const std::string_view view{magic_enum::enum_name<FieldType>(ft)};
        return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size()));
    }

    QString EnumHandler::constraintTypeToString(int typeId)
    {
        const auto ct = static_cast<ConstraintType>(typeId);
        const std::string_view view{magic_enum::enum_name<ConstraintType>(ct)};
        return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size()));
    }

    void EnumHandler::registerEnums()
    {
        qRegisterMetaType<ShapeType>("egret::ShapeID");

        /// 注册三种枚举为独立的 QML 类型
        qmlRegisterUncreatableType<ShapeTypeQmlObj>(
            "EnumHandler",
            1,
            0,
            "ShapeType",
            "ShapeType enum - direct access only");

        qmlRegisterUncreatableType<FieldTypeQmlObj>(
            "EnumHandler",
            1,
            0,
            "FieldType",
            "FieldType enum - direct access only");

        qmlRegisterUncreatableType<ConstraintTypeQmlObj>(
            "EnumHandler",
            1,
            0,
            "ConstraintType",
            "ConstraintType enum - direct access only");
    }
} // egret