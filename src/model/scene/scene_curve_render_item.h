/**
 * @file        scene_curve_render_item.h
 * @brief       曲线渲染轻量化数据快照头文件。
 * @details     定义 SceneCurveRenderItem 结构体，用于从 WorldSceneManager
 *              向 ViewModel 层传递 SlidingRail 的曲线渲染数据。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Scene (场景管理)
 */

#ifndef EGRET_PHYSICS_SCENE_CURVE_RENDER_ITEM_H
#define EGRET_PHYSICS_SCENE_CURVE_RENDER_ITEM_H

#include <cstdint>
#include <memory>

#include <QString>

#include "curve_base.h"

namespace egret
{
    /**
     * @brief       用于视图层渲染的曲线轻量化数据快照。
     * @details     SceneCurveRenderItem 是用于视图层渲染的曲线轻量化数据快照结构体。
     *              该结构不包含任何拥有权，只提供渲染所需的曲线几何数据，
     *              便于 ViewModel 层将曲线数据刷新到 SceneCurveModel。
     *              包含约束 ID、曲线类型以及曲线基类的克隆副本。
     *
     * @invariant   constraintId 是对应的 SlidingRail 约束器标识符
     * @see         WorldSceneManager, SlidingRail, SceneCurveModel
     */
    struct SceneCurveRenderItem
    {
        /** 对应的 SlidingRail 约束器 ID。 */
        std::uint64_t constraintId{0};

        /** 曲线类型，对应 CurveType 枚举值。 */
        std::uint32_t curveType{0};

        /** 曲线对象的克隆副本，供视图层渲染使用。 */
        std::unique_ptr<CurveBase> curve;

        /** 渲染颜色（ARGB 十六进制字符串，如 "#00FF88"）。 */
        QString color;
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_CURVE_RENDER_ITEM_H
