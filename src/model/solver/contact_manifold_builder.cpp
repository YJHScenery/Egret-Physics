/**
 * @file contact_manifold_builder.cpp
 * @brief 通过分离轴与参考面裁剪生成平面支撑流形，避免质心法线回退。
 */
#include "contact_manifold_builder.h"
#include "physical_entity.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_sphere.h"
#include "transform.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>

namespace egret
{
    namespace
    {
        /** @brief 将当前变换预测到恒速 CCD 时刻，并保留缩放。 */
        Transform predictTransform(const PhysicalEntity& body, double time)
        {
            Transform result = body.getTransformCR();
            result.setTranslation(body.getPosition() + time * body.getVelocity());
            const auto omega = body.getAngularVelocity();
            if (omega.norm() > 1e-12)
                result.setRotation(Eigen::Quaterniond(Eigen::AngleAxisd(time*omega.norm(),omega.normalized())) * result.getRotation());
            return result;
        }

        /** @brief 用半空间裁剪凸多边形，保留参考面侧边内的接触区域。 */
        std::vector<Eigen::Vector3d> clipPolygon(const std::vector<Eigen::Vector3d>& polygon,
                                                const Eigen::Vector3d& normal, double offset)
        {
            std::vector<Eigen::Vector3d> output;
            if (polygon.empty()) return output;
            Eigen::Vector3d previous = polygon.back();
            double previousDistance = normal.dot(previous) - offset;
            for (const auto& current : polygon) {
                const double distance = normal.dot(current) - offset;
                if ((distance <= 0.0) != (previousDistance <= 0.0))
                    output.push_back(previous + (current-previous) * (previousDistance/(previousDistance-distance)));
                if (distance <= 0.0) output.push_back(current);
                previous = current; previousDistance = distance;
            }
            return output;
        }

        /** @brief 返回盒体指定朝向上的支撑面顶点，使用循环顺序。 */
        std::vector<Eigen::Vector3d> boxFace(const ShapeBox& box, const Transform& transform,
                                           const Eigen::Vector3d& direction)
        {
            const Eigen::Matrix3d rotation = transform.getRotationMatrix();
            Eigen::Index axis;
            (rotation.transpose()*direction).cwiseAbs().maxCoeff(&axis);
            const Eigen::Vector3d half = box.getHalfSize();
            const int u = (axis+1)%3, v = (axis+2)%3;
            Eigen::Vector3d local = Eigen::Vector3d::Zero();
            local[axis] = (rotation.col(axis).dot(direction) >= 0.0 ? 1.0 : -1.0)*half[axis];
            std::vector<Eigen::Vector3d> result;
            for (auto signs : {Eigen::Vector2d(-1,-1), Eigen::Vector2d(1,-1), Eigen::Vector2d(1,1), Eigen::Vector2d(-1,1)}) {
                local[u] = signs.x()*half[u]; local[v] = signs.y()*half[v];
                result.push_back(transform.localToWorld(local));
            }
            return result;
        }
    }

    std::vector<CcdContactData> buildContactManifold(PhysicalEntity& a, PhysicalEntity& b,
        double time, const Eigen::Vector3d& rawNormal, double penetration, const Eigen::Vector3d& point)
    {
        constexpr double margin = 2e-4;
        const auto ta = predictTransform(a,time), tb = predictTransform(b,time);
        const auto* boxA = dynamic_cast<const ShapeBox*>(a.getShape().get());
        const auto* boxB = dynamic_cast<const ShapeBox*>(b.getShape().get());
        const auto* cylinderA = dynamic_cast<const ShapeCylinder*>(a.getShape().get());
        const auto* cylinderB = dynamic_cast<const ShapeCylinder*>(b.getShape().get());
        const auto* sphereA = dynamic_cast<const ShapeSphere*>(a.getShape().get());
        const auto* sphereB = dynamic_cast<const ShapeSphere*>(b.getShape().get());
        if ((boxA && sphereB) || (boxB && sphereA)) {
            const auto* box = boxA ? boxA : boxB;
            const auto* sphere = sphereB ? sphereB : sphereA;
            const auto& boxTransform = boxA ? ta : tb;
            const auto& sphereTransform = boxA ? tb : ta;
            const Eigen::Vector3d local = boxTransform.worldToLocal(sphereTransform.getTranslation());
            const Eigen::Vector3d closest = local.cwiseMax(-box->getHalfSize()).cwiseMin(box->getHalfSize());
            const Eigen::Vector3d delta = local-closest;
            if (delta.norm()>1e-12) {
                const double gap = delta.norm()-sphere->getRadius();
                if (gap>margin) return {{point,rawNormal,penetration}};
                const Eigen::Vector3d outward = boxTransform.getRotation()*delta.normalized();
                return {{boxTransform.localToWorld(closest)+0.5*gap*outward,
                    boxA ? outward : -outward,std::max(-gap,0.0)}};
            }
        }
        if (!boxA && !boxB) return {{point,rawNormal,penetration}};
        const Eigen::Vector3d center = tb.getTranslation()-ta.getTranslation();
        Eigen::Vector3d n = rawNormal;
        double bestGap = -std::numeric_limits<double>::infinity();
        int reference = -1, referenceAxis = -1;
        bool separated = false;
        /** @brief 测试真实几何分离轴，同时记录最浅穿透参考特征。 */
        auto testAxis = [&](Eigen::Vector3d axis, int owner, int index) {
            if (axis.norm() < 1e-10) return;
            axis.normalize();
            if (axis.dot(center) < 0.0) axis = -axis;
            const double gap = axis.dot(b.getShape()->support(-axis,tb)-a.getShape()->support(axis,ta));
            if (gap > margin) separated = true;
            if (gap > bestGap) { bestGap = gap; n = axis; reference = owner; referenceAxis = index; }
        };
        if (boxA) for (int k=0;k<3;++k) testAxis(ta.getRotationMatrix().col(k),0,k);
        if (boxB) for (int k=0;k<3;++k) testAxis(tb.getRotationMatrix().col(k),1,k);
        if (boxA && boxB) {
            for (int k=0;k<3;++k) for (int j=0;j<3;++j)
                testAxis(ta.getRotationMatrix().col(k).cross(tb.getRotationMatrix().col(j)),-1,-1);
        }
        // 盒体轴对其它凸体并非完整 SAT，只在已由 CCD 确认的接触附近细化面特征。
        if (separated || reference < 0) return {{point,rawNormal,penetration}};
        const ShapeBox* refBox = reference == 0 ? boxA : boxB;
        const auto& refTransform = reference == 0 ? ta : tb;
        const auto& incidentTransform = reference == 0 ? tb : ta;
        const Eigen::Vector3d refNormal = reference == 0 ? n : -n;
        const ShapeBox* incidentBox = reference == 0 ? boxB : boxA;
        const ShapeCylinder* incidentCylinder = reference == 0 ? cylinderB : cylinderA;
        std::vector<Eigen::Vector3d> polygon;
        if (incidentBox) polygon = boxFace(*incidentBox,incidentTransform,-refNormal);
        else if (incidentCylinder) {
            const Eigen::Vector3d axis = incidentTransform.getRotationMatrix().col(2);
            if (std::abs(axis.dot(refNormal)) < 1.0-1e-8) return {{point,rawNormal,penetration}};
            const double z = (axis.dot(refNormal)>0.0 ? -1.0:1.0)*incidentCylinder->getHalfHeight();
            for (int k=0;k<8;++k) {
                const double angle = 2.0*std::numbers::pi*k/8.0;
                polygon.push_back(incidentTransform.localToWorld(Eigen::Vector3d(
                    incidentCylinder->getRadius()*std::cos(angle),incidentCylinder->getRadius()*std::sin(angle),z)));
            }
        } else {
            // 对凸网格、圆盘等支撑映射采集共面支撑特征；曲面单点不会伪造为平面。
            const auto& incidentShape=reference==0 ? b.getShape() : a.getShape();
            const Eigen::Vector3d u=refNormal.unitOrthogonal(), v=refNormal.cross(u);
            const Eigen::Vector3d support=incidentShape->support(-refNormal,incidentTransform);
            for (int k=0;k<8;++k) {
                const double angle=2.0*std::numbers::pi*k/8.0;
                const Eigen::Vector3d p=incidentShape->support(-refNormal+1e-7*(std::cos(angle)*u+std::sin(angle)*v),incidentTransform);
                if (std::abs(refNormal.dot(p-support))>1e-6) return {{point,rawNormal,penetration}};
                if (polygon.empty() || (p-polygon.back()).norm()>1e-6) polygon.push_back(p);
            }
            if (polygon.size()>1 && (polygon.front()-polygon.back()).norm()<1e-6) polygon.pop_back();
            if (polygon.size()<3) return {{point,rawNormal,penetration}};
            Eigen::Vector3d area=Eigen::Vector3d::Zero();
            for (std::size_t k=1;k+1<polygon.size();++k) area+=(polygon[k]-polygon[0]).cross(polygon[k+1]-polygon[0]);
            if (area.norm()<1e-10) return {{point,rawNormal,penetration}};
        }
        const auto linear = refTransform.getLocalToWorldMatrix().topLeftCorner<3,3>().eval();
        const auto half = refBox->getHalfSize();
        for (int k=0;k<3;++k) {
            if (k==referenceAxis) continue;
            const Eigen::Vector3d axis = linear.col(k).normalized();
            const double extent = linear.col(k).norm()*half[k];
            polygon = clipPolygon(polygon,axis,axis.dot(refTransform.getTranslation())+extent);
            polygon = clipPolygon(polygon,-axis,-axis.dot(refTransform.getTranslation())+extent);
        }
        const double surface = refNormal.dot(refBox->support(refNormal,refTransform));
        std::vector<CcdContactData> result;
        for (const auto& p : polygon) {
            const double gap = refNormal.dot(p)-surface;
            if (gap <= margin) result.push_back({p-0.5*gap*refNormal,n,std::max(-gap,0.0)});
        }
        if (result.empty()) return {{point,rawNormal,penetration}};
        // 共面多边形至多八点，保留完整覆盖，避免不稳定的任意单点抽取。
        return result;
    }
}
