/**
 * @file        rigid_body.h
 * @brief       刚体头文件，定义具有转动惯性的物理实体。
 * @details     定义 RigidBody 类，继承自 PhysicalEntity，具有质量、转动惯量等属性，
 *              用于模拟具有旋转特性的物理实体。
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

#ifndef EGRET_PHYSICS_RIGID_BODY_H
#define EGRET_PHYSICS_RIGID_BODY_H
#include "physical_entity.h"
#include "physics_utils.h"

namespace egret
{

	/**
	 * @brief       刚体类，表示具有转动惯性的物理实体。
	 * @details     RigidBody 继承自 PhysicalEntity，具有质量、转动惯量等属性，
	 *              用于模拟具有旋转特性的物理实体（如方块、圆柱、球体等）。
	 *              刚体可以响应力矩产生角加速度，支持完整的六自由度运动。
	 *              转动惯量由形状（ShapeBase）和质量计算得出。
	 *
	 * @invariant   m_mass >= 0，质量为0表示静态刚体
	 * @invariant   转动惯量矩阵由形状的 getInertiaTensor 计算得出
	 * @remark      刚体比粒子计算开销更高，需要处理角动量和转动惯量
	 * @see         PhysicalEntity, Particle, ShapeBase, Transform
	 */
	class RigidBody : public PhysicalEntity
	{
	public:
		RigidBody() = default;
		explicit RigidBody(double mass, std::uint64_t id);
		RigidBody(const Eigen::Vector3d& position, Eigen::Vector3d speed, double mass, std::uint64_t id);
		RigidBody(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, std::vector<Force> forces, std::uint64_t id);

		[[nodiscard]] bool hasRotation() const override;

		Eigen::Vector3d getTorque() override;

		Eigen::Vector3d getAngularMomentum() override;

		double getKineticEnergy() override;

		[[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

	private:
	};

} // egret

#endif // EGRET_PHYSICS_RIGID_BODY_H
