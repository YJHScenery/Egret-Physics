/**
 * @file        scene_curve_model.h
 * @brief       场景曲线视觉数据模型头文件，提供可渲染曲线的 Qt 数据模型。
 * @details     定义 SceneCurveVisualItem 和 SceneCurveModel 类，
 *              用于在 QML 中渲染物理场景中的 SlidingRail 曲线轨道。
 *              仿照 SceneSurfaceModel / SceneSurfaceVisualItem 的模式设计。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#ifndef EGRET_PHYSICS_SCENE_CURVE_MODEL_H
#define EGRET_PHYSICS_SCENE_CURVE_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <memory>
#include <unordered_map>
#include <vector>

#include "curve_geometry.h"
#include "scene_curve_render_item.h"

namespace egret
{
    /**
     * @brief       单个可渲染曲线的 Qt 数据对象。
     * @details     SceneCurveVisualItem 是单个可渲染曲线的 Qt 数据对象结构体。
     *              该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     *              包含约束 ID、曲线类型、CurveGeometry 实例指针、渲染颜色等。
     *              用于 SceneCurveModel 的数据存储。
     *
     * @invariant   constraintId 是对应的 SlidingRail 约束器标识符
     * @invariant   curveGeometry 指向有效的 CurveGeometry 实例
     * @see         SceneCurveModel, SlidingRail, CurveBase
     */
    struct SceneCurveVisualItem
    {
        /** 对应的 SlidingRail 约束器 ID。 */
        quint64 constraintId{0};

        /** 曲线类型，对应 CurveType 枚举值。 */
        quint32 curveKind{0};

        /**
         * @brief CurveGeometry 实例指针。
         * @details 该指针指向由 SceneCurveModel 管理的 CurveGeometry 对象。
         *          QML 通过此指针将几何体绑定到 Model.geometry 属性。
         *          指针生命周期由 SceneCurveModel 保证。
         */
        CurveGeometry* curveGeometry{nullptr};

        /** 渲染颜色。 */
        QString color;
    };

    /**
     * @brief       QML 可直接使用的曲线列表模型。
     * @details     SceneCurveModel 继承自 QAbstractListModel，
     *              是 QML 可直接使用的曲线列表模型。
     *              该模型管理 CurveGeometry 实例的生命周期，
     *              曲线数据更新由 ViewModel 层统一驱动。
     *              支持 QML 的 Repeater3D 等组件绑定。
     *              提供角色名称映射，支持 QML 属性绑定。
     *
     * @invariant   m_items 包含所有可渲染曲线数据
     * @invariant   m_geometryCache 维护约束 ID 到 CurveGeometry 的映射
     * @remark      SceneCurveModel 是 QAbstractListModel，支持 QML 数据绑定
     * @see         SceneCurveVisualItem, SceneManagerViewModel, QAbstractListModel
     */
    class SceneCurveModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        /** 模型角色枚举。 */
        enum CurveRole
        {
            CurveIdRole = Qt::UserRole + 1,
            CurveKindRole,
            CurveGeometryRole,
            CurveColorRole,
        };

        /** 默认构造。 */
        explicit SceneCurveModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneCurveModel() override = default;

        /** 行数。 */
        [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

        /** 模型数据。 */
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的曲线数据刷新模型。
         * @details 接收从 WorldSceneManager 获取的 SceneCurveRenderItem 列表，
         *          更新内部 CurveGeometry 的曲线数据和管理生命周期。
         *          - 对于已存在的约束 ID，复用 CurveGeometry 并更新其曲线数据
         *          - 对于新增的约束 ID，创建新的 CurveGeometry 实例
         *          - 对于已移除的约束 ID，从缓存中删除对应的 CurveGeometry
         * @param renderItems 从 WorldSceneManager 获取的曲线渲染数据列表。
         */
        void setItems(const std::vector<SceneCurveRenderItem>& renderItems);

        /**
         * @brief 清空模型。
         */
        void clear();

    private:
        /**
         * @brief 根据约束 ID 获取或创建 CurveGeometry。
         * @param constraintId 约束器 ID。
         * @return 对应的 CurveGeometry 指针。
         */
        CurveGeometry* getOrCreateGeometry(std::uint64_t constraintId);

        /** 当前模型数据。 */
        QList<SceneCurveVisualItem> m_items;

        /**
         * @brief 曲线几何体缓存。
         * @details key = 约束器 ID, value = CurveGeometry 唯一所有权。
         *          通过 getOrCreateGeometry() 访问。
         *          使用 std::unordered_map 而非 QMap，因为 QMap 不支持 move-only 类型。
         */
        std::unordered_map<quint64, std::unique_ptr<CurveGeometry>> m_geometryCache;
    };
} // egret

#endif // EGRET_PHYSICS_SCENE_CURVE_MODEL_H
