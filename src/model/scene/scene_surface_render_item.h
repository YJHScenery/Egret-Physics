/**
 * @file        scene_surface_render_item.h
 * @brief       曲面渲染轻量化数据快照头文件。
 * @details     定义 SceneSurfaceRenderItem 结构体，用于从 WorldSceneManager
 *              向 ViewModel 层传递 SupportSurface 的曲面渲染数据。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Scene (场景管理)
 */

#ifndef EGRET_PHYSICS_SCENE_SURFACE_RENDER_ITEM_H
#define EGRET_PHYSICS_SCENE_SURFACE_RENDER_ITEM_H

#include <cstdint>
#include <memory>

#include "surface_base.h"

namespace egret
{
    /**
     * @brief       用于视图层渲染的曲面轻量化数据快照。
     * @details     SceneSurfaceRenderItem 是用于视图层渲染的曲面轻量化数据快照结构体。
     *              该结构不包含任何拥有权，只提供渲染所需的曲面几何数据，
     *              便于 ViewModel 层将曲面数据刷新到 SceneSurfaceModel。
     *              包含约束 ID、曲面类型以及曲面基类的克隆副本。
     *
     * @invariant   constraintId 是对应的 SupportSurface 约束器标识符
     * @see         WorldSceneManager, SupportSurface, SceneSurfaceModel
     */
    struct SceneSurfaceRenderItem
    {
        /** 对应的 SupportSurface 约束器 ID。 */
        std::uint64_t constraintId{0};

        /** 曲面类型，对应 SurfaceType 枚举值。 */
        std::uint32_t surfaceType{0};

        /** 曲面对象的克隆副本，供视图层渲染使用。 */
        std::unique_ptr<SurfaceBase> surface;
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_SURFACE_RENDER_ITEM_H
