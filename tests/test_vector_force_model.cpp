/** @file test_vector_force_model.cpp @brief 验证时变力场、作用域边界和无效输入。 */
#include "model/physics/field/vector_force_model.h"
#include <cmath>
#include <limits>
#include <stdexcept>

/** @brief 执行模型行为回归检查。 */
int main()
{
    using egret::VectorForceModel;
    using Eigen::Vector3d;
    int evaluations = 0;
    VectorForceModel field(VectorForceModel::sphere(Vector3d::Zero(), 2),
        [&evaluations](double t, const Vector3d& p) -> Vector3d { ++evaluations; return {t, p.x(), 3}; });
    if (!field.sample(4, {2,0,0}).isApprox(Vector3d(4,2,3))) return 1;
    if (!field.sample(4, {3,0,0}).isZero() || evaluations != 1) return 2;
    auto region = VectorForceModel::box({-1,-2,-3},{1,2,3});
    if (!region({1,2,3}) || region({1.01,0,0})) return 3;
    VectorForceModel cosine(region, VectorForceModel::cosine({2,0,0}, 1));
    if (!cosine.sample(0, Vector3d::Zero()).isApprox(Vector3d(2,0,0))) return 4;
    if (cosine.sample(3.141592653589793, Vector3d::Zero()).x() > -1.999) return 5;
    try { field.sample(std::numeric_limits<double>::infinity(), Vector3d::Zero()); return 6; }
    catch (const std::invalid_argument&) {}
    try { VectorForceModel invalid(region, [](double, const Vector3d&) -> Vector3d { return Vector3d::Constant(std::numeric_limits<double>::quiet_NaN()); });
        invalid.sample(0, Vector3d::Zero()); return 7; }
    catch (const std::runtime_error&) {}
    return 0;
}
