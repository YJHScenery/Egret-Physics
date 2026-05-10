//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_ROD_H
#define EGRET_PHYSICS_SHAPE_ROD_H
#include <string>

#include "shape_base.h"


namespace egret
{
    class ShapeRod : public ShapeBase
    {
    public:
        explicit ShapeRod(double length);
        [[nodiscard]] const std::string& typeId() const override;

        [[nodiscard]] double getLength() const { return m_length; }

        [[nodiscard]] double getVolume() const override { return 0; };

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override ;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] AABB getAABB(const Transform& transform) const override;

        [[nodiscard]] SceneRenderItem getBasicRenderInfo(const Eigen::Vector3d& position) const override;

        void setLength(double length);

        std::pair<Eigen::Vector3d, Eigen::Vector3d> getStartEnd(const Transform& transform) const;

    private:
        double m_length{};
    };
}


#endif //EGRET_PHYSICS_SHAPE_ROD_H
