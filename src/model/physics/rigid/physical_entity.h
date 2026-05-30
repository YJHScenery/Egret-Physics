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

namespace egret
{
    /**
     * @brief       物理实体基类，定义所有物理实体的公共接口。
     * @details     PhysicalEntity 是所有物理实体（刚体、粒子等）的抽象基类，
     *              提供位置、速度、质量、受力、力矩等通用属性和接口。
     *              采用组合模式，通过 Transform 管理空间变换，通过 ShapeBase 管理碰撞形状。
     *              支持多态继承，派生类需要实现 applyForce、movePosition 等虚函数。
     *
     * @invariant   m_mass >= 0，质量为0表示静态体（不参与积分）
     * @invariant   m_restitution 在 [0, 1] 范围内，表示碰撞恢复系数
     * @invariant   m_forces 和 m_torques 列表中的元素 ID 不重复
     * @remark      该类不是线程安全的，多线程访问需要外部同步
     * @see         Particle, RigidBody, Transform, ShapeBase, Force, Torque
     */
    class PhysicalEntity
    {
    public:
        PhysicalEntity() = default;
        explicit PhysicalEntity(double mass);
        PhysicalEntity(Eigen::Vector3d position, Eigen::Vector3d speed, double mass);
        PhysicalEntity(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, std::vector<Force> forces);

        PhysicalEntity(const PhysicalEntity &) = default;
        PhysicalEntity(PhysicalEntity &&) = default;
        PhysicalEntity &operator=(const PhysicalEntity &) = default;
        PhysicalEntity &operator=(PhysicalEntity &&) = default;

        virtual ~PhysicalEntity() = default;

        [[nodiscard]] Eigen::Vector3d getPosition() const { return m_transform.getTranslation(); }

        [[nodiscard]] const Eigen::Vector3d &getPositionCR() const { return m_transform.getTranslation(); }

        [[nodiscard]] Transform &getTransform() { return m_transform; }

        [[nodiscard]] const Transform &getTransformCR() const { return m_transform; }

        [[nodiscard]] std::vector<Force> getForces() const { return m_forces; }

        [[nodiscard]] const std::vector<Force> &getForcesCR() const { return m_forces; }

        [[nodiscard]] std::vector<Torque> getTorques() const { return m_torques; }

        [[nodiscard]] const std::vector<Torque> &getTorquesCR() const { return m_torques; }

        [[nodiscard]] Eigen::Vector3d getVelocity() const { return m_speed; }

        [[nodiscard]] const Eigen::Vector3d &getSpeedCR() const { return m_speed; }

        [[nodiscard]] Eigen::Vector3d getAngularVelocity() const { return m_angular; }

        [[nodiscard]] const Eigen::Vector3d &getAngularCR() const { return m_angular; }

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

        void setSpeed(const Eigen::Vector3d &speed) { m_speed = speed; }

        void setAngular(const Eigen::Vector3d &angular) { m_angular = angular; }

        void setShape(const std::shared_ptr<ShapeBase> &shape) { m_shape = shape; }

        std::shared_ptr<ShapeBase> getShape() { return m_shape; }

        std::shared_ptr<ShapeBase> getShape() const { return m_shape; }

        // 匀加速直线运动
        virtual void applyForce(double time) = 0; // 应用所有的力，更新位置信息

        // 匀速直线运动
        virtual void movePosition(double time) = 0; // 根据当前速度移动相应时间

        // 转动（需要提供初始姿态，这里需要传入 4x4 的旋转矩阵。）
        virtual void applyTorque(double time, Eigen::Matrix4d rotation) = 0;

        // 匀速转动
        virtual void rotate(double time, Eigen::Matrix4d rotation) = 0;

        // 动量
        virtual Eigen::Vector3d getMomentum() = 0;

        // 角动量
        virtual Eigen::Vector3d getAngularMomentum() = 0;

        // 转动惯量
        virtual double getRotationalInertia(const Axis &axis) = 0;

        // 力矩
        virtual Eigen::Vector3d getTorque() = 0;

        // 获取合力
        virtual Force getJoinForce() = 0;

    protected:
        Transform m_transform{}; // 世界变换（包含位置、旋转、缩放）

        Eigen::Vector3d m_speed{}; // 参考点的速度

        Eigen::Vector3d m_angular{}; // 参考点的角速度

        std::vector<Force> m_forces; // 受力组/N

        std::vector<Torque> m_torques; // 力矩组/Nm

        std::shared_ptr<ShapeBase> m_shape;

        double m_mass{}; // 质量/kg

        double m_restitution{1.0}; // 碰撞恢复系数
    };
}

#endif // EGRET_PHYSICS_RIGID_OBJECT_BASE_H
