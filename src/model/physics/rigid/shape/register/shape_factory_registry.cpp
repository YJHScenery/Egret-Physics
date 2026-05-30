/**
* @file        shape_factory_registry.cpp
 * @brief       形状工厂注册表实现文件
 * @details     实现ShapeFactoryRegistry类的各项成员函数。
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
