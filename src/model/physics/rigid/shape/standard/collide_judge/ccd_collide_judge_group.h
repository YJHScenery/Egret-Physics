/**
* @file        filename.h
 * @brief       一句话概述文件的主要功能
 * @details     详细描述文件的用途、设计思路、使用注意事项等。
 *              可以写多行，说明模块在整个系统中的位置。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        创建日期 (如 2025-03-15)
 * @version     版本号 (如 1.0.0)
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     许可证 (如 GPL, MIT, BSD 等)
 *
 * @ingroup     所属模块组 (可选，用于分组)
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_CCD_COLLIDE_JUDGE_GROUP_H
#define EGRET_PHYSICS_CCD_COLLIDE_JUDGE_GROUP_H
#include "physics_utils.h"
#include "transform.h"
#include "constants.h"

namespace egret
{
    class ShapeBase;
    class ShapeRegister;
    class ShapeBox;
    class ShapeCylinder;
    class ShapeCylindricalShell;
    class ShapeDisk;
    class ShapeRing;
    class ShapeRod;
    class ShapeSphere;
    class ShapeSphericalShell;

    inline namespace ccd_box_first
    {
        bool continuousCollideBoxBox(
            const ShapeBox& boxA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeBox& boxB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxCylinder(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeCylinder& cylinder, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxCylindricalShell(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeCylindricalShell& cyShell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxDisk(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeDisk& disk, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxRing(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRing& ring, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxRod(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rod, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxSphere(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideBoxSphericalShell(
            const ShapeBox& box, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_cylinder_first
    {
        bool continuousCollideCylinderCylinder(
            const ShapeCylinder& cylinderA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeCylinder& cylinderB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderCylindricalShell(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeCylindricalShell& cyShell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderDisk(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeDisk& disk, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderRing(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRing& ring, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderRod(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rod, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderSphere(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylinderSphericalShell(
            const ShapeCylinder& cylinder, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_cylindrical_shell_first
    {
        bool continuousCollideCylindricalShellCylindricalShell(
            const ShapeCylindricalShell& cyShellA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeCylindricalShell& cyShellB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylindricalShellDisk(
            const ShapeCylindricalShell& cyShell, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeDisk& disk, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylindricalShellRing(
            const ShapeCylindricalShell& cyShell, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRing& ring, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylindricalShellRod(
            const ShapeCylindricalShell& cyShell, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rod, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylindricalShellSphere(
            const ShapeCylindricalShell& cyShell, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideCylindricalShellSphericalShell(
            const ShapeCylindricalShell& cyShell, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_disk_first
    {
        bool continuousCollideDiskDisk(
            const ShapeDisk& diskA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeDisk& diskB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideDiskRing(
            const ShapeDisk& disk, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRing& ring, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideDiskRod(
            const ShapeDisk& disk, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rod, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideDiskSphere(
            const ShapeDisk& disk, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideDiskSphericalShell(
            const ShapeDisk& disk, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_ring_first
    {
        bool continuousCollideRingRing(
            const ShapeRing& ringA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRing& ringB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideRingRod(
            const ShapeRing& ring, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rod, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideRingSphere(
            const ShapeRing& ring, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideRingSphericalShell(
            const ShapeRing& ring, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_rod_first
    {
        bool continuousCollideRodRod(
            const ShapeRod& rodA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeRod& rodB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideRodSphere(
            const ShapeRod& rod, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphere, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideRodSphericalShell(
            const ShapeRod& rod, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_sphere_first
    {
        bool continuousCollideSphereSphere(
            const ShapeSphere& sphereA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphere& sphereB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);

        bool continuousCollideSphereSphericalShell(
            const ShapeSphere& sphere, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shell, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }

    inline namespace ccd_spherical_shell_first
    {
        bool continuousCollideSphericalShellSphericalShell(
            const ShapeSphericalShell& shellA, const Transform& initTransA,
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const ShapeSphericalShell& shellB, const Transform& initTransB,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            double dt,
            double& collisionTime,
            ContactManifold& manifold);
    }
}
#endif // EGRET_PHYSICS_CCD_COLLIDE_JUDGE_GROUP_H
