//
// Created by jehor on 2026/5/21.
//

#include "ccd_collide_judge_group.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "standard_collide_judge_group.h"

#include "shape_base.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "shape_sphere.h"
#include "shape_spherical_shell.h"
#include "logger.h"

namespace egret
{
    constexpr double kEpsTime = 1e-10;
    constexpr double kEpsDist = 1e-8;
    constexpr double kEpsAngle = 1e-8;
    constexpr double kEpsDiskThickness = 1e-8;
    constexpr double kEpsRodRadius = 1e-8;
    constexpr double kEpsRingRadialThickness = 1e-8;
    constexpr int kMaxIterations = 80;

    Transform integrateTransform(const Transform& initTrans,
                                 const Eigen::Vector3d& linearVel,
                                 const Eigen::Vector3d& angularVel,
                                 double t)
    {
        const Eigen::Vector3d translation = initTrans.getTranslation() + linearVel * t;

        Eigen::Quaterniond rotation = initTrans.getRotation();
        const double angularSpeed = angularVel.norm();
        if (angularSpeed > kEpsAngle) {
            const Eigen::Vector3d axis = angularVel / angularSpeed;
            const Eigen::AngleAxisd angleAxis(angularSpeed * t, axis);
            const Eigen::Quaterniond delta(angleAxis);
            rotation = (delta * rotation).normalized();
        }

        Transform out;
        out.setTRS(translation, rotation, initTrans.getScale());
        return out;
    }

    ContactFeature classifyBoxFeature(const ShapeBox& box, const Transform& trans,
                                      const Eigen::Vector3d& contactPoint)
    {
        const Eigen::Vector3d localPoint = trans.worldToLocal(contactPoint);
        const Eigen::Vector3d half = box.getHalfSize();
        const Eigen::Vector3d distToFace = (localPoint.cwiseAbs() - half).cwiseAbs();

        int nearCount = 0;
        if (distToFace.x() <= kEpsDist) ++nearCount;
        if (distToFace.y() <= kEpsDist) ++nearCount;
        if (distToFace.z() <= kEpsDist) ++nearCount;

        if (nearCount >= 3) return ContactFeature::Vertex;
        if (nearCount == 2) return ContactFeature::Edge;
        return ContactFeature::Face;
    }

    ContactFeature classifyDiskFeature(const ShapeDisk& disk, const Transform& trans,
                                       const Eigen::Vector3d& contactPoint)
    {
        const Eigen::Vector3d localPoint = trans.worldToLocal(contactPoint);
        const double radial = std::sqrt(localPoint.x() * localPoint.x() + localPoint.y() * localPoint.y());
        if (std::abs(radial - disk.getRadius()) <= kEpsDiskThickness * 2.0) {
            return ContactFeature::Edge;
        }
        return ContactFeature::Face;
    }

    ContactFeature classifyRingFeature(const ShapeRing& ring, const Transform& trans,
                                       const Eigen::Vector3d& contactPoint)
    {
        const Eigen::Vector3d localPoint = trans.worldToLocal(contactPoint);
        const double radial = std::sqrt(localPoint.x() * localPoint.x() + localPoint.y() * localPoint.y());
        const double outerRadius = ring.getRadius() + 0.5 * kEpsRingRadialThickness;
        const double innerRadius = std::max(0.0, ring.getRadius() - 0.5 * kEpsRingRadialThickness);
        if (std::abs(radial - outerRadius) <= kEpsRingRadialThickness * 2.0 ||
            std::abs(radial - innerRadius) <= kEpsRingRadialThickness * 2.0) {
            return ContactFeature::Edge;
        }
        return ContactFeature::Face;
    }

    ContactFeature classifyRodFeature(const ShapeRod& rod, const Transform& trans,
                                      const Eigen::Vector3d& contactPoint)
    {
        const auto [start, end] = rod.getStartEnd(trans);
        const Eigen::Vector3d dir = end - start;
        const double lenSq = dir.squaredNorm();
        if (lenSq <= kEpsDist) return ContactFeature::Vertex;
        const double t = (contactPoint - start).dot(dir) / lenSq;
        if (t <= kEpsDist || t >= 1.0 - kEpsDist) return ContactFeature::Vertex;
        return ContactFeature::Edge;
    }

    bool collideBoxRodCcd(const ShapeBox& box, const Transform& transA,
                          const ShapeRod& rod, const Transform& transB,
                          ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double rodHalfLength = rod.getLength() * 0.5;

        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        const Eigen::Vector3d localCenter = transA.worldToLocal(rodCenter);

        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        Eigen::Vector3d closestLocalPoint;
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localCenter[i], min[i], max[i]);
        }

        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = closestWorldPoint - rodEndA;

        const double c1 = w.dot(v);
        double t = 0.0;
        if (c1 <= 0.0) {
            t = 0.0;
        }
        else {
            const double c2 = v.dot(v);
            if (c2 <= c1) {
                t = 1.0;
            }
            else {
                t = c1 / c2;
            }
        }

        const Eigen::Vector3d closestOnRod = rodEndA + t * v;
        const Eigen::Vector3d diff = closestWorldPoint - closestOnRod;
        const double distSq = diff.squaredNorm();
        const double radiusSq = kEpsRodRadius * kEpsRodRadius;

        if (distSq > radiusSq + kEpsDist) {
            return false;
        }

        Eigen::Vector3d normal = diff;
        if (normal.squaredNorm() < kEpsDist) {
            normal = Eigen::Vector3d::UnitX();
            if (std::abs(rodAxis.dot(normal)) > 0.9) {
                normal = Eigen::Vector3d::UnitY();
            }
        }
        else {
            normal.normalize();
        }

        if (normal.dot(rodCenter - transA.getTranslation()) < 0.0) {
            normal = -normal;
        }

        const double penetration = std::max(0.0, kEpsRodRadius - std::sqrt(distSq));
        const Eigen::Vector3d contactPoint = (closestWorldPoint + closestOnRod) * 0.5;

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyBoxFeature(box, transA, contactPoint);
        point.featureB = classifyRodFeature(rod, transB, contactPoint);
        manifold.push_back(point);
        return true;
    }

    bool collideBoxDiskCcd(const ShapeBox& box, const Transform& transA,
                           const ShapeDisk& disk, const Transform& transB,
                           ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double diskRadius = disk.getRadius() + kEpsDiskThickness;

        const Eigen::Vector3d diskCenter = transB.getTranslation();
        const Eigen::Vector3d diskNormal = transB.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        const Eigen::Vector3d localDiskCenter = transA.worldToLocal(diskCenter);

        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        Eigen::Vector3d closestLocalPoint;
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localDiskCenter[i], min[i], max[i]);
        }

        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        const Eigen::Vector3d diff = closestWorldPoint - diskCenter;
        const double distSq = diff.squaredNorm();

        if (distSq > diskRadius * diskRadius + kEpsDist) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = diskRadius - dist;

        Eigen::Vector3d normal;
        if (dist > kEpsDist) {
            normal = diff.normalized();
        }
        else {
            normal = diskNormal;
            if (normal.dot(diskCenter - boxCenter) < 0.0) {
                normal = -normal;
            }
        }

        const Eigen::Vector3d contactPoint = diskCenter + normal * disk.getRadius();

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyBoxFeature(box, transA, contactPoint);
        point.featureB = classifyDiskFeature(disk, transB, contactPoint);
        manifold.push_back(point);
        return true;
    }

    bool collideDiskRodCcd(const ShapeDisk& disk, const Transform& transA,
                           const ShapeRod& rod, const Transform& transB,
                           ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius() + kEpsDiskThickness + kEpsRodRadius;
        const double rodHalfLength = rod.getLength() * 0.5;

        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d diskNormal = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = diskCenter - rodEndA;
        double t = w.dot(v);

        double lenSq = v.dot(v);
        if (lenSq <= kEpsDist) {
            t = 0.0;
        }
        else {
            t /= lenSq;
            t = std::clamp(t, 0.0, 1.0);
        }

        const Eigen::Vector3d closestOnRod = rodEndA + t * v;
        const Eigen::Vector3d diff = diskCenter - closestOnRod;
        const double distSq = diff.squaredNorm();

        if (distSq > diskRadius * diskRadius + kEpsDist) {
            return false;
        }

        Eigen::Vector3d normal = diff;
        if (normal.squaredNorm() < kEpsDist) {
            normal = diskNormal;
        }
        else {
            normal.normalize();
        }

        if (normal.dot(diskCenter - rodCenter) < 0.0) {
            normal = -normal;
        }

        const double penetration = diskRadius - std::sqrt(distSq);
        const Eigen::Vector3d contactPoint = diskCenter + normal * disk.getRadius();

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyDiskFeature(disk, transA, contactPoint);
        point.featureB = classifyRodFeature(rod, transB, contactPoint);
        manifold.push_back(point);
        return true;
    }

    bool collideDiskDiskCcd(const ShapeDisk& diskA, const Transform& transA,
                            const ShapeDisk& diskB, const Transform& transB,
                            ContactManifold& manifold)
    {
        const double radiusA = diskA.getRadius() + kEpsDiskThickness;
        const double radiusB = diskB.getRadius() + kEpsDiskThickness;

        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();

        const Eigen::Vector3d diff = centerB - centerA;
        const double distSq = diff.squaredNorm();
        const double minDist = radiusA + radiusB;
        if (distSq > minDist * minDist + kEpsDist) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        Eigen::Vector3d normal = diff;
        if (normal.squaredNorm() < kEpsDist) {
            normal = transA.getRotation() * Eigen::Vector3d::UnitZ();
        }
        else {
            normal.normalize();
        }

        const double penetration = minDist - dist;
        const Eigen::Vector3d contactPoint = centerA + normal * diskA.getRadius();

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyDiskFeature(diskA, transA, contactPoint);
        point.featureB = classifyDiskFeature(diskB, transB, contactPoint);
        manifold.push_back(point);
        return true;
    }

    bool collideDiskSphereCcd(const ShapeDisk& disk, const Transform& transA,
                              const ShapeSphere& sphere, const Transform& transB,
                              ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius() + kEpsDiskThickness;
        const double sphereRadius = sphere.getRadius();

        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();

        const Eigen::Vector3d diff = sphereCenter - diskCenter;
        const double distSq = diff.squaredNorm();
        const double minDist = diskRadius + sphereRadius;
        if (distSq > minDist * minDist + kEpsDist) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        Eigen::Vector3d normal = diff;
        if (normal.squaredNorm() < kEpsDist) {
            normal = transA.getRotation() * Eigen::Vector3d::UnitZ();
        }
        else {
            normal.normalize();
        }

        const double penetration = minDist - dist;
        const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyDiskFeature(disk, transA, contactPoint);
        point.featureB = ContactFeature::Face;
        manifold.push_back(point);
        return true;
    }

    bool collideRodRodCcd(const ShapeRod& rodA, const Transform& transA,
                          const ShapeRod& rodB, const Transform& transB,
                          ContactManifold& manifold)
    {
        auto [startA, endA] = rodA.getStartEnd(transA);
        auto [startB, endB] = rodB.getStartEnd(transB);

        const Eigen::Vector3d u = endA - startA;
        const Eigen::Vector3d v = endB - startB;
        const Eigen::Vector3d w0 = startA - startB;

        const double a = u.dot(u);
        const double b = u.dot(v);
        const double c = v.dot(v);
        const double d = u.dot(w0);
        const double e = v.dot(w0);

        const double denom = a * c - b * b;
        double s = 0.0;
        double t = 0.0;

        if (denom > kEpsDist) {
            s = (b * e - c * d) / denom;
            t = (a * e - b * d) / denom;
        }
        s = std::clamp(s, 0.0, 1.0);
        t = std::clamp(t, 0.0, 1.0);

        const Eigen::Vector3d p = startA + s * u;
        const Eigen::Vector3d q = startB + t * v;
        const Eigen::Vector3d diff = p - q;
        const double distSq = diff.squaredNorm();
        const double radius = 2.0 * kEpsRodRadius;

        if (distSq > radius * radius + kEpsDist) {
            return false;
        }

        Eigen::Vector3d normal = diff;
        if (normal.squaredNorm() < kEpsDist) {
            normal = u.cross(v);
            if (normal.squaredNorm() < kEpsDist) {
                normal = Eigen::Vector3d::UnitX();
            }
        }
        normal.normalize();

        const double penetration = radius - std::sqrt(distSq);
        const Eigen::Vector3d contactPoint = (p + q) * 0.5;

        ContactPoint point{};
        point.position = contactPoint;
        point.normal = normal;
        point.penetration = penetration;
        point.featureA = classifyRodFeature(rodA, transA, contactPoint);
        point.featureB = classifyRodFeature(rodB, transB, contactPoint);
        manifold.push_back(point);
        return true;
    }

    bool collideRodSphereCcd(const ShapeRod& rod, const Transform& transA,
                             const ShapeSphere& sphere, const Transform& transB,
                             ContactManifold& manifold)
    {
        auto [start, end] = rod.getStartEnd(transA);
        const Eigen::Vector3d center = transB.getTranslation();
        const double radius = sphere.getRadius() + kEpsRodRadius;

        const Eigen::Vector3d dir = end - start;
        const double lenSq = dir.squaredNorm();
        double t = 0.0;
        if (lenSq > kEpsDist) {
            t = (center - start).dot(dir) / lenSq;
            t = std::clamp(t, 0.0, 1.0);
        }

        const Eigen::Vector3d closestPoint = start + t * dir;
        const Eigen::Vector3d delta = closestPoint - center;
        const double dist = delta.norm();

        if (dist > radius + kEpsDist) {
            return false;
        }

        ContactPoint point{};
        point.position = closestPoint;
        if (delta.squaredNorm() < kEpsDist) {
            Eigen::Vector3d fallback = end - start;
            if (fallback.squaredNorm() < kEpsDist) {
                fallback = Eigen::Vector3d::UnitX();
            }
            point.normal = fallback.normalized();
        }
        else {
            point.normal = delta.normalized();
        }
        point.penetration = radius - dist;
        point.featureA = classifyRodFeature(rod, transA, point.position);
        point.featureB = ContactFeature::Face;
        manifold.push_back(point);
        return true;
    }

    bool filterRingContact(const ShapeRing& ring, const Transform& trans, ContactManifold& manifold)
    {
        if (manifold.empty()) {
            return false;
        }

        const double outerRadius = ring.getRadius() + 0.5 * kEpsRingRadialThickness;
        const double innerRadius = std::max(0.0, ring.getRadius() - 0.5 * kEpsRingRadialThickness);

        ContactManifold filtered;
        filtered.reserve(manifold.size());

        for (const ContactPoint& point : manifold) {
            const Eigen::Vector3d localPoint = trans.worldToLocal(point.position);
            const double radial = std::sqrt(localPoint.x() * localPoint.x() + localPoint.y() * localPoint.y());
            if (radial + kEpsDist >= innerRadius && radial <= outerRadius + kEpsDist) {
                filtered.push_back(point);
            }
        }

        manifold = std::move(filtered);
        return !manifold.empty();
    }

    template <typename ShapeA, typename ShapeB, typename CollideFunc, typename PostProcessFunc = std::nullptr_t>
    bool continuousCollideGeneric(const ShapeA& shapeA, const Transform& initTransA,
                                  const Eigen::Vector3d& linearVelA,
                                  const Eigen::Vector3d& angularVelA,
                                  const ShapeB& shapeB, const Transform& initTransB,
                                  const Eigen::Vector3d& linearVelB,
                                  const Eigen::Vector3d& angularVelB,
                                  double dt,
                                  double& collisionTime,
                                  ContactManifold& manifold,
                                  CollideFunc&& collideFunc,
                                  PostProcessFunc&& postProcessFunc = nullptr)
    {
        collisionTime = 0.0;
        manifold.clear();

        if (dt <= kEpsTime) {
            return false;
        }

        auto collideAt = [&](double t, ContactManifold& outManifold) -> bool {
            const Transform transA = integrateTransform(initTransA, linearVelA, angularVelA, t);
            const Transform transB = integrateTransform(initTransB, linearVelB, angularVelB, t);
            outManifold.clear();
            return collideFunc(shapeA, transA, shapeB, transB, outManifold);
        };

        ContactManifold startManifold;
        if (collideAt(0.0, startManifold)) {
            collisionTime = 0.0;
            manifold = startManifold;
            return true;
        }

        ContactManifold endManifold;
        if (!collideAt(dt, endManifold)) {
            return false;
        }

        double lo = 0.0;
        double hi = dt;
        ContactManifold hitManifold = endManifold;
        const double timeEps = std::max(kEpsTime, 1e-6 * dt);

        for (int iter = 0; iter < kMaxIterations && (hi - lo) > timeEps; ++iter) {
            const double mid = 0.5 * (lo + hi);
            ContactManifold midManifold;
            if (collideAt(mid, midManifold)) {
                hi = mid;
                hitManifold = midManifold;
            }
            else {
                lo = mid;
            }
        }

        collisionTime = hi;
        manifold = hitManifold;

        if constexpr (!std::is_same_v<PostProcessFunc, std::nullptr_t>) {
            const Transform transA = integrateTransform(initTransA, linearVelA, angularVelA, collisionTime);
            const Transform transB = integrateTransform(initTransB, linearVelB, angularVelB, collisionTime);
            postProcessFunc(shapeA, transA, shapeB, transB, manifold);
        }

        if (collisionTime < kEpsTime) {
            collisionTime = 0.0;
        }
        if (collisionTime > dt - kEpsTime) {
            return false;
        }

        return true;
    }
}

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
        return continuousCollideGeneric(
            boxA, initTransA, linearVelA, angularVelA,
            boxB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            box_first::collideBoxBox,
            [](const ShapeBox& a, const Transform& transA,
               const ShapeBox& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyBoxFeature(a, transA, point.position);
                    point.featureB = classifyBoxFeature(b, transB, point.position);
                }
            });
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            cylinder, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            box_first::collideBoxCylinder);
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            cyShell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            box_first::collideBoxCylindricalShell);
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            disk, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideBoxDiskCcd);
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
        auto collideFunc = [](const ShapeBox& a, const Transform& transA,
                              const ShapeRing& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideBoxDiskCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(b, transB, outManifold);
        };

        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            ring, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeBox& a, const Transform& transA,
               const ShapeRing& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyBoxFeature(a, transA, point.position);
                    point.featureB = classifyRingFeature(b, transB, point.position);
                }
            });
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            rod, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideBoxRodCcd);
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            box_first::collideBoxSphere,
            [](const ShapeBox& a, const Transform& transA,
               const ShapeSphere&, const Transform&,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyBoxFeature(a, transA, point.position);
                    point.featureB = ContactFeature::Face;
                }
            });
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
        return continuousCollideGeneric(
            box, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            box_first::collideBoxSphericalShell);
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
        return continuousCollideGeneric(
            cylinderA, initTransA, linearVelA, angularVelA,
            cylinderB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderCylinder);
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
        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            cyShell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderCylindricalShell);
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
        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            disk, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderDisk);
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
        auto collideFunc = [](const ShapeCylinder& a, const Transform& transA,
                              const ShapeRing& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!cylinder_first::collideCylinderDisk(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(b, transB, outManifold);
        };

        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            ring, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeCylinder&, const Transform&,
               const ShapeRing& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = ContactFeature::Face;
                    point.featureB = classifyRingFeature(b, transB, point.position);
                }
            });
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
        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            rod, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderRod);
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
        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderSphere);
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
        return continuousCollideGeneric(
            cylinder, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylinder_first::collideCylinderSphericalShell);
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
        return continuousCollideGeneric(
            cyShellA, initTransA, linearVelA, angularVelA,
            cyShellB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylindrical_shell_first::collideCylindricalShellCylindricalShell);
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
        return continuousCollideGeneric(
            cyShell, initTransA, linearVelA, angularVelA,
            disk, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylindrical_shell_first::collideCylindricalShellDisk);
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
        auto collideFunc = [](const ShapeCylindricalShell& a, const Transform& transA,
                              const ShapeRing& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!cylinder_first::collideCylinderDisk(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(b, transB, outManifold);
        };

        return continuousCollideGeneric(
            cyShell, initTransA, linearVelA, angularVelA,
            ring, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeCylindricalShell&, const Transform&,
               const ShapeRing& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = ContactFeature::Face;
                    point.featureB = classifyRingFeature(b, transB, point.position);
                }
            });
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
        return continuousCollideGeneric(
            cyShell, initTransA, linearVelA, angularVelA,
            rod, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylindrical_shell_first::collideCylindricalShellRod);
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
        return continuousCollideGeneric(
            cyShell, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylindrical_shell_first::collideCylindricalShellSphere);
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
        return continuousCollideGeneric(
            cyShell, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            cylindrical_shell_first::collideCylindricalShellSphericalShell);
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
        return continuousCollideGeneric(
            diskA, initTransA, linearVelA, angularVelA,
            diskB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideDiskDiskCcd);
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
        auto collideFunc = [](const ShapeDisk& a, const Transform& transA,
                              const ShapeRing& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideDiskDiskCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(b, transB, outManifold);
        };

        return continuousCollideGeneric(
            disk, initTransA, linearVelA, angularVelA,
            ring, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeDisk& a, const Transform& transA,
               const ShapeRing& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyDiskFeature(a, transA, point.position);
                    point.featureB = classifyRingFeature(b, transB, point.position);
                }
            });
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
        return continuousCollideGeneric(
            disk, initTransA, linearVelA, angularVelA,
            rod, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideDiskRodCcd);
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
        return continuousCollideGeneric(
            disk, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideDiskSphereCcd);
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
        return continuousCollideGeneric(
            disk, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideDiskSphereCcd);
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
        auto collideFunc = [](const ShapeRing& a, const Transform& transA,
                              const ShapeRing& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideDiskDiskCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            if (!filterRingContact(a, transA, outManifold)) {
                return false;
            }
            return filterRingContact(b, transB, outManifold);
        };

        return continuousCollideGeneric(
            ringA, initTransA, linearVelA, angularVelA,
            ringB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeRing& a, const Transform& transA,
               const ShapeRing& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyRingFeature(a, transA, point.position);
                    point.featureB = classifyRingFeature(b, transB, point.position);
                }
            });
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
        auto collideFunc = [](const ShapeRing& a, const Transform& transA,
                              const ShapeRod& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideDiskRodCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(a, transA, outManifold);
        };

        return continuousCollideGeneric(
            ring, initTransA, linearVelA, angularVelA,
            rod, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeRing& a, const Transform& transA,
               const ShapeRod& b, const Transform& transB,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyRingFeature(a, transA, point.position);
                    point.featureB = classifyRodFeature(b, transB, point.position);
                }
            });
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
        auto collideFunc = [](const ShapeRing& a, const Transform& transA,
                              const ShapeSphere& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideDiskSphereCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(a, transA, outManifold);
        };

        return continuousCollideGeneric(
            ring, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeRing& a, const Transform& transA,
               const ShapeSphere&, const Transform&,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyRingFeature(a, transA, point.position);
                    point.featureB = ContactFeature::Face;
                }
            });
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
        auto collideFunc = [](const ShapeRing& a, const Transform& transA,
                              const ShapeSphericalShell& b, const Transform& transB,
                              ContactManifold& outManifold) -> bool
        {
            if (!collideDiskSphereCcd(a, transA, b, transB, outManifold)) {
                return false;
            }
            return filterRingContact(a, transA, outManifold);
        };

        return continuousCollideGeneric(
            ring, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideFunc,
            [](const ShapeRing& a, const Transform& transA,
               const ShapeSphericalShell&, const Transform&,
               ContactManifold& outManifold)
            {
                for (ContactPoint& point : outManifold) {
                    point.featureA = classifyRingFeature(a, transA, point.position);
                    point.featureB = ContactFeature::Face;
                }
            });
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
        return continuousCollideGeneric(
            rodA, initTransA, linearVelA, angularVelA,
            rodB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideRodRodCcd);
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
        return continuousCollideGeneric(
            rod, initTransA, linearVelA, angularVelA,
            sphere, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideRodSphereCcd);
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
        return continuousCollideGeneric(
            rod, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            collideRodSphereCcd);
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
        return continuousCollideGeneric(
            sphereA, initTransA, linearVelA, angularVelA,
            sphereB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            sphere_first::collideSphereSphere);
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
        return continuousCollideGeneric(
            sphere, initTransA, linearVelA, angularVelA,
            shell, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            sphere_first::collideSphereSphericalShell);
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
        return continuousCollideGeneric(
            shellA, initTransA, linearVelA, angularVelA,
            shellB, initTransB, linearVelB, angularVelB,
            dt, collisionTime, manifold,
            spherical_shell_first::collideSphericalShellSphericalShell);
    }

}