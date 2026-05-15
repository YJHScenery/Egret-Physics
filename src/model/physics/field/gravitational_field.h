//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
#define EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
#include "field_base.h"
#include "particle.h"
#include "constants.h"

namespace egret
{
    class GravitationalField : public FieldBase, public Particle
    {
    public:
        GravitationalField();

        GravitationalField(Eigen::Vector3d position,
                           Eigen::Vector3d speed,
                           double mass,
                           double coupling = G,
                           bool fixed = false);

        Eigen::Vector3d sample(const Eigen::Vector3d& position) override;

        double potential(const Eigen::Vector3d& position) override;

        void applyToEntity(PhysicalEntity* entity) override;

        void applyForce(double time) override;

        void setFixed(bool fixed);

        [[nodiscard]] bool isFixed() const;

        void setCouplingCoefficient(double coefficient);

        [[nodiscard]] double getCouplingCoefficient() const;

    private:
        double m_coupling{G};

        bool m_fixed{false};

        static constexpr double MIN_DISTANCE_SQUARED = 1e-6;
    };
} // egret

#endif // EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
