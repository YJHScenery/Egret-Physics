//
// Created by jehor on 2026/4/23.
//

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

        [[nodiscard]] const Eigen::Vector3d& getPositionCR() const { return m_transform.getTranslation(); }

        [[nodiscard]] Transform& getTransform() { return m_transform; }

        [[nodiscard]] const Transform& getTransformCR() const { return m_transform; }

        [[nodiscard]] std::vector<Force> getForces() const { return m_forces; }

        [[nodiscard]] const std::vector<Force> &getForcesCR() const { return m_forces; }

        [[nodiscard]] Eigen::Vector3d getSpeed() const { return m_speed; }

        [[nodiscard]] const Eigen::Vector3d &getSpeedCR() const { return m_speed; }

        [[nodiscard]] Eigen::Vector3d getAngular() const { return m_angular; }

        [[nodiscard]] const Eigen::Vector3d &getAngularCR() const { return m_angular; }

        [[nodiscard]] double getMass() const { return m_mass; }

        [[nodiscard]] bool isFixed() const { return m_mass <= 0; }

        [[nodiscard]] double getRestitution() const { return m_restitution; }

        void setPosition(const Eigen::Vector3d &position) { m_transform.setTranslation(position); }

        void setMass(const double mass) { m_mass = mass; }

        void addForce(const Force &force) { m_forces.push_back(force); }

        void upsertForce(const Force &force);

        void setRestitution(double restitution);

        bool removeForceById(std::uint64_t id);

        [[nodiscard]] bool hasForce(std::uint64_t id) const;

        void removeForce(long long index);

        void setSpeed(const Eigen::Vector3d &speed) { m_speed = speed; }

        void setAngular(const Eigen::Vector3d &angular) { m_angular = angular; }

        void setShape(const std::shared_ptr<ShapeBase> &shape) { m_shape = shape; }

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
        Transform m_transform{};     // 世界变换（包含位置、旋转、缩放）
        Eigen::Vector3d m_speed{};   // 参考点的速度
        Eigen::Vector3d m_angular{}; // 参考点的角速度

        std::vector<Force> m_forces; // 受力组/N

        std::vector<Torque> m_torques; // 力矩组/Nm

        std::shared_ptr<ShapeBase> m_shape;
        double m_mass{};           // 质量/kg
        double m_restitution{1.0}; // 碰撞恢复系数
    };
}

#endif // EGRET_PHYSICS_RIGID_OBJECT_BASE_H
