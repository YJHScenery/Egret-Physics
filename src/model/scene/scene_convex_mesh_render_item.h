/**
 * @file        scene_convex_mesh_render_item.h
 * @brief       凸体网格渲染轻量化数据快照头文件。
 * @details     定义 SceneConvexMeshRenderItem 结构体，用于从 WorldSceneManager
 *              向 ViewModel 层传递 ConvexMesh 形状的渲染数据。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Scene (场景管理)
 */

#ifndef EGRET_PHYSICS_SCENE_CONVEX_MESH_RENDER_ITEM_H
#define EGRET_PHYSICS_SCENE_CONVEX_MESH_RENDER_ITEM_H

#include <cstdint>
#include <memory>

#include <Eigen/Dense>
#include <QString>

#include "tessellated_mesh.h"

namespace egret
{
    /**
     * @brief       用于视图层渲染的凸体网格轻量化数据快照。
     * @details     SceneConvexMeshRenderItem 是用于视图层渲染的凸体网格轻量化数据快照结构体。
     *              该结构包含拥有权，提供渲染所需的网格几何数据，
     *              便于 ViewModel 层将网格数据刷新到 SceneConvexMeshModel。
     *              包含刚体 ID 和 TessellatedMesh 数据的唯一所有权。
     *
     * @invariant   bodyId 是拥有此 ConvexMesh 形状的刚体标识符
     * @see         WorldSceneManager, ConvexMesh, SceneConvexMeshModel
     */
    struct SceneConvexMeshRenderItem
    {
        /** 拥有此 ConvexMesh 形状的刚体 ID。 */
        std::uint64_t bodyId{0};

        /** 细分网格数据的所有权，供视图层构建几何体使用。 */
        std::unique_ptr<TessellatedMesh> mesh;

        /** 渲染中心位置（已转换到 Quick3D 渲染坐标系：y,z,x）。 */
        Eigen::Vector3d position{0.0, 0.0, 0.0};

        /** 渲染缩放（已转换到 Quick3D 渲染坐标系：y,z,x）。 */
        Eigen::Vector3d scale{1.0, 1.0, 1.0};

        /** 旋转四元数（已转换到 Quick3D 渲染坐标系：w, y, z, x）。 */
        Eigen::Quaterniond rotation{Eigen::Quaterniond::Identity()};

        /** 渲染颜色（ARGB 十六进制字符串，如 "#E74C3C"）。 */
        QString color;
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_CONVEX_MESH_RENDER_ITEM_H
