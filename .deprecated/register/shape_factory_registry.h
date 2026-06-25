/**
* @file        shape_factory_registry.h
 * @brief       形状工厂注册表头文件
 * @details     定义ShapeFactoryRegistry类，提供形状工厂的注册和创建功能。
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

    /**
     * @brief       形状工厂注册表类，管理形状工厂的注册和创建。
     * @details     ShapeFactoryRegistry 是一个单例类，提供形状工厂的注册和创建功能。
     *              采用工厂模式，不同类型的形状可以通过注册工厂函数来创建。
     *              通过 ShapeLoadInfo 参数可以动态创建各种形状实例。
     *              支持运行时扩展，可以动态注册新的形状类型。
     * 
     * @invariant   m_factories 存储已注册的形状工厂函数
     * @invariant   typeId 与工厂函数一一对应
     * @remark      ShapeFactoryRegistry 是单例，全局唯一实例
     * @see         ShapeBase, ShapeLoadInfo, ShapeRegister
     */
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
