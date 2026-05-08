//
// Created by jehor on 2026/5/6.
//

#include "shape_register.h"
#include "shape_base.h"

namespace egret
{
    ShapeRegister& ShapeRegister::instance()
    {
        static ShapeRegister shape_register;
        return shape_register;
    }

    void ShapeRegister::registerJudge(const std::string& id1, const std::string& id2, CollideFunc func)
    {
        const auto key = std::make_pair(min(id1, id2), max(id1, id2));
        registry_[key] = std::move(func);
    }

    CollideFunc* ShapeRegister::findJudge(const std::string& id1, const std::string& id2)
    {
        const auto key = std::make_pair(min(id1, id2), max(id1, id2));
        const auto it = registry_.find(key);
        return it != registry_.end() ? &it->second : nullptr;
    }
} // egret