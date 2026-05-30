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
#include <vector>
#include "physics_abstract.h"

enum class ConstraintType : std::uint64_t
{
    Base = 0xFFFFFFAA00000000,
    ConnectingLine = 0xFFFFFFAA00000001,
    SimplePendulum = 0xFFFFFFAA00000002,
    ConnectingRod = 0xFFFFFFAA00000003,
    SlidingRail = 0xFFFFFFAA00000004,
};

namespace egret {

    class PhysicalEntity;
    /**
     * @brief       约束器抽象基类，定义所有约束器的公共接口。
     * @details     ConstraintsBase 是所有约束器（连接线、单摆、连接杆、滑轨等）的抽象基类，
     *              提供速度约束、位置约束、角速度约束等接口。
     *              约束器用于限制物理实体的运动自由度，如保持距离、限制角度等。
     *              采用策略模式，派生类实现具体的约束逻辑。
     * 
     * @invariant   m_id 是唯一的约束器标识符
     * @invariant   m_physicalEntities 列表中的实体指针有效
     * @invariant   m_enabled 表示约束器是否激活
     * @remark      约束器不是线程安全的，多线程访问需要外部同步
     * @see         ConnectingLine, SimplePendulum, ConnectingRod, SlidingRail, PhysicalEntity
     */
    class ConstraintsBase: virtual public PhysicsAbstract {
    public:
        explicit ConstraintsBase(std::uint64_t id);

        ~ConstraintsBase() override = default;

        ConstraintsBase(ConstraintsBase&& other) = default;

        ConstraintsBase& operator=(ConstraintsBase&& other) = default;

        // 返回约束类型
        [[nodiscard]] virtual ConstraintType getType() const;

        // 获取约束施加的所有实体
        [[nodiscard]] virtual std::vector<PhysicalEntity*> getConstrainedEntities();

        [[nodiscard]] virtual const std::vector<PhysicalEntity*>& getConstrainedEntities() const;

        // 应用约束力/冲量到实体上（速度级别）
        virtual void applyVelocityConstraint(double dt) = 0;

        // 应用位置修正（位置级别，解决穿透等问题）
        virtual void applyPositionConstraint(double dt) = 0;

        // 角动量修正
        virtual void applyAngularVelocityConstraint(double dt) = 0;

        // 计算约束满足程度（用于调试，返回误差值）
        [[nodiscard]] virtual double computeConstraintError() const = 0;

        // 设置约束启用/禁用
        void setEnabled(bool enabled);

        [[nodiscard]] bool isEnabled() const;

    protected:

        ConstraintsBase(const ConstraintsBase& other) = default;

        ConstraintsBase& operator=(const ConstraintsBase& other) = default;

        ConstraintsBase();

        std::vector<PhysicalEntity*> m_physicalEntities{};

        bool m_enabled{true};

        std::uint64_t m_id;
    };

} // egret

#endif //EGRET_PHYSICS_CONSTRAINTS_BASE_H
