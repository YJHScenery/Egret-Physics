/**
 * @file        dimension_concept.h
 * @brief       维度约束概念，确保模板参数 Dimension 为大于等于 2 的整数
 * @details     定义 C++20 concept ValidDimension，用于约束 GJK、EPA 等
 *              维度无关算法的模板参数，在编译期保证维度 ≥ 2。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 */

#ifndef EGRET_PHYSICS_DIMENSION_CONCEPT_H
#define EGRET_PHYSICS_DIMENSION_CONCEPT_H

#include <concepts>

namespace egret
{
    /**
     * @brief   有效维度概念
     * @details 约束模板参数为大于等于 2 的无符号整数。
     *          物理模拟空间的维度至少为 2（平面），无理论上限。
     *
     * @tparam  Dim  维度值（编译期常量）
     */
    template <auto Dim>
    concept ValidDimension = (Dim >= 2);

} // namespace egret

#endif // EGRET_PHYSICS_DIMENSION_CONCEPT_H
