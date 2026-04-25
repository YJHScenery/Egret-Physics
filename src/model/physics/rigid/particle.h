//
// Created by jehor on 2026/4/23.
//

#ifndef EGRET_PHYSICS_PARTICLE_H
#define EGRET_PHYSICS_PARTICLE_H
#include "physical_entity.h"

namespace egret {

class Particle: public PhysicalEntity {
public:
    Particle(Eigen::Vector3d position, Eigen::Vector3d speed, double mass);
    void applyForce(double time) override;

    void movePosition(double time) override;

    void rotateMandatory(const Axis& axis, double radians) override;

    double getRotationalInertia(const Axis& axis) override;

    Eigen::Vector3d getTorque(const Eigen::Vector3d& base) override;

    Force getJoinForce() override;

    Eigen::Vector3d getMomentum() override;

    Eigen::Vector3d getAngularMomentum(const Eigen::Vector3d& base) override;
};

} // egret

#endif //EGRET_PHYSICS_PARTICLE_H
