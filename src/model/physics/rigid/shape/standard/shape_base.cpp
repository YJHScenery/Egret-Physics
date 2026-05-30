//
// Created by jehor on 2026/4/24.
//

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

    bool ShapeBase::collide(const ShapeBase* other,
                            const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const
    {
        const auto& myId = (std::uint32_t)this->typeId();
        const auto& otherId = (std::uint32_t)other->typeId();
        const auto func = ShapeRegister::instance().findJudge(myId, otherId);
        if (func) return (*func)(this, thisTrans, other, otherTrans, manifold);
        // 未注册的类对 → 可返回 false / 抛异常 / 默认处理
        return false;
    }

    std::optional<double> ShapeBase::continuousCollide(const ShapeBase* other,
        const Transform& initTransA,
        const Transform& initTransB,
        const Eigen::Vector3d& thisLinearVel, const Eigen::Vector3d& thisAngularVel,
        const Eigen::Vector3d& otherLinearVel, const Eigen::Vector3d& otherAngularVel, double dt,
        ContactManifold& manifold) const
    {
        const auto& myId = (std::uint32_t)this->typeId();
        const auto& otherId = (std::uint32_t)other->typeId();
        const auto func = ShapeRegister::instance().findContinuousJudge(myId, otherId);
        if (func) {
            return (*func)(this, initTransA,
                           thisLinearVel, thisAngularVel, other, initTransB,
                           otherLinearVel, otherAngularVel,
                           dt, manifold);
        }
        // 未注册的类对 → 可返回 nullopt / 抛异常 / 默认处理
        return std::nullopt;
    }
} // egret