/**
 * @file contact_graph_projector.cpp
 * @brief 使用接触岛与凸二次规划同步恢复穿透。
 * @details 对偶投影梯度使用行绝对和给出的安全 Lipschitz 上界；
 *          冗余约束无需求逆，闭环全部保留。每轮仅在整岛求解后提交位姿。
 */
#include "contact_graph_projector.h"
#include "physical_entity.h"
#include "transform.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <Eigen/Sparse>

namespace egret
{
    namespace
    {
        /** @brief 保存双方随刚体运动的接触锚点，深度只用于构造初始分离量。 */
        struct ProjectionContact
        {
            std::size_t a{}, b{};
            Eigen::Vector3d localA, localB, normal;
            bool distanceConstraint{false};
            bool bilateral{false};
            double length{0.0};
        };

        /** @brief 查找并压缩并查集路径；静态边界不会合并独立动态岛。 */
        std::size_t findRoot(std::vector<std::size_t>& roots, std::size_t i)
        {
            while (roots[i] != i) {
                roots[i] = roots[roots[i]];
                i = roots[i];
            }
            return i;
        }
    }

    void ContactGraphProjector::project(std::span<SolverBodyHandle> bodies,
                                        std::span<const SolverContactConstraint> contacts,
                                        const SolverConfig& config, SolverStats& stats, std::span<ConstraintsBase*> joints)
    {
        std::vector<std::size_t> roots(bodies.size());
        std::iota(roots.begin(), roots.end(), 0);
        std::vector<ProjectionContact> rows;
        for (const auto& c : contacts) {
            if (c.bodyAIndex >= bodies.size() || c.bodyBIndex >= bodies.size() ||
                c.bodyAIndex == c.bodyBIndex || !c.contactNormal.allFinite() ||
                c.contactNormal.norm() < 1e-12 || !c.contactPosition.allFinite() ||
                !std::isfinite(c.penetration)) continue;
            const auto& a = bodies[c.bodyAIndex];
            const auto& b = bodies[c.bodyBIndex];
            if (!a.entity || !b.entity || a.inverseMass + b.inverseMass <= 0.0) continue;
            const Eigen::Vector3d n = c.contactNormal.normalized();
            const Eigen::Vector3d half = 0.5 * std::max(c.penetration, 0.0) * n;
            rows.push_back({c.bodyAIndex, c.bodyBIndex,
                a.entity->getTransformCR().getRotation().conjugate() *
                    (c.contactPosition + half - a.entity->getPosition()),
                b.entity->getTransformCR().getRotation().conjugate() *
                    (c.contactPosition - half - b.entity->getPosition()), n});
            if (a.inverseMass > 0.0 && b.inverseMass > 0.0)
                roots[findRoot(roots, c.bodyBIndex)] = findRoot(roots, c.bodyAIndex);
        }
        for (auto* joint : joints) {
            if (!joint || !joint->isEnabled() ||
                (joint->getType()!=ConstraintType::ConnectingRod && joint->getType()!=ConstraintType::ConnectingLine)) continue;
            const auto& entities=joint->getConstrainedEntities();
            if (entities.size()!=2 || !std::isfinite(joint->getReferenceLength())) continue;
            std::vector<std::size_t> pair;
            for (std::size_t i=0;i<bodies.size();++i) if (entities.contains(bodies[i].entity)) pair.push_back(i);
            if (pair.size()!=2 || bodies[pair[0]].inverseMass+bodies[pair[1]].inverseMass<=0) continue;
            ProjectionContact row;
            row.a=pair[0]; row.b=pair[1]; row.localA.setZero(); row.localB.setZero(); row.normal=Eigen::Vector3d::UnitX();
            row.distanceConstraint=true; row.bilateral=joint->getType()==ConstraintType::ConnectingRod;
            row.length=joint->getReferenceLength(); rows.push_back(row);
            if (bodies[row.a].inverseMass>0 && bodies[row.b].inverseMass>0)
                roots[findRoot(roots,row.b)]=findRoot(roots,row.a);
        }
        std::map<std::size_t, std::vector<std::size_t>> islands;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto& c = rows[r];
            const auto dynamicBody = bodies[c.a].inverseMass > 0.0 ? c.a : c.b;
            islands[findRoot(roots, dynamicBody)].push_back(r);
        }
        stats.contactIslandCount += islands.size();
        for (const auto& [root, indices] : islands) {
            std::vector<std::size_t> nodes;
            for (auto r : indices) {
                for (auto i : {rows[r].a, rows[r].b})
                    if (bodies[i].inverseMass > 0.0) nodes.push_back(i);
            }
            std::sort(nodes.begin(), nodes.end());
            nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
            const Eigen::Index count = static_cast<Eigen::Index>(indices.size());
            const Eigen::Index dofs = static_cast<Eigen::Index>(6 * nodes.size());
            std::vector<Eigen::Vector3d> totalTranslation(nodes.size(), Eigen::Vector3d::Zero());
            double totalAngle = 0.0;
            for (std::uint32_t outer = 0; outer < config.positionIterations; ++outer) {
                Eigen::SparseMatrix<double> J(count, dofs), W(dofs, dofs);
                std::vector<Eigen::Triplet<double>> jEntries, wEntries;
                for (std::size_t k = 0; k < nodes.size(); ++k) {
                    const auto& body = bodies[nodes[k]];
                    for (int i=0;i<3;++i) {
                        wEntries.emplace_back(6*k+i,6*k+i,body.inverseMass);
                        if (body.entity->hasRotation()) for (int j=0;j<3;++j)
                            wEntries.emplace_back(6*k+3+i,6*k+3+j,body.inverseInertiaTensor(i,j));
                    }
                }
                Eigen::VectorXd target(count);
                double maxPenetration = 0.0;
                for (Eigen::Index k = 0; k < count; ++k) {
                    auto& c = rows[indices[k]];
                    const auto& a = bodies[c.a]; const auto& b = bodies[c.b];
                    const Eigen::Vector3d ra = a.entity->getTransformCR().getRotation() * c.localA;
                    const Eigen::Vector3d rb = b.entity->getTransformCR().getRotation() * c.localB;
                    double gap = c.normal.dot(b.entity->getPosition() + rb - a.entity->getPosition() - ra);
                    if (c.distanceConstraint) {
                        const Eigen::Vector3d displacement=b.entity->getPosition()-a.entity->getPosition();
                        if (displacement.norm()>1e-12) c.normal=-displacement.normalized();
                        gap=c.length-displacement.norm();
                    }
                    maxPenetration = std::max(maxPenetration, c.bilateral ? std::abs(gap) : -gap);
                    target[k] = config.positionCorrectionBeta * (c.bilateral ? -gap : std::max(-gap - config.penetrationSlop, 0.0));
                    for (const auto i : {c.a, c.b}) {
                        if (bodies[i].inverseMass <= 0.0) continue;
                        const auto slot = std::lower_bound(nodes.begin(), nodes.end(), i) - nodes.begin();
                        const double sign = i == c.a ? -1.0 : 1.0;
                        const Eigen::Vector3d arm = i == c.a ? ra : rb;
                        const Eigen::Vector3d angular=sign*arm.cross(c.normal);
                        for (int j=0;j<3;++j) {
                            jEntries.emplace_back(k,6*slot+j,sign*c.normal[j]);
                            jEntries.emplace_back(k,6*slot+3+j,angular[j]);
                        }
                    }
                }
                if (maxPenetration <= config.penetrationSlop + config.positionTolerance) break;
                J.setFromTriplets(jEntries.begin(),jEntries.end());
                W.setFromTriplets(wEntries.begin(),wEntries.end());
                const Eigen::SparseMatrix<double> absJ=J.cwiseAbs(), absW=W.cwiseAbs();
                const double lipschitz = (absJ*(absW*(absJ.transpose()*Eigen::VectorXd::Ones(count)))).maxCoeff();
                if (!std::isfinite(lipschitz) || lipschitz <= 1e-15) {
                    ++stats.positionUnconvergedCount; break;
                }
                Eigen::VectorXd lambda = Eigen::VectorXd::Zero(count);
                double residual = 0.0;
                for (std::uint32_t iteration = 0; iteration < config.graphIterations; ++iteration) {
                    Eigen::VectorXd next = lambda + (target - J*(W*(J.transpose()*lambda))) / lipschitz;
                    for (Eigen::Index k=0;k<count;++k) if (!rows[indices[k]].bilateral) next[k]=std::max(next[k],0.0);
                    residual = lipschitz * (next - lambda).lpNorm<Eigen::Infinity>();
                    lambda = next;
                    ++stats.positionGraphIterations;
                    if (residual <= config.positionTolerance) break;
                }
                stats.positionGraphResidual = std::max(stats.positionGraphResidual, residual);
                if (residual > config.positionTolerance) ++stats.positionUnconvergedCount;
                const Eigen::VectorXd delta = W * (J.transpose() * lambda);
                if (!delta.allFinite()) { ++stats.positionUnconvergedCount; break; }
                double alpha = 1.0;
                double angle = 0.0;
                for (std::size_t k = 0; k < nodes.size(); ++k) {
                    const double length = delta.segment<3>(6*k).norm();
                    const double budget = std::max(0.0, config.maxPositionCorrectionPerStep - totalTranslation[k].norm());
                    if (length > budget && length > 0.0) alpha = std::min(alpha, budget / length);
                    angle = std::max(angle, delta.segment<3>(6*k+3).norm());
                }
                if (angle > 0.0) alpha = std::min(alpha, std::max(0.0, config.maxAngularCorrectionPerStep-totalAngle) / angle);
                /** @brief 在提交前评估真实旋转锚点误差，阻止线性化增量放大总约束违反量。 */
                auto violation = [&](double step) {
                    std::vector<Eigen::Vector3d> positions(bodies.size());
                    std::vector<Eigen::Quaterniond> rotations(bodies.size());
                    for (std::size_t i=0;i<bodies.size();++i) if (bodies[i].entity) {
                        positions[i]=bodies[i].entity->getPosition();
                        rotations[i]=bodies[i].entity->getTransformCR().getRotation();
                    }
                    for (std::size_t k=0;k<nodes.size();++k) {
                        positions[nodes[k]]+=step*delta.segment<3>(6*k);
                        const Eigen::Vector3d da=step*delta.segment<3>(6*k+3);
                        if (da.norm()>1e-12) rotations[nodes[k]]=Eigen::Quaterniond(Eigen::AngleAxisd(da.norm(),da.normalized()))*rotations[nodes[k]];
                    }
                    double error=0.0;
                    for (auto r:indices) {
                        const auto& c=rows[r];
                        const double gap=c.distanceConstraint ? c.length-(positions[c.b]-positions[c.a]).norm() :
                            c.normal.dot(positions[c.b]+rotations[c.b]*c.localB-positions[c.a]-rotations[c.a]*c.localA);
                        const double amount=c.bilateral ? gap : std::min(gap+config.penetrationSlop,0.0);
                        error+=amount*amount;
                    }
                    return error;
                };
                const double previousError=violation(0.0);
                bool accepted=false;
                for (int attempt=0;attempt<12;++attempt) {
                    const double nextError=violation(alpha);
                    if (std::isfinite(nextError) && nextError<=previousError+1e-20) { accepted=true; break; }
                    alpha*=0.5;
                }
                if (!accepted) { ++stats.positionUnconvergedCount; break; }
                if (alpha <= 1e-12) break;
                // 所有增量都来自同一快照，至此才统一写回；不按接触边即时推开。
                for (std::size_t k = 0; k < nodes.size(); ++k) {
                    auto& body = bodies[nodes[k]];
                    const Eigen::Vector3d dx = alpha * delta.segment<3>(6*k);
                    const Eigen::Vector3d da = alpha * delta.segment<3>(6*k+3);
                    body.entity->setPosition(body.entity->getPosition() + dx);
                    totalTranslation[k] += dx;
                    if (da.norm() > 1e-12) {
                        const Eigen::Matrix3d rotation = Eigen::AngleAxisd(da.norm(), da.normalized()).toRotationMatrix();
                        const Eigen::Vector3d momentum = body.inverseInertiaTensor.completeOrthogonalDecomposition().solve(body.entity->getAngularVelocity());
                        body.inverseInertiaTensor = rotation * body.inverseInertiaTensor * rotation.transpose();
                        body.entity->setRotation(Eigen::Quaterniond(rotation) * body.entity->getTransformCR().getRotation());
                        body.entity->setAngular(body.inverseInertiaTensor * momentum);
                    }
                }
                totalAngle += alpha * angle;
            }
            for (auto r : indices) {
                const auto& c = rows[r];
                const auto& a = bodies[c.a]; const auto& b = bodies[c.b];
                double gap = c.normal.dot(b.entity->getPosition() + b.entity->getTransformCR().getRotation()*c.localB
                    - a.entity->getPosition() - a.entity->getTransformCR().getRotation()*c.localA);
                if (c.distanceConstraint) gap=c.length-(b.entity->getPosition()-a.entity->getPosition()).norm();
                if (c.bilateral) gap=-std::abs(gap);
                stats.maxResidualPenetration = std::max(stats.maxResidualPenetration, std::max(-gap,0.0));
            }
        }
    }
}
