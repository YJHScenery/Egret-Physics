/**
 * @file        surface_base.cpp
 * @brief       曲面抽象基类默认实现。
 * @details     提供 SurfaceBase 的默认 collide 和 velocityAt 实现。
 *
 * @ingroup     Physics
 */

#include "surface_base.h"

namespace egret
{
    bool SurfaceBase::collide(const ShapeBase* shape,
                              const Transform& shapeTransform,
                              ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        // 获取形状质心（局部坐标下的质心，变换到世界空间）
        const Eigen::Vector3d comLocal = shape->getCenterOfMass();
        const Eigen::Vector3d comWorld = shapeTransform.transformPointToWorld(comLocal);

        // 找到曲面上距离质心最近的点
        const Eigen::Vector3d cp = closestPoint(comWorld);
        const Eigen::Vector3d n = normal(comWorld);

        // 使用形状的支撑函数，在 -n 方向找到形状上最"深入"曲面的点
        const Eigen::Vector3d supportLocal = shape->support(-n, shapeTransform);
        const double sd = signedDistance(supportLocal);

        // sd < 0 意味着支撑点已经穿透到曲面"内部"
        if (sd >= 0.0) {
            return false; // 没有穿透
        }

        // 生成接触点
        ContactPoint contact;
        contact.position = cp;                              // 接触点位置（曲面上）
        contact.normal = n;                                  // 法线指向曲面外部
        contact.penetration = -sd;                           // 穿透深度（正值）
        manifold.push_back(contact);
        return true;
    }

    Eigen::Vector3d SurfaceBase::velocityAt(const Eigen::Vector3d& /*worldPoint*/) const
    {
        return Eigen::Vector3d::Zero();
    }
} // egret
