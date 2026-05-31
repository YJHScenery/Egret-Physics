/**
 * @file        surface_sphere.cpp
 * @brief       标准（椭）球面实现文件。
 * @details     提供 SurfaceSphere 的 collide 实现，包括对球体形状的解析碰撞检测。
 *
 * @ingroup     Physics
 */

#include "surface_sphere.h"
// 仅当对球体形状做解析优化时需要
#include "shape_sphere.h"

namespace egret
{
    bool SurfaceSphere::collide(const ShapeBase* shape,
                                const Transform& shapeTransform,
                                ContactManifold& manifold) const
    {
        if (shape == nullptr) {
            return false;
        }

        // 对球形形状使用解析碰撞检测
        if (shape->getType() == ShapeType::Sphere) {
            const auto* sphere = static_cast<const ShapeSphere*>(shape);
            const double sphereRadius = sphere->getRadius();

            // 球心在世界空间中的位置
            const Eigen::Vector3d sphereCenter = shapeTransform.getTranslation();

            // 球心到椭球面的最近点
            const Eigen::Vector3d cp = closestPoint(sphereCenter);
            const Eigen::Vector3d n = normal(sphereCenter);

            // 球心到椭球面的有符号距离
            const double sd = signedDistance(sphereCenter);

            // 如果球心到曲面的距离小于球半径，则发生碰撞
            // 注意：sd > 0 表示球心在外部，sd < 0 表示球心在内部
            // 碰撞条件：|sd| < sphereRadius (球心在曲面附近的球半径范围内)
            // 更精确：球表面与曲面相交
            if (sd > sphereRadius) {
                // 球完全在曲面外部，没有碰撞
                return false;
            }

            // 计算穿透深度
            const double penetrationDepth = sphereRadius - sd;

            ContactPoint contact;
            contact.position = cp;
            contact.normal = n;
            contact.penetration = penetrationDepth;
            manifold.push_back(contact);
            return true;
        }

        // 对其他形状，回退到基类默认实现
        return SurfaceBase::collide(shape, shapeTransform, manifold);
    }
} // egret
