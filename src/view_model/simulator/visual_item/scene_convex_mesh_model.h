/**
 * @file        scene_convex_mesh_model.h
 * @brief       场景凸体网格视觉数据模型头文件，提供可渲染凸体网格的 Qt 数据模型。
 * @details     定义 SceneConvexMeshVisualItem 和 SceneConvexMeshModel 类，
 *              用于在 QML 中渲染物理场景中的 ConvexMesh 形状。
 *              仿照 SceneSurfaceModel / SceneCurveModel 的模式设计。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-10
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#ifndef EGRET_PHYSICS_SCENE_CONVEX_MESH_MODEL_H
#define EGRET_PHYSICS_SCENE_CONVEX_MESH_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <memory>
#include <unordered_map>
#include <vector>

#include "convex_mesh_geometry.h"
#include "scene_convex_mesh_render_item.h"

namespace egret
{
    /**
     * @brief       单个可渲染凸体网格的 Qt 数据对象。
     * @details     SceneConvexMeshVisualItem 是单个可渲染凸体网格的 Qt 数据对象结构体。
     *              该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     *              包含刚体 ID、ConvexMeshGeometry 实例指针、渲染颜色等。
     *              用于 SceneConvexMeshModel 的数据存储。
     *
     * @invariant   bodyId 是拥有此 ConvexMesh 形状的刚体标识符
     * @invariant   meshGeometry 指向有效的 ConvexMeshGeometry 实例
     * @see         SceneConvexMeshModel, ConvexMesh
     */
    struct SceneConvexMeshVisualItem
    {
        /** 拥有此 ConvexMesh 形状的刚体 ID。 */
        quint64 bodyId{0};

        /**
         * @brief ConvexMeshGeometry 实例指针。
         * @details 该指针指向由 SceneConvexMeshModel 管理的 ConvexMeshGeometry 对象。
         *          QML 通过此指针将几何体绑定到 Model.geometry 属性。
         *          指针生命周期由 SceneConvexMeshModel 保证。
         */
        ConvexMeshGeometry* meshGeometry{nullptr};

        /** 渲染中心位置（已转换到 Quick3D 渲染坐标系）。 */
        std::array<double, 3> position{0.0, 0.0, 0.0};

        /** 渲染缩放（已转换到 Quick3D 渲染坐标系）。 */
        std::array<double, 3> scale{1.0, 1.0, 1.0};

        /** 旋转四元数（已转换到 Quick3D 渲染坐标系：w, y, z, x）。 */
        std::array<double, 4> rotation{1.0, 0.0, 0.0, 0.0};

        /** 渲染颜色。 */
        QString color;
    };

    /**
     * @brief       QML 可直接使用的凸体网格列表模型。
     * @details     SceneConvexMeshModel 继承自 QAbstractListModel，
     *              是 QML 可直接使用的凸体网格列表模型。
     *              该模型管理 ConvexMeshGeometry 实例的生命周期，
     *              网格数据更新由 ViewModel 层统一驱动。
     *              支持 QML 的 Repeater3D 等组件绑定。
     *              提供角色名称映射，支持 QML 属性绑定。
     *
     * @invariant   m_items 包含所有可渲染凸体网格数据
     * @invariant   m_geometryCache 维护刚体 ID 到 ConvexMeshGeometry 的映射
     * @remark      SceneConvexMeshModel 是 QAbstractListModel，支持 QML 数据绑定
     * @see         SceneConvexMeshVisualItem, SceneManagerViewModel, QAbstractListModel
     */
    class SceneConvexMeshModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        /** 模型角色枚举。 */
        enum ConvexMeshRole
        {
            ConvexMeshIdRole = Qt::UserRole + 1,
            ConvexMeshGeometryRole,
            ConvexMeshPositionRole,
            ConvexMeshScaleRole,
            ConvexMeshRotationRole,
            ConvexMeshColorRole,
        };

        /** 默认构造。 */
        explicit SceneConvexMeshModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneConvexMeshModel() override = default;

        /** 行数。 */
        [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

        /** 模型数据。 */
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的凸体网格数据刷新模型。
         * @details 接收从 WorldSceneManager 获取的 SceneConvexMeshRenderItem 列表，
         *          更新内部 ConvexMeshGeometry 的网格数据和管理生命周期。
         *          - 对于已存在的刚体 ID，复用 ConvexMeshGeometry 并更新其网格数据
         *          - 对于新增的刚体 ID，创建新的 ConvexMeshGeometry 实例
         *          - 对于已移除的刚体 ID，从缓存中删除对应的 ConvexMeshGeometry
         * @param renderItems 从 WorldSceneManager 获取的凸体网格渲染数据列表。
         */
        void setItems(const std::vector<SceneConvexMeshRenderItem>& renderItems);

        /**
         * @brief 清空模型。
         */
        void clear();

    private:
        /**
         * @brief 根据刚体 ID 获取或创建 ConvexMeshGeometry。
         * @param bodyId 刚体 ID。
         * @return 对应的 ConvexMeshGeometry 指针。
         */
        ConvexMeshGeometry* getOrCreateGeometry(std::uint64_t bodyId);

        /** 当前模型数据。 */
        QList<SceneConvexMeshVisualItem> m_items;

        /**
         * @brief 凸体网格几何体缓存。
         * @details key = 刚体 ID, value = ConvexMeshGeometry 唯一所有权。
         *          通过 getOrCreateGeometry() 访问。
         *          使用 std::unordered_map 而非 QMap，因为 QMap 不支持 move-only 类型。
         */
        std::unordered_map<quint64, std::unique_ptr<ConvexMeshGeometry>> m_geometryCache;
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_CONVEX_MESH_MODEL_H
