//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_DISK_H
#define EGRET_PHYSICS_SHAPE_DISK_H
#include "shape_base.h"

namespace egret {

class ShapeDisk: public ShapeBase {
public:
    explicit ShapeDisk(double radius);

    [[nodiscard]] ShapeID typeId() const override;

    [[nodiscard]] double getRadius() const { return m_radius; }

    [[nodiscard]] double getVolume() const override { return 0; }

    [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

    [[nodiscard]] AABB getAABB(const Transform& transform) const override;

    [[nodiscard]] SceneRenderItem getBasicRenderInfo(const Eigen::Vector3d& position) const override;

    void setRadius(double radius);

    [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d& direction, const Transform& transform) const override;
private:
    double m_radius{};
};

} // egret

#endif //EGRET_PHYSICS_SHAPE_DISK_H
