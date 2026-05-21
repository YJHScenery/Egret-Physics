//
// Created by jehor on 2026/4/23.
//

#ifndef EGRET_PHYSICS_PARTICLE_H
#define EGRET_PHYSICS_PARTICLE_H
#include "physical_entity.h"

namespace egret
{

    class Particle : public PhysicalEntity
    {
    public:
        Particle() = default;

        Particle(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass);

        void applyForce(double time) override;

        void movePosition(double time) override;

        void applyTorque(double time, Eigen::Matrix4d rotation) override;

        void rotate(double time, Eigen::Matrix4d rotation) override;

        double getRotationalInertia(const Axis &axis) override;

        Eigen::Vector3d getTorque() override;

        Force getJoinForce() override;

        Eigen::Vector3d getMomentum() override;

        Eigen::Vector3d getAngularMomentum() override;
        double m_mass{};
    };

} // egret

#endif // EGRET_PHYSICS_PARTICLE_H
