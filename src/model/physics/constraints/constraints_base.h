/**
* @file        constraints_base.h
 * @brief       约束器基类头文件
 * @details     定义ConstraintsBase类，作为所有约束器的基类，
 *              提供速度约束、位置约束、角速度约束等接口。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_CONSTRAINTS_BASE_H
#define EGRET_PHYSICS_CONSTRAINTS_BASE_H
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "physics_abstract.h"
#include <magic_enum.hpp>

namespace egret
{
    enum class ConstraintType : std::uint64_t
    {
        Unknown = 0,

        ConnectingLine, SimplePendulum, ConnectingRod, SlidingRail,

        Spring, SupportSurface, AirResistance,

        Abstract = 127,

        Other = 255,
    };
}

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::ConstraintType>
    {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}


namespace egret
{
    class PhysicalEntity;

    /**
     * @brief       约束器抽象基类，定义所有约束器的公共接口。
     * @details     ConstraintsBase 是所有约束器（连接线、单摆、连接杆、滑轨等）的抽象基类，
     *              提供速度约束、位置约束、角速度约束等接口。
     *              约束器用于限制物理实体的运动自由度，如保持距离、限制角度等。
     *              采用策略模式，派生类实现具体的约束逻辑。
     *
     *              实体管理统一使用 m_registeredEntities 集合 + m_entityIdMap 映射，
     *              通过 registerEntity/unregisterEntity/isRegistered 接口操作。
     *              所有派生类（包括双实体约束和 SupportSurface）共用同一套注册机制。
     *
     * @invariant   m_id 是唯一的约束器标识符
     * @invariant   m_registeredEntities 中的实体指针有效
     * @invariant   m_enabled 表示约束器是否激活
     * @remark      约束器不是线程安全的，多线程访问需要外部同步
     * @see         ConnectingLine, SimplePendulum, ConnectingRod, SlidingRail, PhysicalEntity
     */
    class ConstraintsBase : virtual public PhysicsAbstract
    {
    public:
        explicit ConstraintsBase(std::uint64_t id);

        ~ConstraintsBase() override = default;

        ConstraintsBase(ConstraintsBase&& other)  noexcept = default;

        ConstraintsBase& operator=(ConstraintsBase&& other) = default;

        // 返回约束类型
        [[nodiscard]] virtual ConstraintType getType() const;

        // 获取约束施加的所有实体
        [[nodiscard]] virtual const std::unordered_set<PhysicalEntity*>& getConstrainedEntities() const;

        /**
         * @brief 注册一个受约束的实体。
         * @details 将实体添加到 m_registeredEntities 集合和 m_entityIdMap 映射中。
         *          如果实体已注册，则忽略（幂等）。
         * @param entity 要注册的物理实体指针。
         */
        virtual void registerEntity(PhysicalEntity* entity);

        /**
         * @brief 从约束器中移除实体。
         * @param entityId 要移除的实体 ID。
         * @return 如果实体之前已注册则返回 true。
         */
        virtual bool unregisterEntity(std::uint64_t entityId);

        /**
         * @brief 检查实体是否已注册。
         * @param entityId 实体 ID。
         * @return 如果已注册则返回 true。
         */
        [[nodiscard]] bool isRegistered(std::uint64_t entityId) const;

        /**
         * @brief 获取注册实体的数量。
         * @return 注册实体数量。
         */
        [[nodiscard]] std::size_t getRegisteredEntityCount() const { return m_registeredEntities.size(); }

        // 应用速度修正
        virtual void applyVelocityConstraint([[maybe_unused]] double dt) = 0;

        // 应用位置修正（位置级别，解决穿透等问题）
        virtual void applyPositionConstraint([[maybe_unused]] double dt) = 0;

        // 角动量修正
        virtual void applyAngularVelocityConstraint([[maybe_unused]] double dt) = 0;

        // 计算约束满足程度（用于调试，返回误差值）
        [[nodiscard]] virtual double computeConstraintError() const = 0;

        // 将约束力注入到受约束实体的力累加器中（积分前调用）
        virtual void applyConstraintForces();

        virtual void applyConstraintTorques();

        // 获取参考长度（用于渲染和调试）
        [[nodiscard]] virtual double getReferenceLength() const { return 0.0; }

        // 设置约束启用/禁用
        void setEnabled(bool enabled);

        [[nodiscard]] bool isEnabled() const;

    protected:
        ConstraintsBase(const ConstraintsBase& other) = default;

        ConstraintsBase& operator=(const ConstraintsBase& other) = default;

        ConstraintsBase();

        /** 注册的实体集合（弱引用，不拥有所有权）。 */
        std::unordered_set<PhysicalEntity*> m_registeredEntities;

        /** 从实体 ID 到指针的映射，用于 O(1) 查询和注销。 */
        std::unordered_map<std::uint64_t, PhysicalEntity*> m_entityIdMap;

        bool m_enabled{true};
    };
} // egret

#endif //EGRET_PHYSICS_CONSTRAINTS_BASE_H
