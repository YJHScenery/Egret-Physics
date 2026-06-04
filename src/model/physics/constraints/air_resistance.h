/**
 * @file        air_resistance.h
 * @brief       空气阻力约束器头文件
 * @details     定义 DragModel 枚举、DragConfig 结构体和 AirResistance 类，
 *              提供三种空气阻力模型（速度正比、速度平方正比、恒定大小）以及旋转阻力。
 *              每个实体可独立配置阻力模型和参数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-xx
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_AIR_RESISTANCE_H
#define EGRET_PHYSICS_AIR_RESISTANCE_H

#include <cstdint>
#include <vector>

#include "constraints_base.h"

namespace egret
{
    /**
     * @brief 空气阻力模型枚举。
     * @details 定义三种平动阻力模型：
     *          - Linear: 阻力与速度成正比 F = -b·v
     *          - Quadratic: 阻力与速度平方成正比 F = -½·ρ·Cd·A·|v|·v
     *          - Constant: 阻力大小恒定 F = -c·v̂
     */
    enum class DragModel : std::uint8_t
    {
        Linear    = 0,  ///< 速度正比模型
        Quadratic = 1,  ///< 速度平方正比模型
        Constant  = 2,  ///< 恒定大小模型
    };

    /**
     * @brief 空气阻力配置结构体。
     * @details 存储一个实体的完整空气阻力参数，包括模型选择、平动阻力系数和旋转阻力系数。
     *          旋转阻力默认禁用，通过 enableRotationalDrag 开启。
     *
     * @invariant 所有系数为非负值
     * @invariant fluidDensity > 0（仅在 Quadratic 模型中使用）
     * @invariant crossSectionArea >= 0（仅在 Quadratic 模型中使用）
     */
    struct DragConfig
    {
        /// 阻力模型选择，默认使用速度平方正比模型
        DragModel model = DragModel::Quadratic;

        // ---------- 平动阻力参数 ----------
        /** 线性阻尼系数 b（Linear 模型使用），单位 N·s/m */
        double linearCoeff = 0.01;

        /** 阻力系数 Cd（Quadratic 模型使用），无量纲，球体默认 0.47 */
        double dragCoefficient = 0.47;

        /** 迎风截面积 A（Quadratic 模型使用），单位 m² */
        double crossSectionArea = 1.0;

        /** 流体密度 ρ（Quadratic 模型使用），单位 kg/m³，默认空气密度 1.225 */
        double fluidDensity = 1.225;

        /** 恒定阻力大小 c（Constant 模型使用），单位 N */
        double constantMagnitude = 1.0;

        // ---------- 旋转阻力参数 ----------
        /** 线性旋转阻尼系数 b_rot，单位 N·m·s/rad */
        double rotationalLinearCoeff = 0.01;

        /** 平方旋转阻尼系数 c_rot，单位 N·m·s²/rad² */
        double rotationalQuadraticCoeff = 0.01;

        /** 是否启用旋转阻力（默认关闭） */
        bool enableRotationalDrag = false;
    };

    /**
     * @brief 空气阻力约束器类。
     * @details AirResistance 继承自 ConstraintsBase，通过复写 applyConstraintForces()
     *          向注册的物理实体施加空气阻力（耗散力）。支持三种平动阻力模型和可选的旋转阻力。
     *
     *          每个实体可独立配置 DragConfig。实体与配置在同一个连续 vector 中绑定存储，
     *          热路径遍历时零额外查找。
     *
     *          由于配置和实体指针绑定存储，registerEntity/unregisterEntity 被覆写以同步维护
     *          m_entityDrags vector。同时提供 addEntity() 便捷方法，一步完成注册+配置。
     *
     *          约束器自身的 ID (m_id) 用作力/力矩的标识符传入 upsertForce/upsertTorque，
     *          与 Spring 的实现模式一致。
     *
     * @invariant m_entityDrags 中存储的实体指针与 m_registeredEntities 一致
     * @invariant 所有纯虚约束接口（velocity/position/angular）为空实现，因为空气阻力不限制自由度
     * @see        ConstraintsBase, DragModel, DragConfig, PhysicalEntity
     */
    class AirResistance : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造函数。
         * @param id 约束器唯一标识符。
         */
        explicit AirResistance(std::uint64_t id);

        ~AirResistance() override = default;

        // ---------- 实体注册 ----------

        /**
         * @brief 注册实体并使用默认配置。
         * @details 覆写 ConstraintsBase::registerEntity，在基类注册的同时
         *          向 m_entityDrags 添加一条使用默认配置的条目。
         * @param entity 要注册的物理实体指针。
         */
        void registerEntity(PhysicalEntity* entity) override;

        /**
         * @brief 注销实体。
         * @details 覆写 ConstraintsBase::unregisterEntity，在基类注销的同时
         *          从 m_entityDrags 中移除对应条目。
         * @param entityId 要移除的实体 ID。
         * @return 如果实体之前已注册则返回 true。
         */
        bool unregisterEntity(std::uint64_t entityId) override;

        /**
         * @brief 一步完成实体注册和配置设置。
         * @details 便捷方法，等效于依次调用 registerEntity() + setEntityDragConfig()。
         * @param entity 目标物理实体指针。
         * @param config 该实体的空气阻力配置。
         */
        void addEntity(PhysicalEntity* entity, const DragConfig& config);

        // ---------- 实体配置接口 ----------

        /**
         * @brief 设置指定实体的空气阻力配置。
         * @details 在 m_entityDrags 中查找并更新该实体的配置。
         *          如果实体已注册但未找到（不应发生），则静默忽略。
         * @param entityId 实体 ID。
         * @param config   空气阻力配置。
         */
        void setEntityDragConfig(std::uint64_t entityId, const DragConfig& config);

        /**
         * @brief 获取指定实体的空气阻力配置。
         * @details 优先返回实体的独立配置，未找到则返回默认配置。
         * @param entityId 实体 ID。
         * @return 该实体使用的空气阻力配置。
         */
        [[nodiscard]] DragConfig getEntityDragConfig(std::uint64_t entityId) const;

        /**
         * @brief 移除指定实体的独立配置，使其回退到默认配置。
         * @details 当实体未独立配置时返回 false。
         *          registerEntity 添加的条目不受此方法影响，只是将配置覆盖为默认值。
         * @param entityId 实体 ID。
         * @return 如果之前存在独立配置则返回 true，否则返回 false。
         */
        bool removeEntityDragConfig(std::uint64_t entityId);

        /**
         * @brief 设置默认空气阻力配置。
         * @details 未单独配置的实体将使用此默认配置。
         * @param config 默认空气阻力配置。
         */
        void setDefaultDragConfig(const DragConfig& config);

        /**
         * @brief 获取当前默认空气阻力配置。
         * @return 默认配置的常引用。
         */
        [[nodiscard]] const DragConfig& getDefaultDragConfig() const;

        // ---------- ConstraintsBase 接口覆写 ----------

        /**
         * @brief 应用空气阻力到所有注册实体。
         * @details 直接遍历 m_entityDrags vector，
         *          对每个实体计算平动阻力和旋转阻力并通过 upsertForce/upsertTorque 注入。
         *          由于 config 与 entity 指针在同一缓存行中，此遍历具有最佳 cache 局部性。
         */
        void applyConstraintForces() override;

        void applyVelocityConstraint(double dt) override { std::ignore = dt; }

        void applyPositionConstraint(double dt) override { std::ignore = dt; }

        void applyAngularVelocityConstraint(double dt) override { std::ignore = dt; }

        [[nodiscard]] double computeConstraintError() const override { return 0.0; }

        [[nodiscard]] ConstraintType getType() const override;

        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

    private:
        /**
         * @brief 实体与配置的绑定结构。
         * @details 在一个连续 vector 中存储实体指针及其对应的阻力配置，
         *          热路径遍历时零查找开销。
         */
        struct EntityDragEntry
        {
            PhysicalEntity* entity = nullptr;
            DragConfig config;
        };

        /**
         * @brief 在 m_entityDrags 中查找实体索引。
         * @param entityId 实体 ID。
         * @return 找到时返回迭代器，未找到返回 m_entityDrags.end()。
         */
        [[nodiscard]] std::vector<EntityDragEntry>::iterator findEntry(std::uint64_t entityId);

        /**
         * @brief 在 m_entityDrags 中查找实体索引（常版本）。
         * @param entityId 实体 ID。
         * @return 找到时返回迭代器，未找到返回 m_entityDrags.cend()。
         */
        [[nodiscard]] std::vector<EntityDragEntry>::const_iterator findEntry(std::uint64_t entityId) const;

        /**
         * @brief 对单个实体施加空气阻力。
         * @param entity 目标物理实体指针。
         * @param config 该实体的阻力配置。
         */
        void applyDragToEntity(PhysicalEntity* entity, const DragConfig& config);

        /** 速度极小阈值，低于此值认为速度为零，避免除零。 */
        static constexpr double kVelocityEpsilon = 1e-10;

        /** 默认空气阻力配置，用于未单独配置的实体。 */
        DragConfig m_defaultConfig;

        /**
         * @brief 实体与配置绑定的连续数组。
         * @details applyConstraintForces 直接遍历此数组，无需额外查找。
         *          registerEntity/unregisterEntity 同步维护此数组。
         */
        std::vector<EntityDragEntry> m_entityDrags;
    };
} // egret

#endif //EGRET_PHYSICS_AIR_RESISTANCE_H
