//
// Created by jehor on 2026/4/24.
//

#include "shape_base.h"

namespace egret
{
    ShapeLoadInfo ShapeBase::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = typeId();
        return info;
    }

    bool ShapeBase::collide(const ShapeBase* other,
        const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const
    {
        const auto& myId = this->typeId();
        const auto& otherId = other->typeId();
        const auto func = ShapeRegister::instance().findJudge(myId, otherId);
        if (func) return (*func)(this, thisTrans, other, otherTrans, manifold);
        // 未注册的类对 → 可返回 false / 抛异常 / 默认处理
        return false;
    }
} // egret