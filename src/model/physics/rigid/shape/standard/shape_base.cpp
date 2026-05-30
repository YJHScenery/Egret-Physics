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
        info.typeId = (std::uint32_t)typeId();
        return info;
    }

    ShapeType ShapeBase::typeId() const
    {
        return ShapeType::Abstract;
    }

    bool ShapeBase::collide(const ShapeBase *other,
                            const Transform &thisTrans, const Transform &otherTrans, ContactManifold &manifold) const
    {
        const auto &myId = (std::uint32_t)this->typeId();
        const auto &otherId = (std::uint32_t)other->typeId();
        const auto func = ShapeRegister::instance().findJudge(myId, otherId);
        if (func)
            return (*func)(this, thisTrans, other, otherTrans, manifold);
        // 未注册的类对 → 可返回 false / 抛异常 / 默认处理
        return false;
    }

    std::optional<double> ShapeBase::continuousCollide(const ShapeBase *other,
                                                       const Transform &initTransA,
                                                       const Transform &initTransB,
                                                       const Eigen::Vector3d &thisLinearVel, const Eigen::Vector3d &thisAngularVel,
                                                       const Eigen::Vector3d &otherLinearVel, const Eigen::Vector3d &otherAngularVel, double dt,
                                                       ContactManifold &manifold) const
    {
        const auto &myId = (std::uint32_t)this->typeId();
        const auto &otherId = (std::uint32_t)other->typeId();
        const auto func = ShapeRegister::instance().findContinuousJudge(myId, otherId);
        if (func)
        {
            return (*func)(this, initTransA,
                           thisLinearVel, thisAngularVel, other, initTransB,
                           otherLinearVel, otherAngularVel,
                           dt, manifold);
        }
        // 未注册的类对 → 可返回 nullopt / 抛异常 / 默认处理
        return std::nullopt;
    }
} // egret