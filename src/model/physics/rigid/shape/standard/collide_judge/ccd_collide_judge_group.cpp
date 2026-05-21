//
// Created by jehor on 2026/5/21.
//

#include "ccd_collide_judge_group.h"
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
    bool ccd_box_first::continuousCollideBoxBox(
        const ShapeBox& boxA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeBox& boxB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxCylinder(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeCylinder& cylinder, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxCylindricalShell(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeCylindricalShell& cyShell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxDisk(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeDisk& disk, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxRing(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRing& ring, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxRod(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rod, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxSphere(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_box_first::continuousCollideBoxSphericalShell(
        const ShapeBox& box, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderCylinder(
        const ShapeCylinder& cylinderA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeCylinder& cylinderB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderCylindricalShell(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeCylindricalShell& cyShell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderDisk(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeDisk& disk, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderRing(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRing& ring, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderRod(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rod, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderSphere(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylinder_first::continuousCollideCylinderSphericalShell(
        const ShapeCylinder& cylinder, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellCylindricalShell(
        const ShapeCylindricalShell& cyShellA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeCylindricalShell& cyShellB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellDisk(
        const ShapeCylindricalShell& cyShell, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeDisk& disk, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellRing(
        const ShapeCylindricalShell& cyShell, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRing& ring, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellRod(
        const ShapeCylindricalShell& cyShell, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rod, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellSphere(
        const ShapeCylindricalShell& cyShell, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_cylindrical_shell_first::continuousCollideCylindricalShellSphericalShell(
        const ShapeCylindricalShell& cyShell, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_disk_first::continuousCollideDiskDisk(
        const ShapeDisk& diskA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeDisk& diskB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_disk_first::continuousCollideDiskRing(
        const ShapeDisk& disk, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRing& ring, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_disk_first::continuousCollideDiskRod(
        const ShapeDisk& disk, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rod, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_disk_first::continuousCollideDiskSphere(
        const ShapeDisk& disk, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_disk_first::continuousCollideDiskSphericalShell(
        const ShapeDisk& disk, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_ring_first::continuousCollideRingRing(
        const ShapeRing& ringA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRing& ringB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_ring_first::continuousCollideRingRod(
        const ShapeRing& ring, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rod, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_ring_first::continuousCollideRingSphere(
        const ShapeRing& ring, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_ring_first::continuousCollideRingSphericalShell(
        const ShapeRing& ring, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_rod_first::continuousCollideRodRod(
        const ShapeRod& rodA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeRod& rodB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_rod_first::continuousCollideRodSphere(
        const ShapeRod& rod, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphere, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_rod_first::continuousCollideRodSphericalShell(
        const ShapeRod& rod, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_sphere_first::continuousCollideSphereSphere(
        const ShapeSphere& sphereA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphere& sphereB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_sphere_first::continuousCollideSphereSphericalShell(
        const ShapeSphere& sphere, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shell, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

    bool ccd_spherical_shell_first::continuousCollideSphericalShellSphericalShell(
        const ShapeSphericalShell& shellA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,
        const ShapeSphericalShell& shellB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        double& collisionTime,
        ContactManifold& manifold)
    {
        collisionTime = 0.0;
        return false;
    }

}