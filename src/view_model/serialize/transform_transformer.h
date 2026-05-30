//
// Created by jehor on 2026/5/29.
//

#ifndef EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H
#define EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H

#include <QQuaternion>
#include <QVariantMap>
#include <QVector3D>
#include <cstdint>

#include <Eigen/Dense>

namespace egret
{
    class ModelItemData;

    class TransformTransformer
    {
    public:
        static TransformTransformer &instance();

        [[nodiscard]] static QVariantMap buildQuick3DRenderTransform(const ModelItemData &model);

        [[nodiscard]] static QVector3D buildQuick3DRenderScale(std::uint32_t shapeId,
                                                               const QVector3D &scale,
                                                               const QVector3D &boxSize,
                                                               double radius,
                                                               double height,
                                                               double length);

        [[nodiscard]] static QVector3D buildQuick3DRenderPosition(const QVector3D& position);

        [[nodiscard]] static QQuaternion buildQuick3DRenderRotation(const QQuaternion& rotation);

        [[nodiscard]] static Eigen::Vector3d buildQuick3DRenderScale(std::uint32_t shapeId,
                                                               const Eigen::Vector3d &scale,
                                                               const Eigen::Vector3d &boxSize,
                                                               double radius,
                                                               double height,
                                                               double length);

        [[nodiscard]] static Eigen::Vector3d buildQuick3DRenderPosition(const Eigen::Vector3d& position);

        [[nodiscard]] static Eigen::Quaterniond buildQuick3DRenderRotation(const Eigen::Quaterniond& rotation);



    private:
        TransformTransformer() = default;

    };
} // egret

#endif //EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H
