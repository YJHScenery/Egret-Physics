//
// Created by jehor on 2026/5/29.
//

#include "model_to_render_helper.h"

#include <algorithm>
#include <cmath>

#include "model_item_data.h"
#include "shape_base.h"

namespace egret
{
    ModelToRenderHelper& ModelToRenderHelper::instance()
    {
        static ModelToRenderHelper helper;
        return helper;
    }

    QVariantMap ModelToRenderHelper::buildQuick3DRenderTransform(const ModelItemData& model)
    {
        QVariantMap result;
        result.insert("position", QVector3D{});
        result.insert("scale", QVector3D{static_cast<float>(1.0), static_cast<float>(1.0), static_cast<float>(1.0)});
        result.insert("rotation", QQuaternion{});

        const QVector3D pos{model.pos()};
        const QVector3D scale{model.scale()};
        const QQuaternion rotation{model.rotation()};

        result["position"] = QVector3D{pos.y(), pos.z(), pos.x()};
        result["rotation"] = QQuaternion{rotation.scalar(), rotation.y(), rotation.z(), rotation.x()};

        const std::uint32_t shapeId = model.type();
        const QVector3D boxSize = model.boxSize();
        const double radius = model.radius();
        const double height = model.height();
        const double length = model.length();

        result["scale"] = buildQuick3DRenderScale(shapeId, scale, boxSize, radius, height, length);
        return result;
    }

    QVector3D ModelToRenderHelper::buildQuick3DRenderScaleFromWorldSize(const std::uint32_t shapeId,
                                                                        const QVector3D& size)
    {
        constexpr QVector3D scale{1.0f, 1.0f, 1.0f};
        const QVector3D boxSize{size};
        const double radius = 0.5 * std::max(size.x(), size.y());
        const double height = size.z();
        const double length = size.z();
        return buildQuick3DRenderScale(shapeId, scale, boxSize, radius, height, length);
    }

    QVector3D ModelToRenderHelper::buildQuick3DRenderScale(const std::uint32_t shapeId,
                                                           const QVector3D& scale,
                                                           const QVector3D& boxSize,
                                                           const double radius,
                                                           const double height,
                                                           const double length)
    {
        switch (shapeId) {
        case static_cast<std::uint32_t>(ShapeID::Box):
            return 0.01 * QVector3D{
                (scale.y() * boxSize.y()),
                (scale.z() * boxSize.z()),
                (scale.x() * boxSize.x())
            };
        case static_cast<std::uint32_t>(ShapeID::Cylinder):
            return 0.01 * QVector3D{
                static_cast<float>(scale.y() * height),
                static_cast<float>(scale.z() * radius),
                static_cast<float>(scale.x() * radius)
            };
        case static_cast<std::uint32_t>(ShapeID::CylindricalShell):
            return QVector3D{
                static_cast<float>(scale.y() * radius),
                static_cast<float>(scale.z() * height),
                static_cast<float>(scale.x() * radius)
            };
        case static_cast<std::uint32_t>(ShapeID::Disk): {
            double reduce = 0.01;
            if (radius > 100.0) {
                reduce = std::pow(10.0, -std::log10(radius));
            }
            return 0.01 * QVector3D{
                static_cast<float>(scale.y() * radius),
                static_cast<float>(reduce),
                static_cast<float>(scale.x() * radius)
            };
        }
        case static_cast<std::uint32_t>(ShapeID::Ring):
            return QVector3D{
                static_cast<float>(scale.y() * radius),
                static_cast<float>(1.0),
                static_cast<float>(scale.x() * radius)
            };
        case static_cast<std::uint32_t>(ShapeID::Rod): {
            double reduce = 0.01;
            if (length > 100.0) {
                reduce = std::pow(10.0, -std::log10(length));
            }
            return 0.01 * QVector3D{
                static_cast<float>(length * reduce),
                static_cast<float>(length),
                static_cast<float>(length * reduce)
            };
        }
        case static_cast<std::uint32_t>(ShapeID::Sphere):
        case static_cast<std::uint32_t>(ShapeID::SphericalShell):
            return 0.01 * QVector3D{
                static_cast<float>(scale.y() * radius),
                static_cast<float>(scale.z() * radius),
                static_cast<float>(scale.x() * radius)
            };
        default:
            break;
        }
        return QVector3D{static_cast<float>(1.0), static_cast<float>(1.0), static_cast<float>(1.0)};
    }

    QVector3D ModelToRenderHelper::buildQuick3DRenderPosition(const QVector3D& position)
    {
        return {position.y(), position.z(), position.x()};
    }

    QQuaternion ModelToRenderHelper::buildQuick3DRenderRotation(const QQuaternion& rotation)
    {
        return {rotation.scalar(), rotation.y(), rotation.z(), rotation.x()};
    }
} // egret
