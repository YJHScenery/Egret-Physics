/**
 * @file        ccd_integrator_adapter.h
 * @brief       CCD 刚体运动预测工具头文件
 * @details     提供刚体运动状态的时间插值功能，供 CCD 二分搜索使用。
 *              给定初始位姿和速度，预测任意中间时刻 t 的 Transform。
 *              采用与半隐式欧拉积分器一致的刚体运动方程，
 *              确保 CCD 预测轨迹与求解器实际积分轨迹一致。
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

#ifndef EGRET_PHYSICS_CCD_INTEGRATOR_ADAPTER_H
#define EGRET_PHYSICS_CCD_INTEGRATOR_ADAPTER_H

#include <Eigen/Dense>
#include "transform.h"

namespace egret
{
    /**
     * @brief       CCD 运动预测工具类
     * @details     提供纯函数式的刚体运动预测——给定初态、速度与加速度，
     *              计算任意中间时刻 t 的 Transform，用于 CCD 的 TOI 二分搜索。
     *              严格采用与 SemiImplicitEulerIntegratorStrategy 一致的半隐式欧拉方案。
     *              该类不持有状态，所有方法均为静态工具函数。
     *
     * @remark      平动：v(t) = v₀ + a·t,  p(t) = p₀ + v₀·t + a·t²
     *              转动：ω(t) = ω₀ + α·t,  q(t) 由 ω(t) 做四元数积分
     *              与求解器严格一致。
     */
    class CcdIntegratorAdapter
    {
    public:
        /**
         * @brief       预测刚体在时刻 t 的位姿（严格半隐式欧拉）
         * @param       initialTransform   t=0 时刻的初始位姿
         * @param       linearVelocity     t=0 时刻的线速度 v₀（世界坐标系）
         * @param       angularVelocity    t=0 时刻的角速度 ω₀（世界坐标系）
         * @param       linearAcceleration 线加速度 a = F/m
         * @param       angularAcceleration 角加速度 α = I⁻¹·τ
         * @param       t                  目标时刻（≥ 0）
         * @return      预测的 t 时刻 Transform
         *
         * @note        平动：先 v(t) = v₀ + a·t，再 p(t) = p₀ + v(t)·t
         *              转动：先 ω(t) = ω₀ + α·t，再用 ω(t) 做四元数积分
         */
        [[nodiscard]] static Transform predictTransform(
            const Transform& initialTransform,
            const Eigen::Vector3d& linearVelocity,
            const Eigen::Vector3d& angularVelocity,
            const Eigen::Vector3d& linearAcceleration,
            const Eigen::Vector3d& angularAcceleration,
            double t);
    };

} // namespace egret

#endif // EGRET_PHYSICS_CCD_INTEGRATOR_ADAPTER_H
