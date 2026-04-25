//
// Created by jehor on 2026/4/23.
//

#include "rigid_body.h"

#include <cmath>

namespace egret
{
	void RigidBody::applyForce(const double time)
	{
		if (m_mass <= 0.0) {
			movePosition(time);
			return;
		}

		const Eigen::Vector3d acceleration{getJoinForce().force / m_mass};
		m_position += m_speed * time + 0.5 * acceleration * time * time;
		m_speed += acceleration * time;
	}

	void RigidBody::movePosition(const double time)
	{
		m_position += m_speed * time;
	}

	void RigidBody::rotateMandatory(const Axis& axis, double radians)
	{
		Eigen::Vector3d u = axis.rotationAxis.normalized();
		const Eigen::Vector3d p_rel = m_position - axis.basePoint;

		const double cosTheta = std::cos(radians);
		const double sinTheta = std::sin(radians);

		Eigen::Matrix3d crossMatrix;
		crossMatrix << 0, -u(2), u(1),
			u(2), 0, -u(0),
			-u(1), u(0), 0;

		const Eigen::Matrix3d rotationMatrix = Eigen::Matrix3d::Identity()
			+ sinTheta * crossMatrix
			+ (1 - cosTheta) * (crossMatrix * crossMatrix);

		const Eigen::Vector3d pRotated = rotationMatrix * p_rel;
		m_position = pRotated + axis.basePoint;
	}

	double RigidBody::getRotationalInertia(const Axis& axis)
	{
		const Eigen::Vector3d dir = axis.rotationAxis.normalized();
		const Eigen::Vector3d rel = m_position - axis.basePoint;
		const double distance = rel.cross(dir).norm();
		return distance * distance * m_mass;
	}

	Eigen::Vector3d RigidBody::getTorque(const Eigen::Vector3d& base)
	{
		return (m_position - base).cross(getJoinForce().force);
	}

	Force RigidBody::getJoinForce()
	{
		Force joinForce{};
		for (const auto& [_1, force, _2] : m_forces) {
			joinForce.force += force;
		}
		return joinForce;
	}

	Eigen::Vector3d RigidBody::getMomentum()
	{
		return m_mass * m_speed;
	}

	Eigen::Vector3d RigidBody::getAngularMomentum(const Eigen::Vector3d& base)
	{
		return (m_position - base).cross(getMomentum());
	}
} // egret