//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_FIELD_BASE_H
#define EGRET_PHYSICS_FIELD_BASE_H
#include "Eigen/Dense"

namespace egret
{
    class PhysicalEntity;

    enum class FieldType : std::uint64_t
    {
        Base = 0xFFFFFFFF00000000,
        Gravity = 0xFFFFFFDD00000000,
        Gravitation = 0xFFFFFFBB00000000,
    };

    class FieldBase
    {
    public:
        FieldBase();

        explicit FieldBase(std::uint64_t id);

        virtual ~FieldBase() = default;
        // 返回此位置的场向量
        virtual Eigen::Vector3d sample(const Eigen::Vector3d& position) = 0;

        // 返回势能或势函数值，非保守场以后可以返回空值或默认实现（不包含质量乘数）
        virtual double potential(const Eigen::Vector3d& position) = 0;

        // 把场转换成对 PhysicalEntity 的受力，并注入到实体里
        virtual void applyToEntity(PhysicalEntity* entity) = 0;

        virtual void setEnabled(bool enable);

    protected:
        bool m_enabled{true};

        std::uint64_t m_id;

        inline static std::uint32_t m_createCount_static{0};

        static std::uint64_t generateID(FieldType fieldType);

        // static constexpr char FIELD_FLAG_BASE[] {"FIELD_BASE_"};
    };


} // egret

#endif //EGRET_PHYSICS_FIELD_BASE_H
