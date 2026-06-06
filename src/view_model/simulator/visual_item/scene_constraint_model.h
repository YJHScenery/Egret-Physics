/**
 * @file        scene_constraint_model.h
 * @brief       场景约束视觉数据模型头文件，提供可渲染约束的 Qt 数据模型。
 * @details     定义 SceneConstraintVisualItem 和 SceneConstraintModel 类，
 *              用于在 QML 中渲染物理场景中的约束器。
 *              仿照 SceneBodyModel / SceneBodyVisualItem 的模式设计。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SCENE_CONSTRAINT_MODEL_H
#define EGRET_PHYSICS_SCENE_CONSTRAINT_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>
#include <magic_enum.hpp>
#include <vector>
#include "basic_render_item.h"

namespace egret
{

    /**
     * @brief       单个可渲染约束的 Qt 数据对象。
     * @details     SceneConstraintVisualItem 是单个可渲染约束的 Qt 数据对象结构体。
     *              该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     *              包含约束 ID、类型、两端实体位置、约束原长、当前距离、颜色、标签等。
     *              用于 SceneConstraintModel 的数据存储。
     *
     * @invariant   id 是稳定的约束标识符
     * @invariant   kind 是有效的约束类型枚举
     * @see         SceneConstraintModel, ConstraintsBase
     */

    struct SceneConstraintVisualItem
    {
        BasicRenderItem<QString> basic{};

        /** 两端实体的 ID（用于将来查找实体颜色等）。 */
        quint64 entityIdA{0};
        quint64 entityIdB{0};

        /** 两端实体的当前位置（x, y, z）。 */
        double positionA[3]{0.0, 0.0, 0.0};
        double positionB[3]{0.0, 0.0, 0.0};

        /** 约束原长。 */
        double constraintLength{0.0};

        /** 两端当前实际距离。用于判断松弛/绷紧状态。 */
        double currentDistance{0.0};

    };

    /**
     * @brief       QML 可直接使用的约束列表模型。
     * @details     SceneConstraintModel 继承自 QAbstractListModel，
     *              是 QML 可直接使用的约束列表模型。
     *              该模型只负责展示，不持有物理对象；数据更新由 ViewModel 层统一驱动。
     *              支持 QML 的 ListView、Repeater 等组件绑定。
     *              提供角色名称映射，支持 QML 属性绑定。
     *
     * @invariant   m_items 包含所有可渲染约束数据
     * @remark      SceneConstraintModel 是 QAbstractListModel，支持 QML 数据绑定
     * @see         SceneConstraintVisualItem, SceneManagerViewModel, QAbstractListModel
     */
    class SceneConstraintModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        /** 模型角色枚举。 */
        enum ConstraintRole
        {
            IdRole = Qt::UserRole + 1,
            KindRole,
            EntityIdARole,
            EntityIdBRole,
            PositionARole,
            PositionBRole,
            ConstraintLengthRole,
            CurrentDistanceRole,
            ColorRole,
            LabelRole,
        };

        /** 默认构造。 */
        explicit SceneConstraintModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneConstraintModel() override = default;

        /** 行数。 */
        [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

        /** 模型数据。 */
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的数据覆盖整个模型。
         * @param items 新的约束渲染快照。
         */
        void setItems(const QList<SceneConstraintVisualItem>& items);

        /**
         * @brief 清空模型。
         */
        void clear();

    private:
        /** 当前模型数据。 */
        QList<SceneConstraintVisualItem> m_items;
    };
}

#endif // EGRET_PHYSICS_SCENE_CONSTRAINT_MODEL_H
