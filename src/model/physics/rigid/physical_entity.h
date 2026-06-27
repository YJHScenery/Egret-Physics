/**
 * @file        physical_entity.h
 * @brief       物理实体基类头文件，定义所有物理实体的公共接口。
 * @details     定义 PhysicalEntity 类，作为所有物理实体（刚体、粒子等）的基类，
 *              提供位置、速度、质量等通用属性和接口。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_RIGID_OBJECT_BASE_H
#define EGRET_PHYSICS_RIGID_OBJECT_BASE_H
#include <algorithm>
#include <cstdint>
#include <optional>

#include "Eigen/Dense"
#include <vector>
#include "basic_utils.h"
#include "shape/standard/shape_base.h"
#include <memory>
#include "physics_utils.h"
#include "transform.h"
#include "physics_abstract.h"

namespace egret
{
    /**
     * @brief       物理实体基类，定义所有物理实体的公共接口。
     * @details     PhysicalEntity 是所有物理实体（刚体、粒子等）的抽象基类，
     *              提供位置、速度、质量、受力、力矩等通用属性和接口。
     *              采用组合模式，通过 Transform 管理空间变换，通过 ShapeBase 管理碰撞形状。
     *              派生类通过覆写 hasRotation() 声明是否具有转动自由度。
     *
     * @invariant   m_mass >= 0，质量为0表示静态体（不参与积分）
     * @invariant   m_restitution 在 [0, 1] 范围内，表示碰撞恢复系数
     * @invariant   m_forces 和 m_torques 列表中的元素 ID 不重复
     * @remark      该类不是线程安全的，多线程访问需要外部同步
     * @see         Particle, RigidBody, Transform, ShapeBase, Force, Torque
     */
    class PhysicalEntity: virtual public PhysicsAbstract
    {
    public:
        PhysicalEntity() = default;
        explicit PhysicalEntity(double mass, std::uint64_t id);
        PhysicalEntity(const Eigen::Vector3d& position, Eigen::Vector3d speed, double mass, std::uint64_t id);
        PhysicalEntity(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, std::vector<Force> forces, std::uint64_t id);


        PhysicalEntity(PhysicalEntity &&) = default;
        PhysicalEntity &operator=(PhysicalEntity &&) = default;

        ~PhysicalEntity() override = default;

        [[nodiscard]] Eigen::Vector3d getPosition() const { return m_transform.getTranslation(); }

        [[nodiscard]] const Eigen::Vector3d &getPositionCR() const { return m_transform.getTranslation(); }

        [[nodiscard]] Transform &getTransform() { return m_transform; }

        [[nodiscard]] const Transform &getTransformCR() const { return m_transform; }

        [[nodiscard]] std::vector<Force> getForces() const { return m_forces; }

        [[nodiscard]] const std::vector<Force> &getForcesCR() const { return m_forces; }

        [[nodiscard]] std::vector<Torque> getTorques() const { return m_torques; }

        [[nodiscard]] const std::vector<Torque> &getTorquesCR() const { return m_torques; }

        [[nodiscard]] Eigen::Vector3d getVelocity() const { return m_velocity; }

        [[nodiscard]] const Eigen::Vector3d &getSpeedCR() const { return m_velocity; }

        [[nodiscard]] Eigen::Vector3d getAngularVelocity() const { return m_angularVelocity; }

        [[nodiscard]] const Eigen::Vector3d &getAngularCR() const { return m_angularVelocity; }

        [[nodiscard]] double getMass() const { return m_mass; }

        [[nodiscard]] virtual bool isFixed() const { return m_mass <= 0; }

        [[nodiscard]] double getRestitution() const { return m_restitution; }

        void setPosition(const Eigen::Vector3d &position) { m_transform.setTranslation(position); }

        void setRotation(const Eigen::Quaterniond &rotation) { m_transform.setRotation(rotation); }

        void setRotation(const Eigen::Matrix3d &rotation) { m_transform.setRotation(Eigen::Quaterniond(rotation)); }

        void setScale(const Eigen::Vector3d &scale) { m_transform.setScale(scale); }

        void setMass(const double mass) { m_mass = mass; }

        void addForce(const Force &force) { m_forces.push_back(force); }

        void upsertForce(const Force &force);

        void setRestitution(double restitution);

        bool removeForceById(std::uint64_t id);

        [[nodiscard]] bool hasForce(std::uint64_t id) const;

        void removeForce(long long index);

        void addTorque(const Torque &torque) { m_torques.push_back(torque); }

        void upsertTorque(const Torque &torque);

        bool removeTorqueById(std::uint64_t id);

        [[nodiscard]] bool hasTorque(std::uint64_t id) const;

        void removeTorque(long long index);

        /**
         * @brief 清空所有累积的力矩。
         * @details 与力清理对称，通常在积分消费力矩后由求解器调用。
         */
        void clearTorques() { m_torques.clear(); }

        void setSpeed(const Eigen::Vector3d &speed) { m_velocity = speed; }

        void setAngular(const Eigen::Vector3d &angular) { m_angularVelocity = angular; }

        void setShape(const std::shared_ptr<ShapeBase> &shape) { m_shape = shape; }

        [[nodiscard]] std::shared_ptr<ShapeBase> getShape() { return m_shape; }

        [[nodiscard]] std::shared_ptr<ShapeBase> getShape() const { return m_shape; }

        /**
         * @brief 查询实体是否具有旋转自由度。
         * @details 默认返回 false，表示实体仅具有平动自由度（如 Particle）。
         *          RigidBody 覆写返回 true，表示具有完整的转动自由度。
         *          积分策略据此决定是否执行角速度与姿态的积分。
         * @return true 表示实体支持转动积分，false 表示仅平动。
         */
        [[nodiscard]] virtual bool hasRotation() const;

        /**
         * @brief 获取线动量。
         * @details 默认实现：m_mass * m_speed。
         */
        virtual Eigen::Vector3d getMomentum();

        // 角动量
        virtual Eigen::Vector3d getAngularMomentum() = 0;

        virtual double getKineticEnergy() = 0;

        /**
         * @brief 获取绕指定轴的转动惯量（平行轴定理）。
         * @details 默认实现：distance² × mass，将实体视作点质量。
         */
        virtual double getRotationalInertia(const Axis &axis);

        // 力矩
        virtual Eigen::Vector3d getTorque() = 0;

        /**
         * @brief 获取所有累积力的合力。
         * @details 默认实现：遍历 m_forces 累加各力向量。
         */
        virtual Eigen::Vector3d getJoinForce();

    protected:
        PhysicalEntity(const PhysicalEntity &) = default;

        PhysicalEntity &operator=(const PhysicalEntity &) = default;

        Transform m_transform{}; // 世界变换（包含位置、旋转、缩放）

        Eigen::Vector3d m_velocity{Eigen::Vector3d::Zero()}; // 参考点的速度

        Eigen::Vector3d m_angularVelocity{Eigen::Vector3d::Zero()}; // 参考点的角速度

        std::vector<Force> m_forces; // 受力组/N

        std::vector<Torque> m_torques; // 力矩组/Nm

        std::shared_ptr<ShapeBase> m_shape;

        double m_mass{}; // 质量/kg

        double m_restitution{1.0}; // 碰撞恢复系数

        std::uint64_t m_id{};
    };
}

#endif // EGRET_PHYSICS_RIGID_OBJECT_BASE_H
