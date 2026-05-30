/**
* @file        standard_register_dummy.cpp
 * @brief       标准注册器占位文件
 * @details     标准形状碰撞检测函数的注册占位文件。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#include <array>

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
#include "standard_collide_judge_group.h"
#include "physics_utils.h"

using namespace egret;

static bool registerStandardShape()
{
    auto& reg = ShapeRegister::instance();
    
    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Box,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* boxA = dynamic_cast<const ShapeBox*>(ShapeA);
                          auto* boxB = dynamic_cast<const ShapeBox*>(ShapeB);
                          if (boxA != nullptr && boxB != nullptr) {
                              return collideBoxBox(*boxA, transA, *boxB, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideBoxSphere(*box, transA, *sphere, transB, manifold);
                              }
                          }

                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxSphere(*box, transB, *sphere, transA, manifold);
                              }
                          }

                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Sphere, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* sphereA = dynamic_cast<const ShapeSphere*>(ShapeA);
                          auto* sphereB = dynamic_cast<const ShapeSphere*>(ShapeB);
                          if (sphereA != nullptr && sphereB != nullptr) {
                              return collideSphereSphere(*sphereA, transA, *sphereB, transB, manifold);
                          }
                          return false;
                      });

    // Box 与 其他形状
    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Cylinder,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideBoxCylinder(*box, transA, *cyl, transB, manifold);
                              }
                          }
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxCylinder(*box, transB, *cyl, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::CylindricalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideBoxCylindricalShell(*box, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxCylindricalShell(*box, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Disk,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideBoxDisk(*box, transA, *disk, transB, manifold);
                              }
                          }
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxDisk(*box, transB, *disk, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Ring,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideBoxRing(*box, transA, *ring, transB, manifold);
                              }
                          }
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxRing(*box, transB, *ring, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideBoxRod(*box, transA, *rod, transB, manifold);
                              }
                          }
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxRod(*box, transB, *rod, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Box, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* box = dynamic_cast<const ShapeBox*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideBoxSphericalShell(*box, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* box = dynamic_cast<const ShapeBox*>(ShapeB)) {
                                  return collideBoxSphericalShell(*box, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Cylinder 开头的组合
    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::Cylinder,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeCylinder*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeCylinder*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideCylinderCylinder(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::CylindricalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideCylinderCylindricalShell(*cyl, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderCylindricalShell(*cyl, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::Disk,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideCylinderDisk(*cyl, transA, *disk, transB, manifold);
                              }
                          }
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderDisk(*cyl, transB, *disk, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::Ring,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideCylinderRing(*cyl, transA, *ring, transB, manifold);
                              }
                          }
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderRing(*cyl, transB, *ring, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideCylinderRod(*cyl, transA, *rod, transB, manifold);
                              }
                          }
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderRod(*cyl, transB, *rod, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideCylinderSphere(*cyl, transA, *sphere, transB, manifold);
                              }
                          }
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderSphere(*cyl, transB, *sphere, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Cylinder, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideCylinderSphericalShell(*cyl, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* cyl = dynamic_cast<const ShapeCylinder*>(ShapeB)) {
                                  return collideCylinderSphericalShell(*cyl, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Cylindrical shell 开头的组合
    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::CylindricalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeCylindricalShell*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeCylindricalShell*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideCylindricalShellCylindricalShell(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::Disk,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideCylindricalShellDisk(*shell, transA, *disk, transB, manifold);
                              }
                          }
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideCylindricalShellDisk(*shell, transB, *disk, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::Ring,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideCylindricalShellRing(*shell, transA, *ring, transB, manifold);
                              }
                          }
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideCylindricalShellRing(*shell, transB, *ring, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideCylindricalShellRod(*shell, transA, *rod, transB, manifold);
                              }
                          }
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideCylindricalShellRod(*shell, transB, *rod, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideCylindricalShellSphere(*shell, transA, *sphere, transB, manifold);
                              }
                          }
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeCylindricalShell*>(ShapeB)) {
                                  return collideCylindricalShellSphere(*shell, transB, *sphere, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::CylindricalShell, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* shellA = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* shellB = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideCylindricalShellSphericalShell(*shellA, transA, *shellB, transB, manifold);
                              }
                          }
                          if (auto* shellB = dynamic_cast<const ShapeCylindricalShell*>(ShapeA)) {
                              if (auto* shellA = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideCylindricalShellSphericalShell(*shellB, transB, *shellA, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Disk 开头
    reg.registerJudge((std::uint32_t)ShapeType::Disk, (std::uint32_t)ShapeType::Disk,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeDisk*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeDisk*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideDiskDisk(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Disk, (std::uint32_t)ShapeType::Ring,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideDiskRing(*disk, transA, *ring, transB, manifold);
                              }
                          }
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideDiskRing(*disk, transB, *ring, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Disk, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideDiskRod(*disk, transA, *rod, transB, manifold);
                              }
                          }
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideDiskRod(*disk, transB, *rod, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Disk, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideDiskSphere(*disk, transA, *sphere, transB, manifold);
                              }
                          }
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideDiskSphere(*disk, transB, *sphere, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Disk, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideDiskSphericalShell(*disk, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* disk = dynamic_cast<const ShapeDisk*>(ShapeB)) {
                                  return collideDiskSphericalShell(*disk, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Ring 开头
    reg.registerJudge((std::uint32_t)ShapeType::Ring, (std::uint32_t)ShapeType::Ring,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeRing*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeRing*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideRingRing(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Ring, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideRingRod(*ring, transA, *rod, transB, manifold);
                              }
                          }
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideRingRod(*ring, transB, *rod, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Ring, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideRingSphere(*ring, transA, *sphere, transB, manifold);
                              }
                          }
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideRingSphere(*ring, transB, *sphere, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Ring, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideRingSphericalShell(*ring, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* ring = dynamic_cast<const ShapeRing*>(ShapeB)) {
                                  return collideRingSphericalShell(*ring, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Rod 开头
    reg.registerJudge((std::uint32_t)ShapeType::Rod, (std::uint32_t)ShapeType::Rod,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeRod*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeRod*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideRodRod(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Rod, (std::uint32_t)ShapeType::Sphere,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideRodSphere(*rod, transA, *sphere, transB, manifold);
                              }
                          }
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideRodSphere(*rod, transB, *sphere, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::Rod, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideRodSphericalShell(*rod, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* rod = dynamic_cast<const ShapeRod*>(ShapeB)) {
                                  return collideRodSphericalShell(*rod, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    // Sphere 与 Spherical shell 的组合（spherical_shell - spherical_shell）
    reg.registerJudge((std::uint32_t)ShapeType::Sphere, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeA)) {
                              if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeB)) {
                                  return collideSphereSphericalShell(*sphere, transA, *shell, transB, manifold);
                              }
                          }
                          if (auto* shell = dynamic_cast<const ShapeSphericalShell*>(ShapeA)) {
                              if (auto* sphere = dynamic_cast<const ShapeSphere*>(ShapeB)) {
                                  return collideSphereSphericalShell(*sphere, transB, *shell, transA, manifold);
                              }
                          }
                          return false;
                      });

    reg.registerJudge((std::uint32_t)ShapeType::SphericalShell, (std::uint32_t)ShapeType::SphericalShell,
                      [](const ShapeBase* ShapeA, const Transform& transA, const ShapeBase* ShapeB, const Transform& transB,
                         ContactManifold& manifold)
                      {
                          auto* a = dynamic_cast<const ShapeSphericalShell*>(ShapeA);
                          auto* b = dynamic_cast<const ShapeSphericalShell*>(ShapeB);
                          if (a != nullptr && b != nullptr) {
                              return collideSphericalShellSphericalShell(*a, transA, *b, transB, manifold);
                          }
                          return false;
                      });

    return true;
}


bool dummy = registerStandardShape();