//
// Created by jehor on 2026/5/6.
//

#include "physics_utils.h"

#include "standard_collide_judge_group.h"
#include "shape_base.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "shape_sphere.h"
#include "shape_spherical_shell.h"

namespace egret
{
    bool box_first::collideBoxBox(const ShapeBox& boxA, const Transform& transA, const ShapeBox& boxB,
                                  const Transform& transB, ContactManifold& manifold)
    {
        const Eigen::Vector3d halfA = boxA.getHalfSize();
        const Eigen::Vector3d halfB = boxB.getHalfSize();

        // 获取两个物体的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> axesA{}, axesB{};
        ShapeBox::getLocalAxes(transA, axesA[0], axesA[1], axesA[2]);
        ShapeBox::getLocalAxes(transB, axesB[0], axesB[1], axesB[2]);

        // 中心连线（从 A 指向 B）
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d t = centerB - centerA;

        // 待测试的轴列表（所有分离轴）
        std::vector<Eigen::Vector3d> axes;
        // 添加 A 的三个面法线
        for (int i = 0; i < 3; ++i)
            axes.push_back(axesA[i]);
        // 添加 B 的三个面法线
        for (int i = 0; i < 3; ++i)
            axes.push_back(axesB[i]);
        // 添加边 × 边得到的 9 个轴
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                Eigen::Vector3d axis = axesA[i].cross(axesB[j]);
                if (axis.norm() > 1e-9) {
                    axes.push_back(axis.normalized());
                }
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 辅助函数：计算一个盒子的半长在给定方向上的投影长度
        auto projectHalfExtent = [](const Eigen::Vector3d& half,
                                    const Eigen::Vector3d& axis,
                                    const std::array<Eigen::Vector3d, 3>& localAxes) -> double
        {
            // 盒子三个局部轴方向上的半长在 axis 上的投影绝对值之和
            double proj = 0.0;
            for (int i = 0; i < 3; ++i) {
                proj += std::abs(axis.dot(localAxes[i])) * half[i];
            }
            return proj;
        };

        // 遍历所有候选轴
        for (const Eigen::Vector3d& axis : axes) {
            // 避免轴过短
            if (axis.squaredNorm() < 1e-9)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            // 计算 A 和 B 在该轴上的投影半径
            const double rA = projectHalfExtent(halfA, unitAxis, axesA);
            const double rB = projectHalfExtent(halfB, unitAxis, axesB);

            // 投影中心之间的距离
            const double centerDist = std::abs(unitAxis.dot(t));
            const double sumRadii = rA + rB;

            // 如果间隔 > 0，则在该轴上分离
            if (centerDist > sumRadii) {
                return false; // 无碰撞
            }

            // 记录最小重叠量（穿透深度）及对应轴
            const double overlap = sumRadii - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（使法线从 A 指向 B）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点：取两个形状在法线方向上的支撑点，再取中点
        // 支撑点函数：给定方向 dir，返回盒子在该方向上最远的点（世界坐标）
        auto supportPoint = [&](const ShapeBox& box, const Transform& trans,
                                const Eigen::Vector3d& dir) -> Eigen::Vector3d
        {
            const Eigen::Vector3d half = box.getHalfSize();
            // 将方向变换到局部坐标系
            const Eigen::Vector3d localDir = trans.getRotation().conjugate() * dir;
            // 局部最远点的坐标
            Eigen::Vector3d localPoint;
            for (int i = 0; i < 3; ++i) {
                localPoint[i] = (localDir[i] >= 0) ? half[i] : -half[i];
            }
            // 变换回世界坐标
            return trans.localToWorld(localPoint);
        };

        const Eigen::Vector3d supportA = supportPoint(boxA, transA, bestAxis);
        const Eigen::Vector3d supportB = supportPoint(boxB, transB, -bestAxis);
        const Eigen::Vector3d contactPoint = (supportA + supportB) * 0.5;

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool box_first::collideBoxCylinder(const ShapeBox& box, const Transform& transA,
                                       const ShapeCylinder& cylinder, const Transform& transB,
                                       ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取圆柱的位置和轴向（世界坐标系）
        const Eigen::Vector3d cylCenter = transB.getTranslation();
        const Eigen::Vector3d cylAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 中心连线（从盒子指向圆柱）
        const Eigen::Vector3d t = cylCenter - boxCenter;

        // 收集所有潜在的分离轴
        std::vector<Eigen::Vector3d> axes;

        // 1. 盒子的三个面法线
        for (int i = 0; i < 3; ++i) {
            axes.push_back(boxAxes[i]);
        }

        // 2. 圆柱的轴向
        axes.push_back(cylAxis);

        // 3. 盒子的边与圆柱轴向的叉积
        for (int i = 0; i < 3; ++i) {
            Eigen::Vector3d cross = boxAxes[i].cross(cylAxis);
            if (cross.norm() > 1e-9) {
                axes.push_back(cross.normalized());
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 计算盒子在给定轴上的投影半长
        auto projectBoxHalfExtent = [&](const Eigen::Vector3d& axis) -> double
        {
            double extent = 0.0;
            for (int i = 0; i < 3; ++i) {
                extent += std::abs(axis.dot(boxAxes[i])) * boxHalfSize[i];
            }
            return extent;
        };

        // 计算圆柱在给定轴上的投影半长
        auto projectCylinderHalfExtent = [&](const Eigen::Vector3d& axis) -> double
        {
            const double axisDotCylAxis = std::abs(axis.dot(cylAxis));
            // 圆柱的投影 = 高度方向投影 + 半径方向投影
            return cylHalfHeight * axisDotCylAxis + cylRadius * std::sqrt(1.0 - axisDotCylAxis * axisDotCylAxis);
        };

        // SAT 测试
        for (const Eigen::Vector3d& axis : axes) {
            if (axis.squaredNorm() < 1e-18)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            const double boxExtent = projectBoxHalfExtent(unitAxis);
            const double cylExtent = projectCylinderHalfExtent(unitAxis);
            const double centerDist = std::abs(unitAxis.dot(t));

            if (centerDist > boxExtent + cylExtent + 1e-9) {
                return false; // 分离
            }

            const double overlap = boxExtent + cylExtent - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（从盒子指向圆柱）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点
        // 将圆柱中心投影到盒子坐标系，找到最近点
        const Eigen::Vector3d localCylCenter = transA.worldToLocal(cylCenter);
        Eigen::Vector3d closestLocalPoint;

        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localCylCenter[i], -boxHalfSize[i], boxHalfSize[i]);
        }

        const Eigen::Vector3d boxContact = transA.localToWorld(closestLocalPoint);

        // 圆柱上的接触点：沿法线方向内缩半径
        const Eigen::Vector3d cylContact = cylCenter - bestAxis * cylRadius;

        const Eigen::Vector3d contactPoint = (boxContact + cylContact) * 0.5;

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool box_first::collideBoxCylindricalShell(const ShapeBox& box, const Transform& transA,
                                               const ShapeCylindricalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同（空心圆柱与实心圆柱的碰撞检测逻辑一致）
        return collideBoxCylinder(box, transA, shell, transB, manifold);
    }

    bool box_first::collideBoxDisk(const ShapeBox& box, const Transform& transA,
                                   const ShapeDisk& disk, const Transform& transB,
                                   ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double diskRadius = disk.getRadius();

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取圆盘的位置和法向量（世界坐标系）
        const Eigen::Vector3d diskCenter = transB.getTranslation();
        const Eigen::Vector3d diskNormal = transB.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 将圆盘中心变换到盒子的局部坐标系
        const Eigen::Vector3d localDiskCenter = transA.worldToLocal(diskCenter);

        // 计算盒子边界
        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        // 在盒子内部找到离圆盘中心最近的点（clamp）
        Eigen::Vector3d closestLocalPoint;
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localDiskCenter[i], min[i], max[i]);
        }

        // 将最近点变换回世界坐标系
        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        // 计算从圆盘中心到最近点的向量
        const Eigen::Vector3d diff = closestWorldPoint - diskCenter;
        const double distSq = diff.squaredNorm();

        // 如果距离大于圆盘半径，则无碰撞
        if (distSq > diskRadius * diskRadius + 1e-9) {
            return false;
        }

        // 计算穿透深度
        const double dist = std::sqrt(distSq);
        const double penetration = diskRadius - dist;

        // 计算碰撞法线
        Eigen::Vector3d normal;
        if (dist > 1e-9) {
            normal = diff.normalized();
        }
        else {
            // 如果距离为零，使用圆盘法向量作为法线
            normal = diskNormal;
            // 确保法线指向正确方向（从盒子指向圆盘）
            if (normal.dot(diskCenter - boxCenter) < 0) {
                normal = -normal;
            }
        }

        // 计算接触点（圆盘上的接触点）
        const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool box_first::collideBoxRing(const ShapeBox& box, const Transform& transA,
                                   const ShapeRing& ring, const Transform& transB,
                                   ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideBoxDisk(box, transA, ring, transB, manifold);
    }

    bool box_first::collideBoxRod(const ShapeBox& box, const Transform& transA,
                                  const ShapeRod& rod, const Transform& transB,
                                  ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取杆的位置和方向（世界坐标系）
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的两个端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 找出杆上离盒子中心最近的点
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 将杆的端点变换到盒子局部坐标系
        const Eigen::Vector3d localEndA = transA.worldToLocal(rodEndA);
        const Eigen::Vector3d localEndB = transA.worldToLocal(rodEndB);
        const Eigen::Vector3d localCenter = transA.worldToLocal(rodCenter);

        // 计算盒子边界
        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        // 计算线段与 AABB 的最近点
        Eigen::Vector3d closestLocalPoint = localCenter;

        // 先找到线段在盒子内部的投影点
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localCenter[i], min[i], max[i]);
        }

        // 将最近点变换回世界坐标系
        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        // 计算从最近点到线段的最近点
        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = closestWorldPoint - rodEndA;

        const double c1 = w.dot(v);
        if (c1 <= 0) {
            // 最近点在端点 A
            if ((rodEndA - closestWorldPoint).squaredNorm() > 1e-9) {
                return false;
            }
        }

        double c2 = v.dot(v);
        if (c2 <= c1) {
            // 最近点在端点 B
            if ((rodEndB - closestWorldPoint).squaredNorm() > 1e-9) {
                return false;
            }
        }

        // 最近点在线段内部
        double t = c1 / c2;
        const Eigen::Vector3d closestOnRod = rodEndA + t * v;

        // 计算距离
        const Eigen::Vector3d diff = closestWorldPoint - closestOnRod;
        const double distSq = diff.squaredNorm();

        // 如果距离大于某个极小值，认为没有碰撞
        if (distSq > 1e-9) {
            return false;
        }

        // 计算法线（使用最近点指向杆中心的方向）
        Eigen::Vector3d normal = closestWorldPoint - rodCenter;
        if (normal.squaredNorm() < 1e-12) {
            normal = Eigen::Vector3d::UnitX();
            if (std::abs(rodAxis.dot(normal)) > 0.9) {
                normal = Eigen::Vector3d::UnitY();
            }
        }
        else {
            normal.normalize();
        }

        // 确保法线从盒子指向杆
        if (normal.dot(rodCenter - boxCenter) < 0) {
            normal = -normal;
        }

        // 接触点取中点
        const Eigen::Vector3d contactPoint = (closestWorldPoint + closestOnRod) * 0.5;

        manifold.push_back({contactPoint, normal, std::sqrt(distSq)});
        return true;
    }

    bool box_first::collideBoxSphere(const ShapeBox& box, const Transform& transA, const ShapeSphere& sphere,
                                     const Transform& transB, ContactManifold& manifold)
    {
        const double radius = sphere.getRadius();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();
        const Eigen::Vector3d half = box.getHalfSize();

        // 将球心变换到盒子的局部坐标系
        const Eigen::Vector3d localCenter = transA.worldToLocal(sphereCenter);

        // 计算局部 AABB 边界：[-half, half]
        const Eigen::Vector3d min = -half;
        const Eigen::Vector3d max = half;

        // 找到 AABB 上离球心最近的点（clamp）
        Eigen::Vector3d closestLocal;
        for (int i = 0; i < 3; ++i) {
            closestLocal[i] = std::clamp(localCenter[i], min[i], max[i]);
        }

        // 计算球心到最近点的距离平方
        const Eigen::Vector3d diff = localCenter - closestLocal;
        const double distSq = diff.squaredNorm();
        const double radiusSq = radius * radius;

        if (distSq >= radiusSq) {
            return false; // 无碰撞
        }

        const double dist = std::sqrt(distSq);
        const double penetration = radius - dist;

        // 法线方向：从最近点指向球心（世界坐标系）
        Eigen::Vector3d normalLocal = diff.normalized();
        // 如果球心恰好在中心（diff 为零向量），则取默认法线（例如向上）
        if (dist < 1e-9) {
            normalLocal = Eigen::Vector3d::UnitY();
        }
        const Eigen::Vector3d normalWorld = transA.getRotation() * normalLocal;

        // 接触点：球体表面上离长方体最近的点
        // 等价于 sphereCenter - normalWorld * radius
        // 为了与球体碰撞实现保持一致，取穿透深度的一半偏移
        const Eigen::Vector3d contactPoint = sphereCenter - normalWorld * (radius - penetration * 0.5);

        manifold.push_back({contactPoint, normalWorld, penetration});
        return true;
    }

    bool box_first::collideBoxSphericalShell(const ShapeBox& box, const Transform& transA,
                                             const ShapeSphericalShell& shell, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideBoxSphere(box, transA, shell, transB, manifold);
    }

    bool cylinder_first::collideCylinderCylinder(const ShapeCylinder& cylinderA, const Transform& transA,
                                                 const ShapeCylinder& cylinderB, const Transform& transB,
                                                 ContactManifold& manifold)
    {
        const double radiusA = cylinderA.getRadius();
        const double radiusB = cylinderB.getRadius();
        const double halfHeightA = cylinderA.getHalfHeight();
        const double halfHeightB = cylinderB.getHalfHeight();

        // 获取圆柱的位置和轴向
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d axisA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d axisB = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 圆柱的两个端点
        const Eigen::Vector3d endA1 = centerA + axisA * halfHeightA;
        const Eigen::Vector3d endA2 = centerA - axisA * halfHeightA;
        const Eigen::Vector3d endB1 = centerB + axisB * halfHeightB;
        const Eigen::Vector3d endB2 = centerB - axisB * halfHeightB;

        // 中心连线
        const Eigen::Vector3d t = centerB - centerA;

        // 潜在的分离轴
        std::vector<Eigen::Vector3d> axes;

        // 1. 圆柱A的轴向
        axes.push_back(axisA);

        // 2. 圆柱B的轴向
        axes.push_back(axisB);

        // 3. 两个轴向的叉积
        Eigen::Vector3d crossAB = axisA.cross(axisB);
        if (crossAB.norm() > 1e-9) {
            axes.push_back(crossAB.normalized());
        }

        // 4. 圆柱A端点到圆柱B轴线的垂线
        for (const Eigen::Vector3d& endA : {endA1, endA2}) {
            const Eigen::Vector3d diff = endA - centerB;
            const Eigen::Vector3d proj = (diff.dot(axisB)) * axisB;
            Eigen::Vector3d perp = diff - proj;
            if (perp.norm() > 1e-9) {
                axes.push_back(perp.normalized());
            }
        }

        // 5. 圆柱B端点到圆柱A轴线的垂线
        for (const Eigen::Vector3d& endB : {endB1, endB2}) {
            const Eigen::Vector3d diff = endB - centerA;
            const Eigen::Vector3d proj = (diff.dot(axisA)) * axisA;
            Eigen::Vector3d perp = diff - proj;
            if (perp.norm() > 1e-9) {
                axes.push_back(perp.normalized());
            }
        }

        // 6. 端点连线的方向（两个圆柱端面之间的潜在分离轴）
        for (const Eigen::Vector3d& endA : {endA1, endA2}) {
            for (const Eigen::Vector3d& endB : {endB1, endB2}) {
                Eigen::Vector3d line = endB - endA;
                if (line.norm() > 1e-9) {
                    axes.push_back(line.normalized());
                }
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 计算圆柱在给定轴上的投影半长
        auto projectCylinder = [](double radius, double halfHeight,
                                  const Eigen::Vector3d& axis,
                                  const Eigen::Vector3d& cylinderAxis) -> double
        {
            const double axisDot = std::abs(axis.dot(cylinderAxis));
            return halfHeight * axisDot + radius * std::sqrt(1.0 - axisDot * axisDot);
        };

        // SAT 测试
        for (const Eigen::Vector3d& axis : axes) {
            if (axis.squaredNorm() < 1e-18)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            const double extentA = projectCylinder(radiusA, halfHeightA, unitAxis, axisA);
            const double extentB = projectCylinder(radiusB, halfHeightB, unitAxis, axisB);
            const double centerDist = std::abs(unitAxis.dot(t));

            if (centerDist > extentA + extentB + 1e-9) {
                return false;
            }

            const double overlap = extentA + extentB - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（从A指向B）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点
        // 将圆柱B中心投影到圆柱A的轴线上，找到最近点
        const Eigen::Vector3d diffBA = centerB - centerA;
        const double projParam = diffBA.dot(axisA);
        const double clampedProj = std::clamp(projParam, -halfHeightA, halfHeightA);
        const Eigen::Vector3d closestOnAxisA = centerA + axisA * clampedProj;

        // 将圆柱A中心投影到圆柱B的轴线上，找到最近点
        const Eigen::Vector3d diffAB = centerA - centerB;
        const double projParamB = diffAB.dot(axisB);
        const double clampedProjB = std::clamp(projParamB, -halfHeightB, halfHeightB);
        const Eigen::Vector3d closestOnAxisB = centerB + axisB * clampedProjB;

        // 计算两个最近点之间的连线方向
        const Eigen::Vector3d lineAB = closestOnAxisB - closestOnAxisA;
        const double lineLength = lineAB.norm();

        Eigen::Vector3d contactPoint;
        if (lineLength > 1e-9) {
            // 接触点在连线上，向内偏移半径
            const Eigen::Vector3d lineDir = lineAB / lineLength;
            const Eigen::Vector3d pointOnA = closestOnAxisA + lineDir * radiusA;
            const Eigen::Vector3d pointOnB = closestOnAxisB - lineDir * radiusB;
            contactPoint = (pointOnA + pointOnB) * 0.5;
        }
        else {
            // 轴线相交或平行且重合，使用最佳轴方向
            contactPoint = closestOnAxisA + bestAxis * radiusA;
        }

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool cylinder_first::collideCylinderCylindricalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                                         const ShapeCylindricalShell& cyShell, const Transform& transB,
                                                         ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return collideCylinderCylinder(cylinder, transA, cyShell, transB, manifold);
    }

    bool cylinder_first::collideCylinderDisk(const ShapeCylinder& cylinder, const Transform& transA,
                                             const ShapeDisk& disk, const Transform& transB,
                                             ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double diskRadius = disk.getRadius();

        // 获取圆柱和圆盘的位置和方向
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d diskCenter = transB.getTranslation();
        const Eigen::Vector3d diskNormal = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 将圆盘中心投影到圆柱轴线上
        const Eigen::Vector3d diff = diskCenter - cylCenter;
        const double projLen = diff.dot(cylAxis);

        // 检查投影是否在圆柱高度范围内
        if (std::abs(projLen) > cylHalfHeight + 1e-9) {
            // 最近点在圆柱的一个端面上
            const Eigen::Vector3d closestOnCylAxis = cylCenter + cylAxis * std::clamp(
                projLen, -cylHalfHeight, cylHalfHeight);
            const Eigen::Vector3d closestDiff = diskCenter - closestOnCylAxis;
            const double distSq = closestDiff.squaredNorm();
            const double minDist = cylRadius + diskRadius;

            if (distSq > minDist * minDist + 1e-9) {
                return false;
            }

            const double dist = std::sqrt(distSq);
            const double penetration = minDist - dist;
            Eigen::Vector3d normal = closestDiff.normalized();

            // 确保法线从圆柱指向圆盘
            if (normal.dot(diskCenter - cylCenter) < 0) {
                normal = -normal;
            }

            const Eigen::Vector3d contactPoint = diskCenter - normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        // 最近点在圆柱侧面
        const Eigen::Vector3d closestOnCylAxis = cylCenter + cylAxis * projLen;
        const Eigen::Vector3d closestDiff = diskCenter - closestOnCylAxis;
        const double distSq = closestDiff.squaredNorm();

        // 计算垂直于圆柱轴线的距离
        const double radialDist = std::sqrt(distSq);

        if (radialDist > cylRadius + diskRadius + 1e-9) {
            return false;
        }

        const double penetration = cylRadius + diskRadius - radialDist;
        Eigen::Vector3d normal = closestDiff.normalized();

        if (normal.squaredNorm() < 1e-12) {
            // 如果距离为零，使用圆盘法线
            normal = diskNormal;
        }

        // 确保法线从圆柱指向圆盘
        if (normal.dot(diskCenter - cylCenter) < 0) {
            normal = -normal;
        }

        const Eigen::Vector3d contactPoint = diskCenter - normal * diskRadius;
        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool cylinder_first::collideCylinderRing(const ShapeCylinder& cylinder, const Transform& transA,
                                             const ShapeRing& ring, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideCylinderDisk(cylinder, transA, ring, transB, manifold);
    }

    bool cylinder_first::collideCylinderRod(const ShapeCylinder& cylinder, const Transform& transA,
                                            const ShapeRod& rod, const Transform& transB,
                                            ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取圆柱和杆的位置和方向
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 计算线段与圆柱的最近点
        const Eigen::Vector3d w = rodEndA - cylCenter;
        const Eigen::Vector3d v = rodEndB - rodEndA;

        const double a = v.dot(v);
        const double b = 2.0 * v.dot(w);
        const double c = w.dot(w) - cylRadius * cylRadius;

        // 检查线段是否与圆柱相交
        double t0, t1;
        const double discriminant = b * b - 4.0 * a * c;

        if (discriminant < 0) {
            // 线段不与圆柱相交，检查端点
            const double distA = (rodEndA - cylCenter).norm();
            const double distB = (rodEndB - cylCenter).norm();

            if (distA > cylRadius + 1e-9 && distB > cylRadius + 1e-9) {
                return false;
            }
        }
        else {
            // 线段与圆柱相交
            const double sqrtD = std::sqrt(discriminant);
            t0 = (-b - sqrtD) / (2.0 * a);
            t1 = (-b + sqrtD) / (2.0 * a);

            // 检查交点是否在线段上
            if (t0 >= 0.0 && t0 <= 1.0) {
                const Eigen::Vector3d intersectPoint = rodEndA + t0 * v;
                // 检查交点是否在圆柱高度范围内
                const double proj = (intersectPoint - cylCenter).dot(cylAxis);
                if (std::abs(proj) <= cylHalfHeight + 1e-9) {
                    Eigen::Vector3d normal = (intersectPoint - cylCenter).normalized();
                    if (normal.squaredNorm() < 1e-12) {
                        normal = Eigen::Vector3d::UnitX();
                    }
                    if (normal.dot(rodCenter - cylCenter) < 0) {
                        normal = -normal;
                    }
                    manifold.push_back({intersectPoint, normal, 0.0});
                    return true;
                }
            }
        }

        // 检查杆的端点是否在圆柱内部
        const Eigen::Vector3d diffA = rodEndA - cylCenter;
        const double projA = diffA.dot(cylAxis);
        const double radialDistA = std::sqrt(diffA.dot(diffA) - projA * projA);

        if (std::abs(projA) <= cylHalfHeight + 1e-9 && radialDistA <= cylRadius + 1e-9) {
            // 端点 A 在圆柱内部
            Eigen::Vector3d normal = diffA - cylAxis * projA;
            if (normal.squaredNorm() < 1e-12) {
                normal = Eigen::Vector3d::UnitX();
            }
            else {
                normal.normalize();
            }
            if (normal.dot(rodCenter - cylCenter) < 0) {
                normal = -normal;
            }
            const double penetration = cylRadius - radialDistA;
            const Eigen::Vector3d contactPoint = cylCenter + cylAxis * projA + normal * cylRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        const Eigen::Vector3d diffB = rodEndB - cylCenter;
        const double projB = diffB.dot(cylAxis);
        const double radialDistB = std::sqrt(diffB.dot(diffB) - projB * projB);

        if (std::abs(projB) <= cylHalfHeight + 1e-9 && radialDistB <= cylRadius + 1e-9) {
            // 端点 B 在圆柱内部
            Eigen::Vector3d normal = diffB - cylAxis * projB;
            if (normal.squaredNorm() < 1e-12) {
                normal = Eigen::Vector3d::UnitX();
            }
            else {
                normal.normalize();
            }
            if (normal.dot(rodCenter - cylCenter) < 0) {
                normal = -normal;
            }
            const double penetration = cylRadius - radialDistB;
            const Eigen::Vector3d contactPoint = cylCenter + cylAxis * projB + normal * cylRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        return false;
    }

    bool cylinder_first::collideCylinderSphere(const ShapeCylinder& cylinder, const Transform& transA,
                                               const ShapeSphere& sphere, const Transform& transB,
                                               ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double sphereRadius = sphere.getRadius();

        // 获取圆柱和球体的位置
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();

        // 将球心投影到圆柱轴线上
        const Eigen::Vector3d diff = sphereCenter - cylCenter;
        const double projLen = diff.dot(cylAxis);

        // 找到圆柱轴线上离球心最近的点
        const double clampedProj = std::clamp(projLen, -cylHalfHeight, cylHalfHeight);
        const Eigen::Vector3d closestOnAxis = cylCenter + cylAxis * clampedProj;

        // 计算球心到圆柱表面的距离
        const Eigen::Vector3d closestDiff = sphereCenter - closestOnAxis;
        const double distSq = closestDiff.squaredNorm();
        const double minDist = cylRadius + sphereRadius;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线
        Eigen::Vector3d normal = closestDiff.normalized();

        // 确保法线从圆柱指向球体
        if (normal.dot(sphereCenter - cylCenter) < 0) {
            normal = -normal;
        }

        // 计算接触点
        const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool cylinder_first::collideCylinderSphericalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                                       const ShapeSphericalShell& cyShell, const Transform& transB,
                                                       ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideCylinderSphere(cylinder, transA, cyShell, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellCylindricalShell(
        const ShapeCylindricalShell& cyShellA, const Transform& transA,
        const ShapeCylindricalShell& cyShellB, const Transform& transB,
        ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderCylinder(cyShellA, transA, cyShellB, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellDisk(const ShapeCylindricalShell& cyShell,
                                                              const Transform& transA,
                                                              const ShapeDisk& disk, const Transform& transB,
                                                              ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderDisk(cyShell, transA, disk, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellRing(const ShapeCylindricalShell& cyShell,
                                                              const Transform& transA,
                                                              const ShapeRing& ring, const Transform& transB,
                                                              ContactManifold& manifold)
    {
        // 圆环继承自圆盘
        return cylinder_first::collideCylinderDisk(cyShell, transA, ring, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellRod(const ShapeCylindricalShell& cyShell,
                                                             const Transform& transA,
                                                             const ShapeRod& rod, const Transform& transB,
                                                             ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderRod(cyShell, transA, rod, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellSphere(const ShapeCylindricalShell& cyShell,
                                                                const Transform& transA,
                                                                const ShapeSphere& sphere, const Transform& transB,
                                                                ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderSphere(cyShell, transA, sphere, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellSphericalShell(const ShapeCylindricalShell& cyShell,
                                                                        const Transform& transA,
                                                                        const ShapeSphericalShell& shell,
                                                                        const Transform& transB,
                                                                        ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return cylinder_first::collideCylinderSphere(cyShell, transA, shell, transB, manifold);
    }

    bool disk_first::collideDiskDisk(const ShapeDisk& diskA, const Transform& transA,
                                     const ShapeDisk& diskB, const Transform& transB,
                                     ContactManifold& manifold)
    {
        const double radiusA = diskA.getRadius();
        const double radiusB = diskB.getRadius();

        // 获取圆盘的位置和法向量
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d normalA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d normalB = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算中心距离
        const Eigen::Vector3d diff = centerB - centerA;
        const double distSq = diff.squaredNorm();
        const double minDist = radiusA + radiusB;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线（使用中心连线方向）
        Eigen::Vector3d normal = diff.normalized();

        // 如果距离为零，使用圆盘A的法线
        if (dist < 1e-9) {
            normal = normalA;
            // 确保法线方向正确
            if (normal.dot(diff) < 0) {
                normal = -normal;
            }
        }

        // 计算接触点
        const Eigen::Vector3d contactPoint = centerA + normal * radiusA;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskRing(const ShapeDisk& disk, const Transform& transA,
                                     const ShapeRing& ring, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideDiskDisk(disk, transA, ring, transB, manifold);
    }

    bool disk_first::collideDiskRod(const ShapeDisk& disk, const Transform& transA,
                                    const ShapeRod& rod, const Transform& transB,
                                    ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取圆盘和杆的位置和方向
        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d diskNormal = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 计算线段与圆盘的最近点
        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = diskCenter - rodEndA;

        double t = w.dot(v);
        if (t <= 0) {
            // 最近点在端点 A
            const Eigen::Vector3d diff = diskCenter - rodEndA;
            const double distSq = diff.squaredNorm();
            if (distSq > diskRadius * diskRadius + 1e-9) {
                return false;
            }
            const double dist = std::sqrt(distSq);
            const double penetration = diskRadius - dist;
            Eigen::Vector3d normal = diff.normalized();
            if (normal.squaredNorm() < 1e-12) {
                normal = diskNormal;
            }
            if (normal.dot(diskCenter - rodCenter) < 0) {
                normal = -normal;
            }
            const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        double lenSq = v.dot(v);
        if (t >= lenSq) {
            // 最近点在端点 B
            const Eigen::Vector3d diff = diskCenter - rodEndB;
            const double distSq = diff.squaredNorm();
            if (distSq > diskRadius * diskRadius + 1e-9) {
                return false;
            }
            const double dist = std::sqrt(distSq);
            const double penetration = diskRadius - dist;
            Eigen::Vector3d normal = diff.normalized();
            if (normal.squaredNorm() < 1e-12) {
                normal = diskNormal;
            }
            if (normal.dot(diskCenter - rodCenter) < 0) {
                normal = -normal;
            }
            const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        // 最近点在线段内部
        t = t / lenSq;
        const Eigen::Vector3d closestOnRod = rodEndA + t * v;
        const Eigen::Vector3d diff = diskCenter - closestOnRod;
        const double distSq = diff.squaredNorm();

        if (distSq > diskRadius * diskRadius + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = diskRadius - dist;
        Eigen::Vector3d normal = diff.normalized();

        if (normal.squaredNorm() < 1e-12) {
            normal = diskNormal;
        }

        if (normal.dot(diskCenter - rodCenter) < 0) {
            normal = -normal;
        }

        const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskSphere(const ShapeDisk& disk, const Transform& transA,
                                       const ShapeSphere& sphere, const Transform& transB,
                                       ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius();
        const double sphereRadius = sphere.getRadius();

        // 获取圆盘和球体的位置
        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();

        // 计算中心距离
        const Eigen::Vector3d diff = sphereCenter - diskCenter;
        const double distSq = diff.squaredNorm();
        const double minDist = diskRadius + sphereRadius;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线（使用中心连线方向）
        Eigen::Vector3d normal = diff.normalized();

        // 计算接触点（球体表面上的点）
        const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskSphericalShell(const ShapeDisk& disk, const Transform& transA,
                                               const ShapeSphericalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideDiskSphere(disk, transA, shell, transB, manifold);
    }

    bool ring_first::collideRingRing(const ShapeRing& ringA, const Transform& transA,
                                     const ShapeRing& ringB, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskDisk(ringA, transA, ringB, transB, manifold);
    }

    bool ring_first::collideRingRod(const ShapeRing& ring, const Transform& transA,
                                    const ShapeRod& rod, const Transform& transB,
                                    ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskRod(ring, transA, rod, transB, manifold);
    }

    bool ring_first::collideRingSphere(const ShapeRing& ring, const Transform& transA,
                                       const ShapeSphere& sphere, const Transform& transB,
                                       ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskSphere(ring, transA, sphere, transB, manifold);
    }

    bool ring_first::collideRingSphericalShell(const ShapeRing& ring, const Transform& transA,
                                               const ShapeSphericalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 圆环继承自圆盘，球壳继承自球
        return disk_first::collideDiskSphere(ring, transA, shell, transB, manifold);
    }

    bool rod_first::collideRodRod(const ShapeRod& rodA, const Transform& transA,
                                  const ShapeRod& rodB, const Transform& transB,
                                  ContactManifold& manifold)
    {
        auto [startA, endA]{rodA.getStartEnd(transA)};
        auto [startB, endB]{rodB.getStartEnd(transB)};
        constexpr double eps{1e-12};
        Eigen::Vector3d v = endA - startA; // 方向向量 A
        Eigen::Vector3d w = endB - startB; // 方向向量 B
        Eigen::Vector3d r = startB - startA; // 连接两起点的向量

        double v_len_sq = v.squaredNorm();
        double w_len_sq = w.squaredNorm();

        // 叉积
        Eigen::Vector3d cvw = v.cross(w);
        double cvw_len_sq = cvw.squaredNorm();

        Eigen::Vector3d outIntersection;

        // ---------- 1. 一般情况：v 与 w 不平行 ----------
        if (cvw_len_sq > eps) {
            // 异面检查：若 r · (v×w) ≠ 0，则两直线异面，不相交
            if (std::abs(r.dot(cvw)) > eps)
                return false;

            // 计算参数 t, u
            double t = r.cross(w).dot(cvw) / cvw_len_sq;
            double u = r.cross(v).dot(cvw) / cvw_len_sq;

            if (t >= -eps && t <= 1.0 + eps && u >= -eps && u <= 1.0 + eps) {
                // 将 t 限制到 [0,1] 区间（处理端点边界情况）
                t = std::clamp(t, 0.0, 1.0);
                outIntersection = startA + t * v;
                ContactPoint contactPoint{};
                contactPoint.position = outIntersection;
                contactPoint.normal = {cvw};
                contactPoint.penetration = 0;
                manifold.push_back(contactPoint);
                return true;
            }
            return false;
        }

        // ---------- 2. 平行或共线情况 ----------
        // 检查是否共线：r 与 v 的叉积是否为 0
        if (r.cross(v).squaredNorm() > eps)
            return false; // 平行但不共线

        // ---------- 退化情况：线段退化为点 ----------
        if (v_len_sq < eps) {
            // 线段1是一个点，判断该点是否在线段2上
            double t_on_B = (startA - startB).dot(w) / w_len_sq;
            if (t_on_B >= -eps && t_on_B <= 1.0 + eps) {
                // 检查该点到线段2直线的距离
                Eigen::Vector3d toStart = startA - startB;
                if (toStart.cross(w).squaredNorm() <= eps * w_len_sq) {
                    outIntersection = startA;
                    ContactPoint contactPoint{};
                    contactPoint.position = outIntersection;
                    contactPoint.normal = {cvw};
                    contactPoint.penetration = 0;
                    manifold.push_back(contactPoint);
                    return true;
                }
            }
            return false;
        }

        if (w_len_sq < eps) {
            // 线段2是一个点，判断该点是否在线段1上
            double t_on_A = (startB - startA).dot(v) / v_len_sq;
            if (t_on_A >= -eps && t_on_A <= 1.0 + eps) {
                Eigen::Vector3d toStart = startB - startA;
                if (toStart.cross(v).squaredNorm() <= eps * v_len_sq) {
                    outIntersection = startB;
                    ContactPoint contactPoint{};
                    contactPoint.position = outIntersection;
                    contactPoint.normal = {cvw};
                    contactPoint.penetration = 0;
                    manifold.push_back(contactPoint);
                    return true;
                }
            }
            return false;
        }

        // ---------- 一般共线（两线段在同一直线上） ----------
        // 将线段2的端点投影到线段1的方向上
        double s1 = r.dot(v) / v_len_sq;
        double s2 = (endB - startA).dot(v) / v_len_sq;
        double smin = std::min(s1, s2);
        double smax = std::max(s1, s2);

        double t_left = std::max(0.0, smin);
        double t_right = std::min(1.0, smax);

        if (t_left <= t_right + eps) {
            // 存在重叠部分，取中点作为交点（代表点）
            double t_mid = (t_left + t_right) * 0.5;
            outIntersection = startA + t_mid * v;
            ContactPoint contactPoint{};
            contactPoint.position = outIntersection;
            contactPoint.normal = {cvw};
            contactPoint.penetration = 0;
            manifold.push_back(contactPoint);
            return true;
        }
        return false;
        // const double halfLenA = rodA.getLength() * 0.5;
        // const double halfLenB = rodB.getLength() * 0.5;
        //
        // // 获取杆的位置和方向
        // const Eigen::Vector3d centerA = transA.getTranslation();
        // const Eigen::Vector3d centerB = transB.getTranslation();
        // const Eigen::Vector3d axisA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        // const Eigen::Vector3d axisB = transB.getRotation() * Eigen::Vector3d::UnitZ();
        //
        // // 计算杆的端点
        // const Eigen::Vector3d endA1 = centerA - axisA * halfLenA;
        // const Eigen::Vector3d endA2 = centerA + axisA * halfLenA;
        // const Eigen::Vector3d endB1 = centerB - axisB * halfLenB;
        // const Eigen::Vector3d endB2 = centerB + axisB * halfLenB;
        //
        // // 计算两条线段之间的最近点
        // const Eigen::Vector3d v = endA2 - endA1;
        // const Eigen::Vector3d w = endB2 - endB1;
        // const Eigen::Vector3d u = endA1 - endB1;
        //
        // const double a = v.dot(v);
        // const double b = v.dot(w);
        // const double c = w.dot(w);
        // const double d = v.dot(u);
        // const double e = w.dot(u);
        //
        // const double denom = a * c - b * b;
        //
        // double s, t;
        //
        // if (denom < 1e-9)
        // {
        //     // 线段平行，取端点距离
        //     s = 0;
        //     t = (b > c ? d / b : e / c);
        // }
        // else
        // {
        //     s = (b * e - c * d) / denom;
        //     t = (a * e - b * d) / denom;
        // }
        //
        // // 限制在 [0, 1] 范围内
        // s = std::clamp(s, 0.0, 1.0);
        // t = std::clamp(t, 0.0, 1.0);
        //
        // // 计算最近点
        // const Eigen::Vector3d p = endA1 + s * v;
        // const Eigen::Vector3d q = endB1 + t * w;
        //
        // const Eigen::Vector3d diff = p - q;
        // const double distSq = diff.squaredNorm();
        //
        // // 如果距离大于某个极小值，认为没有碰撞
        // if (distSq > 1e-9)
        // {
        //     return false;
        // }
        //
        // // 计算法线（使用两杆中心连线）
        // Eigen::Vector3d normal = centerB - centerA;
        // if (normal.squaredNorm() < 1e-12)
        // {
        //     normal = Eigen::Vector3d::UnitX();
        //     if (std::abs(axisA.dot(normal)) > 0.9)
        //     {
        //         normal = Eigen::Vector3d::UnitY();
        //     }
        // }
        // else
        // {
        //     normal.normalize();
        // }
        //
        // // 接触点取中点
        // const Eigen::Vector3d contactPoint = (p + q) * 0.5;
        //
        // manifold.push_back({contactPoint, normal, std::sqrt(distSq)});
        // return true;
    }

    bool rod_first::collideRodSphere(const ShapeRod& rod, const Transform& transA,
                                     const ShapeSphere& sphere, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // if (rod.getLength() <= 1e-8) {
        //     const double distance {transA.getTranslation().dot(transB.getTranslation())};
        //     if (distance <= sphere.getRadius()) {
        //         ContactPoint point;
        //         point.position = transA.getTranslation();
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // auto [start, end] = rod.getStartEnd(transA);
        // const Eigen::Vector3d sphereCenter = transB.getTranslation();
        // if ((sphereCenter - start).dot(end - start) <= 1e-12) {
        //     const double distance{(sphereCenter - start).norm()};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //
        //         const double a = (end - sphereCenter).norm();
        //         const double b = (start - sphereCenter).norm();
        //         const double c = rod.getLength();
        //         const double r = sphere.getRadius();
        //
        //         const Eigen::Vector3d elementDirection {(end - start).normalized()};
        //         const double factorA {(a * a + c * c - b * b) / (2 * c)};
        //         const double s {(a + b + c) / 2.0};
        //         const double factorB {sqrt(c * c * r * r - 16 * s * (s - a) * (s - b) * (s - c))/c};
        //         const double deltaLength{factorA - factorB};
        //
        //         ContactPoint point;
        //         point.position = start + deltaLength * elementDirection;
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // if ((sphereCenter - end).dot(start - end) <= 1e-12) {
        //     const double distance{(sphereCenter - end).norm()};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //
        //         const double a = (start - sphereCenter).norm();
        //         const double b = (end - sphereCenter).norm();
        //         const double c = rod.getLength();
        //         const double r = sphere.getRadius();
        //
        //         const Eigen::Vector3d elementDirection {(start - end).normalized()};
        //         const double factorA {(a * a + c * c - b * b) / (2 * c)};
        //         const double s {(a + b + c) / 2.0};
        //         const double factorB {sqrt(c * c * r * r - 16 * s * (s - a) * (s - b) * (s - c))/c};
        //         const double deltaLength{factorA - factorB};
        //
        //         ContactPoint point;
        //         point.position = end + deltaLength * elementDirection;
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        // }else {
        //     const double distance {abs((sphereCenter - start).cross(end - start).norm())};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //         ContactPoint point;
        //         point.position = transA.getTranslation();
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // 获取线段端点（世界坐标）和球心
        auto [start, end] = rod.getStartEnd(transA);
        const Eigen::Vector3d center = transB.getTranslation();
        const double radius = sphere.getRadius();

        // 线段方向向量及长度
        const Eigen::Vector3d dir = end - start;
        const double lenSq = dir.squaredNorm();
        const double len = std::sqrt(lenSq);

        // 退化：线段长度为点
        if (len < 1e-8) {
            const double dist = (start - center).norm();
            if (dist <= radius) {
                ContactPoint point{};
                point.position = start;
                point.normal = (start - center).normalized(); // 从球心指向接触点
                point.penetration = radius - dist;
                manifold.push_back(point);
                return true;
            }
            return false;
        }

        // 计算球心到线段所在直线的垂足参数 t
        const Eigen::Vector3d ac = start - center;
        const double t = -ac.dot(dir) / lenSq;
        // 这里直接用 -ac·dir 是因为 ac = start - center，原式 t = (center - start)·dir / lenSq
        // 等价于 t = (C-A)·(B-A) / |B-A|^2

        Eigen::Vector3d closestPoint;
        if (t <= 0.0) {
            closestPoint = start;
        }
        else if (t >= 1.0) {
            closestPoint = end;
        }
        else {
            closestPoint = start + t * dir;
        }

        const Eigen::Vector3d delta = closestPoint - center;
        const double dist = delta.norm();

        // 无碰撞
        if (dist > radius) return false;

        // 有碰撞，填充接触点
        ContactPoint point{};
        point.position = closestPoint;
        point.normal = delta.normalized(); // 从球心指向接触点
        point.penetration = radius - dist;
        manifold.push_back(point);
        return true;
        // const double rodHalfLength = rod.getLength() * 0.5;
        // const double sphereRadius = sphere.getRadius();
        //
        // // 获取杆和球体的位置
        // const Eigen::Vector3d rodCenter = transA.getTranslation();
        // const Eigen::Vector3d rodAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        // const Eigen::Vector3d sphereCenter = transB.getTranslation();
        //
        // // 计算杆的端点
        // const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        // const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;
        //
        // // 计算球心到线段的最近点
        // const Eigen::Vector3d v = rodEndB - rodEndA;
        // const Eigen::Vector3d w = sphereCenter - rodEndA;
        //
        // double c1 = w.dot(v);
        // if (c1 <= 0)
        // {
        //     // 最近点在端点 A
        //     const Eigen::Vector3d diff = sphereCenter - rodEndA;
        //     const double distSq = diff.squaredNorm();
        //     if (distSq > sphereRadius * sphereRadius + 1e-9)
        //     {
        //         return false;
        //     }
        //     const double dist = std::sqrt(distSq);
        //     const double penetration = sphereRadius - dist;
        //     Eigen::Vector3d normal = diff.normalized();
        //     if (normal.squaredNorm() < 1e-12)
        //     {
        //         normal = Eigen::Vector3d::UnitX();
        //     }
        //     if (normal.dot(sphereCenter - rodCenter) < 0)
        //     {
        //         normal = -normal;
        //     }
        //     const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;
        //     manifold.push_back({contactPoint, normal, penetration});
        //     return true;
        // }
        //
        // double c2 = v.dot(v);
        // if (c2 <= c1)
        // {
        //     // 最近点在端点 B
        //     const Eigen::Vector3d diff = sphereCenter - rodEndB;
        //     const double distSq = diff.squaredNorm();
        //     if (distSq > sphereRadius * sphereRadius + 1e-9)
        //     {
        //         return false;
        //     }
        //     const double dist = std::sqrt(distSq);
        //     const double penetration = sphereRadius - dist;
        //     Eigen::Vector3d normal = diff.normalized();
        //     if (normal.squaredNorm() < 1e-12)
        //     {
        //         normal = Eigen::Vector3d::UnitX();
        //     }
        //     if (normal.dot(sphereCenter - rodCenter) < 0)
        //     {
        //         normal = -normal;
        //     }
        //     const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;
        //     manifold.push_back({contactPoint, normal, penetration});
        //     return true;
        // }
        //
        // // 最近点在线段内部
        // double t = c1 / c2;
        // const Eigen::Vector3d closestOnRod = rodEndA + t * v;
        // const Eigen::Vector3d diff = sphereCenter - closestOnRod;
        // const double distSq = diff.squaredNorm();
        //
        // if (distSq > sphereRadius * sphereRadius + 1e-9)
        // {
        //     return false;
        // }
        //
        // const double dist = std::sqrt(distSq);
        // const double penetration = sphereRadius - dist;
        // Eigen::Vector3d normal = diff.normalized();
        //
        // if (normal.squaredNorm() < 1e-12)
        // {
        //     normal = Eigen::Vector3d::UnitX();
        // }
        //
        // if (normal.dot(sphereCenter - rodCenter) < 0)
        // {
        //     normal = -normal;
        // }
        //
        // const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;
        // manifold.push_back({contactPoint, normal, penetration});
        // return true;
        return false;
    }

    bool rod_first::collideRodSphericalShell(const ShapeRod& rod, const Transform& transA,
                                             const ShapeSphericalShell& shell, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideRodSphere(rod, transA, shell, transB, manifold);
    }

    bool sphere_first::collideSphereSphere(const ShapeSphere& sphereA, const Transform& transA,
                                           const ShapeSphere& sphereB,
                                           const Transform& transB, ContactManifold& manifold)
    {
        const Eigen::Vector3d& thisGeomCenter{transA.getTranslation()};
        const Eigen::Vector3d& otherGeomCenter{transB.getTranslation()};
        const double distance{(thisGeomCenter - otherGeomCenter).norm()};
        const double sumRadius{sphereA.getRadius() + sphereB.getRadius()};
        if (distance >= sumRadius) {
            return false;
        }

        Eigen::Vector3d normal = otherGeomCenter - thisGeomCenter;
        if (normal.squaredNorm() < 1e-12) {
            normal = Eigen::Vector3d::UnitX();
        }
        else {
            normal.normalize();
        }
        const double penetration{sumRadius - distance};

        const Eigen::Vector3d contactPoint{thisGeomCenter + normal * (sphereA.getRadius() - penetration * 0.5)};
        const ContactPoint pointInfo{
            .position = contactPoint, .normal = normal, .penetration = penetration
        };
        manifold.push_back(pointInfo);
        return true;
    }

    bool sphere_first::collideSphereSphericalShell(const ShapeSphere& sphere, const Transform& transA,
                                                   const ShapeSphericalShell& shell, const Transform& transB,
                                                   ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideSphereSphere(sphere, transA, shell, transB, manifold);
    }

    bool spherical_shell_first::collideSphericalShellSphericalShell(const ShapeSphericalShell& shellA,
                                                                    const Transform& transA,
                                                                    const ShapeSphericalShell& shellB,
                                                                    const Transform& transB,
                                                                    ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return sphere_first::collideSphereSphere(shellA, transA, shellB, transB, manifold);
    }
}
