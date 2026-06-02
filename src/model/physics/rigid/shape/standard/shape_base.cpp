/**
 * @file        shape_base.cpp
 * @brief       物理形状基类实现文件，定义所有物理形状的公共接口。
 * @details     实现 ShapeBase 类的各项成员函数。
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
#include "shape_base.h"

namespace egret
{
    ShapeLoadInfo ShapeBase::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = (std::uint32_t)getType();
        return info;
    }

    ShapeType ShapeBase::getType() const
    {
        return ShapeType::Abstract;
    }

    bool ShapeBase::collide(const ShapeBase *other,
                            const Transform &thisTrans, const Transform &otherTrans, ContactManifold &manifold) const
    {
        const auto myId = static_cast<std::uint32_t>(this->getType());
        const auto otherId = static_cast<std::uint32_t>(other->getType());
        const auto func = ShapeRegister::instance().findJudge(myId, otherId);
        if (!func) {
            return false;
        }

        // 规范顺序：myId 为注册 key 中较小的类型 ID，Lambda 期望 ShapeA = 该类型
        if (myId <= otherId) {
            return (*func)(this, thisTrans, other, otherTrans, manifold);
        }

        // 非规范顺序：交换参数调用 Lambda，然后翻转所有法线以保持 A→B 方向
        const bool result = (*func)(other, otherTrans, this, thisTrans, manifold);
        for (ContactPoint& contact : manifold) {
            contact.normal = -contact.normal;
        }
        return result;
    }

    std::optional<double> ShapeBase::continuousCollide(const ShapeBase *other,
                                                       const Transform &initTransA,
                                                       const Transform &initTransB,
                                                       const Eigen::Vector3d &thisLinearVel, const Eigen::Vector3d &thisAngularVel,
                                                       const Eigen::Vector3d &otherLinearVel, const Eigen::Vector3d &otherAngularVel, double dt,
                                                       ContactManifold &manifold) const
    {
        const auto myId = static_cast<std::uint32_t>(this->getType());
        const auto otherId = static_cast<std::uint32_t>(other->getType());
        const auto func = ShapeRegister::instance().findContinuousJudge(myId, otherId);
        if (!func) {
            return std::nullopt;
        }

        // 规范顺序：myId 为注册 key 中较小的类型 ID
        if (myId <= otherId) {
            return (*func)(this, initTransA,
                           thisLinearVel, thisAngularVel, other, initTransB,
                           otherLinearVel, otherAngularVel,
                           dt, manifold);
        }

        // 非规范顺序：交换参数调用，然后翻转所有法线
        const auto result = (*func)(other, initTransB,
                                    otherLinearVel, otherAngularVel,
                                    this, initTransA,
                                    thisLinearVel, thisAngularVel,
                                    dt, manifold);
        for (ContactPoint& contact : manifold) {
            contact.normal = -contact.normal;
        }
        return result;
    }

    std::string reflectShapeToString(const ShapeType type)
    {
        const std::string_view view {magic_enum::enum_name(type)};
        return std::string(view);
    }
} // egret