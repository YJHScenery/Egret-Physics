/**
 * @file        ccd_solver_adapter.cpp
 * @brief       Solver 与维度泛化 CCD 的桥接层实现
 * @details     包含所有重型模板头文件（CCD/GJK/EPA/GeneralizationRigidMotion），
 *              仅在当前翻译单元编译，避免污染包含该适配器头文件的其他翻译单元。
 *
 * @author      jehor
 * @date        2026/7/10
 * @version     2.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "ccd_solver_adapter.h"

// ── 重型模板头文件 —— 仅在 .cpp 中引入 ──

#include "motion/generalization_rigid_motion.h"
#include "motion/motion_utils.h"
#include "rigid/physical_entity.h"
#include "transform.h"

#include <memory>

import collision;


namespace egret
{
    // 内部辅助：向量 ↔ 反对称矩阵 转换
    /**
     * @brief   将 3D 角速度向量转换为反对称矩阵
     * @details ω = (ωx, ωy, ωz) → Ω ∈ 𝔰𝔬(3)
     */
    static Eigen::Matrix3d vectorToSkewSymmetric(const Eigen::Vector3d& omega)
    {
        Eigen::Matrix3d mat{};
        mat << 0.0, -omega.z(), omega.y(),
            omega.z(), 0.0, -omega.x(),
            -omega.y(), omega.x(), 0.0;
        return mat;
    }


    // 内部辅助：包围半径计算
    /**
     * @brief   从形状的 AABB 估算包围半径
     */
    static double computeBoundingRadiusFromShape(
        const std::shared_ptr<ShapeBase>& shape,
        const Transform& transform)
    {
        if (shape == nullptr) {
            return 0.0;
        }

        const AABB aabb = shape->getAABB(transform);
        const Eigen::Vector3d halfExtent = (aabb.max - aabb.min) * 0.5;
        return halfExtent.norm();
    }


    // 内部辅助：PhysicalEntity → GeneralizationRigidMotion<3>
    /**
     * @brief   从 PhysicalEntity 构造维度泛化运动状态
     * @details 使用 Screw 模型（恒定速度），加速度和角加速度置零。
     */
    static GeneralizationRigidMotion<3> physicalEntityToMotion(
        const PhysicalEntity& entity)
    {
        GeneralizationRigidMotion<3> motion{};

        // 平动
        motion.setPosition(entity.getTransformCR().getTranslation());
        motion.setVelocity(entity.getVelocity());
        motion.setAcceleration(Eigen::Vector3d::Zero());

        // 转动
        motion.setAttitude(entity.getTransformCR().getRotationMatrix());
        motion.setAngularVelocity(vectorToSkewSymmetric(entity.getAngularVelocity()));
        motion.setAngularAcceleration(Eigen::Matrix3d::Zero());

        // 质量
        motion.setMass(entity.getMass());

        // 包围半径
        double radius{0.0};
        if (entity.getShape() != nullptr) {
            radius = computeBoundingRadiusFromShape(entity.getShape(), entity.getTransformCR());
        }
        motion.setBoundingRadius(radius);

        // 力/力矩（CCD 不使用）
        motion.setForce(Eigen::Vector3d::Zero());
        motion.setTorque(Eigen::Matrix3d::Zero());

        motion.setMotionModel(MotionModel::Screw);

        return motion;
    }


    // 内部辅助：ShapeBase → SupportCallback
    /**
     * @brief   从 ShapeBase 创建 CCD 所需的支撑函数回调
     * @details 内部维护一个 Transform 实例，每次调用时原地更新位置和旋转，
     *          通过 ShapeBase::support() 计算世界空间支撑点。
     */
    static typename ContinuousCollisionDetector<3>::SupportCallback
    makeSupportCallback(const std::shared_ptr<ShapeBase>& shape)
    {
        auto transform = std::make_shared<Transform>();

        return [shape, transform](
            const Eigen::Vector3d& worldDir,
            const Eigen::Vector3d& position,
            const Eigen::Matrix3d& rotation) -> Eigen::Vector3d
        {
            if (shape == nullptr) {
                return position;
            }

            transform->setTranslation(position);
            transform->setRotation(Eigen::Quaterniond(rotation));
            return shape->support(worldDir, *transform);
        };
    }

    /**
     * @brief   从 ShapeBase 创建支撑回调（裸指针版本，无所有权）
     */
    static typename ContinuousCollisionDetector<3>::SupportCallback
    makeSupportCallbackRaw(ShapeBase* shape)
    {
        auto transform = std::make_shared<Transform>();

        return [shape, transform](
            const Eigen::Vector3d& worldDir,
            const Eigen::Vector3d& position,
            const Eigen::Matrix3d& rotation) -> Eigen::Vector3d
        {
            if (shape == nullptr) {
                return position;
            }

            transform->setTranslation(position);
            transform->setRotation(Eigen::Quaterniond(rotation));
            return shape->support(worldDir, *transform);
        };
    }


    // 公共接口实现
    std::optional<EPAResult3D> runGjkEpaOnEntities(
        PhysicalEntity* entityA,
        PhysicalEntity* entityB)
    {
        if (entityA == nullptr || entityB == nullptr) {
            return std::nullopt;
        }

        if (entityA->getShape() == nullptr || entityB->getShape() == nullptr) {
            return std::nullopt;
        }

        // 创建支撑函数回调（复用 makeSupportCallbackRaw）
        auto supportA = makeSupportCallbackRaw(entityA->getShape().get());
        auto supportB = makeSupportCallbackRaw(entityB->getShape().get());

        // 提取当前世界空间姿态
        const Transform& transA = entityA->getTransformCR();
        const Transform& transB = entityB->getTransformCR();
        const Eigen::Vector3d posA = transA.getTranslation();
        const Eigen::Vector3d posB = transB.getTranslation();
        const Eigen::Matrix3d rotA = transA.getRotationMatrix();
        const Eigen::Matrix3d rotB = transB.getRotationMatrix();

        // GJK 检测
        GilbertJohnsonKeerthi<3> gjk(supportA, supportB, posA, rotA, posB, rotB);
        gjk.initialize();
        GJKResult<3> gjkResult = gjk.run();

        if (!gjkResult.isCollided) {
            return std::nullopt;
        }

        // EPA 提取碰撞信息
        ExpandingPolytopeAlgorithm<3> epa(gjkResult, supportA, supportB, posA, rotA, posB, rotB);
        EPAResult<3> epaResult = epa.run();

        return epaResult;
    }

    std::optional<AdapterCcdResult> runCcdOnEntities(
        PhysicalEntity* entityA,
        PhysicalEntity* entityB,
        double dt)
    {
        if (entityA == nullptr || entityB == nullptr) {
            return std::nullopt;
        }

        if (entityA->getShape() == nullptr || entityB->getShape() == nullptr) {
            return std::nullopt;
        }

        // 转换为维度泛化运动状态
        GeneralizationRigidMotion<3> motionA = physicalEntityToMotion(*entityA);
        GeneralizationRigidMotion<3> motionB = physicalEntityToMotion(*entityB);

        // 创建支撑回调（使用裸指针避免 shared_ptr 拷贝开销）
        auto supportA = makeSupportCallbackRaw(entityA->getShape().get());
        auto supportB = makeSupportCallbackRaw(entityB->getShape().get());

        // 构造 CCD 检测器
        ContinuousCollisionDetector<3> detector(
            motionA, motionB,
            std::move(supportA), std::move(supportB),
            MotionModel::Screw
        );

        // 执行 CCD（启用 TOI 精化，80 次迭代匹配旧版二分搜索精度）
        CCDResult<3> ccdResult = detector.run(dt, true, 80);

        if (!ccdResult.isCollided) {
            return std::nullopt;
        }

        // 转换为轻量级结果
        AdapterCcdResult result{};
        result.isCollided = true;
        result.timeOfImpact = ccdResult.timeOfImpact;
        result.normal = ccdResult.normal;
        result.penetrationDepth = ccdResult.penetrationDepth;
        result.contactPoint = ccdResult.contactPoint;

        return result;
    }
} // namespace egret
