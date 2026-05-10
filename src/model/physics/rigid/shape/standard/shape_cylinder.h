//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_SHAPE_CYLINDER_H
#define EGRET_PHYSICS_SHAPE_CYLINDER_H
#include "shape_base.h"

namespace egret {

class ShapeCylinder: public ShapeBase {
public:
    // 构造函数
    ShapeCylinder(double radius, double height);

    [[nodiscard]] const std::string& typeId() const override;

    // 获取几何属性
    [[nodiscard]] double getRadius() const { return m_radius; }
    [[nodiscard]] double getHeight() const { return m_height; }
    [[nodiscard]] double getHalfHeight() const { return m_height * 0.5; }

    [[nodiscard]] double getVolume() const override;
    [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;
    [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;
    [[nodiscard]] AABB getAABB(const Transform& transform) const override;
    [[nodiscard]] SceneRenderItem getBasicRenderInfo(const Eigen::Vector3d& position) const override;

    // [[nodiscard]] bool collideWith(const ShapeBase* other,
    //                                const Transform& thisTrans, const Transform& otherTrans,
    //                                ContactManifold& manifold) const override;

    void setRadius(double radius);

    void setHeight(double height);
protected:
    double m_radius{};
    double m_height{};
};

} // egret

#endif //EGRET_PHYSICS_SHAPE_CYLINDER_H
