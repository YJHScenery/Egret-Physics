//
// Created by jehor on 2026/5/29.
//

#ifndef EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H
#define EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H

#include <QQuaternion>
#include <QVariantMap>
#include <QVector3D>
#include <cstdint>

namespace egret
{
    class ModelItemData;

    class ModelToRenderHelper
    {
    public:
        static ModelToRenderHelper &instance();

        [[nodiscard]] static QVariantMap buildQuick3DRenderTransform(const ModelItemData &model);

        [[nodiscard]] static QVector3D buildQuick3DRenderScaleFromWorldSize(std::uint32_t shapeId,
                                                                            const QVector3D &size);
        [[nodiscard]] static QVector3D buildQuick3DRenderScale(std::uint32_t shapeId,
                                                               const QVector3D &scale,
                                                               const QVector3D &boxSize,
                                                               double radius,
                                                               double height,
                                                               double length);

        [[nodiscard]] static QVector3D buildQuick3DRenderPosition(const QVector3D& position);

        [[nodiscard]] static QQuaternion buildQuick3DRenderRotation(const QQuaternion& rotation);
    private:
        ModelToRenderHelper() = default;

    };
} // egret

#endif //EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H
