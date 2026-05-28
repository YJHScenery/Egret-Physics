//
// Created by GitHub Copilot on 2026/5/10.
//

#include "shape_factory_registry.h"

namespace egret
{
    ShapeFactoryRegistry& ShapeFactoryRegistry::instance()
    {
        static ShapeFactoryRegistry registry;
        return registry;
    }

    void ShapeFactoryRegistry::registerFactory(std::uint32_t typeId, ShapeFactory factory)
    {
        m_factories[typeId] = std::move(factory);
    }

    std::unique_ptr<ShapeBase> ShapeFactoryRegistry::create(const ShapeLoadInfo& info) const
    {
        const auto it = m_factories.find(info.typeId);
        if (it == m_factories.end()) {
            return nullptr;
        }
        return it->second(info);
    }
}
