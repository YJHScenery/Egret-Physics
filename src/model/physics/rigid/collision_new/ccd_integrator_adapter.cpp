/**
 * @file        ccd_integrator_adapter.cpp
 * @brief       CCD 刚体运动预测工具实现
 * @details     严格采用半隐式欧拉方案：
 *              平动 → v(t)=v₀+a·t, p(t)=p₀+v(t)·t
 *              转动 → ω(t)=ω₀+α·t, q(t) 用 ω(t) 做四元数积分
 *              与 SemiImplicitEulerIntegratorStrategy 严格一致。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-23
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "ccd_integrator_adapter.h"

namespace egret
{
    Transform CcdIntegratorAdapter::predictTransform(
        const Transform& initialTransform,
        const Eigen::Vector3d& linearVelocity,
        const Eigen::Vector3d& angularVelocity,
        const Eigen::Vector3d& linearAcceleration,
        const Eigen::Vector3d& angularAcceleration,
        const double t)
    {
        
        //  平动：严格半隐式欧拉
        //  v(t) = v₀ + a·t
        //  p(t) = p₀ + v(t)·t = p₀ + v₀·t + a·t²
        
        const Eigen::Vector3d velocityAtT = linearVelocity + linearAcceleration * t;
        const Eigen::Vector3d predictedTranslation =
            initialTransform.getTranslation() + velocityAtT * t;

        
        //  转动：严格半隐式欧拉
        //  ω(t) = ω₀ + α·t
        //  q(t) 用 ω(t) 做四元数积分：dq/dt = 0.5 * ω(t)_q * q
        
        Eigen::Quaterniond predictedRotation = initialTransform.getRotation();

        const Eigen::Vector3d angularVelocityAtT = angularVelocity + angularAcceleration * t;
        const double angularSpeed = angularVelocityAtT.norm();

        if (angularSpeed >= 1e-10) {
            // 四元数微分：dq/dt = 0.5 * ω_q * q
            const Eigen::Quaterniond omegaQuat(
                0.0,
                angularVelocityAtT.x(),
                angularVelocityAtT.y(),
                angularVelocityAtT.z());

            Eigen::Quaterniond deltaQ = omegaQuat * predictedRotation;
            deltaQ.coeffs() *= 0.5;
            predictedRotation.coeffs() += deltaQ.coeffs() * t;
            predictedRotation.normalize();
        }

        return Transform(predictedTranslation, predictedRotation, initialTransform.getScale());
    }

} // namespace egret
