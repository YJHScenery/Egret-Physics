//
// Created by jehor on 2026/5/21.
//
#include <optional>

#include "shape_base.h"
#include "shape_register.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "shape_sphere.h"
#include "shape_spherical_shell.h"
#include "ccd_collide_judge_group.h"
#include "physics_utils.h"

using namespace egret;

static bool registerContinuousStandardShape()
{
    auto& reg = ShapeRegister::instance();

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Box,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* boxA = dynamic_cast<const ShapeBox*>(shapeA);
                                    auto* boxB = dynamic_cast<const ShapeBox*>(shapeB);
                                    if (boxA != nullptr && boxB != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideBoxBox(*boxA, initTransA, linearVelA, angularVelA,
                                                                    *boxB, initTransB, linearVelB, angularVelB,
                                                                    dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxSphere(*box, initTransA, linearVelA, angularVelA,
                                                                           *sphere, initTransB, linearVelB, angularVelB,
                                                                           dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }

                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxSphere(*box, initTransB, linearVelB, angularVelB,
                                                                           *sphere, initTransA, linearVelA, angularVelA,
                                                                           dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }

                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Sphere, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* sphereA = dynamic_cast<const ShapeSphere*>(shapeA);
                                    auto* sphereB = dynamic_cast<const ShapeSphere*>(shapeB);
                                    if (sphereA != nullptr && sphereB != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideSphereSphere(*sphereA, initTransA, linearVelA, angularVelA,
                                                                          *sphereB, initTransB, linearVelB, angularVelB,
                                                                          dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Box 与 其他形状
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Cylinder,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxCylinder(*box, initTransA, linearVelA, angularVelA,
                                                                             *cyl, initTransB, linearVelB, angularVelB,
                                                                             dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxCylinder(*box, initTransB, linearVelB, angularVelB,
                                                                             *cyl, initTransA, linearVelA, angularVelA,
                                                                             dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::CylindricalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxCylindricalShell(*box, initTransA, linearVelA, angularVelA,
                                                                                     *shell, initTransB, linearVelB, angularVelB,
                                                                                     dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxCylindricalShell(*box, initTransB, linearVelB, angularVelB,
                                                                                     *shell, initTransA, linearVelA, angularVelA,
                                                                                     dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Disk,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxDisk(*box, initTransA, linearVelA, angularVelA,
                                                                         *disk, initTransB, linearVelB, angularVelB,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxDisk(*box, initTransB, linearVelB, angularVelB,
                                                                         *disk, initTransA, linearVelA, angularVelA,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Ring,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxRing(*box, initTransA, linearVelA, angularVelA,
                                                                         *ring, initTransB, linearVelB, angularVelB,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxRing(*box, initTransB, linearVelB, angularVelB,
                                                                         *ring, initTransA, linearVelA, angularVelA,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxRod(*box, initTransA, linearVelA, angularVelA,
                                                                        *rod, initTransB, linearVelB, angularVelB,
                                                                        dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxRod(*box, initTransB, linearVelB, angularVelB,
                                                                        *rod, initTransA, linearVelA, angularVelA,
                                                                        dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Box, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* box = dynamic_cast<const ShapeBox*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxSphericalShell(*box, initTransA, linearVelA, angularVelA,
                                                                                   *shell, initTransB, linearVelB, angularVelB,
                                                                                   dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* box = dynamic_cast<const ShapeBox*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideBoxSphericalShell(*box, initTransB, linearVelB, angularVelB,
                                                                                   *shell, initTransA, linearVelA, angularVelA,
                                                                                   dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Cylinder 开头的组合
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::Cylinder,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeCylinder*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeCylinder*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideCylinderCylinder(*a, initTransA, linearVelA, angularVelA,
                                                                              *b, initTransB, linearVelB, angularVelB,
                                                                              dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::CylindricalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderCylindricalShell(*cyl, initTransA, linearVelA, angularVelA,
                                                                                          *shell, initTransB, linearVelB, angularVelB,
                                                                                          dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderCylindricalShell(*cyl, initTransB, linearVelB, angularVelB,
                                                                                          *shell, initTransA, linearVelA, angularVelA,
                                                                                          dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::Disk,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderDisk(*cyl, initTransA, linearVelA, angularVelA,
                                                                              *disk, initTransB, linearVelB, angularVelB,
                                                                              dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderDisk(*cyl, initTransB, linearVelB, angularVelB,
                                                                              *disk, initTransA, linearVelA, angularVelA,
                                                                              dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::Ring,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderRing(*cyl, initTransA, linearVelA, angularVelA,
                                                                              *ring, initTransB, linearVelB, angularVelB,
                                                                              dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderRing(*cyl, initTransB, linearVelB, angularVelB,
                                                                              *ring, initTransA, linearVelA, angularVelA,
                                                                              dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderRod(*cyl, initTransA, linearVelA, angularVelA,
                                                                             *rod, initTransB, linearVelB, angularVelB,
                                                                             dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderRod(*cyl, initTransB, linearVelB, angularVelB,
                                                                             *rod, initTransA, linearVelA, angularVelA,
                                                                             dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderSphere(*cyl, initTransA, linearVelA, angularVelA,
                                                                                *sphere, initTransB, linearVelB, angularVelB,
                                                                                dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderSphere(*cyl, initTransB, linearVelB, angularVelB,
                                                                                *sphere, initTransA, linearVelA, angularVelA,
                                                                                dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Cylinder, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderSphericalShell(*cyl, initTransA, linearVelA, angularVelA,
                                                                                         *shell, initTransB, linearVelB, angularVelB,
                                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* cyl = dynamic_cast<const ShapeCylinder*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylinderSphericalShell(*cyl, initTransB, linearVelB, angularVelB,
                                                                                         *shell, initTransA, linearVelA, angularVelA,
                                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Cylindrical shell 开头的组合
    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::CylindricalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeCylindricalShell*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeCylindricalShell*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideCylindricalShellCylindricalShell(*a, initTransA, linearVelA, angularVelA,
                                                                                             *b, initTransB, linearVelB, angularVelB,
                                                                                             dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::Disk,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellDisk(*shell, initTransA, linearVelA, angularVelA,
                                                                                      *disk, initTransB, linearVelB, angularVelB,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellDisk(*shell, initTransB, linearVelB, angularVelB,
                                                                                      *disk, initTransA, linearVelA, angularVelA,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::Ring,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellRing(*shell, initTransA, linearVelA, angularVelA,
                                                                                      *ring, initTransB, linearVelB, angularVelB,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellRing(*shell, initTransB, linearVelB, angularVelB,
                                                                                      *ring, initTransA, linearVelA, angularVelA,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellRod(*shell, initTransA, linearVelA, angularVelA,
                                                                                     *rod, initTransB, linearVelB, angularVelB,
                                                                                     dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellRod(*shell, initTransB, linearVelB, angularVelB,
                                                                                     *rod, initTransA, linearVelA, angularVelA,
                                                                                     dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellSphere(*shell, initTransA, linearVelA, angularVelA,
                                                                                        *sphere, initTransB, linearVelB, angularVelB,
                                                                                        dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellSphere(*shell, initTransB, linearVelB, angularVelB,
                                                                                        *sphere, initTransA, linearVelA, angularVelA,
                                                                                        dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::CylindricalShell, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* shellA = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* shellB = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellSphericalShell(*shellA, initTransA, linearVelA, angularVelA,
                                                                                                 *shellB, initTransB, linearVelB, angularVelB,
                                                                                                 dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shellB = dynamic_cast<const ShapeCylindricalShell*>(shapeA)) {
                                        if (auto* shellA = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideCylindricalShellSphericalShell(*shellB, initTransB, linearVelB, angularVelB,
                                                                                                 *shellA, initTransA, linearVelA, angularVelA,
                                                                                                 dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Disk 开头
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Disk, (std::uint32_t)ShapeID::Disk,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeDisk*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeDisk*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideDiskDisk(*a, initTransA, linearVelA, angularVelA,
                                                                      *b, initTransB, linearVelB, angularVelB,
                                                                      dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Disk, (std::uint32_t)ShapeID::Ring,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskRing(*disk, initTransA, linearVelA, angularVelA,
                                                                          *ring, initTransB, linearVelB, angularVelB,
                                                                          dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskRing(*disk, initTransB, linearVelB, angularVelB,
                                                                          *ring, initTransA, linearVelA, angularVelA,
                                                                          dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Disk, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskRod(*disk, initTransA, linearVelA, angularVelA,
                                                                         *rod, initTransB, linearVelB, angularVelB,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskRod(*disk, initTransB, linearVelB, angularVelB,
                                                                         *rod, initTransA, linearVelA, angularVelA,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Disk, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskSphere(*disk, initTransA, linearVelA, angularVelA,
                                                                            *sphere, initTransB, linearVelB, angularVelB,
                                                                            dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskSphere(*disk, initTransB, linearVelB, angularVelB,
                                                                            *sphere, initTransA, linearVelA, angularVelA,
                                                                            dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Disk, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskSphericalShell(*disk, initTransA, linearVelA, angularVelA,
                                                                                    *shell, initTransB, linearVelB, angularVelB,
                                                                                    dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* disk = dynamic_cast<const ShapeDisk*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideDiskSphericalShell(*disk, initTransB, linearVelB, angularVelB,
                                                                                    *shell, initTransA, linearVelA, angularVelA,
                                                                                    dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Ring 开头
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Ring, (std::uint32_t)ShapeID::Ring,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeRing*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeRing*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideRingRing(*a, initTransA, linearVelA, angularVelA,
                                                                      *b, initTransB, linearVelB, angularVelB,
                                                                      dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Ring, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingRod(*ring, initTransA, linearVelA, angularVelA,
                                                                         *rod, initTransB, linearVelB, angularVelB,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingRod(*ring, initTransB, linearVelB, angularVelB,
                                                                         *rod, initTransA, linearVelA, angularVelA,
                                                                         dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Ring, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingSphere(*ring, initTransA, linearVelA, angularVelA,
                                                                            *sphere, initTransB, linearVelB, angularVelB,
                                                                            dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingSphere(*ring, initTransB, linearVelB, angularVelB,
                                                                            *sphere, initTransA, linearVelA, angularVelA,
                                                                            dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Ring, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* ring = dynamic_cast<const ShapeRing*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingSphericalShell(*ring, initTransA, linearVelA, angularVelA,
                                                                                    *shell, initTransB, linearVelB, angularVelB,
                                                                                    dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* ring = dynamic_cast<const ShapeRing*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRingSphericalShell(*ring, initTransB, linearVelB, angularVelB,
                                                                                    *shell, initTransA, linearVelA, angularVelA,
                                                                                    dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Rod 开头
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Rod, (std::uint32_t)ShapeID::Rod,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeRod*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeRod*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideRodRod(*a, initTransA, linearVelA, angularVelA,
                                                                    *b, initTransB, linearVelB, angularVelB,
                                                                    dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Rod, (std::uint32_t)ShapeID::Sphere,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRodSphere(*rod, initTransA, linearVelA, angularVelA,
                                                                           *sphere, initTransB, linearVelB, angularVelB,
                                                                           dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRodSphere(*rod, initTransB, linearVelB, angularVelB,
                                                                           *sphere, initTransA, linearVelA, angularVelA,
                                                                           dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::Rod, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* rod = dynamic_cast<const ShapeRod*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRodSphericalShell(*rod, initTransA, linearVelA, angularVelA,
                                                                                   *shell, initTransB, linearVelB, angularVelB,
                                                                                   dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* rod = dynamic_cast<const ShapeRod*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideRodSphericalShell(*rod, initTransB, linearVelB, angularVelB,
                                                                                   *shell, initTransA, linearVelA, angularVelA,
                                                                                   dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    // Sphere 与 Spherical shell 的组合
    reg.registerContinuousJudge((std::uint32_t)ShapeID::Sphere, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeA)) {
                                        if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideSphereSphericalShell(*sphere, initTransA, linearVelA, angularVelA,
                                                                                      *shell, initTransB, linearVelB, angularVelB,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(shapeA)) {
                                        if (auto* sphere = dynamic_cast<const ShapeSphere*>(shapeB)) {
                                            double collisionTime = 0.0;
                                            if (continuousCollideSphereSphericalShell(*sphere, initTransB, linearVelB, angularVelB,
                                                                                      *shell, initTransA, linearVelA, angularVelA,
                                                                                      dt, collisionTime, manifold)) {
                                                return collisionTime;
                                            }
                                        }
                                    }
                                    return std::nullopt;
                                });

    reg.registerContinuousJudge((std::uint32_t)ShapeID::SphericalShell, (std::uint32_t)ShapeID::SphericalShell,
                                [](const ShapeBase* shapeA, const Transform& initTransA,
                                   const Eigen::Vector3d& linearVelA, const Eigen::Vector3d& angularVelA,
                                   const ShapeBase* shapeB, const Transform& initTransB,
                                   const Eigen::Vector3d& linearVelB, const Eigen::Vector3d& angularVelB,
                                   double dt, ContactManifold& manifold) -> std::optional<double>
                                {
                                    auto* a = dynamic_cast<const ShapeSphericalShell*>(shapeA);
                                    auto* b = dynamic_cast<const ShapeSphericalShell*>(shapeB);
                                    if (a != nullptr && b != nullptr) {
                                        double collisionTime = 0.0;
                                        if (continuousCollideSphericalShellSphericalShell(*a, initTransA, linearVelA, angularVelA,
                                                                                          *b, initTransB, linearVelB, angularVelB,
                                                                                          dt, collisionTime, manifold)) {
                                            return collisionTime;
                                        }
                                    }
                                    return std::nullopt;
                                });

    return true;
}

bool continuous_dummy = registerContinuousStandardShape();
