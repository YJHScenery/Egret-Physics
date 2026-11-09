/** @file vector_force_model.h @brief 与求解器和渲染解耦的时变三维力场采样模型。 */
#pragma once
#include <Eigen/Core>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <utility>

namespace egret
{
    /** @brief 表示与实体标识无关、返回牛顿单位力向量的场。 */
    class VectorForceModel
    {
    public:
        using Region = std::function<bool(const Eigen::Vector3d&)>;
        using ForceFunction = std::function<Eigen::Vector3d(double, const Eigen::Vector3d&)>;

        /** @brief 保存作用域和场函数；调用方负责回调捕获对象的生命周期及线程安全。 */
        VectorForceModel(Region region, ForceFunction force)
            : m_region(std::move(region)), m_force(std::move(force))
        {
            if (!m_region || !m_force) throw std::invalid_argument("场函数和作用域不能为空");
        }

        /** @brief 在指定时间和位置采样；作用域外返回零力。 */
        [[nodiscard]] Eigen::Vector3d sample(double time, const Eigen::Vector3d& position) const
        {
            if (!std::isfinite(time) || !position.allFinite()) throw std::invalid_argument("场采样输入必须有限");
            if (!m_region(position)) return Eigen::Vector3d::Zero();
            Eigen::Vector3d value = m_force(time, position);
            if (!value.allFinite()) throw std::runtime_error("场函数返回非有限值");
            return value;
        }

        /** @brief 构造包含边界的球形作用域。 */
        [[nodiscard]] static Region sphere(Eigen::Vector3d center, double radius)
        {
            if (!center.allFinite() || !std::isfinite(radius) || radius < 0.0)
                throw std::invalid_argument("球形作用域参数无效");
            return [center, radius](const Eigen::Vector3d& p) { return (p - center).stableNorm() <= radius; };
        }

        /** @brief 构造包含边界的轴对齐盒形作用域。 */
        [[nodiscard]] static Region box(Eigen::Vector3d minimum, Eigen::Vector3d maximum)
        {
            if (!minimum.allFinite() || !maximum.allFinite() || (minimum.array() > maximum.array()).any())
                throw std::invalid_argument("盒形作用域参数无效");
            return [minimum, maximum](const Eigen::Vector3d& p) {
                return (p.array() >= minimum.array()).all() && (p.array() <= maximum.array()).all();
            };
        }

        /** @brief 构造方向和振幅由向量指定的余弦时变场。 */
        [[nodiscard]] static ForceFunction cosine(Eigen::Vector3d amplitude, double angularFrequency, double phase = 0.0)
        {
            if (!amplitude.allFinite() || !std::isfinite(angularFrequency) || !std::isfinite(phase))
                throw std::invalid_argument("余弦场参数无效");
            return [amplitude, angularFrequency, phase](double t, const Eigen::Vector3d&) -> Eigen::Vector3d {
                return amplitude * std::cos(angularFrequency * t + phase);
            };
        }
    private:
        Region m_region;
        ForceFunction m_force;
    };
}
