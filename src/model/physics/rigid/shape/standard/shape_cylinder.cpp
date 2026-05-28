//
// Created by jehor on 2026/4/24.
//

#include "shape_cylinder.h"
#include "constants.h"
#include "world_scene_manager.h"

namespace egret
{
    ShapeCylinder::ShapeCylinder(double radius, double height): m_radius(radius), m_height(height)
    {
    }

    ShapeID ShapeCylinder::typeId() const
    {
        return ShapeID::Cylinder;
    }

    double ShapeCylinder::getVolume() const
    {
        return m_height * quickSquare(m_radius) * PI;
    }

    Eigen::Vector3d ShapeCylinder::getCenterOfMass() const
    {
        return {0.0, 0.0, 0.0};
    }

    Eigen::Matrix3d ShapeCylinder::getInertiaTensor(double mass) const
    {
        const double i_xx{1.0 / 12.0 * (3 * quickSquare(m_radius) + quickSquare(m_height)) * mass};
        const double i_yy{i_xx};

        const double i_zz{0.5 * quickSquare(m_radius) * mass};

        Eigen::Matrix3d inertiaTensor;
        inertiaTensor(0, 0) = i_xx;
        inertiaTensor(1, 1) = i_yy;
        inertiaTensor(2, 2) = i_zz;
        return inertiaTensor;
    }

    AABB ShapeCylinder::getAABB(const Transform& transform) const
    {
        // Eigen::Matrix3d rotationMat{};
        const std::function<double(double)> squareD{[](const double x) { return x * x; }};

        // const Eigen::Quaterniond& rQuat{transform.getRotation()};
        // const double x{rQuat.x()};
        // const double y{rQuat.y()};
        // const double z{rQuat.z()};
        // const double w{rQuat.w()};

        const Eigen::Vector3d& sVec{transform.getScale()};

        const Eigen::Vector3d tVec{transform.getTranslation()};

        // rotationMat << 1.0 - 2.0 * (squareD(y) + squareD(z)), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y),
        //     2.0 * (x * y + w * z), 1.0 - 2.0 * (squareD(x) + squareD(z)), 2.0 * (y * z - w * x),
        //     2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (squareD(x) + squareD(y));

        std::array<double, 3> bias{};
        for (int i = 0; i < 3; i++) {
            const Eigen::Vector3d uVec{transform.getRotationMatrix().row(i)};
            bias[i] = m_radius * sqrt(
                squareD(sVec.x() * uVec.x()) + squareD(sVec.y() * uVec.y())
            ) + m_height / 2.0 * std::abs(sVec.z() * uVec.z());
        }


        const Eigen::Vector3d minVec{tVec.x() - bias[0], tVec.y() - bias[1], tVec.z() - bias[2]};
        const Eigen::Vector3d maxVec{tVec.x() + bias[0], tVec.y() + bias[1], tVec.z() + bias[2]};
        return AABB{minVec, maxVec};
    }

    SceneRenderItem ShapeCylinder::getBasicRenderInfo(const Eigen::Vector3d& position) const
    {
        SceneRenderItem item{};
        const double diameter = m_radius * 2.0;
        item.kind = (std::uint32_t)this->typeId();
        item.width = diameter;
        item.height = diameter;
        item.x = position.x() - m_radius;
        item.y = position.y() - m_radius;
        return item;
    }

    Eigen::Vector3d ShapeCylinder::support(const Eigen::Vector3d& direction, const Transform& transform) const
    {
        // 将方向从世界坐标系变换到局部坐标系（注意：只考虑旋转部分，不考虑平移和缩放）
        // 因为方向向量不需要平移，缩放会影响方向，通常需要逆缩放或归一化处理
        Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;

        // 归一化方向（避免数值问题）
        const double len = localDir.norm();
        if (len < 1e-12) {
            localDir = Eigen::Vector3d(0, 0, 1);
        } else {
            localDir /= len;
        }

        // 在局部坐标系中计算圆柱体的支持点
        // 圆柱体：中心在原点，半径 m_radius，高度 m_height（沿z轴从 -h/2 到 h/2）
        Eigen::Vector3d localSupport;

        double halfHeight = m_height * 0.5;

        // 检查方向是否主要指向圆柱的端面还是侧面
        double absZ = std::abs(localDir.z());

        if (absZ > 1e-12) {
            // 计算沿z轴方向的支持点（顶面或底面）
            double zSign = localDir.z() > 0 ? 1.0 : -1.0;
            double zCoord = zSign * halfHeight;

            // 计算水平方向（x-y平面）的投影
            Eigen::Vector2d xyDir(localDir.x(), localDir.y());
            double xyLen = xyDir.norm();

            if (xyLen > 1e-12) {
                // 有水平分量，支持点在圆柱边缘（圆侧面与端面的交线）
                xyDir /= xyLen;
                localSupport = Eigen::Vector3d(xyDir.x() * m_radius, xyDir.y() * m_radius, zCoord);
            } else {
                // 纯垂直方向，支持点在端面中心
                localSupport = Eigen::Vector3d(0, 0, zCoord);
            }
        } else {
            // 方向几乎水平，支持点在圆柱侧面
            Eigen::Vector2d xyDir(localDir.x(), localDir.y());
            double xyLen = xyDir.norm();
            if (xyLen > 1e-12) {
                xyDir /= xyLen;
                // 侧面支持点：z坐标需要取能最大化投影的值，即 ±h/2 取决于方向在z上的微小分量
                // 由于 absZ 很小，取顶面或底面都可，但为保证凸包正确，取方向z符号对应的端面
                double zCoord = (localDir.z() >= 0 ? halfHeight : -halfHeight);
                localSupport = Eigen::Vector3d(xyDir.x() * m_radius, xyDir.y() * m_radius, zCoord);
            } else {
                // 退化情况
                localSupport = Eigen::Vector3d(m_radius, 0, halfHeight);
            }
        }

        // 将局部支持点变换到世界坐标（包含平移、旋转、缩放）
        return transform.transformPointToWorld(localSupport);
    }

    void ShapeCylinder::setRadius(const double radius)
    {
        m_radius = radius;
    }

    void ShapeCylinder::setHeight(const double height)
    {
        m_height = height;
    }

} // egret
