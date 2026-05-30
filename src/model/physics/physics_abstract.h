//
// Created by jehor on 2026/5/30.
//

#ifndef EGRET_PHYSICS_PHYSICS_ID_SYSTEM_H
#define EGRET_PHYSICS_PHYSICS_ID_SYSTEM_H
#include <cstdint>
#include <memory>

namespace egret
{
    class PhysicsAbstract
    {
    public:
        explicit PhysicsAbstract(const std::uint64_t id): m_id{id} {};

        virtual ~PhysicsAbstract() = default;
        PhysicsAbstract(PhysicsAbstract&& other) = default;
        PhysicsAbstract& operator=(PhysicsAbstract&& other) = default;

        void setId(std::uint64_t id);

        std::uint64_t getId() const;

        virtual std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const = 0;

    protected:
        PhysicsAbstract() = default;

        PhysicsAbstract(const PhysicsAbstract& other) = default;
        PhysicsAbstract& operator=(const PhysicsAbstract& other) = default;

        std::uint64_t m_id{};
    };
} // egret

#endif //EGRET_PHYSICS_PHYSICS_ID_SYSTEM_H
