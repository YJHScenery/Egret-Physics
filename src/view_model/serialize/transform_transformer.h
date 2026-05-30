/**
 * @file        transform_transformer.h
 * @brief       变换转换器头文件，提供模型数据到渲染层的变换转换功能。
 * @details     定义 TransformTransformer 类，负责将模型层的变换数据转换为渲染层可用的格式。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

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

    /**
     * @brief       变换转换器类，提供模型数据到渲染层的变换转换功能。
     * @details     TransformTransformer 是变换转换器类，负责将模型层的变换数据
     *              转换为渲染层可用的格式。
     *              采用单例模式，全局唯一实例。
     *              提供位置、旋转、缩放的转换函数。
     *              支持 Qt 类型（QVector3D、QQuaternion）和 Eigen 类型。
     *              用于 QML 渲染和物理引擎之间的数据转换。
     *
     * @invariant   instance() 返回全局唯一实例
     * @remark      TransformTransformer 是单例，全局唯一实例
     * @see         ModelItemData, Transform, QVector3D, QQuaternion
     */
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

        [[nodiscard]] static QVector3D buildQuick3DRenderPosition(const QVector3D &position);

        [[nodiscard]] static QQuaternion buildQuick3DRenderRotation(const QQuaternion &rotation);

        [[nodiscard]] static Eigen::Vector3d buildQuick3DRenderScale(std::uint32_t shapeId,
                                                                     const Eigen::Vector3d &scale,
                                                                     const Eigen::Vector3d &boxSize,
                                                                     double radius,
                                                                     double height,
                                                                     double length);

        [[nodiscard]] static Eigen::Vector3d buildQuick3DRenderPosition(const Eigen::Vector3d &position);

        [[nodiscard]] static Eigen::Quaterniond buildQuick3DRenderRotation(const Eigen::Quaterniond &rotation);

    private:
        TransformTransformer() = default;
    };
} // egret

#endif // EGRET_PHYSICS_MODEL_TO_RENDER_HELPER_H
