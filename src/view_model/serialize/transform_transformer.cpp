//
// Created by jehor on 2026/5/29.
//

#include "transform_transformer.h"

#include <algorithm>
#include <cmath>

#include "model_item_data.h"
#include "shape_base.h"
#include <magic_enum.hpp>

#include "logger.h"

namespace egret
{
    TransformTransformer& TransformTransformer::instance()
    {
        static TransformTransformer helper;
        return helper;
    }

    QVariantMap TransformTransformer::buildQuick3DRenderTransform(const ModelItemData& model)
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


    QVector3D TransformTransformer::buildQuick3DRenderScale(const std::uint32_t shapeId,
                                                            const QVector3D& scale,
                                                            const QVector3D& boxSize,
                                                            const double radius,
                                                            const double height,
                                                            const double length)
    {
        const Eigen::Vector3d scaleEigen{scale.x(), scale.y(), scale.z()};
        const Eigen::Vector3d boxSizeEigen{boxSize.x(), boxSize.y(), boxSize.z()};
        const Eigen::Vector3d result{
            buildQuick3DRenderScale(shapeId, scaleEigen, boxSizeEigen, radius, height, length)
        };
        return {static_cast<float>(result.x()), static_cast<float>(result.y()), static_cast<float>(result.z())};
    }

    QVector3D TransformTransformer::buildQuick3DRenderPosition(const QVector3D& position)
    {
        const Eigen::Vector3d positionEigen{position.x(), position.y(), position.z()};
        const Eigen::Vector3d result{buildQuick3DRenderPosition(positionEigen)};
        return {static_cast<float>(result.x()), static_cast<float>(result.y()), static_cast<float>(result.z())};
    }

    QQuaternion TransformTransformer::buildQuick3DRenderRotation(const QQuaternion& rotation)
    {
        const Eigen::Quaterniond rotationEigen{rotation.scalar(), rotation.x(), rotation.y(), rotation.z()};
        const Eigen::Quaterniond result{buildQuick3DRenderRotation(rotationEigen)};
        return {
            static_cast<float>(result.w()), static_cast<float>(result.x()), static_cast<float>(result.y()),
            static_cast<float>(result.z())
        };
    }

    Eigen::Vector3d TransformTransformer::buildQuick3DRenderScale(const std::uint32_t shapeId,
                                                                  const Eigen::Vector3d& scale,
                                                                  const Eigen::Vector3d& boxSize, const double radius,
                                                                  const double height, const double length)
    {
        double unitScale = 0.01;
        const std::optional<ShapeType> shape{magic_enum::enum_cast<ShapeType>(shapeId)};
        if (!shape.has_value()) {
            LOG_WARN_LITERAL("Unknown Shape ID");
            return {};
        }


        switch (shape.value()) {
        case ShapeType::Box:{
            std::ignore = radius;
            std::ignore = height;
            std::ignore = length;

            const Eigen::Vector3d result {unitScale * Eigen::Vector3d{
                (scale.y() * boxSize.y()),
                (scale.z() * boxSize.z()),
                (scale.x() * boxSize.x())
            }};
            return result;
        }
        case ShapeType::Cylinder:{
            std::ignore = boxSize;
            std::ignore = length;
            return unitScale * Eigen::Vector3d{
                (scale.y() * height),
                (scale.z() * radius),
                (scale.x() * radius)
            };
        }
        case ShapeType::CylindricalShell:{
            std::ignore = boxSize;
            std::ignore = length;
            return Eigen::Vector3d{
                (scale.y() * radius),
                (scale.z() * height),
                (scale.x() * radius)
            };
        }
        case ShapeType::Disk:{
            std::ignore = boxSize;
            std::ignore = length;
            std::ignore = height;
            return unitScale * Eigen::Vector3d{
                (scale.y() * radius),
                0.01 * (scale.z() * radius),
                (scale.x() * radius)
            };
        }
        case ShapeType::Ring:{
            std::ignore = boxSize;
            std::ignore = length;
            std::ignore = height;
            return Eigen::Vector3d{
                (scale.y() * radius),
                (scale.z() * radius),
                (scale.x() * radius)
            };
        }
        case ShapeType::Rod:{
            std::ignore = boxSize;
            std::ignore = height;
            std::ignore = radius;
            return unitScale * Eigen::Vector3d{
                1.0f,
                (scale.y() * length),
                1.0f
            };
        }
        case ShapeType::Sphere:
            [[fallthrough]];
        case ShapeType::SphericalShell:{
            std::ignore = boxSize;
            std::ignore = length;
            std::ignore = height;
            return 2 * unitScale * Eigen::Vector3d{
                (scale.y() * radius),
                (scale.z() * radius),
                (scale.x() * radius)
            };
        }
        default:
            break;
        }
        return Eigen::Vector3d{1.0, 1.0, 1.0};
    }

    Eigen::Vector3d TransformTransformer::buildQuick3DRenderPosition(const Eigen::Vector3d& position)
    {
        return {position.y(), position.z(), position.x()};
    }

    Eigen::Quaterniond TransformTransformer::buildQuick3DRenderRotation(const Eigen::Quaterniond& rotation)
    {
        return {rotation.w(), rotation.y(), rotation.z(), rotation.x()};
    }
} // egret
