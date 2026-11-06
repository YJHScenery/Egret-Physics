/** @file constant_force_field.h @brief 向指定实体持续施加世界坐标系恒力。 */
#pragma once
#include "field_base.h"
#include "physical_entity.h"

namespace egret
{
    /** @brief 在每次积分力重算时向指定实体注入恒力，不产生力矩。 */
    class ConstantForceField final : public FieldBase
    {
    public:
        /** @brief 指定力向量、目标实体和唯一场标识。 */
        ConstantForceField(Eigen::Vector3d force, std::uint64_t target, std::uint64_t id)
            : PhysicsAbstract(id), FieldBase(id), m_force(std::move(force)), m_target(target) {}
        /** @brief 返回世界坐标系的力向量，单位牛顿。 */
        Eigen::Vector3d sample(const Eigen::Vector3d&) override { return m_force; }
        /** @brief 返回恒力的势能，单位焦耳。 */
        double potential(const Eigen::Vector3d& position) override { return -m_force.dot(position); }
        /** @brief 仅对匹配且处于活动状态的目标施力。 */
        void applyToEntity(PhysicalEntity* entity) override
        {
            if (m_enabled && entity && entity->isActive() && entity->getId() == m_target)
                entity->upsertForce(Force{m_force, getId()});
        }
        /** @brief 返回自定义场类型。 */
        FieldType getType() const override { return FieldType::Other; }
        /** @brief 复制配置并分配新的场标识。 */
        std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override
        {
            auto result = std::make_unique<ConstantForceField>(*this);
            result->setId(id);
            return result;
        }
    private:
        Eigen::Vector3d m_force;
        std::uint64_t m_target;
    };
}
