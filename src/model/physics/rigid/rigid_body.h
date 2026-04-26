//
// Created by jehor on 2026/4/23.
//

#ifndef EGRET_PHYSICS_RIGID_BODY_H
#define EGRET_PHYSICS_RIGID_BODY_H
#include "physical_entity.h"
#include "physics_utils.h"

namespace egret {

class RigidBody: public PhysicalEntity {
public:
	using PhysicalEntity::PhysicalEntity;

	void applyForce(double time) override;

	void movePosition(double time) override;

	void rotateMandatory(const Axis& axis, double radians) override;

	double getRotationalInertia(const Axis& axis) override;

	Eigen::Vector3d getTorque(const Eigen::Vector3d& base) override;

	Force getJoinForce() override;

	Eigen::Vector3d getMomentum() override;

	Eigen::Vector3d getAngularMomentum(const Eigen::Vector3d& base) override;

private:
};

} // egret

#endif //EGRET_PHYSICS_RIGID_BODY_H
