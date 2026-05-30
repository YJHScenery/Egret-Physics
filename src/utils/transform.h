/**
 * @file        transform.h
 * @brief       刚体变换数据类文件，提供平移、旋转、缩放及矩阵缓存功能。
 * @details     定义 Transform 类，管理刚体的位置、旋转和缩放变换，
 *              并自动维护局部坐标到世界坐标的变换矩阵缓存。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_TRANSFORM_H
#define EGRET_PHYSICS_TRANSFORM_H
#include <Eigen/Dense>
#include <utility>

namespace egret
{
    /**
     * @brief       刚体变换数据类，管理刚体的位置、旋转和缩放变换。
     * @details     Transform 是刚体变换数据类，管理刚体的位置、旋转和缩放变换，
     *              并自动维护局部坐标到世界坐标的变换矩阵缓存。
     *              设计要点：
     *              1. 对外只暴露受控接口，不直接开放内部数据成员，保证状态一致性。
     *              2. 任意 TRS 分量修改后立即自动重建矩阵缓存，调用方无需手动刷新。
     *              3. 同时维护 localToWorld 与 worldToLocal 两个矩阵，减少重复逆矩阵计算。
     *              注意：
     *              - setRotation / setTRS 会对四元数做 normalized()，避免非单位四元数导致畸变。
     *              - worldToLocal / localToWorld 两个便捷函数按"仅旋转 + 平移"处理，
     *                与旧版辅助函数语义一致，不考虑缩放。
     *              - transformPointToWorld / transformPointToLocal 走完整 4x4 矩阵流程，会考虑缩放。
     * 
     * @invariant   m_rotation 是单位四元数（归一化）
     * @invariant   m_scale 各分量 > 0（缩放为正值）
     * @invariant   m_localToWorld 和 m_worldToLocal 矩阵始终同步更新
     * @remark      Transform 类不是线程安全的，多线程访问需要外部同步
     * @see         PhysicalEntity, ShapeBase, Eigen::Vector3d, Eigen::Quaterniond
     */
    class Transform
    {
    public:
        /** @brief 构造单位变换：零平移、单位旋转、单位缩放。 */
        Transform()
            : m_translation(Eigen::Vector3d::Zero()),
              m_rotation(Eigen::Quaterniond::Identity()),
              m_scale(Eigen::Vector3d::Ones())
        {
            updateMatrix();
        }

        /**
         * @brief 使用给定 TRS 构造变换。
         * @param translation 世界空间平移。
         * @param rotation 世界空间旋转（自动归一化）。
         * @param scale 各轴缩放。
         */
        explicit Transform(Eigen::Vector3d translation,
                           const Eigen::Quaterniond &rotation = Eigen::Quaterniond::Identity(),
                           Eigen::Vector3d scale = Eigen::Vector3d::Ones())
            : m_translation(std::move(translation)),
              m_rotation(rotation.normalized()),
              m_scale(std::move(scale))
        {
            updateMatrix();
        }

        /** @brief 获取平移分量。 */
        [[nodiscard]] const Eigen::Vector3d &getTranslation() const { return m_translation; }

        /** @brief 获取旋转分量（单位四元数）。 */
        [[nodiscard]] const Eigen::Quaterniond &getRotation() const { return m_rotation; }

        [[nodiscard]] Eigen::Matrix3d getRotationMatrix() const;

        /** @brief 获取缩放分量。 */
        [[nodiscard]] const Eigen::Vector3d &getScale() const { return m_scale; }

        /** @brief 获取局部坐标到世界坐标矩阵。 */
        [[nodiscard]] const Eigen::Matrix4d &getLocalToWorldMatrix() const { return m_localToWorld; }

        /** @brief 获取世界坐标到局部坐标矩阵。 */
        [[nodiscard]] const Eigen::Matrix4d &getWorldToLocalMatrix() const { return m_worldToLocal; }

        /**
         * @brief 设置平移并自动刷新矩阵缓存。
         */
        void setTranslation(const Eigen::Vector3d &translation);

        /**
         * @brief 设置旋转并自动刷新矩阵缓存。
         * @note 传入四元数会在内部归一化。
         */
        void setRotation(const Eigen::Quaterniond &rotation);

        /**
         * @brief 设置缩放并自动刷新矩阵缓存。
         */
        void setScale(const Eigen::Vector3d &scale);

        /**
         * @brief 一次性设置平移、旋转、缩放并自动刷新矩阵缓存。
         */
        void setTRS(const Eigen::Vector3d &translation,
                    const Eigen::Quaterniond &rotation,
                    const Eigen::Vector3d &scale);

        /**
         * @brief 便捷函数：将局部点变换到世界坐标（仅旋转 + 平移，不考虑缩放）。
         */
        [[nodiscard]] Eigen::Vector3d localToWorld(const Eigen::Vector3d &localPoint) const;

        /**
         * @brief 便捷函数：将世界点变换到局部坐标（仅旋转 + 平移，不考虑缩放）。
         */
        [[nodiscard]] Eigen::Vector3d worldToLocal(const Eigen::Vector3d &point) const;

        /**
         * @brief 使用完整 4x4 矩阵将局部点变换到世界坐标（考虑缩放）。
         */
        [[nodiscard]] Eigen::Vector3d transformPointToWorld(const Eigen::Vector3d &localPoint) const;

        /**
         * @brief 使用完整 4x4 矩阵将世界点变换到局部坐标（考虑缩放）。
         */
        [[nodiscard]] Eigen::Vector3d transformPointToLocal(const Eigen::Vector3d &worldPoint) const;

    private:
        /** @brief 重建 localToWorld 与 worldToLocal 两个矩阵缓存。 */
        void updateMatrix();

        // TRS 原始分量
        Eigen::Vector3d m_translation;
        Eigen::Quaterniond m_rotation;
        Eigen::Vector3d m_scale;
        // 矩阵缓存
        Eigen::Matrix4d m_localToWorld{};
        Eigen::Matrix4d m_worldToLocal{};
    };
}
#endif // EGRET_PHYSICS_TRANSFORM_H
