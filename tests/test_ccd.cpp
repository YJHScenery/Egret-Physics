#define BOOST_TEST_MODULE CcdTests
#include "boost_test_dependency.h"
#include "model/physics/rigid/shape/standard/shape_box.h"
#include "model/physics/rigid/shape/standard/collide_judge/ccd_collide_judge_group.h"

using namespace egret;
using namespace egret::ccd_box_first;

Transform createTransform(const Eigen::Vector3d &pos, const Eigen::Vector3d &rot = Eigen::Vector3d::Zero())
{
    Transform t;
    t.setTranslation(pos);
    Eigen::Quaterniond q;
    q = Eigen::AngleAxisd(rot(0), Eigen::Vector3d::UnitX()) * Eigen::AngleAxisd(rot(1), Eigen::Vector3d::UnitY()) * Eigen::AngleAxisd(rot(2), Eigen::Vector3d::UnitZ());
    t.setRotation(q);
    return t;
}

BOOST_AUTO_TEST_CASE(test_ccd_box_box_colliding)
{
    ShapeBox boxA(Eigen::Vector3d(1, 1, 1));
    ShapeBox boxB(Eigen::Vector3d(1, 1, 1));

    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(4, 0, 0));

    Eigen::Vector3d linearVelA(0, 0, 0);
    Eigen::Vector3d angularVelA(0, 0, 0);
    Eigen::Vector3d linearVelB(-5, 0, 0);
    Eigen::Vector3d angularVelB(0, 0, 0);

    double dt = 1.0;
    double collisionTime = -1.0;
    ContactManifold manifold;

    bool result = continuousCollideBoxBox(boxA, transA, linearVelA, angularVelA,
                                          boxB, transB, linearVelB, angularVelB,
                                          dt, collisionTime, manifold);

    BOOST_CHECK(result);
    BOOST_CHECK(collisionTime >= 0.0 && collisionTime <= dt);
    BOOST_CHECK(!manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_ccd_box_box_no_collision)
{
    ShapeBox boxA(Eigen::Vector3d(1, 1, 1));
    ShapeBox boxB(Eigen::Vector3d(1, 1, 1));

    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(5, 0, 0));

    Eigen::Vector3d linearVelA(0, 0, 0);
    Eigen::Vector3d angularVelA(0, 0, 0);
    Eigen::Vector3d linearVelB(-1, 0, 0);
    Eigen::Vector3d angularVelB(0, 0, 0);

    double dt = 1.0;
    double collisionTime = -1.0;
    ContactManifold manifold;

    bool result = continuousCollideBoxBox(boxA, transA, linearVelA, angularVelA,
                                          boxB, transB, linearVelB, angularVelB,
                                          dt, collisionTime, manifold);

    BOOST_CHECK(!result);
    BOOST_CHECK(manifold.empty());
}

BOOST_AUTO_TEST_CASE(test_ccd_box_box_already_overlapping)
{
    ShapeBox boxA(Eigen::Vector3d(1, 1, 1));
    ShapeBox boxB(Eigen::Vector3d(1, 1, 1));

    Transform transA = createTransform(Eigen::Vector3d(0, 0, 0));
    Transform transB = createTransform(Eigen::Vector3d(0.5, 0, 0));

    Eigen::Vector3d linearVelA(0, 0, 0);
    Eigen::Vector3d angularVelA(0, 0, 0);
    Eigen::Vector3d linearVelB(0, 0, 0);
    Eigen::Vector3d angularVelB(0, 0, 0);

    double dt = 1.0;
    double collisionTime = -1.0;
    ContactManifold manifold;

    bool result = continuousCollideBoxBox(boxA, transA, linearVelA, angularVelA,
                                          boxB, transB, linearVelB, angularVelB,
                                          dt, collisionTime, manifold);

    BOOST_CHECK(result);
    BOOST_CHECK(collisionTime <= 0.0);
    BOOST_CHECK(!manifold.empty());
}