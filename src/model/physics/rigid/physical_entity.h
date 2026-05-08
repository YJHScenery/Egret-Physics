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

namespace egret
{
    class PhysicalEntity
    {
    public:
        PhysicalEntity() = default;
        explicit PhysicalEntity(double mass);
        PhysicalEntity(Eigen::Vector3d position, Eigen::Vector3d speed, double mass);
        PhysicalEntity(const Eigen::Vector3d& position, const Eigen::Vector3d& speed, double mass, std::vector<Force> forces);

        PhysicalEntity(const PhysicalEntity&) = default;
        PhysicalEntity(PhysicalEntity&&) = default;
        PhysicalEntity& operator=(const PhysicalEntity&) = default;
        PhysicalEntity& operator=(PhysicalEntity&&) = default;

        virtual ~PhysicalEntity() = default;

        [[nodiscard]] Eigen::Vector3d getPosition() const { return m_position; }

        [[nodiscard]] const Eigen::Vector3d& getPositionCR() const { return m_position; }

        [[nodiscard]] std::vector<Force> getForces() const { return m_forces; }

        [[nodiscard]] const std::vector<Force>& getForcesCR() const { return m_forces; }

        [[nodiscard]] Eigen::Vector3d getSpeed() const { return m_speed; }

        [[nodiscard]] const Eigen::Vector3d& getSpeedCR() const { return m_speed; }

        [[nodiscard]] double getMass() const { return m_mass; }

        [[nodiscard]] double getRestitution() const { return m_restitution; }

        void setPosition(const Eigen::Vector3d& position) { m_position = position; }

        void setMass(const double mass) { m_mass = mass; }

        void addForce(const Force& force) { m_forces.push_back(force); }

        void upsertForce(const Force& force);

        void setRestitution(double restitution);

        bool removeForceById(std::uint64_t id);

        [[nodiscard]] bool hasForce(std::uint64_t id) const;

        void removeForce(long long index);

        void setSpeed(const Eigen::Vector3d& speed) { m_speed = speed; }

        void setShape(const std::shared_ptr<ShapeBase>& shape) { m_shape = shape; }

        virtual void rotateMandatory(const Axis& axis, double radians) = 0;

        // 匀加速直线运动
        virtual void applyForce(double time) = 0; // 应用所有的力，更新位置信息

        // 匀速直线运动
        virtual void movePosition(double time) = 0; // 根据当前速度移动相应时间

        // 动量
        virtual Eigen::Vector3d getMomentum() = 0;

        // 角动量
        virtual Eigen::Vector3d getAngularMomentum(const Eigen::Vector3d& base) = 0;

        // 转动惯量
        virtual double getRotationalInertia(const Axis& axis) = 0;

        // 力矩
        virtual Eigen::Vector3d getTorque(const Eigen::Vector3d& base) = 0;

        // 获取合力
        virtual Force getJoinForce() = 0;

    protected:
        Eigen::Vector3d m_position; // 参考点位置
        Eigen::Vector3d m_speed; // 参考点的速度

        std::vector<Force> m_forces; // 受力组/N
        std::shared_ptr<ShapeBase> m_shape;
        double m_mass{}; // 质量/kg
        double m_restitution{1.0}; // 碰撞恢复系数
    };
}


#endif //EGRET_PHYSICS_RIGID_OBJECT_BASE_H
