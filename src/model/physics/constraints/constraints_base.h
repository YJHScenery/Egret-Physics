//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_CONSTRAINTS_BASE_H
#define EGRET_PHYSICS_CONSTRAINTS_BASE_H
#include <cstdint>
#include <vector>


enum class ConstraintType : std::uint64_t
{
    Base = 0xFFFFFFAA00000000,
    ConnectingLine = 0xFFFFFFAA00000001,
    SimplePendulum = 0xFFFFFFAA00000002,
    ConnectingRod = 0xFFFFFFAA00000003,
    SlidingRail = 0xFFFFFFAA00000004,
    SupportSurface = 0xFFFFFFAA00000005,
};

namespace egret {

    // 我们约定使用一个数组存放约束器的施加对象列表。
    class PhysicalEntity;

    // 约束器抽象基类
    class ConstraintsBase {
    public:
        ConstraintsBase();

        explicit ConstraintsBase(std::uint64_t id);

        virtual ~ConstraintsBase() = default;

        // 返回约束类型
        [[nodiscard]] virtual ConstraintType getType() const;

        // 获取约束施加的所有实体
        [[nodiscard]] virtual std::vector<PhysicalEntity*> getConstrainedEntities() = 0;

        [[nodiscard]] virtual const std::vector<PhysicalEntity*>& getConstrainedEntities() const = 0;

        // 应用约束力/冲量到实体上（速度级别）
        virtual void applyVelocityConstraint(double dt) = 0;

        // 应用位置修正（位置级别，解决穿透等问题）
        virtual void applyPositionConstraint(double dt) = 0;

        // 计算约束满足程度（用于调试，返回误差值）
        [[nodiscard]] virtual double computeConstraintError() const = 0;

        // 设置约束启用/禁用
        void setEnabled(bool enabled);
        [[nodiscard]] bool isEnabled() const;

        // 获取约束ID
        [[nodiscard]] uint64_t getId() const;

    protected:
        bool m_enabled{true};

        std::uint64_t m_id;
        
        inline static std::uint32_t m_createCount_static{0};

        static std::uint64_t generateID(ConstraintType type);
    };

} // egret

#endif //EGRET_PHYSICS_CONSTRAINTS_BASE_H
