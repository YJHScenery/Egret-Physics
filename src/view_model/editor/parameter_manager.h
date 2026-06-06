/**
 * @file        parameter_manager.h
 * @brief       编辑器参数管理器头文件，提供数据驱动的参数编辑功能。
 * @details     定义 ParameterDescriptor、TypeParameterSchema 结构体和
 *              EditorParameterManager 类，用于根据实体类型动态生成编辑参数列表。
 *              替代 QML 端的硬编码 switch 语句，支持 Shape / Field / Constraint
 *              三种实体类别，以及 Constraint → SupportSurface → Surface → Curve
 *              的子类型链。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-12
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    Editor (编辑器)
 */

#ifndef EGRET_PHYSICS_PARAMETER_MANAGER_H
#define EGRET_PHYSICS_PARAMETER_MANAGER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QList>
#include <QString>
#include <QHash>

namespace egret
{

    /**
     * @brief       参数描述符结构体，描述单个可编辑参数。
     * @details     每个 ParameterDescriptor 定义了一个参数的元数据：
     *              - 唯一标识符（key），如 "radius", "boxSize"
     *              - 显示名称（displayName），如 "半径", "长、宽、高"
     *              - 控件类型（type），如 "float", "vector3d", "color", "enum"
     *              - 数值约束（min/max/step/decimals），仅对 float 类型有效
     *              - 默认值（defaultValue）
     *              - 枚举选项（enumOptions），仅对 enum 类型有效
     *
     *              此结构体用于构建 QML 端 Repeater 的数据模型，
     *              每个参数在 QML 中对应一个 Label + Editor 组合。
     *
     * @invariant   key 非空且在所有参数中唯一
     * @invariant   type 为 "float" / "vector3d" / "color" / "enum" 之一
     */
    struct ParameterDescriptor
    {
        /** 参数唯一标识符，如 "radius", "boxSize", "stiffness"。 */
        QString key;

        /** 参数显示名称，如 "半径", "长、宽、高"（中文）。 */
        QString displayName;

        /**
         * @brief 参数对应的编辑器控件类型。
         * @details 可选值：
         *          - "float"   → 单个 FloatField
         *          - "vector3d" → 三个 FloatField（x, y, z）
         *          - "color"   → 颜色选择按钮 + 预览
         *          - "enum"    → ComboBox（选项来自 enumOptions）
         */
        QString type;

        /** 参数默认值。 */
        QVariant defaultValue;

        // ---- float 类型约束 ----

        /** 最小值（仅 float 类型有效）。 */
        double min = -1000000.0;

        /** 最大值（仅 float 类型有效）。 */
        double max = 1000000.0;

        /** 步长（仅 float 类型有效）。 */
        double step = 0.1;

        /** 小数位数（仅 float 类型有效）。 */
        int decimals = 3;

        // ---- enum 类型选项 ----

        /** 枚举选项列表（仅 enum 类型有效）。 */
        QStringList enumOptions;
    };

    /**
     * @brief       实体类型参数模式结构体，定义一种实体类型的完整参数列表。
     * @details     每个 TypeParameterSchema 对应一种实体类型
     *              （如 Shape::Box, Field::Gravity, Constraint::Spring），
     *              包含该类型的显示名称和所有可编辑参数。
     *
     *              如果该类型会触发子类型选择（如 Constraint::SupportSurface
     *              需要选择 Surface 类型），则 subCategory 字段指示子类别。
     *
     * @invariant   displayName 非空
     * @invariant   typeId 在所属类别中唯一
     */
    struct TypeParameterSchema
    {
        /** 类型 ID，对应 ShapeType / FieldType / ConstraintType 枚举值。 */
        int typeId;

        /** 类型显示名称，如 "标准盒体"、"重力场"、"弹簧"。 */
        QString displayName;

        /** 该类型的所有可编辑参数列表。 */
        QList<ParameterDescriptor> parameters;

        /**
         * @brief 子类别标识。
         * @details 0 = 无子类别，1 = Surface（曲面类型选择），2 = Curve（曲线类型选择）。
         *          当此值非零时，选择此类型后需要在 UI 中展开子类型 ComboBox。
         */
        int subCategory = 0;
    };

    /**
     * @brief       编辑器参数管理器类，提供数据驱动的参数编辑功能。
     * @details     EditorParameterManager 是编辑器参数管理器的核心类，
     *              采用单例模式，负责：
     *              - 管理实体类别（Shape / Field / Constraint）的切换
     *              - 根据当前选中的类别和类型，动态生成参数列表
     *              - 管理子类型选择链（Constraint → SupportSurface → Surface → Curve）
     *              - 存储和读写当前所有参数的值
     *              - 提供与 ModelItemData 的数据同步接口
     *
     *              QML 端通过绑定 currentParameters 等属性，
     *              完全数据驱动地渲染编辑面板，无需硬编码 switch 语句。
     *
     *              使用示例（QML）：
     *              @code
     *              ComboBox {
     *                  model: EditorManager.availableTypes
     *                  textRole: "displayName"
     *                  onCurrentIndexChanged: EditorManager.currentTypeId = model[currentIndex].typeId
     *              }
     *              Repeater {
     *                  model: EditorManager.currentParameters
     *                  delegate: RowLayout {
     *                      Label { text: modelData.displayName }
     *                      Loader {
     *                          sourceComponent: {
     *                              if (modelData.type === "float") return floatEditor;
     *                              if (modelData.type === "vector3d") return vec3Editor;
     *                              // ...
     *                          }
     *                      }
     *                  }
     *              }
     *              @endcode
     *
     * @invariant   instance() 返回全局唯一实例
     * @invariant   currentCategory 为 0/1/2 之一
     * @invariant   currentTypeId 对应当前类别中的有效类型
     * @remark      EditorParameterManager 是单例，全局唯一实例
     * @see         ParameterDescriptor, TypeParameterSchema, ModelItemData
     */
    class EditorParameterManager : public QObject
    {
        Q_OBJECT

        // ---- 主类别与类型选择 ----

        /**
         * @brief 当前实体类别。
         * @details 0 = Shape（形状），1 = Field（场），2 = Constraint（约束器）。
         */
        Q_PROPERTY(int currentCategory READ currentCategory WRITE setCurrentCategory NOTIFY
                   currentCategoryChanged)

        /**
         * @brief 当前类别下的可选类型列表。
         * @details 每个元素为 QVariantMap: { typeId: int, displayName: string }。
         *          适合直接作为 ComboBox 的 model。
         */
        Q_PROPERTY(
            QVariantList availableTypes READ availableTypes NOTIFY availableTypesChanged)

        /**
         * @brief 当前选中的类型 ID。
         * @details 对应当前类别中的 ShapeType / FieldType / ConstraintType 枚举值。
         */
        Q_PROPERTY(int currentTypeId READ currentTypeId WRITE setCurrentTypeId NOTIFY
                   currentTypeIdChanged)

        /**
         * @brief 当前类型的参数列表。
         * @details 每个元素为 QVariantMap:
         *          { key, displayName, type, value, min, max, step, decimals, enumOptions }。
         *          适合直接作为 Repeater 的 model。
         */
        Q_PROPERTY(QVariantList currentParameters READ currentParameters NOTIFY
                   currentParametersChanged)

        // ---- 子类型第 1 层（如 Constraint=SupportSurface 时的 Surface 类型选择） ----

        /** @brief 当前类型是否需要子类型选择（第 1 层）。 */
        Q_PROPERTY(bool hasSubTypes READ hasSubTypes NOTIFY hasSubTypesChanged)

        /** @brief 第 1 层子类型的可选列表（格式同 availableTypes）。 */
        Q_PROPERTY(QVariantList subTypes READ subTypes NOTIFY subTypesChanged)

        /** @brief 第 1 层子类型的当前选中 ID。 */
        Q_PROPERTY(int subTypeId READ subTypeId WRITE setSubTypeId NOTIFY subTypeIdChanged)

        /** @brief 第 1 层子类型当前选中项的参数列表（格式同 currentParameters）。 */
        Q_PROPERTY(QVariantList subParameters READ subParameters NOTIFY subParametersChanged)

        // ---- 子类型第 2 层（如 Surface=Cylindrical 时的 Curve 类型选择） ----

        /** @brief 第 1 层子类型是否需要再展开一层子类型选择。 */
        Q_PROPERTY(bool hasSubSubTypes READ hasSubSubTypes NOTIFY hasSubSubTypesChanged)

        /** @brief 第 2 层子类型的可选列表。 */
        Q_PROPERTY(QVariantList subSubTypes READ subSubTypes NOTIFY subSubTypesChanged)

        /** @brief 第 2 层子类型的当前选中 ID。 */
        Q_PROPERTY(int subSubTypeId READ subSubTypeId WRITE setSubSubTypeId NOTIFY
                   subSubTypeIdChanged)

        /** @brief 第 2 层子类型当前选中项的参数列表。 */
        Q_PROPERTY(QVariantList subSubParameters READ subSubParameters NOTIFY
                   subSubParametersChanged)

        /**
         * @brief 当前所有参数值的映射表（用于 QML 绑定依赖）。
         * @details NOTIFY 信号挂载在 currentParametersChanged 上，
         *          使得任何参数值变更时 QML 绑定能够感知并重新求值。
         *          解决 Q_INVOKABLE parameterValue() 无法被绑定追踪的问题。
         */
        Q_PROPERTY(QVariantMap parameterValues READ parameterValues NOTIFY
                   currentParametersChanged)

    public:
        /**
         * @brief 构造编辑器参数管理器。
         * @param parent 父对象指针。
         */
        explicit EditorParameterManager(QObject *parent = nullptr);

        /**
         * @brief 获取全局唯一实例。
         * @return EditorParameterManager 单例指针。
         */
        static EditorParameterManager *instance();

        // ==================== 类别管理 ====================

        /**
         * @brief 获取当前实体类别。
         * @return 0 = Shape, 1 = Field, 2 = Constraint。
         */
        [[nodiscard]] int currentCategory() const;

        /**
         * @brief 设置当前实体类别。
         * @details 切换类别时会重置类型 ID 为该类别下的第一个类型，
         *          并清空子类型选择状态、重置所有参数值为默认值。
         * @param category 0 = Shape, 1 = Field, 2 = Constraint。
         */
        void setCurrentCategory(int category);

        // ==================== 类型管理 ====================

        /**
         * @brief 获取当前类别下的可选类型列表。
         * @return QVariantList，每个元素为 { typeId, displayName } 的 QVariantMap。
         */
        [[nodiscard]] QVariantList availableTypes() const;

        /**
         * @brief 获取当前选中的类型 ID。
         * @return 类型 ID（ShapeType / FieldType / ConstraintType 枚举值）。
         */
        [[nodiscard]] int currentTypeId() const;

        /**
         * @brief 设置当前选中的类型 ID。
         * @details 切换类型时会：
         *          - 将该类型的所有参数值重置为默认值
         *          - 如果新类型有子类别，自动加载子类型选项
         *          - 如果新类型没有子类别，清空所有子类型状态
         * @param typeId 类型 ID。
         */
        void setCurrentTypeId(int typeId);

        // ==================== 参数访问 ====================

        /**
         * @brief 获取当前类型的参数列表。
         * @return QVariantList，每个元素为包含参数元数据和当前值的 QVariantMap。
         */
        [[nodiscard]] QVariantList currentParameters() const;

        /**
         * @brief 获取指定 key 的参数当前值。
         * @param key 参数唯一标识符。
         * @return 参数当前值，如果 key 不存在则返回 QVariant()。
         */
         [[nodiscard]] Q_INVOKABLE QVariant parameterValue(const QString &key) const;

        /** @brief 获取当前所有参数值的映射表（用于 QML 绑定依赖追踪）。NOTIFY 挂载在 currentParametersChanged 上。 */
        [[nodiscard]] QVariantMap parameterValues() const { return m_values; }

        /**
         * @brief 设置指定 key 的参数值。
         * @details 更新内部值存储并触发 currentParametersChanged 信号，
         *          使 QML 端绑定自动刷新。
         * @param key 参数唯一标识符。
         * @param value 新值。
         */
        Q_INVOKABLE void setParameterValue(const QString &key, const QVariant &value);

        // ==================== 子类型管理（第 1 层） ====================

        /**
         * @brief 当前类型是否需要第 1 层子类型选择。
         * @return 需要则返回 true。
         */
        [[nodiscard]] bool hasSubTypes() const;

        /**
         * @brief 获取第 1 层子类型的可选列表。
         * @return QVariantList，格式同 availableTypes。
         */
        [[nodiscard]] QVariantList subTypes() const;

        /**
         * @brief 获取第 1 层子类型的当前选中 ID。
         * @return 子类型 ID。
         */
        [[nodiscard]] int subTypeId() const;

        /**
         * @brief 设置第 1 层子类型的选中 ID。
         * @param typeId 子类型 ID。
         */
        void setSubTypeId(int typeId);

        /**
         * @brief 获取第 1 层子类型当前选中项的参数列表。
         * @return QVariantList，格式同 currentParameters。
         */
        [[nodiscard]] QVariantList subParameters() const;

        // ==================== 子类型管理（第 2 层） ====================

        /** @brief 当前第 1 层子类型是否需要第 2 层子类型选择。 */
        [[nodiscard]] bool hasSubSubTypes() const;

        /** @brief 获取第 2 层子类型的可选列表。 */
        [[nodiscard]] QVariantList subSubTypes() const;

        /** @brief 获取第 2 层子类型的当前选中 ID。 */
        [[nodiscard]] int subSubTypeId() const;

        /** @brief 设置第 2 层子类型的选中 ID。 */
        void setSubSubTypeId(int typeId);

        /** @brief 获取第 2 层子类型当前选中项的参数列表。 */
        [[nodiscard]] QVariantList subSubParameters() const;
        void _initDefaults();

        // ==================== 序列化辅助 ====================

        /**
         * @brief 收集当前所有参数值（含子类型链）为一个 QVariantMap。
         * @details 用于序列化为 JSON 或传递给 ModelManager。
         *          key 格式：主参数直接用 key，子类型参数前缀 "sub_"，孙子类型前缀 "subsub_"。
         * @return 所有参数值的映射表。
         */
         [[nodiscard]] Q_INVOKABLE QVariantMap collectAllValues() const;

        /**
         * @brief 从 QVariantMap 恢复所有参数值。
         * @param values 参数值映射表（格式同 collectAllValues 输出）。
         */
        Q_INVOKABLE void loadFromValues(const QVariantMap &values);

        /**
         * @brief 将所有参数重置为默认值。
         */
        Q_INVOKABLE void _initSubDefaults();

        void _initSubSubDefaults();
        
        void resetToDefaults();
        
        void emitHasSubTypes();
        
        void emitSubTypesChanged();
        
        void emitSubParametersChanged();
        
        void emitHasSubSubTypes();
        
        void emitSubSubTypesChanged();

    signals:
        /** @brief 当前类别变更信号。 */
        void currentCategoryChanged();

        /** @brief 可选类型列表变更信号。 */
        void availableTypesChanged();

        /** @brief 当前类型 ID 变更信号。 */
        void currentTypeIdChanged();

        /** @brief 当前参数列表变更信号（类型切换或值修改时触发）。 */
        void currentParametersChanged();

        /** @brief hasSubTypes 变更信号。 */
        void hasSubTypesChanged();

        /** @brief 第 1 层子类型列表变更信号。 */
        void subTypesChanged();

        /** @brief 第 1 层子类型 ID 变更信号。 */
        void subTypeIdChanged();

        /** @brief 第 1 层子类型参数列表变更信号。 */
        void subParametersChanged();

        /** @brief hasSubSubTypes 变更信号。 */
        void hasSubSubTypesChanged();

        /** @brief 第 2 层子类型列表变更信号。 */
        void subSubTypesChanged();

        /** @brief 第 2 层子类型 ID 变更信号。 */
        void subSubTypeIdChanged();

        /** @brief 第 2 层子类型参数列表变更信号。 */
        void subSubParametersChanged();

    private:
        // ==================== Schema 注册 ====================

        /**
         * @brief 注册所有实体类型的参数模式。
         * @details 在构造函数中调用，注册 Shape 8 种 + Field 2 种 + Constraint 5 种
         *          + Surface 3 种 + Curve 3 种。
         */
        void registerSchemas();

        /**
         * @brief 根据参数描述符列表构建 QML 可用的 QVariantList。
         * @details 将每个 ParameterDescriptor 转换为包含当前值和元数据的 QVariantMap。
         * @param descriptors 参数描述符列表。
         * @return 适合 QML Repeater 使用的 QVariantList。
         */
        [[nodiscard]] QVariantList buildParameterList(
            const QList<ParameterDescriptor> &descriptors) const;

        /**
         * @brief 根据子类别枚举获取对应的 schema 列表。
         * @param subCategory 0 = 无, 1 = Surface, 2 = Curve。
         * @return 对应子类别的 schema 列表引用。
         */
        [[nodiscard]] const QList<TypeParameterSchema> &schemasForSubCategory(
            int subCategory) const;

        // ==================== 数据成员 ====================

        /** Shape 类别下的所有类型 schema。 */
        QList<TypeParameterSchema> m_shapeSchemas;

        /** Field 类别下的所有类型 schema。 */
        QList<TypeParameterSchema> m_fieldSchemas;

        /** Constraint 类别下的所有类型 schema。 */
        QList<TypeParameterSchema> m_constraintSchemas;

        /** Surface 子类别下的所有类型 schema（Plane / Bezier / Cylindrical）。 */
        QList<TypeParameterSchema> m_surfaceSchemas;

        /** Curve 子类别下的所有类型 schema（Bezier / ConicSection / Helix）。 */
        QList<TypeParameterSchema> m_curveSchemas;

        /** 当前实体类别。 */
        int m_currentCategory = 0; // Shape

        /** 当前主类型 ID。 */
        int m_currentTypeId = 1; // Box

        /** 第 1 层子类型 ID。 */
        int m_subTypeId = 0;

        /** 第 2 层子类型 ID。 */
        int m_subSubTypeId = 0;

        /**
         * @brief 当 m_hasSubTypes==false 时，此值为 0。
         * @details 当 m_hasSubTypes==true 时，此值指示当前主类型的子类别：
         *          1 = Surface, 2 = Curve。
         */
        int m_currentSubCategory = 0;

        /**
         * @brief 当 m_hasSubSubTypes==false 时，此值为 0。
         * @details 当 m_hasSubSubTypes==true 时，此值指示第 1 层子类型的子类别：
         *          1 = Surface, 2 = Curve。
         */
        int m_subSubCategory = 0;

        /** 当前所有参数的值存储，key = 参数唯一标识符。 */
        QVariantMap m_values;

        /** 第 1 层子类型参数的值存储。 */
        QVariantMap m_subValues;

        /** 第 2 层子类型参数的值存储。 */
        QVariantMap m_subSubValues;
    };

} // namespace egret

#endif // EGRET_PHYSICS_PARAMETER_MANAGER_H
