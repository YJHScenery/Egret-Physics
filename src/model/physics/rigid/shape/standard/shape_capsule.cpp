/**
 * @file        shape_capsule.cpp
 * @brief       胶囊体刚体形状实现文件。
 * @details     实现 ShapeCapsule 类的各项成员函数，
 *              包括体积、质心、转动惯量、AABB、支撑函数及序列化。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-20
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "shape_capsule.h"
#include "constants.h"
#include "world_scene_manager.h"

namespace egret
{

    //  构造 / 参数修改

    ShapeCapsule::ShapeCapsule(const double radius, const double height)
        : m_radius(radius), m_height(height)
    {
        if (radius < 1e-12 || height < 1e-12) {
            setValid(false);
        }
    }

    void ShapeCapsule::setRadius(const double radius)
    {
        m_radius = radius;
        if (m_radius < 1e-12) {
            setValid(false);
        }
    }

    void ShapeCapsule::setHeight(const double height)
    {
        m_height = height;
        if (m_height < 1e-12) {
            setValid(false);
        }
    }

    //  类型标识

    ShapeType ShapeCapsule::getType() const
    {
        return ShapeType::Capsule;
    }

    //  体积

    double ShapeCapsule::getVolume() const
    {
        // V = V_cylinder + V_sphere
        //   = π r² h + 4/3 π r³
        //   = π r² (h + 4r/3)
        return quickSquare(m_radius) * (m_height + 4.0 / 3.0 * m_radius) * PI;
    }

    //  质心（原点对称）

    Eigen::Vector3d ShapeCapsule::getCenterOfMass() const
    {
        return {0.0, 0.0, 0.0};
    }

    //  转动惯量
    //  将胶囊体分解为中央圆柱 + 两端半球，应用平行轴定理。
    //  设总质量为 M，圆柱段质量 m_c，两端半球总质量 m_s：
    //    m_c = M · h / (h + 4r/3)
    //    m_s = M · (4r/3) / (h + 4r/3)
    //
    //  圆柱（关于自身质心）：
    //    I_c_xx = m_c · (h²/12 + r²/4)
    //    I_c_zz = m_c · r²/2
    //
    //  单个半球（关于自身质心）：
    //    I_h_xx = (83/320) · m_h · r²    （垂直于对称轴）
    //    I_h_zz = (2/5)   · m_h · r²    （平行于对称轴）
    //  半球质心与胶囊体质心的距离：d = h/2 + 3r/8
    //
    //  应用平行轴定理：
    //    I_xx = I_c_xx + 2·[I_h_xx + m_h·d²]
    //    I_zz = I_c_zz + 2·I_h_zz

    Eigen::Matrix3d ShapeCapsule::getInertiaTensor(const double mass) const
    {
        const double r  = m_radius;
        const double h  = m_height;
        const double r2 = quickSquare(r);
        const double h2 = quickSquare(h);

        // 体积比例因子
        const double denom = h + 4.0 / 3.0 * r;   // h + 4r/3
        const double m_c   = mass * h / denom;     // 圆柱段质量
        const double m_s   = mass * (4.0 / 3.0 * r) / denom; // 两半球总质量

        // 圆柱段惯量（关于胶囊体质心）
        const double i_c_xx = m_c * (h2 / 12.0 + r2 / 4.0);
        const double i_c_zz = m_c * r2 / 2.0;

        // 两半球惯量
        // 半球质心到胶囊体质心的距离
        const double d  = h / 2.0 + 3.0 / 8.0 * r;
        const double d2 = quickSquare(d);

        // 两个半球对 xx/yy 轴的贡献（平行轴定理）
        const double i_hems_xx = m_s * (83.0 / 320.0 * r2 + d2);
        // 两个半球对 zz 轴的贡献（平行轴不影响 z 轴，位移沿 z 方向）
        const double i_hems_zz = 2.0 / 5.0 * m_s * r2;

        Eigen::Matrix3d inertiaTensor = Eigen::Matrix3d::Zero();
        inertiaTensor(0, 0) = i_c_xx + i_hems_xx;   // I_xx
        inertiaTensor(1, 1) = i_c_xx + i_hems_xx;   // I_yy = I_xx（关于 z 轴对称）
        inertiaTensor(2, 2) = i_c_zz + i_hems_zz;   // I_zz
        return inertiaTensor;
    }

    //  世界空间 AABB
    //  胶囊体是球体⊕线段的 Minkowski 和，沿方向 v 的投影范围为：
    //    r · |v| + (h/2) · |v_z|
    //  对每个世界轴分别计算此投影即得 AABB 半边长。

    AABB ShapeCapsule::getAABB(const Transform &transform) const
    {
        const std::function<double(double)> squareD{
            [](const double x) { return x * x; }
        };

        const Eigen::Vector3d &sVec = transform.getScale();
        const Eigen::Vector3d  tVec = transform.getTranslation();
        const double halfH = m_height * 0.5;

        std::array<double, 3> bias{};
        for (int i = 0; i < 3; ++i) {
            // 世界轴 i 在局部坐标系中的方向（考虑缩放）
            const Eigen::Vector3d uVec = transform.getRotationMatrix().row(i);
            const double vx = sVec.x() * uVec.x();
            const double vy = sVec.y() * uVec.y();
            const double vz = sVec.z() * uVec.z();

            // extent = r · |v| + (h/2) · |v_z|
            bias[i] = m_radius * std::sqrt(squareD(vx) + squareD(vy) + squareD(vz))
                      + halfH * std::abs(vz);
        }

        const Eigen::Vector3d minVec(tVec.x() - bias[0],
                                     tVec.y() - bias[1],
                                     tVec.z() - bias[2]);
        const Eigen::Vector3d maxVec(tVec.x() + bias[0],
                                     tVec.y() + bias[1],
                                     tVec.z() + bias[2]);
        return AABB{minVec, maxVec};
    }

    //  序列化信息

    ShapeLoadInfo ShapeCapsule::getLoadInfo() const
    {
        ShapeLoadInfo info{};
        info.typeId = static_cast<std::uint32_t>(getType());
        info.numberParams["radius"] = std::vector<double>{m_radius};
        info.numberParams["height"] = std::vector<double>{m_height};
        return info;
    }

    //  GJK 支撑函数
    //  胶囊体 = 球体(半径 r) ⊕ z 轴线段 [−h/2, h/2]
    //  Minkowski 和的支撑函数为各分量支撑函数之和：
    //    x*(d) = r · d̂ + (0, 0, (h/2)·sgn(d_z))
    //  其中 d̂ = d / ‖d‖ 为归一化方向。
    //  胶囊体不存在不可导位置，处处光滑。

    Eigen::Vector3d ShapeCapsule::support(const Eigen::Vector3d &direction,
                                          const Transform &transform) const
    {
        // 1. 将方向从世界坐标转换到局部坐标（仅旋转，无需平移）
        Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;

        // 2. 归一化（避免数值问题）
        const double len = localDir.norm();
        if (len < 1e-12) {
            localDir = Eigen::Vector3d(0.0, 0.0, 1.0);
        } else {
            localDir /= len;
        }

        // 3. 在局部坐标系中计算支撑点
        //    x*(d) = (r·d_x, r·d_y, r·d_z + (h/2)·sgn(d_z))
        const double dz = localDir.z();
        const double sgn_dz = (dz > 0.0) ? 1.0 : ((dz < 0.0) ? -1.0 : 0.0);

        Eigen::Vector3d localSupport;
        localSupport.x() = m_radius * localDir.x();
        localSupport.y() = m_radius * localDir.y();
        localSupport.z() = m_radius * localDir.z() + m_height * 0.5 * sgn_dz;

        // 4. 变换到世界坐标
        return transform.getRotation() * localSupport + transform.getTranslation();
    }

} // egret
