//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_GRAVITY_FIELD_H
#define EGRET_PHYSICS_GRAVITY_FIELD_H
#include "field_base.h"
#include "constants.h"

namespace egret {

class GravityField: public FieldBase {
public:
    GravityField();

    ~GravityField() override = default;

    explicit GravityField(Eigen::Vector3d  gravity, Eigen::Vector3d  referencePoint);

    explicit GravityField(double gravity, double referenceZ);

    Eigen::Vector3d sample(const Eigen::Vector3d& position) override;

    double potential(const Eigen::Vector3d& position) override;

    void applyToEntity(PhysicalEntity* entity) override;

private:
    // static constexpr char FIELD_FLAG_GRAVITY[] {"FIELD_BASE"};

    Eigen::Vector3d m_gravity{0, 0, -STANDARD_GRAVITY};

    Eigen::Vector3d m_zeroReferencePoint{};
};

} // egret

#endif //EGRET_PHYSICS_GRAVITY_FIELD_H
