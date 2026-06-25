/**
 * @file        generalization_rigid_motion.h
 * @brief       n 维泛化刚体运动状态管理器
 * @details     保存并管理 n 维刚体的完整运动属性（位置、姿态、速度、角速度、
 *              加速度、力、角加速度、力矩、质量、惯性张量），提供合规性查验的设置器，
 *              支持三级运动模型（LINEAR / SCREW / ACCELERATED）的状态推进。
 *
 *              力 ↔ 加速度通过牛顿第二定律互转（F = m·a），
 *              力矩 ↔ 角加速度通过高维欧拉方程互转（τ = 𝓘[α] + [Ω, 𝓘[Ω]]）。
 *
 * @author      jehor
 * @date        2026/6/25
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Motion
 */

#ifndef EGRET_PHYSICS_GENERALIZATION_RIGID_MOTION_H
#define EGRET_PHYSICS_GENERALIZATION_RIGID_MOTION_H

#include "dimension_concept.h"
#include "motion_utils.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace egret
{
    /**
     * @brief   n 维泛化刚体运动状态
     * @details 模板类，管理单个刚体在 n 维欧氏空间中的全部运动学与动力学属性。
     *          所有设置器在赋值前执行合规性查验（维度 ≥ 2 已在编译期由 ValidDimension 保证）。
     *
     * @tparam  Dimension  空间维度（≥ 2）
     */
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    class GeneralizationRigidMotion
    {
    public:
        // ── 类型别名 ──
        /// @name 对外暴露的类型别名
        /// @{

        using Position = position_type<Dimension>;
        using Velocity = velocity_type<Dimension>;
        using Acceleration = acceleration_type<Dimension>;
        using Force = force_type<Dimension>;
        using Attitude = attitude_type<Dimension>;
        using AngularVelocity = angular_velocity_type<Dimension>;
        using AngularAcceleration = angular_acceleration_type<Dimension>;
        using Torque = torque_type<Dimension>;
        using InertiaTensor = inertia_tensor_type<Dimension>;
        using InertiaMatrixFlat = inertia_matrix_flat_type<Dimension>;

        /// @}

        //  构造与析构

        /**
         * @brief   默认构造
         * @details 位置/速度/加速度/力 置零；
         *          姿态初始化为单位矩阵；
         *          角速度/角加速度/力矩 置零；
         *          质量默认为 1.0（防止除零）；
         *          惯性张量初始化为单位四阶张量（球对称惯性）；
         *          运动模型默认为 Screw。
         */
        GeneralizationRigidMotion();

        /**
         * @brief   参数构造
         * @details 传入质量和惯性张量，其余属性取默认值。
         *
         * @param   mass           质量（必须 > 0）
         * @param   inertia_tensor 惯性张量（必须满足四阶张量对称性）
         * @throws  std::invalid_argument 若 mass ≤ 0 或惯性张量不合规
         */
        GeneralizationRigidMotion(double mass, const InertiaTensor& inertia_tensor);

        ~GeneralizationRigidMotion() = default;

        //  质量与惯性

        /**
         * @brief   设置质量
         * @param   mass 质量（纯量）
         * @throws  std::invalid_argument 若 mass ≤ 0
         */
        void setMass(double mass);

        /**
         * @brief   获取质量
         * @return  质量值（>= 0）
         */
        [[nodiscard]] double mass() const { return m_mass; }


        /**
         * @brief   设置惯性张量
         * @param   tensor 四阶惯性张量
         * @throws  std::invalid_argument 若不满足对称性约束
         *          （I_{ijkl} = -I_{jikl} = -I_{ijlk} = I_{klij}）
         */
        void setInertiaTensor(const InertiaTensor& tensor);

        /**
         * @brief   获取惯性张量（只读）
         * @return  四阶惯性张量的常量引用
         */
        [[nodiscard]] const InertiaTensor& inertiaTensor() const { return m_inertia_tensor; }


        //  平动属性
        /**
         * @brief   设置位置
         * @param   pos n 维位置向量（无约束）
         */
        void setPosition(const Position& pos) { m_position = pos; }

        /// @brief 获取位置
        [[nodiscard]] const Position& position() const { return m_position; }


        /**
         * @brief   设置线速度
         * @param   vel n 维速度向量（无约束）
         */
        void setVelocity(const Velocity& vel) { m_velocity = vel; }

        /// @brief 获取线速度
        [[nodiscard]] const Velocity& velocity() const { return m_velocity; }


        /**
         * @brief   设置线加速度
         * @details 自动同步更新力：F = m·a。
         * @param   acc n 维加速度向量（无约束）
         */
        void setAcceleration(const Acceleration& acc);

        /// @brief 获取线加速度
        [[nodiscard]] const Acceleration& acceleration() const { return m_acceleration; }


        /**
         * @brief   设置力
         * @details 自动同步更新加速度：a = F / m（m > 0 由 setMass 保证）。
         * @param   f n 维力向量（无约束）
         */
        void setForce(const Force& f)
        {
            m_force = f;
            m_acceleration = decltype(m_acceleration)::Zero();

            if (m_mass != 0) {
                m_acceleration = m_force / m_mass;
            }

        }

        /// @brief 获取力
        [[nodiscard]] const Force& force() const { return m_force; }


        //  转动属性
        /**
         * @brief   设置姿态矩阵
         * @param   att n×n 矩阵
         * @throws  std::invalid_argument 若非 SO(n)（不满足正交性或行列式 ≠ +1）
         */
        void setAttitude(const Attitude& att);

        /// @brief 获取姿态矩阵
        [[nodiscard]] const Attitude& attitude() const { return m_attitude; }

        /**
         * @brief   设置角速度
         * @param   omega n×n 矩阵
         * @throws  std::invalid_argument 若非反对称（Ωᵀ ≠ -Ω）
         */
        void setAngularVelocity(const AngularVelocity& omega);

        /// @brief 获取角速度
        [[nodiscard]] const AngularVelocity& angularVelocity() const { return m_angular_velocity; }


        /**
         * @brief   设置角加速度
         * @details 自动通过高维欧拉方程同步更新力矩：
         *          τ = 𝓘[α] + [Ω, 𝓘[Ω]]
         *          其中 [·,·] 为矩阵交换子（李括号），𝓘[Ω] 为角动量。
         *
         * @param   alpha n×n 矩阵
         * @throws  std::invalid_argument 若非反对称
         */
        void setAngularAcceleration(const AngularAcceleration& alpha);

        /// @brief 获取角加速度
        [[nodiscard]] const AngularAcceleration& angularAcceleration() const { return m_angular_acceleration; }


        /**
         * @brief   设置力矩
         * @details 自动通过高维欧拉方程同步更新角加速度：
         *          α = 𝓘⁻¹(τ - [Ω, 𝓘[Ω]])
         *          其中陀螺项 [Ω, 𝓘[Ω]] 使用当前角速度和惯性张量计算。
         *
         * @param   tau n×n 矩阵
         * @throws  std::invalid_argument 若非反对称
         */
        void setTorque(const Torque& tau);
        /// @brief 获取力矩
        [[nodiscard]] const Torque& torque() const { return m_torque; }


        //  包围半径

        /**
         * @brief   设置包围半径
         * @details 用于 CCD 保守推进速度上界计算。
         *          r = max_{p∈Shape} ‖p - centroid‖
         * @param   r 包围半径（必须 ≥ 0）
         * @throws  std::invalid_argument 若 r < 0
         */
        void setBoundingRadius(double r);


        /// @brief 获取包围半径
        [[nodiscard]] double boundingRadius() const { return m_bounding_radius; }


        //  运动模型

        /**
         * @brief   设置运动模型
         * @param   model LINEAR / Screw / Accelerated
         */
        void setMotionModel(MotionModel model) { m_motion_model = model; }

        /// @brief 获取当前运动模型
        [[nodiscard]] MotionModel motionModel() const { return m_motion_model; }


        //  状态推进

        /**
         * @brief   按当前运动模型推进 dt 时间
         * @details 假设在 [0, dt] 内加速度和角加速度保持恒定（极短时间假设）。
         *
         *          - LINEAR：仅更新位置（x += v·dt）
         *          - SCREW：更新位置 + 姿态（R *= exp(Ω·dt)）
         *          - ACCELERATED：更新位置、速度、姿态、角速度
         *            （x += v·dt + ½a·dt², v += a·dt,
         *             R *= exp(Ω·dt + ½α·dt²), Ω += α·dt）
         *
         * @param   dt 时间步长（必须 ≥ 0）
         * @throws  std::invalid_argument 若 dt < 0
         */
        void advance(double dt);

    private:
        //  内部推进实现

        /**
         * @brief LINEAR 模型推进：仅平移
         */
        void advanceLinear(double dt);

        /**
         * @brief SCREW 模型推进：匀速平移 + 恒角速度旋转
         */
        void advanceScrew(double dt);

        /**
         * @brief ACCELERATED 模型推进：匀加速平移 + 匀角加速度旋转
         */
        void advanceAccelerated(double dt);

        //  力矩 ↔ 角加速度 同步
        /**
         * @brief   由角加速度计算力矩（欧拉方程正向）
         * @details τ = 𝓘[α] + [Ω, 𝓘[Ω]]
         *          调用时机：setAngularAcceleration、setAngularVelocity、setInertiaTensor
         */
        void syncTorqueFromAngularAcceleration();

        /**
         * @brief   由力矩计算角加速度（欧拉方程逆向）
         * @details α = 𝓘⁻¹(τ - [Ω, 𝓘[Ω]])
         *          调用时机：setTorque
         */
        void syncAngularAccelerationFromTorque();

        /**
         * @brief   初始化默认惯性张量（球对称单位张量）
         * @details 构造一个等效于"质量分布完全球对称"的简单惯性张量，
         *          满足 𝓘[Ω] = Ω（恒等映射）。
         *          仅用于默认构造，用户应通过 setInertiaTensor 设置实际值。
         *
         *          对每对 (i, j)，i ≠ j，设定四个对称相关分量：
         *          - I_{ijij} = +1/2
         *          - I_{jiij} = -1/2  （前两指标反对称）
         *          - I_{ijji} = -1/2  （后两指标反对称）
         *          - I_{jiji} = +1/2  （前后对调对称）
         */
        void initDefaultInertiaTensor();


        //  成员变量
        double m_mass; ///< 质量（> 0）

        // 平动
        Position m_position; ///< 质心位置
        Velocity m_velocity; ///< 线速度
        Acceleration m_acceleration; ///< 线加速度（与 m_force 互转）
        Force m_force; ///< 合外力（与 m_acceleration 互转）

        // 转动
        Attitude m_attitude; ///< 姿态矩阵 R ∈ SO(n)
        AngularVelocity m_angular_velocity; ///< 角速度 Ω ∈ 𝔰𝔬(n)
        AngularAcceleration m_angular_acceleration; ///< 角加速度 α ∈ 𝔰𝔬(n)（与 m_torque 互转）
        Torque m_torque; ///< 力矩 τ ∈ 𝔰𝔬(n)（与 m_angular_acceleration 互转）

        // 惯性
        InertiaTensor m_inertia_tensor; ///< 四阶惯性张量
        InertiaMatrixFlat m_inertia_flat; ///< 扁平化惯性矩阵（N²×N²，用于高效计算）

        // 运动模型
        MotionModel m_motion_model; ///< 当前运动模型

        // CCD 辅助
        double m_bounding_radius; ///< 包围半径（≥ 0）
    };


    template <unsigned int Dimension> requires ValidDimension<Dimension>
    GeneralizationRigidMotion<Dimension>::GeneralizationRigidMotion()
        : m_mass(1.0)
          , m_position(Position::Zero())
          , m_velocity(Velocity::Zero())
          , m_acceleration(Acceleration::Zero())
          , m_force(Force::Zero())
          , m_attitude(Attitude::Identity())
          , m_angular_velocity(AngularVelocity::Zero())
          , m_angular_acceleration(AngularAcceleration::Zero())
          , m_torque(Torque::Zero())
          , m_inertia_tensor()
          , m_inertia_flat(InertiaMatrixFlat::Zero())
          , m_motion_model(MotionModel::Screw)
          , m_bounding_radius(0.0)
    {
        // 初始化惯性张量为球对称单位张量
        initDefaultInertiaTensor();
        m_inertia_flat = flattenInertiaTensor<Dimension>(m_inertia_tensor);
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    GeneralizationRigidMotion<Dimension>::GeneralizationRigidMotion(double mass, const InertiaTensor& inertia_tensor)
        : GeneralizationRigidMotion() // 委托默认构造初始化其余字段
    {
        setMass(mass);
        setInertiaTensor(inertia_tensor);
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setMass(double mass)

    {
        if (mass < 0.0) {
            throw std::invalid_argument(std::format(
                "GeneralizationRigidMotion::setMass: 质量必须为正数，当前值 = {}", std::to_string(mass)));
        }
        m_mass = mass;
        // 质量变更后同步更新力（保持加速度不变）
        m_force = m_mass * m_acceleration;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setInertiaTensor(const InertiaTensor& tensor)

    {
        if (!isValidInertiaTensor<Dimension>(tensor)) {
            throw std::invalid_argument(
                std::format("GeneralizationRigidMotion::setInertiaTensor: {}", "惯性张量不满足四阶对称性约束"));
        }
        m_inertia_tensor = tensor;
        m_inertia_flat = flattenInertiaTensor<Dimension>(tensor);
        // 惯性张量变更后同步更新力矩（保持角加速度不变）
        syncTorqueFromAngularAcceleration();
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setAcceleration(const Acceleration& acc)
    {
        m_acceleration = acc;
        m_force = m_mass * m_acceleration;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setAttitude(const Attitude& att)
    {
        if (!isSpecialOrthogonal<Dimension>(att)) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::setAttitude: "
                "姿态矩阵不满足 SO(n) 约束（RᵀR = I 且 det(R) = +1）");
        }
        m_attitude = att;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setAngularVelocity(const AngularVelocity& omega)
    {
        if (!isAntisymmetric<Dimension>(omega)) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::setAngularVelocity: "
                "角速度矩阵必须为反对称矩阵（Ωᵀ = -Ω）");
        }
        m_angular_velocity = omega;
        // 角速度变更影响陀螺项 → 同步更新力矩
        syncTorqueFromAngularAcceleration();
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setAngularAcceleration(const AngularAcceleration& alpha)
    {
        if (!isAntisymmetric<Dimension>(alpha)) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::setAngularAcceleration: "
                "角加速度矩阵必须为反对称矩阵");
        }
        m_angular_acceleration = alpha;
        syncTorqueFromAngularAcceleration();
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setTorque(const Torque& tau)
    {
        if (!isAntisymmetric<Dimension>(tau)) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::setTorque: "
                "力矩矩阵必须为反对称矩阵");
        }
        m_torque = tau;
        syncAngularAccelerationFromTorque();
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::setBoundingRadius(double r)
    {
        if (r < 0.0) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::setBoundingRadius: "
                "包围半径不能为负数，当前值 = " + std::to_string(r));
        }
        m_bounding_radius = r;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::advance(double dt)
    {
        if (dt < 0.0) {
            throw std::invalid_argument(
                "GeneralizationRigidMotion::advance: 时间步长不能为负数，dt = "
                + std::to_string(dt));
        }
        if (dt == 0.0) return;

        switch (m_motion_model) {
        case MotionModel::Linear:
            advanceLinear(dt);
            break;
        case MotionModel::Screw:
            advanceScrew(dt);
            break;
        case MotionModel::Accelerated:
            advanceAccelerated(dt);
            break;
        }
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::advanceLinear(double dt)

    {
        m_position += m_velocity * dt;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::advanceScrew(double dt)

    {
        m_position += m_velocity * dt;

        const AngularVelocity Theta = m_angular_velocity * dt;
        m_attitude = m_attitude * matrixExponential<Dimension>(Theta);
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::advanceAccelerated(double dt)

    {
        // 平动：x += v·dt + ½a·dt²，v += a·dt
        m_position += m_velocity * dt + 0.5 * m_acceleration * (dt * dt);
        m_velocity += m_acceleration * dt;

        // 转动：积分角速度 Θ = Ω·dt + ½α·dt²
        const AngularAcceleration Theta = m_angular_velocity * dt
            + 0.5 * m_angular_acceleration * (dt * dt);
        m_attitude = m_attitude * matrixExponential<Dimension>(Theta);

        // 角速度更新：Ω += α·dt
        m_angular_velocity += m_angular_acceleration * dt;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::syncTorqueFromAngularAcceleration()

    {
        // 惯性项：𝓘[α]
        const AngularVelocity inertial_term =
            applyInertiaTensor<Dimension>(m_inertia_flat, m_angular_acceleration);

        // 陀螺项：[Ω, 𝓘[Ω]]
        const AngularVelocity angular_momentum =
            applyInertiaTensor<Dimension>(m_inertia_flat, m_angular_velocity);
        const AngularVelocity gyro_term =
            lieBracket<Dimension>(m_angular_velocity, angular_momentum);

        m_torque = inertial_term + gyro_term;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::syncAngularAccelerationFromTorque()

    {
        // 陀螺项：[Ω, 𝓘[Ω]]
        const AngularVelocity angular_momentum =
            applyInertiaTensor<Dimension>(m_inertia_flat, m_angular_velocity);
        const AngularVelocity gyro_term =
            lieBracket<Dimension>(m_angular_velocity, angular_momentum);

        // 净力矩：τ_net = τ - [Ω, 𝓘[Ω]]
        const Torque tau_net = m_torque - gyro_term;

        // α = 𝓘⁻¹[τ_net]
        m_angular_acceleration =
            applyInertiaTensorInverse<Dimension>(m_inertia_flat, tau_net);
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GeneralizationRigidMotion<Dimension>::initDefaultInertiaTensor()
    {
        m_inertia_tensor.setZero();

        for (unsigned int i = 0; i < Dimension; ++i) {
            for (unsigned int j = 0; j < Dimension; ++j) {
                if (i == j) continue;
                // I_{ijij} = 1/2
                m_inertia_tensor(i, j, i, j) = 0.5;
                // I_{jiij} = -1/2 （前两指标反对称）
                m_inertia_tensor(j, i, i, j) = -0.5;
                // I_{ijji} = -1/2 （后两指标反对称）
                m_inertia_tensor(i, j, j, i) = -0.5;
                // I_{jiji} = +1/2 （前后对调对称）
                m_inertia_tensor(j, i, j, i) = 0.5;
            }
        }
    }
} // namespace egret

#endif // EGRET_PHYSICS_GENERALIZATION_RIGID_MOTION_H
