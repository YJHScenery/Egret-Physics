//
// Created by jehor on 2026/5/6.
//

#ifndef EGRET_PHYSICS_STANDARD_COLLIDE_JUDGE_GROUP_H
#define EGRET_PHYSICS_STANDARD_COLLIDE_JUDGE_GROUP_H
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

    inline namespace box_first
    {
        // 已实现
        bool collideBoxBox(const ShapeBox& boxA, const Transform& transA,
                                     const ShapeBox& boxB, const Transform& transB,
                                     ContactManifold& manifold);

        bool collideBoxCylinder(const ShapeBox& box, const Transform& transA,
                                         const ShapeCylinder& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideBoxCylindricalShell(const ShapeBox& box, const Transform& transA,
                                         const ShapeCylindricalShell& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideBoxDisk(const ShapeBox& box, const Transform& transA,
                                         const ShapeDisk& disk, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideBoxRing(const ShapeBox& box, const Transform& transA,
                                         const ShapeRing& ring, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideBoxRod(const ShapeBox& box, const Transform& transA,
                                         const ShapeRod& rod, const Transform& transB,
                                         ContactManifold& manifold);


        // 已实现
        bool collideBoxSphere(const ShapeBox& box, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideBoxSphericalShell(const ShapeBox& box, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace cylinder_first
    {
        bool collideCylinderCylinder(const ShapeCylinder& cylinderA, const Transform& transA,
                                 const ShapeCylinder& cylinderB, const Transform& transB,
                                 ContactManifold& manifold);


        bool collideCylinderCylindricalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                         const ShapeCylindricalShell& cyShell, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylinderDisk(const ShapeCylinder& cylinder, const Transform& transA,
                                         const ShapeDisk& disk, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylinderRing(const ShapeCylinder& cylinder, const Transform& transA,
                                         const ShapeRing& ring, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylinderRod(const ShapeCylinder& cylinder, const Transform& transA,
                                         const ShapeRod& rod, const Transform& transB,
                                         ContactManifold& manifold);


        bool collideCylinderSphere(const ShapeCylinder& cylinder, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylinderSphericalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                     const ShapeSphericalShell& cyShell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace cylindrical_shell_first
    {
        bool collideCylindricalShellCylindricalShell(const ShapeCylindricalShell& cyShellA, const Transform& transA,
                                 const ShapeCylindricalShell& cyShellB, const Transform& transB,
                                 ContactManifold& manifold);

        bool collideCylindricalShellDisk(const ShapeCylindricalShell& cyShell, const Transform& transA,
                                         const ShapeDisk& disk, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylindricalShellRing(const ShapeCylindricalShell& cyShell, const Transform& transA,
                                         const ShapeRing& ring, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylindricalShellRod(const ShapeCylindricalShell& cyShell, const Transform& transA,
                                         const ShapeRod& rod, const Transform& transB,
                                         ContactManifold& manifold);


        bool collideCylindricalShellSphere(const ShapeCylindricalShell& cyShell, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideCylindricalShellSphericalShell(const ShapeCylindricalShell& cyShell, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace disk_first
    {
        bool collideDiskDisk(const ShapeDisk& diskA, const Transform& transA,
                                 const ShapeDisk& diskB, const Transform& transB,
                                 ContactManifold& manifold);

        bool collideDiskRing(const ShapeDisk& disk, const Transform& transA,
                                         const ShapeRing& ring, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideDiskRod(const ShapeDisk& disk, const Transform& transA,
                                         const ShapeRod& rod, const Transform& transB,
                                         ContactManifold& manifold);


        bool collideDiskSphere(const ShapeDisk& disk, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideDiskSphericalShell(const ShapeDisk& disk, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace ring_first
    {
        bool collideRingRing(const ShapeRing& ringA, const Transform& transA,
                                 const ShapeRing& ringB, const Transform& transB,
                                 ContactManifold& manifold);

        bool collideRingRod(const ShapeRing& ring, const Transform& transA,
                                         const ShapeRod& rod, const Transform& transB,
                                         ContactManifold& manifold);


        bool collideRingSphere(const ShapeRing& ring, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideRingSphericalShell(const ShapeRing& ring, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace rod_first
    {
        bool collideRodRod(const ShapeRod& rodA, const Transform& transA,
                                 const ShapeRod& rodB, const Transform& transB,
                                 ContactManifold& manifold);


        bool collideRodSphere(const ShapeRod& rod, const Transform& transA,
                                         const ShapeSphere& sphere, const Transform& transB,
                                         ContactManifold& manifold);

        bool collideRodSphericalShell(const ShapeRod& rod, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace sphere_first
    {
        bool collideSphereSphere(const ShapeSphere& sphereA, const Transform& transA,
                                 const ShapeSphere& sphereB, const Transform& transB,
                                 ContactManifold& manifold);

        bool collideSphereSphericalShell(const ShapeSphere& sphere, const Transform& transA,
                                     const ShapeSphericalShell& shell, const Transform& transB,
                                     ContactManifold& manifold);
    }

    inline namespace spherical_shell_first
    {
        bool collideSphericalShellSphericalShell(const ShapeSphericalShell& shellA, const Transform& transA,
                             const ShapeSphericalShell& shellB, const Transform& transB,
                             ContactManifold& manifold);
    }
}
#endif //EGRET_PHYSICS_STANDARD_COLLIDE_JUDGE_GROUP_H
