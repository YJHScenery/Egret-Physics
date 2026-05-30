/**
* @file        field_base.h
 * @brief       物理场基类头文件
 * @details     定义FieldBase类，作为所有物理场（重力场、引力场等）的基类，
 *              提供场向量采样、势能计算、施力到实体等接口。
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

#ifndef EGRET_PHYSICS_FIELD_BASE_H
#define EGRET_PHYSICS_FIELD_BASE_H
#include "Eigen/Dense"
#include "physics_abstract.h"

namespace egret
{
    class PhysicalEntity;
    
    enum class FieldType : std::uint32_t
    {
        Unknown = 0,

        Gravity, Gravitation,

        Abstract = 127,

        Other = 255,
    };

    /**
     * @brief       物理场抽象基类，定义所有物理场的公共接口。
     * @details     FieldBase 是所有物理场（重力场、引力场等）的抽象基类，
     *              提供场向量采样、势能计算、施力到实体等接口。
     *              物理场用于模拟环境中的力场，如重力场、引力场、电磁场等。
     *              采用策略模式，派生类实现具体的场逻辑。
     * 
     * @invariant   m_id 是唯一的场标识符
     * @invariant   m_enabled 表示场是否激活
     * @remark      场不是线程安全的，多线程访问需要外部同步
     * @see         GravityField, GravitationalField, PhysicalEntity
     */
    class FieldBase: virtual public PhysicsAbstract
    {
    public:
        explicit FieldBase(std::uint64_t id);

        FieldBase(FieldBase&& other) = default;

        FieldBase& operator=(FieldBase&& other) = default;

        ~FieldBase() override = default;
        // 返回此位置的场向量
        virtual Eigen::Vector3d sample(const Eigen::Vector3d& position) = 0;

        // 返回势能或势函数值，非保守场以后可以返回空值或默认实现（不包含质量乘数）
        virtual double potential(const Eigen::Vector3d& position) = 0;

        // 把场转换成对 PhysicalEntity 的受力，并注入到实体里
        virtual void applyToEntity(PhysicalEntity* entity) = 0;

        virtual void setEnabled(bool enable);

        virtual FieldType getType() const;


    protected:
        FieldBase();

        FieldBase(const FieldBase& other) = default;

        FieldBase& operator=(const FieldBase& other) = default;

        bool m_enabled{true};
    };
} // egret

#endif //EGRET_PHYSICS_FIELD_BASE_H
