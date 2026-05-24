//
// Created by jehor on 2026/4/23.
//

#include "rigid_body.h"

#include <cmath>

namespace egret
{
	void RigidBody::applyForce(const double time)
	{
		if (m_mass <= 0.0)
		{
			movePosition(time);
			return;
		}

		const Eigen::Vector3d acceleration{getJoinForce().force / m_mass};
		Eigen::Vector3d newPosition = m_transform.getTranslation() + m_speed * time + 0.5 * acceleration * time * time;
		m_transform.setTranslation(newPosition);
		m_speed += acceleration * time;
	}

	void RigidBody::movePosition(const double time)
	{
		m_transform.setTranslation(m_transform.getTranslation() + m_speed * time);
	}

	void RigidBody::applyTorque(const double time, Eigen::Matrix4d rotation)
	{
		if (m_mass <= 0.0 || !m_shape)
		{
			return;
		}

		Eigen::Vector3d torque = getTorque();
		if (torque.norm() < 1e-10)
		{
			return;
		}

		Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
		Eigen::Matrix3d R = m_transform.getRotationMatrix();
		Eigen::Matrix3d I_world = R * I_local * R.transpose();

		Eigen::Vector3d angularAcceleration = I_world.inverse() * torque;

		m_angular += angularAcceleration * time;
	}

	void RigidBody::rotate(const double time, Eigen::Matrix4d rotation)
	{
		if (m_angular.norm() < 1e-10)
		{
			return;
		}

		Eigen::Quaterniond currentRot = m_transform.getRotation();
		Eigen::Quaterniond omegaQuat(0, m_angular.x(), m_angular.y(), m_angular.z());

		// 四元数微分：d(q)/dt = 0.5 * ω_q * q
		Eigen::Quaterniond deltaQ = omegaQuat * currentRot;
		deltaQ.coeffs() *= 0.5;
		currentRot.coeffs() += deltaQ.coeffs() * time;
		currentRot.normalize();

		m_transform.setRotation(currentRot);
	}

	double RigidBody::getRotationalInertia(const Axis &axis)
	{
		const Eigen::Vector3d dir = axis.rotationAxis.normalized();
		const Eigen::Vector3d rel = m_transform.getTranslation() - axis.basePoint;
		const double distance = rel.cross(dir).norm();
		return distance * distance * m_mass;
	}

	Eigen::Vector3d RigidBody::getTorque()
	{
		// 力矩来源：直接存储的力矩 m_torques（力作用于零点）
		Eigen::Vector3d totalTorque = Eigen::Vector3d::Zero();

		for (const Torque &torque : m_torques)
		{
			totalTorque += torque.torque;
		}

		return totalTorque;
	}

	Force RigidBody::getJoinForce()
	{
		Force joinForce{};
		for (const auto &[force, _2] : m_forces)
		{
			joinForce.force += force;
		}
		return joinForce;
	}

	Eigen::Vector3d RigidBody::getMomentum()
	{
		return m_mass * m_speed;
	}

	Eigen::Vector3d RigidBody::getAngularMomentum()
	{
		if (!m_shape)
		{
			return Eigen::Vector3d::Zero();
		}
		Eigen::Matrix3d I_local = m_shape->getInertiaTensor(m_mass);
		Eigen::Matrix3d R = m_transform.getRotationMatrix();
		Eigen::Matrix3d I_world = R * I_local * R.transpose();
		return I_world * m_angular;
	}
} // egret