//
// Created by GitHub Copilot on 2026/5/10.
//

#ifndef EGRET_PHYSICS_SHAPE_FACTORY_REGISTRY_H
#define EGRET_PHYSICS_SHAPE_FACTORY_REGISTRY_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "shape_base.h"

namespace egret
{
    using ShapeFactory = std::function<std::unique_ptr<ShapeBase>(const ShapeLoadInfo& info)>;

    class ShapeFactoryRegistry
    {
    public:
        static ShapeFactoryRegistry& instance();

        void registerFactory(std::uint32_t typeId, ShapeFactory factory);

        [[nodiscard]] std::unique_ptr<ShapeBase> create(const ShapeLoadInfo& info) const;

    private:
        std::unordered_map<std::uint32_t, ShapeFactory> m_factories;
    };
}

#endif // EGRET_PHYSICS_SHAPE_FACTORY_REGISTRY_H
