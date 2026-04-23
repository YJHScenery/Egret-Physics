//
// Created by jehor on 2026/4/23.
//

#ifndef EGRET_PHYSICS_PARTICLE_H
#define EGRET_PHYSICS_PARTICLE_H
#include "physical_entity.h"

namespace egret {

class Particle: public PhysicalEntity {
public:
    void applyForce(double time) override;

    void movePosition(double time) override;

    void rotateMandatory(const Axis& axis, double radians) override;

    double getRotationalInertia(const Axis& axis) override;

    Eigen::Vector3f getTorque(const Eigen::Vector3f& base) override;

    Force getJoinForce() override;

    Eigen::Vector3f getMomentum() override;

    Eigen::Vector3f getAngularMomentum(const Eigen::Vector3f& base) override;
};

} // egret

#endif //EGRET_PHYSICS_PARTICLE_H
