/** @file vector_force_field.h @brief 支持任意作用域和时变向量函数的原生力场。 */
#pragma once
#include "field_base.h"
#include "physical_entity.h"
#include "vector_force_model.h"

namespace egret
{
    /** @brief 按世界坐标位置对任意活动实体施加以牛顿表示的力。 */
    class VectorForceField : public FieldBase
    {
    public:
        /** @brief 创建拥有唯一标识及采样模型的力场。 */
        VectorForceField(VectorForceModel model, std::uint64_t id)
            : PhysicsAbstract(id), FieldBase(id), m_model(std::move(model)) {}

        /** @brief 使用零时刻兼容旧接口。 */
        Eigen::Vector3d sample(const Eigen::Vector3d& position) override { return sampleAt(0.0, position); }
        /** @brief 返回指定时间和位置的力，停用时返回零。 */
        Eigen::Vector3d sampleAt(double time, const Eigen::Vector3d& position) override
        { return m_enabled ? m_model.sample(time, position) : Eigen::Vector3d::Zero().eval(); }
        /** @brief 非保守场没有通用势能，此接口返回零。 */
        double potential(const Eigen::Vector3d&) override { return 0.0; }
        /** @brief 使用零时刻兼容旧接口。 */
        void applyToEntity(PhysicalEntity* entity) override { applyToEntityAt(entity, 0.0); }
        /** @brief 按时间更新作用力，离开区域或停用时移除旧力。 */
        void applyToEntityAt(PhysicalEntity* entity, double time) override
        {
            if (!entity) return;
            if (!m_enabled || !entity->isActive()) { entity->removeForceById(getId()); return; }
            const Eigen::Vector3d force = sampleAt(time, entity->getPosition());
            if (force.isZero(0.0)) entity->removeForceById(getId());
            else entity->upsertForce(Force{force, getId()});
        }
        /** @brief 声明该场需要在连续碰撞子步更新。 */
        [[nodiscard]] bool isTimeDependent() const override { return true; }
        /** @brief 返回通用场类型。 */
        [[nodiscard]] FieldType getType() const override { return FieldType::VectorForce; }
        /** @brief 复制场模型并赋予新的标识。 */
        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override
        {
            auto result = std::make_unique<VectorForceField>(*this); result->setId(id); return result;
        }
    private:
        VectorForceModel m_model;
    };
}

