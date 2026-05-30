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

#define BOOST_TEST_MODULE CollisionTests
#include "boost_test_dependency.h"
#include "model/physics/rigid/shape/standard/shape_box.h"
#include "model/physics/rigid/shape/standard/shape_cylinder.h"
#include "model/physics/rigid/shape/standard/shape_cylindrical_shell.h"
#include "model/physics/rigid/shape/standard/shape_disk.h"
#include "model/physics/rigid/shape/standard/shape_ring.h"
#include "model/physics/rigid/shape/standard/shape_rod.h"
#include "model/physics/rigid/shape/standard/shape_sphere.h"
#include "model/physics/rigid/shape/standard/shape_spherical_shell.h"
#include "model/physics/rigid/shape/standard/collide_judge/standard_collide_judge_group.h"

using namespace egret;
using namespace egret::box_first;
using namespace egret::cylinder_first;
using namespace egret::cylindrical_shell_first;
using namespace egret::disk_first;
using namespace egret::ring_first;
using namespace egret::rod_first;
using namespace egret::sphere_first;
using namespace egret::spherical_shell_first;

Transform createTransform(const Eigen::Vector3d& pos, const Eigen::Vector3d& rot = Eigen::Vector3d::Zero()) {
    Transform t;
    t.setTranslation(pos);
    Eigen::Quaterniond q;
    q = Eigen::AngleAxisd(rot(0), Eigen::Vector3d::UnitX())
      * Eigen::AngleAxisd(rot(1), Eigen::Vector3d::UnitY())
      * Eigen::AngleAxisd(rot(2), Eigen::Vector3d::UnitZ());
    t.setRotation(q);
    return t;
}

BOOST_AUTO_TEST_CASE(test_box_box_collision_overlapping) {
    ShapeBox boxA(Eigen::Vector3d(1, 1, 1));
    ShapeBox boxB(Eigen::Vector3d(1, 1, 1));
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxBox(boxA, transA, boxB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
    BOOST_CHECK(manifold[0].penetration > 0);
}

BOOST_AUTO_TEST_CASE(test_box_box_collision_separated) {
    ShapeBox boxA(Eigen::Vector3d(1, 1, 1));
    ShapeBox boxB(Eigen::Vector3d(1, 1, 1));
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxBox(boxA, transA, boxB, transB, manifold);
    
    BOOST_CHECK(!result);
    BOOST_CHECK(manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_box_sphere_collision_overlapping) {
    ShapeBox box(Eigen::Vector3d(1, 1, 1));
    ShapeSphere sphere(0.8);
    
    Transform transBox = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxSphere(box, transBox, sphere, transSphere, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_box_sphere_collision_separated) {
    ShapeBox box(Eigen::Vector3d(1, 1, 1));
    ShapeSphere sphere(0.5);
    
    Transform transBox = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxSphere(box, transBox, sphere, transSphere, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_sphere_sphere_collision_overlapping) {
    ShapeSphere sphereA(1.0);
    ShapeSphere sphereB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideSphereSphere(sphereA, transA, sphereB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(manifold.size(), 1);
    BOOST_CHECK(manifold[0].penetration > 0);
}

BOOST_AUTO_TEST_CASE(test_sphere_sphere_collision_separated) {
    ShapeSphere sphereA(1.0);
    ShapeSphere sphereB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideSphereSphere(sphereA, transA, sphereB, transB, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_cylinder_cylinder_collision_overlapping) {
    ShapeCylinder cylinderA(1.0, 2.0);
    ShapeCylinder cylinderB(1.0, 2.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideCylinderCylinder(cylinderA, transA, cylinderB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_cylinder_cylinder_collision_separated) {
    ShapeCylinder cylinderA(1.0, 2.0);
    ShapeCylinder cylinderB(1.0, 2.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(4, 0, 0));
    
    ContactManifold manifold;
    bool result = collideCylinderCylinder(cylinderA, transA, cylinderB, transB, manifold);

    // std::cout << "Result: " << result << std::endl;
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_cylinder_sphere_collision_overlapping) {
    ShapeCylinder cylinder(1.0, 2.0);
    ShapeSphere sphere(0.8);
    
    Transform transCylinder = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideCylinderSphere(cylinder, transCylinder, sphere, transSphere, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_cylinder_sphere_collision_separated) {
    ShapeCylinder cylinder(1.0, 2.0);
    ShapeSphere sphere(0.5);
    
    Transform transCylinder = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(4, 0, 0));
    
    ContactManifold manifold;
    bool result = collideCylinderSphere(cylinder, transCylinder, sphere, transSphere, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_disk_disk_collision_overlapping) {
    ShapeDisk diskA(1.0);
    ShapeDisk diskB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideDiskDisk(diskA, transA, diskB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_disk_disk_collision_separated) {
    ShapeDisk diskA(1.0);
    ShapeDisk diskB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideDiskDisk(diskA, transA, diskB, transB, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_disk_sphere_collision_overlapping) {
    ShapeDisk disk(1.0);
    ShapeSphere sphere(0.8);
    
    Transform transDisk = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideDiskSphere(disk, transDisk, sphere, transSphere, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_disk_sphere_collision_separated) {
    ShapeDisk disk(1.0);
    ShapeSphere sphere(0.5);
    
    Transform transDisk = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideDiskSphere(disk, transDisk, sphere, transSphere, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_rod_sphere_collision_overlapping) {
    ShapeRod rod(2.0);
    ShapeSphere sphere(0.8);
    
    Transform transRod = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(0, 0, 0.5));
    
    ContactManifold manifold;
    bool result = collideRodSphere(rod, transRod, sphere, transSphere, manifold);

    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_rod_sphere_collision_separated) {
    ShapeRod rod(2.0);
    ShapeSphere sphere(0.5);
    
    Transform transRod = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(3, 0, 0));
    
    ContactManifold manifold;
    bool result = collideRodSphere(rod, transRod, sphere, transSphere, manifold);
    
    BOOST_CHECK(!result);
}

BOOST_AUTO_TEST_CASE(test_rod_rod_collision_overlapping) {
    ShapeRod rodA(4.0);
    ShapeRod rodB(4.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(0, 0.5, 0), Eigen::Vector3d(PI/4, 0, 0));
    
    ContactManifold manifold;
    bool result = collideRodRod(rodA, transA, rodB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_box_cylinder_collision_overlapping) {
    ShapeBox box(Eigen::Vector3d(1, 1, 1));
    ShapeCylinder cylinder(0.8, 1.5);
    
    Transform transBox = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transCylinder = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxCylinder(box, transBox, cylinder, transCylinder, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_box_disk_collision_overlapping) {
    ShapeBox box(Eigen::Vector3d(1, 1, 1));
    ShapeDisk disk(1.0);
    
    Transform transBox = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transDisk = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxDisk(box, transBox, disk, transDisk, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_box_rod_collision_overlapping) {
    ShapeBox box(Eigen::Vector3d(1, 1, 1));
    ShapeRod rod(1.5);
    
    Transform transBox = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transRod = createTransform(Eigen::Vector3d(0, 0, 0));
    
    ContactManifold manifold;
    bool result = collideBoxRod(box, transBox, rod, transRod, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_spherical_shell_spherical_shell_collision) {
    ShapeSphericalShell shellA(1.0);
    ShapeSphericalShell shellB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideSphericalShellSphericalShell(shellA, transA, shellB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_sphere_spherical_shell_collision) {
    ShapeSphere sphere(1.0);
    ShapeSphericalShell shell(1.0);
    
    Transform transSphere = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transShell = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideSphereSphericalShell(sphere, transSphere, shell, transShell, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_cylindrical_shell_cylindrical_shell_collision) {
    ShapeCylindricalShell shellA(1.0, 2.0);
    ShapeCylindricalShell shellB(1.0, 2.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideCylindricalShellCylindricalShell(shellA, transA, shellB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_ring_ring_collision) {
    ShapeRing ringA(1.0);
    ShapeRing ringB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideRingRing(ringA, transA, ringB, transB, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_ring_sphere_collision) {
    ShapeRing ring(1.0);
    ShapeSphere sphere(0.8);
    
    Transform transRing = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transSphere = createTransform(Eigen::Vector3d(0.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideRingSphere(ring, transRing, sphere, transSphere, manifold);
    
    BOOST_CHECK(result);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_contact_manifold_properties) {
    ShapeSphere sphereA(1.0);
    ShapeSphere sphereB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    bool result = collideSphereSphere(sphereA, transA, sphereB, transB, manifold);
    
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(!manifold.empty());
    
    const ContactPoint& cp = manifold[0];
    
    BOOST_CHECK_EQUAL(cp.normal.norm(), 1.0);
    BOOST_CHECK(cp.penetration > 0);
    BOOST_CHECK(cp.penetration < 1.0);
}

BOOST_AUTO_TEST_CASE(test_collision_normal_direction) {
    ShapeSphere sphereA(1.0);
    ShapeSphere sphereB(1.0);
    
    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(1.5, 0, 0));
    
    ContactManifold manifold;
    collideSphereSphere(sphereA, transA, sphereB, transB, manifold);
    
    BOOST_REQUIRE(!manifold.empty());
    
    Eigen::Vector3d expectedNormal = (transB.getTranslation() - transA.getTranslation()).normalized();
    BOOST_CHECK_CLOSE(manifold[0].normal.dot(expectedNormal), 1.0, 0.001);
}