/**
 * @file        motion_utils.h
 * @brief       n 维刚体运动的类型别名与工具函数
 * @details     定义 position_type、velocity_type、attitude_type、
 *              惯性张量类型等 n 维泛化物理量，以及反对称性验证、
 *              SO(n) 验证、矩阵指数等工具函数。所有模板受 ValidDimension<Dimension> 约束。
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

#ifndef EGRET_PHYSICS_MOTION_UTILS_H
#define EGRET_PHYSICS_MOTION_UTILS_H

#include "dimension_concept.h"
#include <Eigen/Dense>
#include <unsupported/Eigen/CXX11/Tensor>
#include <cmath>
#include <cstdint>

namespace egret
{
    //  类型别名 —— n 维泛化物理量

    /// @brief 位置向量（n 维列向量）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using position_type = Eigen::Matrix<double, Dimension, 1>;

    /// @brief 线速度向量（n 维列向量）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using velocity_type = Eigen::Matrix<double, Dimension, 1>;

    /// @brief 线加速度向量（n 维列向量）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using acceleration_type = Eigen::Matrix<double, Dimension, 1>;

    /// @brief 力向量（n 维列向量）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using force_type = Eigen::Matrix<double, Dimension, 1>;

    /// @brief 姿态矩阵（SO(n) 元素，n×n 正交矩阵且行列式为 +1）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using attitude_type = Eigen::Matrix<double, Dimension, Dimension>;

    /// @brief 角速度（𝔰𝔬(n) 元素，n×n 实反对称矩阵）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using angular_velocity_type = Eigen::Matrix<double, Dimension, Dimension>;

    /// @brief 角加速度（𝔰𝔬(n) 元素，n×n 实反对称矩阵）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using angular_acceleration_type = Eigen::Matrix<double, Dimension, Dimension>;

    /// @brief 力矩（𝔰𝔬(n) 元素，n×n 实反对称矩阵）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using torque_type = Eigen::Matrix<double, Dimension, Dimension>;

    /// @brief 惯性张量（四阶张量，将 𝔰𝔬(n) 映射到 𝔰𝔬(n)）
    /// @details 分量满足 I_{ijkl} = -I_{jikl} = -I_{ijlk} = I_{klij}
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using inertia_tensor_type = Eigen::TensorFixedSize<double, Eigen::Sizes<Dimension, Dimension, Dimension, Dimension>>
    ;

    /// @brief 扁平化惯性矩阵（N² × N²，用于高效计算 𝓘[Ω] 和 𝓘⁻¹）
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    using inertia_matrix_flat_type = Eigen::Matrix<double, Dimension * Dimension, Dimension * Dimension>;


    //  运动模型枚举

    /**
     * @brief   运动模型分级
     * @details 按描述精度从低到高：
     *          - Linear：纯匀速平移，无旋转
     *          - Screw：恒速螺旋运动（推荐默认模型）
     *          - Accelerated：含线加速度与角加速度
     */
    enum class MotionModel : std::uint8_t
    {
        Linear, ///< 纯平移模型
        Screw, ///< 螺旋运动模型（推荐默认）
        Accelerated ///< 加速运动模型
    };


    //  合规性查验工具函数

    /**
     * @brief   判断矩阵是否为反对称矩阵
     * @details 检查 mat + matᵀ 的 Frobenius 范数是否小于给定容差。
     *          反对称矩阵满足 matᵀ = -mat。
     *
     * @tparam  N   矩阵维度
     * @param   mat 待查验矩阵
     * @param   eps 容差（默认 1e-12）
     * @return  true 若矩阵在容差范围内反对称
     */
    template <unsigned int N>
    bool isAntisymmetric(const Eigen::Matrix<double, N, N>& mat, double eps = 1e-12)
    {
        return (mat + mat.transpose()).norm() < eps;
    }

    /**
     * @brief   判断矩阵是否属于 SO(n)
     * @details 检查正交性（R·Rᵀ ≈ I）且行列式 ≈ +1。
     *
     * @tparam  N   矩阵维度
     * @param   mat 待查验矩阵
     * @param   eps 容差（默认 1e-10）
     * @return  true 若矩阵在容差范围内为特殊正交矩阵
     */
    template <unsigned int N>
    bool isSpecialOrthogonal(const Eigen::Matrix<double, N, N>& mat, double eps = 1e-10)
    {
        const auto shouldBeIdentity = mat * mat.transpose();
        const auto identity = Eigen::Matrix<double, N, N>::Identity();
        return (shouldBeIdentity - identity).norm() < eps
            && std::abs(mat.determinant() - 1.0) < eps;
    }

    /**
     * @brief   验证四阶惯性张量的对称性
     * @details 检查 I_{ijkl} = -I_{jikl}、I_{ijkl} = -I_{ijlk}、I_{ijkl} = I_{klij}。
     *
     * @tparam  N      空间维度
     * @param   tensor 待查验的四阶张量
     * @param   eps    容差（默认 1e-12）
     * @return  true 若满足全部对称性约束
     */
    template <unsigned int N>
    bool isValidInertiaTensor(const inertia_tensor_type<N>& tensor, double eps = 1e-12)
    {
        for (unsigned int i = 0; i < N; ++i) {
            for (unsigned int j = 0; j < N; ++j) {
                for (unsigned int k = 0; k < N; ++k) {
                    for (unsigned int l = 0; l < N; ++l) {
                        const double v = tensor(i, j, k, l);
                        // 前两指标反对称
                        if (std::abs(v + tensor(j, i, k, l)) > eps) return false;
                        // 后两指标反对称
                        if (std::abs(v + tensor(i, j, l, k)) > eps) return false;
                        // 前后对调对称
                        if (std::abs(v - tensor(k, l, i, j)) > eps) return false;
                    }
                }
            }
        }
        return true;
    }


    //  范数计算

    /**
     * @brief   反对称矩阵的谱范数（最大奇异值）
     * @details 对 𝔰𝔬(n) 元素 Ω，谱范数 ‖Ω‖₂ 等于其在各二维旋转平面上的最大角速度。
     *          对于 n=3，等于角速度向量的模 ‖ω‖。
     *          实现上使用 JacobiSVD 计算最大奇异值。
     *
     * @tparam  N     矩阵维度
     * @param   omega 反对称矩阵
     * @return  谱范数 ‖Ω‖₂
     */
    template <unsigned int N>
    double spectralNorm(const Eigen::Matrix<double, N, N>& omega)
    {
        Eigen::JacobiSVD<Eigen::Matrix<double, N, N>> svd(omega);
        return svd.singularValues()(0); // 最大奇异值
    }

    /**
     * @brief   矩阵的 Frobenius 范数
     * @details ‖M‖_F = sqrt(Σ_{i,j} M_{ij}²)。
     *          对反对称矩阵，‖Ω‖_F 是谱范数的上界，保守因子 ≤ √⌊n/2⌋。
     *
     * @tparam  N     矩阵维度
     * @param   mat   任意矩阵
     * @return  Frobenius 范数
     */
    template <unsigned int N>
    double frobeniusNorm(const Eigen::Matrix<double, N, N>& mat)
    {
        return mat.norm(); // Eigen::Matrix::norm() 默认即 Frobenius 范数
    }


    //  矩阵运算

    /**
     * @brief   矩阵李括号（交换子）
     * @details [A, B] = A·B - B·A。
     *          当 A, B ∈ 𝔰𝔬(n) 时，结果仍在 𝔰𝔬(n) 中。
     *
     * @tparam  N 矩阵维度
     * @param   A 左操作数
     * @param   B 右操作数
     * @return  交换子 [A, B]
     */
    template <unsigned int N>
    Eigen::Matrix<double, N, N> lieBracket(
        const Eigen::Matrix<double, N, N>& A,
        const Eigen::Matrix<double, N, N>& B)
    {
        return A * B - B * A;
    }

    /**
     * @brief   反对称矩阵的指数映射 exp: 𝔰𝔬(n) → SO(n)
     * @details 使用截断泰勒级数计算矩阵指数。
     *          对反对称矩阵（特征值为纯虚数），级数收敛迅速。
     *          当 ‖Θ‖ 较小时（dt 很小），前 10 项已充分精确。
     *
     * @tparam  N       矩阵维度
     * @param   Theta   反对称矩阵（𝔰𝔬(n) 元素）
     * @param   maxTerms 泰勒级数最大项数（默认 24）
     * @return  exp(Θ) ∈ SO(n)
     */
    template <unsigned int N>
    Eigen::Matrix<double, N, N> matrixExponential(
        const Eigen::Matrix<double, N, N>& Theta,
        int maxTerms = 24)
    {
        Eigen::Matrix<double, N, N> result = Eigen::Matrix<double, N, N>::Identity();
        Eigen::Matrix<double, N, N> term = Eigen::Matrix<double, N, N>::Identity();

        double factorial = 1.0;

        for (int k = 1; k <= maxTerms; ++k) {
            factorial *= static_cast<double>(k);
            term = term * Theta; // term = Θ^k
            const double coeff = 1.0 / factorial;

            // 提前退出：当贡献足够小时停止
            if (coeff * term.norm() < 1e-16)
                break;

            result += coeff * term;
        }

        return result;
    }


    //  惯性张量运算

    /**
     * @brief   将四阶惯性张量扁平化为 N² × N² 矩阵
     * @details 将分量 I_{ijkl} 映射到矩阵的第 (i·N + j) 行、第 (k·N + l) 列。
     *          该矩阵将 vec(Ω) 映射为 vec(L)，其中 L = 𝓘[Ω]。
     *
     * @tparam  N      空间维度
     * @param   tensor 四阶惯性张量
     * @return  扁平化矩阵（N² × N²）
     */
    template <unsigned int N>
    inertia_matrix_flat_type<N> flattenInertiaTensor(const inertia_tensor_type<N>& tensor)
    {
        inertia_matrix_flat_type<N> flat;
        flat.setZero();

        for (unsigned int i = 0; i < N; ++i) {
            for (unsigned int j = 0; j < N; ++j) {
                const unsigned int row = i * N + j;
                for (unsigned int k = 0; k < N; ++k) {
                    for (unsigned int l = 0; l < N; ++l) {
                        const unsigned int col = k * N + l;
                        flat(row, col) = tensor(i, j, k, l);
                    }
                }
            }
        }

        return flat;
    }

    /**
     * @brief   应用惯性张量：L = 𝓘[Ω]
     * @details 将反对称矩阵 Ω 扁平化为 N² 维向量，经扁平化惯性矩阵映射后再还原为 N×N 矩阵。
     *
     * @tparam  N      空间维度
     * @param   I_flat 扁平化惯性矩阵（N² × N²）
     * @param   Omega  角速度（𝔰𝔬(n) 反对称矩阵）
     * @return  角动量 L ∈ 𝔰𝔬(n)
     */
    template <unsigned int N>
    Eigen::Matrix<double, N, N> applyInertiaTensor(
        const inertia_matrix_flat_type<N>& I_flat,
        const Eigen::Matrix<double, N, N>& Omega)
    {
        // 扁平化 Ω → N² 维列向量
        Eigen::Matrix<double, N * N, 1> omega_vec;
        for (unsigned int i = 0; i < N; ++i)
            for (unsigned int j = 0; j < N; ++j)
                omega_vec(i * N + j) = Omega(i, j);

        // 矩阵乘法
        const Eigen::Matrix<double, N * N, 1> L_vec = I_flat * omega_vec;

        // 还原为 N×N 矩阵
        Eigen::Matrix<double, N, N> L;
        for (unsigned int i = 0; i < N; ++i)
            for (unsigned int j = 0; j < N; ++j)
                L(i, j) = L_vec(i * N + j);

        return L;
    }

    /**
     * @brief   应用惯性张量的逆：α = 𝓘⁻¹[τ_net]
     * @details 通过求解线性方程组 I_flat · α_vec = tau_net_vec 得到角加速度。
     *          使用 ColPivHouseholderQR 分解以保证数值稳定性。
     *
     * @tparam  N       空间维度
     * @param   I_flat  扁平化惯性矩阵（N² × N²）
     * @param   TauNet  净力矩（𝔰𝔬(n) 反对称矩阵）
     * @return  角加速度 α ∈ 𝔰𝔬(n)
     */
    template <unsigned int N>
    Eigen::Matrix<double, N, N> applyInertiaTensorInverse(
        const inertia_matrix_flat_type<N>& I_flat,
        const Eigen::Matrix<double, N, N>& TauNet)
    {
        // 扁平化 τ → N² 维列向量
        Eigen::Matrix<double, N * N, 1> tau_vec;
        for (unsigned int i = 0; i < N; ++i)
            for (unsigned int j = 0; j < N; ++j)
                tau_vec(i * N + j) = TauNet(i, j);

        // 求解线性方程组 I_flat · α_vec = tau_vec
        const Eigen::Matrix<double, N * N, 1> alpha_vec =
            I_flat.colPivHouseholderQr().solve(tau_vec);

        // 还原为 N×N 矩阵
        Eigen::Matrix<double, N, N> alpha;
        for (unsigned int i = 0; i < N; ++i)
            for (unsigned int j = 0; j < N; ++j)
                alpha(i, j) = alpha_vec(i * N + j);

        return alpha;
    }
} // namespace egret

#endif // EGRET_PHYSICS_MOTION_UTILS_H
