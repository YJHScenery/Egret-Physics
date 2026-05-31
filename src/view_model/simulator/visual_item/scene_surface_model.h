/**
 * @file        scene_surface_model.h
 * @brief       场景曲面视觉数据模型头文件，提供可渲染曲面的 Qt 数据模型。
 * @details     定义 SceneSurfaceVisualItem 和 SceneSurfaceModel 类，
 *              用于在 QML 中渲染物理场景中的 SupportSurface 曲面。
 *              仿照 SceneBodyModel / SceneConstraintModel 的模式设计。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#ifndef EGRET_PHYSICS_SCENE_SURFACE_MODEL_H
#define EGRET_PHYSICS_SCENE_SURFACE_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>
#include <memory>
#include <unordered_map>
#include <vector>

#include "surface_geometry.h"
#include "scene_surface_render_item.h"

namespace egret
{
    /**
     * @brief       单个可渲染曲面的 Qt 数据对象。
     * @details     SceneSurfaceVisualItem 是单个可渲染曲面的 Qt 数据对象结构体。
     *              该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     *              包含约束 ID、曲面类型、SurfaceGeometry 实例指针、渲染颜色等。
     *              用于 SceneSurfaceModel 的数据存储。
     *
     * @invariant   constraintId 是对应的 SupportSurface 约束器标识符
     * @invariant   surfaceGeometry 指向有效的 SurfaceGeometry 实例
     * @see         SceneSurfaceModel, SupportSurface, SurfaceBase
     */
    struct SceneSurfaceVisualItem
    {
        /** 对应的 SupportSurface 约束器 ID。 */
        quint64 constraintId{0};

        /** 曲面类型，对应 SurfaceType 枚举值。 */
        quint32 surfaceKind{0};

        /**
         * @brief SurfaceGeometry 实例指针。
         * @details 该指针指向由 SceneSurfaceModel 管理的 SurfaceGeometry 对象。
         *          QML 通过此指针将几何体绑定到 Model.geometry 属性。
         *          指针生命周期由 SceneSurfaceModel 保证。
         */
        SurfaceGeometry* surfaceGeometry{nullptr};

        /** 渲染颜色。 */
        QString color;
    };

    /**
     * @brief       QML 可直接使用的曲面列表模型。
     * @details     SceneSurfaceModel 继承自 QAbstractListModel，
     *              是 QML 可直接使用的曲面列表模型。
     *              该模型管理 SurfaceGeometry 实例的生命周期，
     *              曲面数据更新由 ViewModel 层统一驱动。
     *              支持 QML 的 Repeater3D 等组件绑定。
     *              提供角色名称映射，支持 QML 属性绑定。
     *
     * @invariant   m_items 包含所有可渲染曲面数据
     * @invariant   m_geometryCache 维护约束 ID 到 SurfaceGeometry 的映射
     * @remark      SceneSurfaceModel 是 QAbstractListModel，支持 QML 数据绑定
     * @see         SceneSurfaceVisualItem, SceneManagerViewModel, QAbstractListModel
     */
    class SceneSurfaceModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        /** 模型角色枚举。 */
        enum SurfaceRole
        {
            SurfaceIdRole = Qt::UserRole + 1,
            SurfaceKindRole,
            SurfaceGeometryRole,
            SurfaceColorRole,
        };

        /** 默认构造。 */
        explicit SceneSurfaceModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneSurfaceModel() override = default;

        /** 行数。 */
        [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

        /** 模型数据。 */
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的曲面数据刷新模型。
         * @details 接收从 WorldSceneManager 获取的 SceneSurfaceRenderItem 列表，
         *          更新内部 SurfaceGeometry 的曲面数据和管理生命周期。
         *          - 对于已存在的约束 ID，复用 SurfaceGeometry 并更新其曲面数据
         *          - 对于新增的约束 ID，创建新的 SurfaceGeometry 实例
         *          - 对于已移除的约束 ID，从缓存中删除对应的 SurfaceGeometry
         * @param renderItems 从 WorldSceneManager 获取的曲面渲染数据列表。
         */
        void setItems(const std::vector<SceneSurfaceRenderItem>& renderItems);

        /**
         * @brief 清空模型。
         */
        void clear();

        /**
         * @brief 设置无限大平面的显示范围。
         * @details 仅对 SurfacePlane 类型有效，后续刷新时自动应用到 Plane 几何体。
         * @param extent 平面边长，最小值 1.0。
         */
        void setPlaneExtent(double extent);

        /** @brief 获取无限大平面的显示范围。 */
        [[nodiscard]] double planeExtent() const { return m_planeExtent; }

    private:
        /**
         * @brief 根据约束 ID 获取或创建 SurfaceGeometry。
         * @param constraintId 约束器 ID。
         * @return 对应的 SurfaceGeometry 指针。
         */
        SurfaceGeometry* getOrCreateGeometry(std::uint64_t constraintId);

        /** 当前模型数据。 */
        QList<SceneSurfaceVisualItem> m_items;

        /**
         * @brief 曲面几何体缓存。
         * @details key = 约束器 ID, value = SurfaceGeometry 唯一所有权。
         *          通过 getOrCreateGeometry() 访问。
         *          使用 std::unordered_map 而非 QMap，因为 QMap 不支持 move-only 类型。
         */
        std::unordered_map<quint64, std::unique_ptr<SurfaceGeometry>> m_geometryCache;

        /** 无限大平面的显示范围（边长），仅对 SurfacePlane 有效。 */
        double m_planeExtent{200.0};
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_SURFACE_MODEL_H
