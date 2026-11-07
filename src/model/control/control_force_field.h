/** @file control_force_field.h @brief 将控制动作保存为可被积分器重复求值的持久力源。 */
#pragma once
#include "field_base.h"
#include "physical_entity.h"
#include <unordered_map>

namespace egret::control
{
    /** @brief 一个稳定场记录承载多个实体控制力，运行时不改变场表长度。 */
    class ControlForceField final : public FieldBase
    {
    public:
        /** @brief 使用独立场标识构造零控制力源。 */
        explicit ControlForceField(std::uint64_t id):PhysicsAbstract(id),FieldBase(id) {}
        /** @brief 替换指定实体的控制力，零向量表示取消。 */
        void set(std::uint64_t id,const Eigen::Vector3d& force) {m_forces[id]=force;}
        /** @brief 替换持久控制力矩，与控制力共享稳定场身份。 */
        void setTorque(std::uint64_t id,const Eigen::Vector3d& torque){m_torques[id]=torque;}
        /** @brief 清除全部控制力。 */
        void clear() {m_forces.clear();m_torques.clear();}
        /** @brief 永久删除后清除对应力槽。 */
        void erase(std::uint64_t id){m_forces.erase(id);m_torques.erase(id);}
        /** @brief 多目标控制力没有统一空间场向量。 */
        Eigen::Vector3d sample(const Eigen::Vector3d&) override {return Eigen::Vector3d::Zero();}
        /** @brief 动态控制力不提供保守势能。 */
        double potential(const Eigen::Vector3d&) override {return 0;}
        /** @brief 仅向活动实体注入最近一次控制动作指定的力。 */
        void applyToEntity(PhysicalEntity* entity) override
        {
            if (!m_enabled || !entity || !entity->isActive()) return;
            const auto it=m_forces.find(entity->getId());
            if (it!=m_forces.end()) entity->upsertForce(Force{it->second,getId()});
            const auto torque=m_torques.find(entity->getId());if(torque!=m_torques.end())entity->upsertTorque(Torque{torque->second,getId()});
        }
        /** @brief 将控制源标记为扩展力场。 */
        FieldType getType() const override {return FieldType::Other;}
        /** @brief 克隆当前控制力配置并分配新场标识。 */
        std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override
        {
            auto result=std::make_unique<ControlForceField>(*this);result->setId(id);return result;
        }
    private:
        std::unordered_map<std::uint64_t,Eigen::Vector3d> m_forces;
        std::unordered_map<std::uint64_t,Eigen::Vector3d> m_torques;
    };
}
