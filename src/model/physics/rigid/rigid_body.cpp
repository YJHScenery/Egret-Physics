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
		// 刚体转动：α = τ / I，更新角速度
		Eigen::Vector3d torque = getTorque();
		if (torque.norm() < 1e-10)
		{
			return;
		}
		// 简化处理：使用总转动惯量的模来计算角加速度方向
		Axis defaultAxis{{0, 0, 0}, {0, 0, 1}};
		double I = getRotationalInertia(defaultAxis);
		if (I > 1e-10)
		{
			Eigen::Vector3d angularAcceleration = torque / I;
			m_angular += angularAcceleration * time;
		}
	}

	void RigidBody::rotate(const double time, Eigen::Matrix4d rotation)
	{
		// 刚体匀速转动：根据角速度更新姿态
		// 使用旋转矩阵更新位置（绕参考点的旋转）
		if (m_angular.norm() < 1e-10)
		{
			return;
		}
		Eigen::Vector3d angularDisplacement = m_angular * time;
		Eigen::AngleAxisd rx(angularDisplacement[0], Eigen::Vector3d::UnitX());
		Eigen::AngleAxisd ry(angularDisplacement[1], Eigen::Vector3d::UnitY());
		Eigen::AngleAxisd rz(angularDisplacement[2], Eigen::Vector3d::UnitZ());
		Eigen::Matrix3d rotationMatrix = (rz * ry * rx).toRotationMatrix();
		m_transform.setTranslation(rotationMatrix * m_transform.getTranslation());
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
		// 角动量 L = r × p = position × (mass * velocity)
		return m_transform.getTranslation().cross(m_mass * m_speed);
	}
} // egret