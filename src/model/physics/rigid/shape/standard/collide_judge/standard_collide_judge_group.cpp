/**
* @file        filename.h
 * @brief       一句话概述文件的主要功能
 * @details     详细描述文件的用途、设计思路、使用注意事项等。
 *              可以写多行，说明模块在整个系统中的位置。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        创建日期 (如 2025-03-15)
 * @version     版本号 (如 1.0.0)
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     许可证 (如 GPL, MIT, BSD 等)
 *
 * @ingroup     所属模块组 (可选，用于分组)
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "physics_utils.h"

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

#include <array>

// ============================================================================
// 匿名命名空间：GJK / EPA 算法内部辅助结构及函数
// ============================================================================
namespace {

/// GJK 最大迭代次数
constexpr int kGjkMaxIters = 32;

/// GJK 收敛容差
constexpr double kGjkEpsilon = 1e-8;

/// EPA 最大迭代次数
constexpr int kEpaMaxIters = 8;

/// EPA 收敛容差
constexpr double kEpaTolerance = 1e-6;

/**
 * @brief  Minkowski 差 support 函数。
 * @details 计算 C = A − B 在方向 dir 上的最远点：
 *          C.support(dir) = A.support(dir) − B.support(−dir)
 *
 * @param a       形状 A
 * @param ta      形状 A 的变换
 * @param b       形状 B
 * @param tb      形状 B 的变换
 * @param dir     世界空间方向（无需归一化，但建议归一化）
 * @return        Minkowski 差中 dir 方向的最远点（世界坐标）
 */
Eigen::Vector3d minkowskiSupport(const egret::ShapeBase& a, const egret::Transform& ta,
                                  const egret::ShapeBase& b, const egret::Transform& tb,
                                  const Eigen::Vector3d& dir)
{
    return a.support(dir, ta) - b.support(-dir, tb);
}

/**
 * @brief  GJK 单形体，存储最多 4 个 Minkowski 差顶点。
 */
struct Simplex
{
    Eigen::Vector3d v[4];  ///< 顶点数组（v[0..n-1] 有效）
    int n = 0;             ///< 当前顶点数（1～4）

    /// 赋值初始化列表
    Simplex& operator=(std::initializer_list<Eigen::Vector3d> list)
    {
        n = 0;
        for (const auto& p : list) v[n++] = p;
        return *this;
    }

    /// 在最前面插入顶点（用于 GJK 进化）
    void pushFront(const Eigen::Vector3d& p)
    {
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = p;
        n = std::min(n + 1, 4);
    }

    /// 用新的单点重置
    void reset(const Eigen::Vector3d& p)
    {
        v[0] = p;
        n = 1;
    }
};

// ---------------------------------------------------------------------------
// Simplex 进化子函数 — 各维度下寻找离原点最近的特征并更新搜索方向
// ---------------------------------------------------------------------------

/**
 * @brief  在线段 AB 上寻找离原点最近的点，更新单形体和搜索方向。
 * @return true  原点在线段上（已包含在单形体内）
 * @return false 未包含，但已更新单形体和方向
 */
bool closestOnLine(Simplex& s, Eigen::Vector3d& dir)
{
    const Eigen::Vector3d& a = s.v[0];
    const Eigen::Vector3d& b = s.v[1];
    const Eigen::Vector3d ab = b - a;
    const double denom = ab.squaredNorm();

    // 退化线段保护
    if (denom < kGjkEpsilon * kGjkEpsilon) {
        dir = -a;
        s.reset(a);
        return dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon;
    }

    const double t = std::clamp(-a.dot(ab) / denom, 0.0, 1.0);

    if (t <= 0.0) {
        dir = -a;
        s.reset(a);
    } else if (t >= 1.0) {
        dir = -b;
        s.reset(b);
    } else {
        // 最近点在线段上，保留线段
        dir = -(a + t * ab);
    }

    return dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon;
}

/**
 * @brief  在三角形 ABC 上寻找离原点最近的点，更新单形体和搜索方向。
 * @return true  原点在三角形内
 * @return false 未包含，但已更新单形体和方向
 */
bool closestOnTriangle(Simplex& s, Eigen::Vector3d& dir)
{
    const Eigen::Vector3d& a = s.v[0];
    const Eigen::Vector3d& b = s.v[1];
    const Eigen::Vector3d& c = s.v[2];

    const Eigen::Vector3d ab = b - a;
    const Eigen::Vector3d ac = c - a;
    const Eigen::Vector3d ao = -a;

    // 检查顶点 A 区域：法线 (AB×AC)×AC 指向外侧判断
    const Eigen::Vector3d abCrossAc = ab.cross(ac);
    if (abCrossAc.cross(ac).dot(ao) > 0.0) {
        if (ac.dot(ao) > 0.0) { s = {a, c}; return closestOnLine(s, dir); }
        if (ab.dot(ao) > 0.0) { s = {a, b}; return closestOnLine(s, dir); }
        dir = ao;
        s.reset(a);
        return dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon;
    }

    const Eigen::Vector3d bc = c - b;
    const Eigen::Vector3d bo = -b;

    // 检查顶点 B 区域
    if (bc.cross(ab).cross(ab).dot(bo) > 0.0) {
        if (ab.dot(bo) > 0.0) { s = {b, a}; return closestOnLine(s, dir); }
        if (bc.dot(bo) > 0.0) { s = {b, c}; return closestOnLine(s, dir); }
        dir = bo;
        s.reset(b);
        return dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon;
    }

    const Eigen::Vector3d co = -c;

    // 检查顶点 C 区域
    if (ac.cross(bc).cross(bc).dot(co) > 0.0) {
        if (ac.dot(co) > 0.0) { s = {c, a}; return closestOnLine(s, dir); }
        if (bc.dot(co) > 0.0) { s = {c, b}; return closestOnLine(s, dir); }
        dir = co;
        s.reset(c);
        return dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon;
    }

    // 原点投影在三角形内部 — 沿面法线搜索
    if (abCrossAc.dot(ao) > 0.0) {
        dir = abCrossAc;
    } else {
        dir = -abCrossAc;
        s = {a, c, b};  // 翻转缠绕顺序以匹配法线方向
    }

    return false;
}

/**
 * @brief  在四面体 ABCD 中寻找离原点最近的点。
 * @return true  原点在四面体内部（碰撞确认）
 * @return false 原点在某面外侧，已降维为三角形/线段/点
 */
bool closestInTetrahedron(Simplex& s, Eigen::Vector3d& dir)
{
    const Eigen::Vector3d& a = s.v[0];
    const Eigen::Vector3d& b = s.v[1];
    const Eigen::Vector3d& c = s.v[2];
    const Eigen::Vector3d& d = s.v[3];

    // 面 ABC — 法线指向远离 D 的一侧（外侧）
    Eigen::Vector3d faceN = (b - a).cross(c - a);
    if (faceN.dot(a - d) < 0.0) faceN = -faceN;
    if (faceN.dot(-a) > 0.0) { s = {a, b, c}; return closestOnTriangle(s, dir); }

    // 面 ACD
    faceN = (c - a).cross(d - a);
    if (faceN.dot(a - b) < 0.0) faceN = -faceN;
    if (faceN.dot(-a) > 0.0) { s = {a, c, d}; return closestOnTriangle(s, dir); }

    // 面 ADB
    faceN = (d - a).cross(b - a);
    if (faceN.dot(a - c) < 0.0) faceN = -faceN;
    if (faceN.dot(-a) > 0.0) { s = {a, d, b}; return closestOnTriangle(s, dir); }

    // 面 BDC
    faceN = (d - b).cross(c - b);
    if (faceN.dot(b - a) < 0.0) faceN = -faceN;
    if (faceN.dot(-b) > 0.0) { s = {b, d, c}; return closestOnTriangle(s, dir); }

    // 原点在所有面的内侧 → 在四面体内部 → 碰撞！
    return true;
}

/**
 * @brief  进化 GJK 单形体：根据当前维度选择对应的最近点搜索子程序。
 * @return true  原点包含在单形体内（碰撞）
 * @return false 未包含，但单形体和搜索方向已更新
 */
bool evolveSimplex(Simplex& s, Eigen::Vector3d& dir)
{
    switch (s.n) {
    case 2:  return closestOnLine(s, dir);
    case 3:  return closestOnTriangle(s, dir);
    case 4:  return closestInTetrahedron(s, dir);
    default: return false;
    }
}

// ---------------------------------------------------------------------------
// GJK 核心
// ---------------------------------------------------------------------------

/**
 * @brief  静态 GJK 碰撞检测。
 * @details 通过迭代构建 Minkowski 差 C = A − B 的单形体，判断 C 是否包含原点。
 *          包含原点 → A 与 B 相交。
 *
 * @param a         形状 A
 * @param ta        形状 A 的变换
 * @param b         形状 B
 * @param tb        形状 B 的变换
 * @param simplex   [out] 当碰撞时包含结果的单形体（至少 4 个顶点）
 * @param iters     [out] 实际迭代次数
 * @return true   碰撞
 * @return false  分离
 */
bool gjkIntersect(const egret::ShapeBase& a, const egret::Transform& ta,
                  const egret::ShapeBase& b, const egret::Transform& tb,
                  Simplex& simplex, int& iters)
{
    // 初始搜索方向：从 A 中心指向 B 中心
    Eigen::Vector3d dir = tb.getTranslation() - ta.getTranslation();
    if (dir.squaredNorm() < kGjkEpsilon * kGjkEpsilon) {
        dir = Eigen::Vector3d::UnitX();
    }

    // 第一步：获取初始 support 点
    simplex.reset(minkowskiSupport(a, ta, b, tb, dir));

    // 搜索方向翻转向原点
    dir = -simplex.v[0];

    for (iters = 0; iters < kGjkMaxIters; ++iters) {
        const Eigen::Vector3d w = minkowskiSupport(a, ta, b, tb, dir);

        // 如果新 support 点没有越过原点，则原点不在 Minkowski 差内 → 无碰撞
        if (w.dot(dir) < 0.0) {
            return false;
        }

        simplex.pushFront(w);

        // 进化单形体并更新搜索方向
        if (evolveSimplex(simplex, dir)) {
            return true; // 原点在单形体内 → 碰撞
        }
    }

    // 达到最大迭代次数未收敛 — 保守返回 false
    return false;
}

// ---------------------------------------------------------------------------
// EPA 简化版 — 从 GJK 单形体计算穿透信息
// ---------------------------------------------------------------------------

/**
 * @brief  从四面体单形体的 4 个三角面中找出离原点最近的面。
 *
 * @param simplex   包含原点的四面体单形体
 * @param normal    [out] 最近面的外法线（指向包含原点的一侧，即从 Minkowski
 *                  差边界指向原点）
 * @return          最近面到原点的距离（穿透深度的初次估计）
 */
double closestFaceFromSimplex(const Simplex& simplex, Eigen::Vector3d& normal)
{
    // 四面体的 4 个三角面（顶点索引）
    static constexpr int faceIndices[4][3] = {
        {0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}
    };

    double bestDist = std::numeric_limits<double>::max();
    Eigen::Vector3d bestNormal = Eigen::Vector3d::UnitY();

    for (const auto& fi : faceIndices) {
        const Eigen::Vector3d& p0 = simplex.v[fi[0]];
        const Eigen::Vector3d& p1 = simplex.v[fi[1]];
        const Eigen::Vector3d& p2 = simplex.v[fi[2]];

        Eigen::Vector3d n = (p1 - p0).cross(p2 - p0);
        const double lenSq = n.squaredNorm();
        if (lenSq < 1e-20) continue;
        n /= std::sqrt(lenSq);

        // 让法线指向包含原点的一侧
        if (n.dot(p0) > 0.0) n = -n;

        const double dist = -n.dot(p0); // 面平面到原点的距离
        if (dist < bestDist) {
            bestDist = dist;
            bestNormal = n;
        }
    }

    normal = bestNormal;
    return bestDist;
}

/**
 * @brief  简化 EPA：通过迭代扩展多面体逼近真实的穿透深度和法线。
 *
 * @details 从 GJK 单形体出发，每次向当前最近面的法线方向采样新 support 点。
 *          如果新点在面外侧（距原点更远），则用其替换多面体中最远的面顶点。
 *
 * @param a / ta    形状 A 及其变换
 * @param b / tb    形状 B 及其变换
 * @param simplex   GJK 碰撞检测输出的单形体（含原点）
 * @param normal    [out] 碰撞法线（世界空间，从 A 指向 B）
 * @param pointA    [out] 形状 A 上的接触点（世界空间）
 * @param pointB    [out] 形状 B 上的接触点（世界空间）
 * @return          穿透深度
 */
double computePenetration(const egret::ShapeBase& a, const egret::Transform& ta,
                          const egret::ShapeBase& b, const egret::Transform& tb,
                          const Simplex& simplex,
                          Eigen::Vector3d& normal,
                          Eigen::Vector3d& pointA,
                          Eigen::Vector3d& pointB)
{
    // Phase 1: 从 GJK 单形体获取初始估计
    Simplex poly = simplex; // 可变副本
    double penetration = closestFaceFromSimplex(poly, normal);

    // Phase 2: 迭代扩展
    for (int iter = 0; iter < kEpaMaxIters; ++iter) {
        // 沿当前法线方向采样 Minkowski 差边界
        const Eigen::Vector3d s = minkowskiSupport(a, ta, b, tb, normal);
        const double supportDist = s.dot(normal);

        const double gap = supportDist - penetration;
        if (gap < kEpaTolerance) {
            break; // 收敛 — 当前面已足够接近真实边界
        }

        // 用新 support 点替换多面体中对该面贡献最小的顶点
        // （在法线方向上投影最远的顶点）
        int replaceIdx = 0;
        double worstDot = poly.v[0].dot(normal);
        for (int i = 1; i < poly.n; ++i) {
            const double d = poly.v[i].dot(normal);
            if (d > worstDot) {
                worstDot = d;
                replaceIdx = i;
            }
        }
        poly.v[replaceIdx] = s;

        // 更新最近面距离和法线
        penetration = closestFaceFromSimplex(poly, normal);
    }

    // 翻转法线以匹配碰撞惯例：法线从 A 指向 B。
    // closestFaceFromSimplex 返回的是 Minkowski 差面 → 原点的方向，
    // 在 C = A − B 中该方向对应 B → A，故需要翻转。
    normal = -normal;

    // 接触点：分别计算 A 和 B 上沿碰撞法线方向的支撑点
    pointA = a.support(normal, ta);
    pointB = b.support(-normal, tb);

    return penetration;
}

} // anonymous namespace

namespace egret
{
    /**
     * @brief  通用的静态 GJK 碰撞检测函数。
     * @details 基于 ShapeBase::support() 支撑函数，使用 GJK 算法检测任意两个
     *          凸形状之间的碰撞，并通过简化 EPA 算法计算穿透深度、碰撞法线和
     *          接触点。不同形状类型无需各自编写专用碰撞检测即可复用此函数。
     *
     * @param shapeA     形状 A（任意凸形状）
     * @param transA     形状 A 的变换
     * @param shapeB     形状 B（任意凸形状）
     * @param transB     形状 B 的变换
     * @param manifold   [out] 碰撞信息输出（法线、穿透深度、接触点）
     * @return true     发生碰撞
     * @return false    未发生碰撞
     */
    bool box_first::collideGeneric(const ShapeBase& shapeA, const Transform& transA, const ShapeBase& shapeB,
        const Transform& transB, ContactManifold& manifold)
    {
        // ---------- Step 1: GJK 碰撞检测 ----------
        Simplex simplex{};
        int iters = 0;
        const bool colliding = gjkIntersect(shapeA, transA, shapeB, transB, simplex, iters);

        if (!colliding) {
            return false;
        }

        // ---------- Step 2: EPA 计算穿透信息 ----------
        Eigen::Vector3d normal;
        Eigen::Vector3d pointA, pointB;
        const double penetration = computePenetration(
            shapeA, transA, shapeB, transB, simplex, normal, pointA, pointB);

        if (penetration <= 0.0) {
            return false;
        }

        // ---------- Step 3: 填充接触流形 ----------
        ContactPoint contact{};
        contact.position = (pointA + pointB) * 0.5;  // 取中点作为接触点
        contact.normal = normal;                       // 法线方向：从 A 指向 B
        contact.penetration = penetration;
        contact.featureA = ContactFeature::Unknown;
        contact.featureB = ContactFeature::Unknown;

        manifold.push_back(contact);
        return true;
    }

    bool box_first::collideBoxBox(const ShapeBox& boxA, const Transform& transA, const ShapeBox& boxB,
                                  const Transform& transB, ContactManifold& manifold)
    {
        const Eigen::Vector3d halfA = boxA.getHalfSize();
        const Eigen::Vector3d halfB = boxB.getHalfSize();

        // 获取两个物体的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> axesA{}, axesB{};
        ShapeBox::getLocalAxes(transA, axesA[0], axesA[1], axesA[2]);
        ShapeBox::getLocalAxes(transB, axesB[0], axesB[1], axesB[2]);

        // 中心连线（从 A 指向 B）
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d t = centerB - centerA;

        // 待测试的轴列表（所有分离轴）
        std::vector<Eigen::Vector3d> axes;
        // 添加 A 的三个面法线
        for (int i = 0; i < 3; ++i)
            axes.push_back(axesA[i]);
        // 添加 B 的三个面法线
        for (int i = 0; i < 3; ++i)
            axes.push_back(axesB[i]);
        // 添加边 × 边得到的 9 个轴
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                Eigen::Vector3d axis = axesA[i].cross(axesB[j]);
                if (axis.norm() > 1e-9) {
                    axes.push_back(axis.normalized());
                }
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 辅助函数：计算一个盒子的半长在给定方向上的投影长度
        auto projectHalfExtent = [](const Eigen::Vector3d& half,
                                    const Eigen::Vector3d& axis,
                                    const std::array<Eigen::Vector3d, 3>& localAxes) -> double
        {
            // 盒子三个局部轴方向上的半长在 axis 上的投影绝对值之和
            double proj = 0.0;
            for (int i = 0; i < 3; ++i) {
                proj += std::abs(axis.dot(localAxes[i])) * half[i];
            }
            return proj;
        };

        // 遍历所有候选轴
        for (const Eigen::Vector3d& axis : axes) {
            // 避免轴过短
            if (axis.squaredNorm() < 1e-9)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            // 计算 A 和 B 在该轴上的投影半径
            const double rA = projectHalfExtent(halfA, unitAxis, axesA);
            const double rB = projectHalfExtent(halfB, unitAxis, axesB);

            // 投影中心之间的距离
            const double centerDist = std::abs(unitAxis.dot(t));
            const double sumRadii = rA + rB;

            // 如果间隔 > 0，则在该轴上分离
            if (centerDist > sumRadii) {
                return false; // 无碰撞
            }

            // 记录最小重叠量（穿透深度）及对应轴
            const double overlap = sumRadii - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（使法线从 A 指向 B）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点：取两个形状在法线方向上的支撑点，再取中点
        // 支撑点函数：给定方向 dir，返回盒子在该方向上最远的点（世界坐标）
        auto supportPoint = [&](const ShapeBox& box, const Transform& trans,
                                const Eigen::Vector3d& dir) -> Eigen::Vector3d
        {
            const Eigen::Vector3d half = box.getHalfSize();
            // 将方向变换到局部坐标系
            const Eigen::Vector3d localDir = trans.getRotation().conjugate() * dir;
            // 局部最远点的坐标
            Eigen::Vector3d localPoint;
            for (int i = 0; i < 3; ++i) {
                localPoint[i] = (localDir[i] >= 0) ? half[i] : -half[i];
            }
            // 变换回世界坐标
            return trans.localToWorld(localPoint);
        };

        const Eigen::Vector3d supportA = supportPoint(boxA, transA, bestAxis);
        const Eigen::Vector3d supportB = supportPoint(boxB, transB, -bestAxis);
        const Eigen::Vector3d contactPoint = (supportA + supportB) * 0.5;

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool box_first::collideBoxCylinder(const ShapeBox& box, const Transform& transA,
                                       const ShapeCylinder& cylinder, const Transform& transB,
                                       ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取圆柱的位置和轴向（世界坐标系）
        const Eigen::Vector3d cylCenter = transB.getTranslation();
        const Eigen::Vector3d cylAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 中心连线（从盒子指向圆柱）
        const Eigen::Vector3d t = cylCenter - boxCenter;

        // 收集所有潜在的分离轴
        std::vector<Eigen::Vector3d> axes;

        // 1. 盒子的三个面法线
        for (int i = 0; i < 3; ++i) {
            axes.push_back(boxAxes[i]);
        }

        // 2. 圆柱的轴向
        axes.push_back(cylAxis);

        // 3. 盒子的边与圆柱轴向的叉积
        for (int i = 0; i < 3; ++i) {
            Eigen::Vector3d cross = boxAxes[i].cross(cylAxis);
            if (cross.norm() > 1e-9) {
                axes.push_back(cross.normalized());
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 计算盒子在给定轴上的投影半长
        auto projectBoxHalfExtent = [&](const Eigen::Vector3d& axis) -> double
        {
            double extent = 0.0;
            for (int i = 0; i < 3; ++i) {
                extent += std::abs(axis.dot(boxAxes[i])) * boxHalfSize[i];
            }
            return extent;
        };

        // 计算圆柱在给定轴上的投影半长
        auto projectCylinderHalfExtent = [&](const Eigen::Vector3d& axis) -> double
        {
            const double axisDotCylAxis = std::abs(axis.dot(cylAxis));
            // 圆柱的投影 = 高度方向投影 + 半径方向投影
            return cylHalfHeight * axisDotCylAxis + cylRadius * std::sqrt(1.0 - axisDotCylAxis * axisDotCylAxis);
        };

        // SAT 测试
        for (const Eigen::Vector3d& axis : axes) {
            if (axis.squaredNorm() < 1e-18)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            const double boxExtent = projectBoxHalfExtent(unitAxis);
            const double cylExtent = projectCylinderHalfExtent(unitAxis);
            const double centerDist = std::abs(unitAxis.dot(t));

            if (centerDist > boxExtent + cylExtent + 1e-9) {
                return false; // 分离
            }

            const double overlap = boxExtent + cylExtent - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（从盒子指向圆柱）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点
        // 将圆柱中心投影到盒子坐标系，找到最近点
        const Eigen::Vector3d localCylCenter = transA.worldToLocal(cylCenter);
        Eigen::Vector3d closestLocalPoint;

        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localCylCenter[i], -boxHalfSize[i], boxHalfSize[i]);
        }

        const Eigen::Vector3d boxContact = transA.localToWorld(closestLocalPoint);

        // 圆柱上的接触点：沿法线方向内缩半径
        const Eigen::Vector3d cylContact = cylCenter - bestAxis * cylRadius;

        const Eigen::Vector3d contactPoint = (boxContact + cylContact) * 0.5;

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool box_first::collideBoxCylindricalShell(const ShapeBox& box, const Transform& transA,
                                               const ShapeCylindricalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同（空心圆柱与实心圆柱的碰撞检测逻辑一致）
        return collideBoxCylinder(box, transA, shell, transB, manifold);
    }

    bool box_first::collideBoxDisk(const ShapeBox& box, const Transform& transA,
                                   const ShapeDisk& disk, const Transform& transB,
                                   ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double diskRadius = disk.getRadius();

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取圆盘的位置和法向量（世界坐标系）
        const Eigen::Vector3d diskCenter = transB.getTranslation();
        const Eigen::Vector3d diskNormal = transB.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 将圆盘中心变换到盒子的局部坐标系
        const Eigen::Vector3d localDiskCenter = transA.worldToLocal(diskCenter);

        // 计算盒子边界
        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        // 在盒子内部找到离圆盘中心最近的点（clamp）
        Eigen::Vector3d closestLocalPoint;
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localDiskCenter[i], min[i], max[i]);
        }

        // 将最近点变换回世界坐标系
        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        // 计算从圆盘中心到最近点的向量
        const Eigen::Vector3d diff = closestWorldPoint - diskCenter;
        const double distSq = diff.squaredNorm();

        // 如果距离大于圆盘半径，则无碰撞
        if (distSq > diskRadius * diskRadius + 1e-9) {
            return false;
        }

        // 计算穿透深度
        const double dist = std::sqrt(distSq);
        const double penetration = diskRadius - dist;

        // 计算碰撞法线
        Eigen::Vector3d normal;
        if (dist > 1e-9) {
            normal = diff.normalized();
        }
        else {
            // 圆盘中心在 Box 内部或恰好在表面：计算到最近面的法线方向
            int bestAxis = 0;
            double bestRatio = 0.0;
            for (int i = 0; i < 3; ++i) {
                const double ratio = (boxHalfSize[i] > 1e-12) ? std::abs(localDiskCenter[i]) / boxHalfSize[i] : 0.0;
                if (ratio > bestRatio) {
                    bestRatio = ratio;
                    bestAxis = i;
                }
            }
            if (bestRatio < 1e-12) {
                // 恰好在 Box 几何中心，回退使用圆盘法向量
                normal = diskNormal;
                if (normal.dot(diskCenter - boxCenter) < 0) {
                    normal = -normal;
                }
            } else {
                normal = Eigen::Vector3d::Zero();
                normal[bestAxis] = (localDiskCenter[bestAxis] >= 0.0) ? 1.0 : -1.0;
                // 将局部法线变换到世界坐标系
                normal = transA.getRotation() * normal;
            }
        }

        // 计算接触点（圆盘上的接触点）
        const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool box_first::collideBoxRing(const ShapeBox& box, const Transform& transA,
                                   const ShapeRing& ring, const Transform& transB,
                                   ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideBoxDisk(box, transA, ring, transB, manifold);
    }

    bool box_first::collideBoxRod(const ShapeBox& box, const Transform& transA,
                                  const ShapeRod& rod, const Transform& transB,
                                  ContactManifold& manifold)
    {
        const Eigen::Vector3d boxHalfSize = box.getHalfSize();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取盒子的局部轴（世界坐标系）
        std::array<Eigen::Vector3d, 3> boxAxes{};
        ShapeBox::getLocalAxes(transA, boxAxes[0], boxAxes[1], boxAxes[2]);

        // 获取杆的位置和方向（世界坐标系）
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的两个端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 找出杆上离盒子中心最近的点
        const Eigen::Vector3d boxCenter = transA.getTranslation();

        // 将杆的端点变换到盒子局部坐标系
        const Eigen::Vector3d localEndA = transA.worldToLocal(rodEndA);
        const Eigen::Vector3d localEndB = transA.worldToLocal(rodEndB);
        const Eigen::Vector3d localCenter = transA.worldToLocal(rodCenter);

        // 计算盒子边界
        const Eigen::Vector3d min = -boxHalfSize;
        const Eigen::Vector3d max = boxHalfSize;

        // 计算线段与 AABB 的最近点
        Eigen::Vector3d closestLocalPoint = localCenter;

        // 先找到线段在盒子内部的投影点
        for (int i = 0; i < 3; ++i) {
            closestLocalPoint[i] = std::clamp(localCenter[i], min[i], max[i]);
        }

        // 将最近点变换回世界坐标系
        const Eigen::Vector3d closestWorldPoint = transA.localToWorld(closestLocalPoint);

        // 计算从最近点到线段的最近点
        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = closestWorldPoint - rodEndA;

        const double c1 = w.dot(v);
        if (c1 <= 0) {
            // 最近点在端点 A
            if ((rodEndA - closestWorldPoint).squaredNorm() > 1e-9) {
                return false;
            }
        }

        double c2 = v.dot(v);
        if (c2 <= c1) {
            // 最近点在端点 B
            if ((rodEndB - closestWorldPoint).squaredNorm() > 1e-9) {
                return false;
            }
        }

        // 最近点在线段内部
        double t = c1 / c2;
        const Eigen::Vector3d closestOnRod = rodEndA + t * v;

        // 计算距离
        const Eigen::Vector3d diff = closestWorldPoint - closestOnRod;
        const double distSq = diff.squaredNorm();

        // 如果距离大于某个极小值，认为没有碰撞
        if (distSq > 1e-9) {
            return false;
        }

        // 计算法线（使用最近点指向杆中心的方向）
        Eigen::Vector3d normal = closestWorldPoint - rodCenter;
        if (normal.squaredNorm() < 1e-12) {
            // 杆中心在 Box 内部或恰好在表面：计算到最近面的法线方向
            int bestAxis = 0;
            double bestRatio = 0.0;
            for (int i = 0; i < 3; ++i) {
                const double ratio = (boxHalfSize[i] > 1e-12) ? std::abs(localCenter[i]) / boxHalfSize[i] : 0.0;
                if (ratio > bestRatio) {
                    bestRatio = ratio;
                    bestAxis = i;
                }
            }
            if (bestRatio < 1e-12) {
                // 恰好在 Box 几何中心，回退默认方向（避开杆轴向）
                normal = Eigen::Vector3d::UnitX();
                if (std::abs(rodAxis.dot(normal)) > 0.9) {
                    normal = Eigen::Vector3d::UnitY();
                }
            } else {
                normal = Eigen::Vector3d::Zero();
                normal[bestAxis] = (localCenter[bestAxis] >= 0.0) ? 1.0 : -1.0;
                normal = transA.getRotation() * normal;
            }
        }
        else {
            normal.normalize();
        }

        // 确保法线从盒子指向杆
        if (normal.dot(rodCenter - boxCenter) < 0) {
            normal = -normal;
        }

        // 接触点取中点
        const Eigen::Vector3d contactPoint = (closestWorldPoint + closestOnRod) * 0.5;

        manifold.push_back({contactPoint, normal, std::sqrt(distSq)});
        return true;
    }

    bool box_first::collideBoxSphere(const ShapeBox& box, const Transform& transA, const ShapeSphere& sphere,
                                     const Transform& transB, ContactManifold& manifold)
    {
        const double radius = sphere.getRadius();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();
        const Eigen::Vector3d half = box.getHalfSize();

        // 将球心变换到盒子的局部坐标系
        const Eigen::Vector3d localCenter = transA.worldToLocal(sphereCenter);

        // 计算局部 AABB 边界：[-half, half]
        const Eigen::Vector3d min = -half;
        const Eigen::Vector3d max = half;

        // 找到 AABB 上离球心最近的点（clamp）
        Eigen::Vector3d closestLocal;
        for (int i = 0; i < 3; ++i) {
            closestLocal[i] = std::clamp(localCenter[i], min[i], max[i]);
        }

        // 计算球心到最近点的距离平方
        const Eigen::Vector3d diff = localCenter - closestLocal;
        const double distSq = diff.squaredNorm();
        const double radiusSq = radius * radius;

        if (distSq >= radiusSq) {
            return false; // 无碰撞
        }

        const double dist = std::sqrt(distSq);
        const double penetration = radius - dist;

        // 法线方向：从最近点指向球心（世界坐标系）
        Eigen::Vector3d normalLocal;
        if (dist < 1e-9) {
            // 球心在 Box 内部或恰好在表面：计算到最近面的法线方向
            int bestAxis = 0;
            double bestRatio = 0.0;
            for (int i = 0; i < 3; ++i) {
                const double ratio = (half[i] > 1e-12) ? std::abs(localCenter[i]) / half[i] : 0.0;
                if (ratio > bestRatio) {
                    bestRatio = ratio;
                    bestAxis = i;
                }
            }
            if (bestRatio < 1e-12) {
                // 球心恰好在 Box 几何中心，回退默认方向
                normalLocal = Eigen::Vector3d::UnitY();
            } else {
                normalLocal = Eigen::Vector3d::Zero();
                normalLocal[bestAxis] = (localCenter[bestAxis] >= 0.0) ? 1.0 : -1.0;
            }
        } else {
            normalLocal = diff.normalized();
        }
        const Eigen::Vector3d normalWorld = transA.getRotation() * normalLocal;

        // 接触点：球体表面上离长方体最近的点
        // 等价于 sphereCenter - normalWorld * radius
        // 为了与球体碰撞实现保持一致，取穿透深度的一半偏移
        const Eigen::Vector3d contactPoint = sphereCenter - normalWorld * (radius - penetration * 0.5);

        manifold.push_back({contactPoint, normalWorld, penetration});
        return true;
    }

    bool box_first::collideBoxSphericalShell(const ShapeBox& box, const Transform& transA,
                                             const ShapeSphericalShell& shell, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideBoxSphere(box, transA, shell, transB, manifold);
    }

    bool cylinder_first::collideCylinderCylinder(const ShapeCylinder& cylinderA, const Transform& transA,
                                                 const ShapeCylinder& cylinderB, const Transform& transB,
                                                 ContactManifold& manifold)
    {
        const double radiusA = cylinderA.getRadius();
        const double radiusB = cylinderB.getRadius();
        const double halfHeightA = cylinderA.getHalfHeight();
        const double halfHeightB = cylinderB.getHalfHeight();

        // 获取圆柱的位置和轴向
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d axisA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d axisB = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 圆柱的两个端点
        const Eigen::Vector3d endA1 = centerA + axisA * halfHeightA;
        const Eigen::Vector3d endA2 = centerA - axisA * halfHeightA;
        const Eigen::Vector3d endB1 = centerB + axisB * halfHeightB;
        const Eigen::Vector3d endB2 = centerB - axisB * halfHeightB;

        // 中心连线
        const Eigen::Vector3d t = centerB - centerA;

        // 潜在的分离轴
        std::vector<Eigen::Vector3d> axes;

        // 1. 圆柱A的轴向
        axes.push_back(axisA);

        // 2. 圆柱B的轴向
        axes.push_back(axisB);

        // 3. 两个轴向的叉积
        Eigen::Vector3d crossAB = axisA.cross(axisB);
        if (crossAB.norm() > 1e-9) {
            axes.push_back(crossAB.normalized());
        }

        // 4. 圆柱A端点到圆柱B轴线的垂线
        for (const Eigen::Vector3d& endA : {endA1, endA2}) {
            const Eigen::Vector3d diff = endA - centerB;
            const Eigen::Vector3d proj = (diff.dot(axisB)) * axisB;
            Eigen::Vector3d perp = diff - proj;
            if (perp.norm() > 1e-9) {
                axes.push_back(perp.normalized());
            }
        }

        // 5. 圆柱B端点到圆柱A轴线的垂线
        for (const Eigen::Vector3d& endB : {endB1, endB2}) {
            const Eigen::Vector3d diff = endB - centerA;
            const Eigen::Vector3d proj = (diff.dot(axisA)) * axisA;
            Eigen::Vector3d perp = diff - proj;
            if (perp.norm() > 1e-9) {
                axes.push_back(perp.normalized());
            }
        }

        // 6. 端点连线的方向（两个圆柱端面之间的潜在分离轴）
        for (const Eigen::Vector3d& endA : {endA1, endA2}) {
            for (const Eigen::Vector3d& endB : {endB1, endB2}) {
                Eigen::Vector3d line = endB - endA;
                if (line.norm() > 1e-9) {
                    axes.push_back(line.normalized());
                }
            }
        }

        double minOverlap = std::numeric_limits<double>::max();
        Eigen::Vector3d bestAxis = Eigen::Vector3d::Zero();

        // 计算圆柱在给定轴上的投影半长
        auto projectCylinder = [](double radius, double halfHeight,
                                  const Eigen::Vector3d& axis,
                                  const Eigen::Vector3d& cylinderAxis) -> double
        {
            const double axisDot = std::abs(axis.dot(cylinderAxis));
            return halfHeight * axisDot + radius * std::sqrt(1.0 - axisDot * axisDot);
        };

        // SAT 测试
        for (const Eigen::Vector3d& axis : axes) {
            if (axis.squaredNorm() < 1e-18)
                continue;

            const Eigen::Vector3d unitAxis = axis.normalized();

            const double extentA = projectCylinder(radiusA, halfHeightA, unitAxis, axisA);
            const double extentB = projectCylinder(radiusB, halfHeightB, unitAxis, axisB);
            const double centerDist = std::abs(unitAxis.dot(t));

            if (centerDist > extentA + extentB + 1e-9) {
                return false;
            }

            const double overlap = extentA + extentB - centerDist;
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = unitAxis;
            }
        }

        // 确定法线方向（从A指向B）
        if (bestAxis.dot(t) < 0) {
            bestAxis = -bestAxis;
        }

        // 计算接触点
        // 将圆柱B中心投影到圆柱A的轴线上，找到最近点
        const Eigen::Vector3d diffBA = centerB - centerA;
        const double projParam = diffBA.dot(axisA);
        const double clampedProj = std::clamp(projParam, -halfHeightA, halfHeightA);
        const Eigen::Vector3d closestOnAxisA = centerA + axisA * clampedProj;

        // 将圆柱A中心投影到圆柱B的轴线上，找到最近点
        const Eigen::Vector3d diffAB = centerA - centerB;
        const double projParamB = diffAB.dot(axisB);
        const double clampedProjB = std::clamp(projParamB, -halfHeightB, halfHeightB);
        const Eigen::Vector3d closestOnAxisB = centerB + axisB * clampedProjB;

        // 计算两个最近点之间的连线方向
        const Eigen::Vector3d lineAB = closestOnAxisB - closestOnAxisA;
        const double lineLength = lineAB.norm();

        Eigen::Vector3d contactPoint;
        if (lineLength > 1e-9) {
            // 接触点在连线上，向内偏移半径
            const Eigen::Vector3d lineDir = lineAB / lineLength;
            const Eigen::Vector3d pointOnA = closestOnAxisA + lineDir * radiusA;
            const Eigen::Vector3d pointOnB = closestOnAxisB - lineDir * radiusB;
            contactPoint = (pointOnA + pointOnB) * 0.5;
        }
        else {
            // 轴线相交或平行且重合，使用最佳轴方向
            contactPoint = closestOnAxisA + bestAxis * radiusA;
        }

        manifold.push_back({contactPoint, bestAxis, minOverlap});
        return true;
    }

    bool cylinder_first::collideCylinderCylindricalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                                         const ShapeCylindricalShell& cyShell, const Transform& transB,
                                                         ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return collideCylinderCylinder(cylinder, transA, cyShell, transB, manifold);
    }

    bool cylinder_first::collideCylinderDisk(const ShapeCylinder& cylinder, const Transform& transA,
                                             const ShapeDisk& disk, const Transform& transB,
                                             ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double diskRadius = disk.getRadius();

        // 获取圆柱和圆盘的位置和方向
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d diskCenter = transB.getTranslation();
        const Eigen::Vector3d diskNormal = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 将圆盘中心投影到圆柱轴线上
        const Eigen::Vector3d diff = diskCenter - cylCenter;
        const double projLen = diff.dot(cylAxis);

        // 检查投影是否在圆柱高度范围内
        if (std::abs(projLen) > cylHalfHeight + 1e-9) {
            // 最近点在圆柱的一个端面上
            const Eigen::Vector3d closestOnCylAxis = cylCenter + cylAxis * std::clamp(
                projLen, -cylHalfHeight, cylHalfHeight);
            const Eigen::Vector3d closestDiff = diskCenter - closestOnCylAxis;
            const double distSq = closestDiff.squaredNorm();
            const double minDist = cylRadius + diskRadius;

            if (distSq > minDist * minDist + 1e-9) {
                return false;
            }

            const double dist = std::sqrt(distSq);
            const double penetration = minDist - dist;
            Eigen::Vector3d normal = closestDiff.normalized();

            // 确保法线从圆柱指向圆盘
            if (normal.dot(diskCenter - cylCenter) < 0) {
                normal = -normal;
            }

            const Eigen::Vector3d contactPoint = diskCenter - normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        // 最近点在圆柱侧面
        const Eigen::Vector3d closestOnCylAxis = cylCenter + cylAxis * projLen;
        const Eigen::Vector3d closestDiff = diskCenter - closestOnCylAxis;
        const double distSq = closestDiff.squaredNorm();

        // 计算垂直于圆柱轴线的距离
        const double radialDist = std::sqrt(distSq);

        if (radialDist > cylRadius + diskRadius + 1e-9) {
            return false;
        }

        const double penetration = cylRadius + diskRadius - radialDist;
        Eigen::Vector3d normal = closestDiff.normalized();

        if (normal.squaredNorm() < 1e-12) {
            // 如果距离为零，使用圆盘法线
            normal = diskNormal;
        }

        // 确保法线从圆柱指向圆盘
        if (normal.dot(diskCenter - cylCenter) < 0) {
            normal = -normal;
        }

        const Eigen::Vector3d contactPoint = diskCenter - normal * diskRadius;
        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool cylinder_first::collideCylinderRing(const ShapeCylinder& cylinder, const Transform& transA,
                                             const ShapeRing& ring, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideCylinderDisk(cylinder, transA, ring, transB, manifold);
    }

    bool cylinder_first::collideCylinderRod(const ShapeCylinder& cylinder, const Transform& transA,
                                            const ShapeRod& rod, const Transform& transB,
                                            ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取圆柱和杆的位置和方向
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 计算线段与圆柱的最近点
        const Eigen::Vector3d w = rodEndA - cylCenter;
        const Eigen::Vector3d v = rodEndB - rodEndA;

        const double a = v.dot(v);
        const double b = 2.0 * v.dot(w);
        const double c = w.dot(w) - cylRadius * cylRadius;

        // 检查线段是否与圆柱相交
        double t0, t1;
        const double discriminant = b * b - 4.0 * a * c;

        if (discriminant < 0) {
            // 线段不与圆柱相交，检查端点
            const double distA = (rodEndA - cylCenter).norm();
            const double distB = (rodEndB - cylCenter).norm();

            if (distA > cylRadius + 1e-9 && distB > cylRadius + 1e-9) {
                return false;
            }
        }
        else {
            // 线段与圆柱相交
            const double sqrtD = std::sqrt(discriminant);
            t0 = (-b - sqrtD) / (2.0 * a);
            t1 = (-b + sqrtD) / (2.0 * a);

            // 检查交点是否在线段上
            if (t0 >= 0.0 && t0 <= 1.0) {
                const Eigen::Vector3d intersectPoint = rodEndA + t0 * v;
                // 检查交点是否在圆柱高度范围内
                const double proj = (intersectPoint - cylCenter).dot(cylAxis);
                if (std::abs(proj) <= cylHalfHeight + 1e-9) {
                    Eigen::Vector3d normal = (intersectPoint - cylCenter).normalized();
                    if (normal.squaredNorm() < 1e-12) {
                        normal = Eigen::Vector3d::UnitX();
                    }
                    if (normal.dot(rodCenter - cylCenter) < 0) {
                        normal = -normal;
                    }
                    manifold.push_back({intersectPoint, normal, 0.0});
                    return true;
                }
            }
        }

        // 检查杆的端点是否在圆柱内部
        const Eigen::Vector3d diffA = rodEndA - cylCenter;
        const double projA = diffA.dot(cylAxis);
        const double radialDistA = std::sqrt(diffA.dot(diffA) - projA * projA);

        if (std::abs(projA) <= cylHalfHeight + 1e-9 && radialDistA <= cylRadius + 1e-9) {
            // 端点 A 在圆柱内部
            Eigen::Vector3d normal = diffA - cylAxis * projA;
            if (normal.squaredNorm() < 1e-12) {
                normal = Eigen::Vector3d::UnitX();
            }
            else {
                normal.normalize();
            }
            if (normal.dot(rodCenter - cylCenter) < 0) {
                normal = -normal;
            }
            const double penetration = cylRadius - radialDistA;
            const Eigen::Vector3d contactPoint = cylCenter + cylAxis * projA + normal * cylRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        const Eigen::Vector3d diffB = rodEndB - cylCenter;
        const double projB = diffB.dot(cylAxis);
        const double radialDistB = std::sqrt(diffB.dot(diffB) - projB * projB);

        if (std::abs(projB) <= cylHalfHeight + 1e-9 && radialDistB <= cylRadius + 1e-9) {
            // 端点 B 在圆柱内部
            Eigen::Vector3d normal = diffB - cylAxis * projB;
            if (normal.squaredNorm() < 1e-12) {
                normal = Eigen::Vector3d::UnitX();
            }
            else {
                normal.normalize();
            }
            if (normal.dot(rodCenter - cylCenter) < 0) {
                normal = -normal;
            }
            const double penetration = cylRadius - radialDistB;
            const Eigen::Vector3d contactPoint = cylCenter + cylAxis * projB + normal * cylRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        return false;
    }

    bool cylinder_first::collideCylinderSphere(const ShapeCylinder& cylinder, const Transform& transA,
                                               const ShapeSphere& sphere, const Transform& transB,
                                               ContactManifold& manifold)
    {
        const double cylRadius = cylinder.getRadius();
        const double cylHalfHeight = cylinder.getHalfHeight();
        const double sphereRadius = sphere.getRadius();

        // 获取圆柱和球体的位置
        const Eigen::Vector3d cylCenter = transA.getTranslation();
        const Eigen::Vector3d cylAxis = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();

        // 将球心投影到圆柱轴线上
        const Eigen::Vector3d diff = sphereCenter - cylCenter;
        const double projLen = diff.dot(cylAxis);

        // 找到圆柱轴线上离球心最近的点
        const double clampedProj = std::clamp(projLen, -cylHalfHeight, cylHalfHeight);
        const Eigen::Vector3d closestOnAxis = cylCenter + cylAxis * clampedProj;

        // 计算球心到圆柱表面的距离
        const Eigen::Vector3d closestDiff = sphereCenter - closestOnAxis;
        const double distSq = closestDiff.squaredNorm();
        const double minDist = cylRadius + sphereRadius;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线
        Eigen::Vector3d normal = closestDiff.normalized();

        // 确保法线从圆柱指向球体
        if (normal.dot(sphereCenter - cylCenter) < 0) {
            normal = -normal;
        }

        // 计算接触点
        const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool cylinder_first::collideCylinderSphericalShell(const ShapeCylinder& cylinder, const Transform& transA,
                                                       const ShapeSphericalShell& cyShell, const Transform& transB,
                                                       ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideCylinderSphere(cylinder, transA, cyShell, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellCylindricalShell(
        const ShapeCylindricalShell& cyShellA, const Transform& transA,
        const ShapeCylindricalShell& cyShellB, const Transform& transB,
        ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderCylinder(cyShellA, transA, cyShellB, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellDisk(const ShapeCylindricalShell& cyShell,
                                                              const Transform& transA,
                                                              const ShapeDisk& disk, const Transform& transB,
                                                              ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderDisk(cyShell, transA, disk, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellRing(const ShapeCylindricalShell& cyShell,
                                                              const Transform& transA,
                                                              const ShapeRing& ring, const Transform& transB,
                                                              ContactManifold& manifold)
    {
        // 圆环继承自圆盘
        return cylinder_first::collideCylinderDisk(cyShell, transA, ring, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellRod(const ShapeCylindricalShell& cyShell,
                                                             const Transform& transA,
                                                             const ShapeRod& rod, const Transform& transB,
                                                             ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderRod(cyShell, transA, rod, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellSphere(const ShapeCylindricalShell& cyShell,
                                                                const Transform& transA,
                                                                const ShapeSphere& sphere, const Transform& transB,
                                                                ContactManifold& manifold)
    {
        // 圆柱壳与圆柱碰撞检测相同
        return cylinder_first::collideCylinderSphere(cyShell, transA, sphere, transB, manifold);
    }

    bool cylindrical_shell_first::collideCylindricalShellSphericalShell(const ShapeCylindricalShell& cyShell,
                                                                        const Transform& transA,
                                                                        const ShapeSphericalShell& shell,
                                                                        const Transform& transB,
                                                                        ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return cylinder_first::collideCylinderSphere(cyShell, transA, shell, transB, manifold);
    }

    bool disk_first::collideDiskDisk(const ShapeDisk& diskA, const Transform& transA,
                                     const ShapeDisk& diskB, const Transform& transB,
                                     ContactManifold& manifold)
    {
        const double radiusA = diskA.getRadius();
        const double radiusB = diskB.getRadius();

        // 获取圆盘的位置和法向量
        const Eigen::Vector3d centerA = transA.getTranslation();
        const Eigen::Vector3d centerB = transB.getTranslation();
        const Eigen::Vector3d normalA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d normalB = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算中心距离
        const Eigen::Vector3d diff = centerB - centerA;
        const double distSq = diff.squaredNorm();
        const double minDist = radiusA + radiusB;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线（使用中心连线方向）
        Eigen::Vector3d normal = diff.normalized();

        // 如果距离为零，使用圆盘A的法线
        if (dist < 1e-9) {
            normal = normalA;
            // 确保法线方向正确
            if (normal.dot(diff) < 0) {
                normal = -normal;
            }
        }

        // 计算接触点
        const Eigen::Vector3d contactPoint = centerA + normal * radiusA;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskRing(const ShapeDisk& disk, const Transform& transA,
                                     const ShapeRing& ring, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return collideDiskDisk(disk, transA, ring, transB, manifold);
    }

    bool disk_first::collideDiskRod(const ShapeDisk& disk, const Transform& transA,
                                    const ShapeRod& rod, const Transform& transB,
                                    ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius();
        const double rodHalfLength = rod.getLength() * 0.5;

        // 获取圆盘和杆的位置和方向
        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d diskNormal = transA.getRotation() * Eigen::Vector3d::UnitZ();
        const Eigen::Vector3d rodCenter = transB.getTranslation();
        const Eigen::Vector3d rodAxis = transB.getRotation() * Eigen::Vector3d::UnitZ();

        // 计算杆的端点
        const Eigen::Vector3d rodEndA = rodCenter - rodAxis * rodHalfLength;
        const Eigen::Vector3d rodEndB = rodCenter + rodAxis * rodHalfLength;

        // 计算线段与圆盘的最近点
        const Eigen::Vector3d v = rodEndB - rodEndA;
        const Eigen::Vector3d w = diskCenter - rodEndA;

        double t = w.dot(v);
        if (t <= 0) {
            // 最近点在端点 A
            const Eigen::Vector3d diff = diskCenter - rodEndA;
            const double distSq = diff.squaredNorm();
            if (distSq > diskRadius * diskRadius + 1e-9) {
                return false;
            }
            const double dist = std::sqrt(distSq);
            const double penetration = diskRadius - dist;
            Eigen::Vector3d normal = diff.normalized();
            if (normal.squaredNorm() < 1e-12) {
                normal = diskNormal;
            }
            if (normal.dot(diskCenter - rodCenter) < 0) {
                normal = -normal;
            }
            const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        double lenSq = v.dot(v);
        if (t >= lenSq) {
            // 最近点在端点 B
            const Eigen::Vector3d diff = diskCenter - rodEndB;
            const double distSq = diff.squaredNorm();
            if (distSq > diskRadius * diskRadius + 1e-9) {
                return false;
            }
            const double dist = std::sqrt(distSq);
            const double penetration = diskRadius - dist;
            Eigen::Vector3d normal = diff.normalized();
            if (normal.squaredNorm() < 1e-12) {
                normal = diskNormal;
            }
            if (normal.dot(diskCenter - rodCenter) < 0) {
                normal = -normal;
            }
            const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
            manifold.push_back({contactPoint, normal, penetration});
            return true;
        }

        // 最近点在线段内部
        t = t / lenSq;
        const Eigen::Vector3d closestOnRod = rodEndA + t * v;
        const Eigen::Vector3d diff = diskCenter - closestOnRod;
        const double distSq = diff.squaredNorm();

        if (distSq > diskRadius * diskRadius + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = diskRadius - dist;
        Eigen::Vector3d normal = diff.normalized();

        if (normal.squaredNorm() < 1e-12) {
            normal = diskNormal;
        }

        if (normal.dot(diskCenter - rodCenter) < 0) {
            normal = -normal;
        }

        const Eigen::Vector3d contactPoint = diskCenter + normal * diskRadius;
        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskSphere(const ShapeDisk& disk, const Transform& transA,
                                       const ShapeSphere& sphere, const Transform& transB,
                                       ContactManifold& manifold)
    {
        const double diskRadius = disk.getRadius();
        const double sphereRadius = sphere.getRadius();

        // 获取圆盘和球体的位置
        const Eigen::Vector3d diskCenter = transA.getTranslation();
        const Eigen::Vector3d sphereCenter = transB.getTranslation();

        // 计算中心距离
        const Eigen::Vector3d diff = sphereCenter - diskCenter;
        const double distSq = diff.squaredNorm();
        const double minDist = diskRadius + sphereRadius;
        const double minDistSq = minDist * minDist;

        if (distSq > minDistSq + 1e-9) {
            return false;
        }

        const double dist = std::sqrt(distSq);
        const double penetration = minDist - dist;

        // 计算法线（使用中心连线方向）
        Eigen::Vector3d normal = diff.normalized();

        // 计算接触点（球体表面上的点）
        const Eigen::Vector3d contactPoint = sphereCenter - normal * sphereRadius;

        manifold.push_back({contactPoint, normal, penetration});
        return true;
    }

    bool disk_first::collideDiskSphericalShell(const ShapeDisk& disk, const Transform& transA,
                                               const ShapeSphericalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideDiskSphere(disk, transA, shell, transB, manifold);
    }

    bool ring_first::collideRingRing(const ShapeRing& ringA, const Transform& transA,
                                     const ShapeRing& ringB, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskDisk(ringA, transA, ringB, transB, manifold);
    }

    bool ring_first::collideRingRod(const ShapeRing& ring, const Transform& transA,
                                    const ShapeRod& rod, const Transform& transB,
                                    ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskRod(ring, transA, rod, transB, manifold);
    }

    bool ring_first::collideRingSphere(const ShapeRing& ring, const Transform& transA,
                                       const ShapeSphere& sphere, const Transform& transB,
                                       ContactManifold& manifold)
    {
        // 圆环继承自圆盘，碰撞检测逻辑相同
        return disk_first::collideDiskSphere(ring, transA, sphere, transB, manifold);
    }

    bool ring_first::collideRingSphericalShell(const ShapeRing& ring, const Transform& transA,
                                               const ShapeSphericalShell& shell, const Transform& transB,
                                               ContactManifold& manifold)
    {
        // 圆环继承自圆盘，球壳继承自球
        return disk_first::collideDiskSphere(ring, transA, shell, transB, manifold);
    }

    bool rod_first::collideRodRod(const ShapeRod& rodA, const Transform& transA,
                                  const ShapeRod& rodB, const Transform& transB,
                                  ContactManifold& manifold)
    {
        auto [startA, endA]{rodA.getStartEnd(transA)};
        auto [startB, endB]{rodB.getStartEnd(transB)};
        constexpr double eps{1e-12};
        Eigen::Vector3d v = endA - startA; // 方向向量 A
        Eigen::Vector3d w = endB - startB; // 方向向量 B
        Eigen::Vector3d r = startB - startA; // 连接两起点的向量

        double v_len_sq = v.squaredNorm();
        double w_len_sq = w.squaredNorm();

        // 叉积
        Eigen::Vector3d cvw = v.cross(w);
        double cvw_len_sq = cvw.squaredNorm();

        Eigen::Vector3d outIntersection;

        // ---------- 1. 一般情况：v 与 w 不平行 ----------
        if (cvw_len_sq > eps) {
            // 异面检查：若 r · (v×w) ≠ 0，则两直线异面，不相交
            if (std::abs(r.dot(cvw)) > eps)
                return false;

            // 计算参数 t, u
            double t = r.cross(w).dot(cvw) / cvw_len_sq;
            double u = r.cross(v).dot(cvw) / cvw_len_sq;

            if (t >= -eps && t <= 1.0 + eps && u >= -eps && u <= 1.0 + eps) {
                // 将 t 限制到 [0,1] 区间（处理端点边界情况）
                t = std::clamp(t, 0.0, 1.0);
                outIntersection = startA + t * v;
                ContactPoint contactPoint{};
                contactPoint.position = outIntersection;
                contactPoint.normal = {cvw};
                contactPoint.penetration = 0;
                manifold.push_back(contactPoint);
                return true;
            }
            return false;
        }

        // ---------- 2. 平行或共线情况 ----------
        // 检查是否共线：r 与 v 的叉积是否为 0
        if (r.cross(v).squaredNorm() > eps)
            return false; // 平行但不共线

        // ---------- 退化情况：线段退化为点 ----------
        if (v_len_sq < eps) {
            // 线段1是一个点，判断该点是否在线段2上
            double t_on_B = (startA - startB).dot(w) / w_len_sq;
            if (t_on_B >= -eps && t_on_B <= 1.0 + eps) {
                // 检查该点到线段2直线的距离
                Eigen::Vector3d toStart = startA - startB;
                if (toStart.cross(w).squaredNorm() <= eps * w_len_sq) {
                    outIntersection = startA;
                    ContactPoint contactPoint{};
                    contactPoint.position = outIntersection;
                    contactPoint.normal = {cvw};
                    contactPoint.penetration = 0;
                    manifold.push_back(contactPoint);
                    return true;
                }
            }
            return false;
        }

        if (w_len_sq < eps) {
            // 线段2是一个点，判断该点是否在线段1上
            double t_on_A = (startB - startA).dot(v) / v_len_sq;
            if (t_on_A >= -eps && t_on_A <= 1.0 + eps) {
                Eigen::Vector3d toStart = startB - startA;
                if (toStart.cross(v).squaredNorm() <= eps * v_len_sq) {
                    outIntersection = startB;
                    ContactPoint contactPoint{};
                    contactPoint.position = outIntersection;
                    contactPoint.normal = {cvw};
                    contactPoint.penetration = 0;
                    manifold.push_back(contactPoint);
                    return true;
                }
            }
            return false;
        }

        // ---------- 一般共线（两线段在同一直线上） ----------
        // 将线段2的端点投影到线段1的方向上
        double s1 = r.dot(v) / v_len_sq;
        double s2 = (endB - startA).dot(v) / v_len_sq;
        double smin = std::min(s1, s2);
        double smax = std::max(s1, s2);

        double t_left = std::max(0.0, smin);
        double t_right = std::min(1.0, smax);

        if (t_left <= t_right + eps) {
            // 存在重叠部分，取中点作为交点（代表点）
            double t_mid = (t_left + t_right) * 0.5;
            outIntersection = startA + t_mid * v;
            ContactPoint contactPoint{};
            contactPoint.position = outIntersection;
            contactPoint.normal = {cvw};
            contactPoint.penetration = 0;
            manifold.push_back(contactPoint);
            return true;
        }
        return false;
        // const double halfLenA = rodA.getLength() * 0.5;
        // const double halfLenB = rodB.getLength() * 0.5;
        //
        // // 获取杆的位置和方向
        // const Eigen::Vector3d centerA = transA.getTranslation();
        // const Eigen::Vector3d centerB = transB.getTranslation();
        // const Eigen::Vector3d axisA = transA.getRotation() * Eigen::Vector3d::UnitZ();
        // const Eigen::Vector3d axisB = transB.getRotation() * Eigen::Vector3d::UnitZ();
        //
        // // 计算杆的端点
        // const Eigen::Vector3d endA1 = centerA - axisA * halfLenA;
        // const Eigen::Vector3d endA2 = centerA + axisA * halfLenA;
        // const Eigen::Vector3d endB1 = centerB - axisB * halfLenB;
        // const Eigen::Vector3d endB2 = centerB + axisB * halfLenB;
        //
        // // 计算两条线段之间的最近点
        // const Eigen::Vector3d v = endA2 - endA1;
        // const Eigen::Vector3d w = endB2 - endB1;
        // const Eigen::Vector3d u = endA1 - endB1;
        //
        // const double a = v.dot(v);
        // const double b = v.dot(w);
        // const double c = w.dot(w);
        // const double d = v.dot(u);
        // const double e = w.dot(u);
        //
        // const double denom = a * c - b * b;
        //
        // double s, t;
        //
        // if (denom < 1e-9)
        // {
        //     // 线段平行，取端点距离
        //     s = 0;
        //     t = (b > c ? d / b : e / c);
        // }
        // else
        // {
        //     s = (b * e - c * d) / denom;
        //     t = (a * e - b * d) / denom;
        // }
        //
        // // 限制在 [0, 1] 范围内
        // s = std::clamp(s, 0.0, 1.0);
        // t = std::clamp(t, 0.0, 1.0);
        //
        // // 计算最近点
        // const Eigen::Vector3d p = endA1 + s * v;
        // const Eigen::Vector3d q = endB1 + t * w;
        //
        // const Eigen::Vector3d diff = p - q;
        // const double distSq = diff.squaredNorm();
        //
        // // 如果距离大于某个极小值，认为没有碰撞
        // if (distSq > 1e-9)
        // {
        //     return false;
        // }
        //
        // // 计算法线（使用两杆中心连线）
        // Eigen::Vector3d normal = centerB - centerA;
        // if (normal.squaredNorm() < 1e-12)
        // {
        //     normal = Eigen::Vector3d::UnitX();
        //     if (std::abs(axisA.dot(normal)) > 0.9)
        //     {
        //         normal = Eigen::Vector3d::UnitY();
        //     }
        // }
        // else
        // {
        //     normal.normalize();
        // }
        //
        // // 接触点取中点
        // const Eigen::Vector3d contactPoint = (p + q) * 0.5;
        //
        // manifold.push_back({contactPoint, normal, std::sqrt(distSq)});
        // return true;
    }

    bool rod_first::collideRodSphere(const ShapeRod& rod, const Transform& transA,
                                     const ShapeSphere& sphere, const Transform& transB,
                                     ContactManifold& manifold)
    {
        // if (rod.getLength() <= 1e-8) {
        //     const double distance {transA.getTranslation().dot(transB.getTranslation())};
        //     if (distance <= sphere.getRadius()) {
        //         ContactPoint point;
        //         point.position = transA.getTranslation();
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // auto [start, end] = rod.getStartEnd(transA);
        // const Eigen::Vector3d sphereCenter = transB.getTranslation();
        // if ((sphereCenter - start).dot(end - start) <= 1e-12) {
        //     const double distance{(sphereCenter - start).norm()};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //
        //         const double a = (end - sphereCenter).norm();
        //         const double b = (start - sphereCenter).norm();
        //         const double c = rod.getLength();
        //         const double r = sphere.getRadius();
        //
        //         const Eigen::Vector3d elementDirection {(end - start).normalized()};
        //         const double factorA {(a * a + c * c - b * b) / (2 * c)};
        //         const double s {(a + b + c) / 2.0};
        //         const double factorB {sqrt(c * c * r * r - 16 * s * (s - a) * (s - b) * (s - c))/c};
        //         const double deltaLength{factorA - factorB};
        //
        //         ContactPoint point;
        //         point.position = start + deltaLength * elementDirection;
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // if ((sphereCenter - end).dot(start - end) <= 1e-12) {
        //     const double distance{(sphereCenter - end).norm()};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //
        //         const double a = (start - sphereCenter).norm();
        //         const double b = (end - sphereCenter).norm();
        //         const double c = rod.getLength();
        //         const double r = sphere.getRadius();
        //
        //         const Eigen::Vector3d elementDirection {(start - end).normalized()};
        //         const double factorA {(a * a + c * c - b * b) / (2 * c)};
        //         const double s {(a + b + c) / 2.0};
        //         const double factorB {sqrt(c * c * r * r - 16 * s * (s - a) * (s - b) * (s - c))/c};
        //         const double deltaLength{factorA - factorB};
        //
        //         ContactPoint point;
        //         point.position = end + deltaLength * elementDirection;
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        // }else {
        //     const double distance {abs((sphereCenter - start).cross(end - start).norm())};
        //     if (distance - sphere.getRadius() <= 1e-12) {
        //         ContactPoint point;
        //         point.position = transA.getTranslation();
        //         point.normal = point.position - transB.getTranslation();
        //         point.penetration = sphere.getRadius() - distance;
        //         manifold.push_back(point);
        //         return true;
        //     }
        //     return false;
        // }
        // 获取线段端点（世界坐标）和球心
        auto [start, end] = rod.getStartEnd(transA);
        const Eigen::Vector3d center = transB.getTranslation();
        const double radius = sphere.getRadius();

        // 线段方向向量及长度
        const Eigen::Vector3d dir = end - start;
        const double lenSq = dir.squaredNorm();
        const double len = std::sqrt(lenSq);

        // 退化：线段长度为点
        if (len < 1e-8) {
            const double dist = (start - center).norm();
            if (dist <= radius) {
                ContactPoint point{};
                point.position = start;
                point.normal = (start - center).normalized(); // 从球心指向接触点
                point.penetration = radius - dist;
                manifold.push_back(point);
                return true;
            }
            return false;
        }

        // 计算球心到线段所在直线的垂足参数 t
        const Eigen::Vector3d ac = start - center;
        const double t = -ac.dot(dir) / lenSq;
        // 这里直接用 -ac·dir 是因为 ac = start - center，原式 t = (center - start)·dir / lenSq
        // 等价于 t = (C-A)·(B-A) / |B-A|^2

        Eigen::Vector3d closestPoint;
        if (t <= 0.0) {
            closestPoint = start;
        }
        else if (t >= 1.0) {
            closestPoint = end;
        }
        else {
            closestPoint = start + t * dir;
        }

        const Eigen::Vector3d delta = closestPoint - center;
        const double dist = delta.norm();

        // 无碰撞
        if (dist > radius) return false;

        // 有碰撞，填充接触点
        ContactPoint point{};
        point.position = closestPoint;
        point.normal = delta.normalized(); // 从球心指向接触点
        point.penetration = radius - dist;
        manifold.push_back(point);
        return true;
    }

    bool rod_first::collideRodSphericalShell(const ShapeRod& rod, const Transform& transA,
                                             const ShapeSphericalShell& shell, const Transform& transB,
                                             ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideRodSphere(rod, transA, shell, transB, manifold);
    }

    bool sphere_first::collideSphereSphere(const ShapeSphere& sphereA, const Transform& transA,
                                           const ShapeSphere& sphereB,
                                           const Transform& transB, ContactManifold& manifold)
    {
        const Eigen::Vector3d& thisGeomCenter{transA.getTranslation()};
        const Eigen::Vector3d& otherGeomCenter{transB.getTranslation()};
        const double distance{(thisGeomCenter - otherGeomCenter).norm()};
        const double sumRadius{sphereA.getRadius() + sphereB.getRadius()};
        if (distance >= sumRadius) {
            return false;
        }

        Eigen::Vector3d normal = otherGeomCenter - thisGeomCenter;
        if (normal.squaredNorm() < 1e-12) {
            normal = Eigen::Vector3d::UnitX();
        }
        else {
            normal.normalize();
        }
        const double penetration{sumRadius - distance};

        const Eigen::Vector3d contactPoint{thisGeomCenter + normal * (sphereA.getRadius() - penetration * 0.5)};
        const ContactPoint pointInfo{
            .position = contactPoint, .normal = normal, .penetration = penetration
        };
        manifold.push_back(pointInfo);
        return true;
    }

    bool sphere_first::collideSphereSphericalShell(const ShapeSphere& sphere, const Transform& transA,
                                                   const ShapeSphericalShell& shell, const Transform& transB,
                                                   ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return collideSphereSphere(sphere, transA, shell, transB, manifold);
    }

    bool spherical_shell_first::collideSphericalShellSphericalShell(const ShapeSphericalShell& shellA,
                                                                    const Transform& transA,
                                                                    const ShapeSphericalShell& shellB,
                                                                    const Transform& transB,
                                                                    ContactManifold& manifold)
    {
        // 球壳与球碰撞检测相同
        return sphere_first::collideSphereSphere(shellA, transA, shellB, transB, manifold);
    }
}
