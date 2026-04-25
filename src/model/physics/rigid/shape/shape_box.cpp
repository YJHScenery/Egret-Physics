//
// Created by jehor on 2026/4/24.
//

#include "shape_box.h"

#include "shape_sphere.h"

namespace egret
{
    ShapeBox::ShapeBox(Eigen::Vector3d size) : m_size(std::move(size))
    {
    }

    ShapeBox::ShapeBox(const double length, const double width, const double height) : m_size(length, width, height)
    {
    }

    void ShapeBox::setSize(Eigen::Vector3d size)
    {
        m_size = std::move(size);
    }

    void ShapeBox::setSize(const double length, const double width, const double height)
    {
        m_size = Eigen::Vector3d(length, width, height);
    }

    Eigen::Vector3d ShapeBox::getHalfSize() const
    {
        return m_size * 0.5;
    }

    Eigen::Vector3d ShapeBox::getSize() const
    {
        return m_size;
    }

    double ShapeBox::getLength() const
    {
        return m_size.x();
    }

    double ShapeBox::getWidth() const
    {
        return m_size.y();
    }

    double ShapeBox::getHeight() const
    {
        return m_size.z();
    }

    double ShapeBox::getVolume() const
    {
        return m_size.x() * m_size.y() * m_size.z();
    }

    Eigen::Vector3d ShapeBox::getCenterOfMass() const
    {
        return {0, 0, 0};
    }

    Eigen::Matrix3d ShapeBox::getInertiaTensor(double mass) const
    {
        constexpr double coefficient{1.0 / 12.0};
        const double i_xx{coefficient * mass * (pow(m_size.y(), 2) + pow(m_size.z(), 2))};
        const double i_yy{coefficient * mass * (pow(m_size.x(), 2) + pow(m_size.z(), 2))};
        const double i_zz{coefficient * mass * (pow(m_size.x(), 2) + pow(m_size.y(), 2))};
        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    bool ShapeBox::collideWith(const ShapeBase* other, const Transform& thisTrans, const Transform& otherTrans,
                               ContactManifold& manifold) const
    {
        // 尝试转为球体
        const auto* sphere = dynamic_cast<const ShapeSphere*>(other);
        if (sphere != nullptr) {
            return collideBoxSphere(*this, thisTrans, *sphere, otherTrans, manifold);
        }

        // 尝试转为长方体
        const auto* box = dynamic_cast<const ShapeBox*>(other);
        if (box != nullptr) {
            return collideBoxBox(*this, thisTrans, *box, otherTrans, manifold);
        }

        // 其他形状暂不支持
        return false;
    }

    AABB ShapeBox::getAABB(const Transform& transform) const
    {
        // 局部顶点（中心在原点）
        const double length{m_size.x()};
        const double width{m_size.y()};
        const double height{m_size.z()};
        
        const std::vector<Eigen::Vector3d> localVertices = {
            {length / 2, width / 2, height / 2}, {length / 2, width / 2, -height / 2},
            {length / 2, -width / 2, height / 2}, {length / 2, -width / 2, -height / 2},
            {-length / 2, width / 2, height / 2}, {-length / 2, width / 2, -height / 2},
            {-length / 2, -width / 2, height / 2}, {-length / 2, -width / 2, -height / 2}
        };

        Eigen::Vector3d min_pt = Eigen::Vector3d::Constant(std::numeric_limits<double>::max());
        Eigen::Vector3d max_pt = Eigen::Vector3d::Constant(-std::numeric_limits<double>::max());

        for (const auto& v_local : localVertices) {
            // 使用 localToWorld 矩阵变换
            Eigen::Vector4d v_local_homogeneous(v_local.x(), v_local.y(), v_local.z(), 1.0);
            Eigen::Vector4d v_world_homogeneous = transform.getLocalToWorldMatrix() * v_local_homogeneous;
            Eigen::Vector3d v_world = v_world_homogeneous.head<3>();

            min_pt = min_pt.cwiseMin(v_world);
            max_pt = max_pt.cwiseMax(v_world);
        }

        AABB result;
        result.min = min_pt;
        result.max = max_pt;
        return result;
    }

    void ShapeBox::getLocalAxes(const Transform& trans, Eigen::Vector3d& axisX, Eigen::Vector3d& axisY,
                                Eigen::Vector3d& axisZ)
    {
        axisX = trans.getRotation() * Eigen::Vector3d::UnitX();
        axisY = trans.getRotation() * Eigen::Vector3d::UnitY();
        axisZ = trans.getRotation() * Eigen::Vector3d::UnitZ();
    }

    bool ShapeBox::collideBoxSphere(const ShapeBox& box, const Transform& boxTrans, const ShapeSphere& sphere,
                                    const Transform& sphereTrans, ContactManifold& manifold)
    {
        const double radius = sphere.getRadius();
        const Eigen::Vector3d sphereCenter = sphereTrans.getTranslation();
        const Eigen::Vector3d half = box.getHalfSize();

        // 将球心变换到盒子的局部坐标系
        const Eigen::Vector3d localCenter = boxTrans.worldToLocal(sphereCenter);

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
        const Eigen::Vector3d normalWorld = boxTrans.getRotation() * normalLocal;

        // 接触点：球体表面上离长方体最近的点
        // 等价于 sphereCenter - normalWorld * radius
        // 为了与球体碰撞实现保持一致，取穿透深度的一半偏移
        const Eigen::Vector3d contactPoint = sphereCenter - normalWorld * (radius - penetration * 0.5);

        manifold.push_back({contactPoint, normalWorld, penetration});
        return true;
    }

    bool ShapeBox::collideBoxBox(const ShapeBox& boxA, const Transform& transA, const ShapeBox& boxB,
                                 const Transform& transB, ContactManifold& manifold)
    {
        const Eigen::Vector3d halfA = boxA.getHalfSize();
        const Eigen::Vector3d halfB = boxB.getHalfSize();

        // 获取两个物体的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> axesA, axesB;
        getLocalAxes(transA, axesA[0], axesA[1], axesA[2]);
        getLocalAxes(transB, axesB[0], axesB[1], axesB[2]);

        // 中心连线（从 A 指向 B）
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d t = centerB - centerA;

        // 待测试的轴列表（所有分离轴）
        std::vector<Eigen::Vector3d> axes;
        // 添加 A 的三个面法线
        for (int i = 0; i < 3; ++i) axes.push_back(axesA[i]);
        // 添加 B 的三个面法线
        for (int i = 0; i < 3; ++i) axes.push_back(axesB[i]);
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
            if (axis.squaredNorm() < 1e-9) continue;

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
} // egret
