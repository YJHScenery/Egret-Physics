/**
 * @file        parameter_manager.cpp
 * @brief       编辑器参数管理器实现文件。
 * @details     实现 EditorParameterManager 类的所有成员函数，
 *              包括 schema 注册、值管理、子类型链切换等。
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

#include "parameter_manager.h"

#include "shape_base.h"
#include "field_base.h"
#include "constraints_base.h"
#include "surface_base.h"
#include "curve_base.h"

#include <QDebug>
#include <QVector3D>

namespace egret
{
    //  单例
    EditorParameterManager* EditorParameterManager::instance()
    {
        static EditorParameterManager manager;
        return &manager;
    }


    //  构造与 Schema 注册
    EditorParameterManager::EditorParameterManager(QObject* parent)
        : QObject(parent)
    {
        registerSchemas();
    }

    void EditorParameterManager::registerSchemas()
    {
        
        //  Shape 类别 (ShapeType 枚举)
        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Box),
            QStringLiteral("标准盒体"),
            {
                {
                    QStringLiteral("box_size"), QStringLiteral("长、宽、高"),
                    QStringLiteral("vector3d"), QVector3D(1.0f, 1.0f, 1.0f),
                    0.001, 100000.0, 0.1, 3
                },
            },
            0 // 无子类别
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Cylinder),
            QStringLiteral("标准圆柱体"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("height"), QStringLiteral("高"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::CylindricalShell),
            QStringLiteral("标准圆柱面"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("height"), QStringLiteral("高"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Disk),
            QStringLiteral("标准圆盘"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Ring),
            QStringLiteral("标准圆环"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Rod),
            QStringLiteral("标准细杆"),
            {
                {
                    QStringLiteral("length"), QStringLiteral("长度"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::Sphere),
            QStringLiteral("标准球体"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_shapeSchemas.append({
            static_cast<int>(ShapeType::SphericalShell),
            QStringLiteral("标准球壳"),
            {
                {
                    QStringLiteral("circle_radius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        
        //  Field 类别 (FieldType 枚举)

        m_fieldSchemas.append({
            static_cast<int>(FieldType::Gravity),
            QStringLiteral("重力场"),
            {
                {
                    QStringLiteral("gravity"), QStringLiteral("重力加速度"),
                    QStringLiteral("vector3d"), QVector3D(0.0f, -9.8f, 0.0f),
                    -100000.0, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_fieldSchemas.append({
            static_cast<int>(FieldType::Gravitation),
            QStringLiteral("引力场"),
            {
                {
                    QStringLiteral("mass"), QStringLiteral("场源质量"),
                    QStringLiteral("float"), 1.0,
                    0.001, 1.0e15, 0.1, 3
                },
                {
                    QStringLiteral("coupling"), QStringLiteral("耦合系数"),
                    QStringLiteral("float"), 6.67430e-11,
                    0.0, 1.0e10, 1.0e-11, 11
                },
                {
                    QStringLiteral("sourcePos"), QStringLiteral("场源位置"),
                    QStringLiteral("vector3d"), QVector3D(0.0f, 0.0f, 0.0f),
                    -100000.0, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("fixed"), QStringLiteral("固定场源"),
                    QStringLiteral("enum"), 0,
                    0.0, 0.0, 0.0, 0,
                    {QStringLiteral("否"), QStringLiteral("是")}
                },
            },
            0
        });

        
        //  Constraint 类别 (ConstraintType 枚举)

        m_constraintSchemas.append({
            static_cast<int>(ConstraintType::ConnectingLine),
            QStringLiteral("连接线"),
            {
                {
                    QStringLiteral("length"), QStringLiteral("约束长度"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });
        m_constraintSchemas.append({
            static_cast<int>(ConstraintType::SimplePendulum),
            QStringLiteral("单摆"),
            {
                {
                    QStringLiteral("length"), QStringLiteral("摆长"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_constraintSchemas.append({
            static_cast<int>(ConstraintType::ConnectingRod),
            QStringLiteral("连接杆"),
            {
                {
                    QStringLiteral("length"), QStringLiteral("杆长"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_constraintSchemas.append({
            static_cast<int>(ConstraintType::Spring),
            QStringLiteral("弹簧"),
            {
                {
                    QStringLiteral("stiffness"), QStringLiteral("弹性系数"),
                    QStringLiteral("float"), 100.0,
                    0.001, 1.0e10, 1.0, 3
                },
                {
                    QStringLiteral("restLength"), QStringLiteral("原长"),
                    QStringLiteral("float"), 1.0,
                    0.0, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("damping"), QStringLiteral("阻尼"),
                    QStringLiteral("float"), 0.1,
                    0.0, 1000.0, 0.01, 3
                },
            },
            0
        });

        // SupportSurface — 触发子类型链
        m_constraintSchemas.append({
            static_cast<int>(ConstraintType::SupportSurface),
            QStringLiteral("支撑曲面"),
            {
                {
                    QStringLiteral("restitution"), QStringLiteral("恢复系数"),
                    QStringLiteral("float"), -1.0,
                    -1.0, 1.0, 0.05, 3
                },
                {
                    QStringLiteral("frictionCoeff"), QStringLiteral("摩擦系数"),
                    QStringLiteral("float"), 0.0,
                    0.0, 10.0, 0.05, 3
                },
            },
            1 // subCategory = Surface
        });

        
        //  Surface 子类别 (SurfaceType 枚举)
        

        m_surfaceSchemas.append({
            static_cast<int>(SurfaceType::Plane),
            QStringLiteral("平面"),
            {
                {
                    QStringLiteral("planeNormal"), QStringLiteral("法线方向"),
                    QStringLiteral("vector3d"), QVector3D(0.0f, 1.0f, 0.0f),
                    -1.0, 1.0, 0.01, 3
                },
                {
                    QStringLiteral("planeOffset"), QStringLiteral("偏移"),
                    QStringLiteral("float"), 0.0,
                    -100000.0, 100000.0, 0.1, 3
                },
            },
            0
        });

        m_surfaceSchemas.append({
            static_cast<int>(SurfaceType::Bezier),
            QStringLiteral("Bezier 曲面"),
            {
                {
                    QStringLiteral("bezierUSubdiv"), QStringLiteral("U 方向细分"),
                    QStringLiteral("float"), 16.0,
                    2.0, 256.0, 1.0, 0
                },
                {
                    QStringLiteral("bezierVSubdiv"), QStringLiteral("V 方向细分"),
                    QStringLiteral("float"), 16.0,
                    2.0, 256.0, 1.0, 0
                },
            },
            0
        });

        // Cylindrical 曲面 — 触发 Curve 子类型链
        m_surfaceSchemas.append({
            static_cast<int>(SurfaceType::Cylindrical),
            QStringLiteral("柱面"),
            {
                {
                    QStringLiteral("cylindricalHeight"), QStringLiteral("高度"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("cylindricalRadius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 0.1,
                    0.0, 100000.0, 0.05, 3
                },
            },
            2 // subCategory = Curve
        });

        
        //  Curve 子类别 (CurveType 枚举)
        m_curveSchemas.append({
            static_cast<int>(CurveType::Bezier),
            QStringLiteral("Bezier 曲线"),
            {
                {
                    QStringLiteral("curveBezierOrder"), QStringLiteral("阶数"),
                    QStringLiteral("float"), 3.0,
                    1.0, 10.0, 1.0, 0
                },
            },
            0
        });

        m_curveSchemas.append({
            static_cast<int>(CurveType::ConicSection),
            QStringLiteral("圆锥曲线"),
            {
                {
                    QStringLiteral("curveConicType"), QStringLiteral("曲线类型"),
                    QStringLiteral("enum"), 0,
                    0.0, 0.0, 0.0, 0,
                    {
                        QStringLiteral("椭圆"), QStringLiteral("抛物线"),
                        QStringLiteral("双曲线")
                    }
                },
            },
            0
        });

        m_curveSchemas.append({
            static_cast<int>(CurveType::Helix),
            QStringLiteral("螺旋线"),
            {
                {
                    QStringLiteral("helixRadius"), QStringLiteral("半径"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("helixPitch"), QStringLiteral("螺距"),
                    QStringLiteral("float"), 1.0,
                    0.001, 100000.0, 0.1, 3
                },
                {
                    QStringLiteral("helixTurns"), QStringLiteral("圈数"),
                    QStringLiteral("float"), 3.0,
                    0.1, 1000.0, 0.5, 1
                },
            },
            0
        });

        // 初始化默认值
        resetToDefaults();
    }


    //  类别管理


    int EditorParameterManager::currentCategory() const
    {
        return m_currentCategory;
    }

    void EditorParameterManager::setCurrentCategory(const int category)
    {
        if (m_currentCategory == category)
            return;

        m_currentCategory = category;
        emit currentCategoryChanged();

        // 切换类别时，选择该类别下的第一个类型
        const auto& schemas = [this]() -> const QList<TypeParameterSchema>&
        {
            switch (m_currentCategory) {
            case 0: return m_shapeSchemas;
            case 1: return m_fieldSchemas;
            case 2: return m_constraintSchemas;
            default: return m_shapeSchemas;
            }
        }();

        emit availableTypesChanged();

        if (!schemas.isEmpty()) {
            // 直接设置 m_currentTypeId，跳过 setCurrentTypeId 的重复信号
            m_currentTypeId = schemas.first().typeId;
            m_currentSubCategory = schemas.first().subCategory;
            m_subTypeId = 0;
            m_subSubTypeId = 0;
            m_subSubCategory = 0;
            resetToDefaults();

            emit currentTypeIdChanged();
            emit currentParametersChanged();
            emitHasSubTypes();
            emitSubTypesChanged();
            emitSubParametersChanged();
            emitHasSubSubTypes();
        }
    }


    //  类型管理


    const QList<TypeParameterSchema>& EditorParameterManager::schemasForSubCategory(
        const int subCategory) const
    {
        switch (subCategory) {
        case 1: return m_surfaceSchemas;
        case 2: return m_curveSchemas;
        default: return m_surfaceSchemas; // 不会到达
        }
    }

    const QList<TypeParameterSchema>& currentSchemasRef(
        const int category,
        const QList<TypeParameterSchema>& shapeSchemas,
        const QList<TypeParameterSchema>& fieldSchemas,
        const QList<TypeParameterSchema>& constraintSchemas)
    {
        switch (category) {
        case 0: return shapeSchemas;
        case 1: return fieldSchemas;
        case 2: return constraintSchemas;
        default: return shapeSchemas;
        }
    }

    QVariantList EditorParameterManager::availableTypes() const
    {
        QVariantList result;
        const auto& schemas = currentSchemasRef(
            m_currentCategory, m_shapeSchemas, m_fieldSchemas, m_constraintSchemas);

        for (const auto& schema : schemas) {
            QVariantMap item;
            item[QStringLiteral("typeId")] = schema.typeId;
            item[QStringLiteral("displayName")] = schema.displayName;
            result.append(item);
        }
        return result;
    }

    int EditorParameterManager::currentTypeId() const
    {
        return m_currentTypeId;
    }

    void EditorParameterManager::setCurrentTypeId(const int typeId)
    {
        if (m_currentTypeId == typeId)
            return;

        m_currentTypeId = typeId;
        emit currentTypeIdChanged();

        // 查找该类型的 schema，获取 subCategory
        const auto& schemas = currentSchemasRef(
            m_currentCategory, m_shapeSchemas, m_fieldSchemas, m_constraintSchemas);

        int newSubCategory = 0;
        for (const auto& schema : schemas) {
            if (schema.typeId == typeId) {
                newSubCategory = schema.subCategory;
                break;
            }
        }

        // 清空子类型状态
        m_subTypeId = 0;
        m_subSubTypeId = 0;
        m_subSubCategory = 0;

        // 更新子类别
        m_currentSubCategory = newSubCategory;

        // 如果新类型有子类别，自动选择第一个子类型
        if (m_currentSubCategory != 0) {
            const auto& subSchemas = schemasForSubCategory(m_currentSubCategory);
            if (!subSchemas.isEmpty()) {
                m_subTypeId = subSchemas.first().typeId;
                // 查找该子类型是否又有子类别
                for (const auto& subSchema : subSchemas) {
                    if (subSchema.typeId == m_subTypeId) {
                        m_subSubCategory = subSchema.subCategory;
                        break;
                    }
                }
                // 如果第 1 层子类型又有子类别，自动选择第一个
                if (m_subSubCategory != 0) {
                    const auto& subSubSchemas = schemasForSubCategory(m_subSubCategory);
                    if (!subSubSchemas.isEmpty()) {
                        m_subSubTypeId = subSubSchemas.first().typeId;
                    }
                }
            }
        }

        // 重置参数值为新类型的默认值
        resetToDefaults();

        emit currentParametersChanged();
        emitHasSubTypes();
        emitSubTypesChanged();
        emitSubParametersChanged();
        emitHasSubSubTypes();
    }
    

    QVariantList EditorParameterManager::currentParameters() const
    {
        const auto& schemas = currentSchemasRef(
            m_currentCategory, m_shapeSchemas, m_fieldSchemas, m_constraintSchemas);

        for (const auto& schema : schemas) {
            if (schema.typeId == m_currentTypeId) {
                return buildParameterList(schema.parameters);
            }
        }
        return {};
    }

    QVariant EditorParameterManager::parameterValue(const QString& key) const
    {
        return m_values.value(key);
    }

    void EditorParameterManager::setParameterValue(const QString& key, const QVariant& value)
    {
        if (m_values.value(key) == value)
            return;

        m_values[key] = value;
        emit currentParametersChanged();
    }

    QVariantList EditorParameterManager::buildParameterList(
        const QList<ParameterDescriptor>& descriptors) const
    {
        QVariantList result;
        for (const auto& desc : descriptors) {
            QVariantMap item;
            item[QStringLiteral("key")] = desc.key;
            item[QStringLiteral("displayName")] = desc.displayName;
            item[QStringLiteral("type")] = desc.type;

            // 当前值
            const QVariant currentValue = m_values.value(desc.key, desc.defaultValue);
            item[QStringLiteral("value")] = currentValue;

            // float 类型约束
            if (desc.type == QStringLiteral("float") || desc.type == QStringLiteral("vector3d")) {
                item[QStringLiteral("min")] = desc.min;
                item[QStringLiteral("max")] = desc.max;
                item[QStringLiteral("step")] = desc.step;
                item[QStringLiteral("decimals")] = desc.decimals;
            }

            // enum 类型选项
            if (desc.type == QStringLiteral("enum")) {
                item[QStringLiteral("enumOptions")] = QVariant(desc.enumOptions);
            }

            result.append(item);
        }
        return result;
    }


    //  子类型管理（第 1 层）


    bool EditorParameterManager::hasSubTypes() const
    {
        return m_currentSubCategory != 0;
    }

    QVariantList EditorParameterManager::subTypes() const
    {
        if (m_currentSubCategory == 0)
            return {};

        QVariantList result;
        const auto& schemas = schemasForSubCategory(m_currentSubCategory);
        for (const auto& schema : schemas) {
            QVariantMap item;
            item[QStringLiteral("typeId")] = schema.typeId;
            item[QStringLiteral("displayName")] = schema.displayName;
            result.append(item);
        }
        return result;
    }

    int EditorParameterManager::subTypeId() const
    {
        return m_subTypeId;
    }

    void EditorParameterManager::setSubTypeId(const int typeId)
    {
        if (m_subTypeId == typeId)
            return;

        m_subTypeId = typeId;
        emit subTypeIdChanged();

        // 检查新的子类型是否有子子类别
        const auto& schemas = schemasForSubCategory(m_currentSubCategory);
        int newSubSubCategory = 0;
        for (const auto& schema : schemas) {
            if (schema.typeId == typeId) {
                newSubSubCategory = schema.subCategory;
                break;
            }
        }

        m_subSubCategory = newSubSubCategory;
        m_subSubTypeId = 0;

        // 如果新的子类型有子子类别，自动选择第一个
        if (m_subSubCategory != 0) {
            const auto& subSubSchemas = schemasForSubCategory(m_subSubCategory);
            if (!subSubSchemas.isEmpty()) {
                m_subSubTypeId = subSubSchemas.first().typeId;
            }
        }

        // 重置子类型参数值
        m_subValues.clear();
        m_subSubValues.clear();
        _initSubDefaults();
        _initSubSubDefaults();

        emit subParametersChanged();
        emitHasSubSubTypes();
    }

    QVariantList EditorParameterManager::subParameters() const
    {
        if (m_currentSubCategory == 0)
            return {};

        const auto& schemas = schemasForSubCategory(m_currentSubCategory);
        for (const auto& schema : schemas) {
            if (schema.typeId == m_subTypeId) {
                QVariantList result;
                for (const auto& desc : schema.parameters) {
                    QVariantMap item;
                    item[QStringLiteral("key")] = desc.key;
                    item[QStringLiteral("displayName")] = desc.displayName;
                    item[QStringLiteral("type")] = desc.type;

                    const QVariant currentValue = m_subValues.value(desc.key, desc.defaultValue);
                    item[QStringLiteral("value")] = currentValue;

                    if (desc.type == QStringLiteral("float") || desc.type == QStringLiteral("vector3d")) {
                        item[QStringLiteral("min")] = desc.min;
                        item[QStringLiteral("max")] = desc.max;
                        item[QStringLiteral("step")] = desc.step;
                        item[QStringLiteral("decimals")] = desc.decimals;
                    }
                    if (desc.type == QStringLiteral("enum")) {
                        item[QStringLiteral("enumOptions")] = QVariant(desc.enumOptions);
                    }

                    result.append(item);
                }
                return result;
            }
        }
        return {};
    }


    //  子类型管理（第 2 层）


    bool EditorParameterManager::hasSubSubTypes() const
    {
        return m_subSubCategory != 0;
    }

    QVariantList EditorParameterManager::subSubTypes() const
    {
        if (m_subSubCategory == 0)
            return {};

        QVariantList result;
        const auto& schemas = schemasForSubCategory(m_subSubCategory);
        for (const auto& schema : schemas) {
            QVariantMap item;
            item[QStringLiteral("typeId")] = schema.typeId;
            item[QStringLiteral("displayName")] = schema.displayName;
            result.append(item);
        }
        return result;
    }

    int EditorParameterManager::subSubTypeId() const
    {
        return m_subSubTypeId;
    }

    void EditorParameterManager::setSubSubTypeId(const int typeId)
    {
        if (m_subSubTypeId == typeId)
            return;

        m_subSubTypeId = typeId;
        emit subSubTypeIdChanged();

        // 重置子子类型参数值
        m_subSubValues.clear();
        _initSubSubDefaults();

        emit subSubParametersChanged();
    }

    QVariantList EditorParameterManager::subSubParameters() const
    {
        if (m_subSubCategory == 0)
            return {};

        const auto& schemas = schemasForSubCategory(m_subSubCategory);
        for (const auto& schema : schemas) {
            if (schema.typeId == m_subSubTypeId) {
                QVariantList result;
                for (const auto& desc : schema.parameters) {
                    QVariantMap item;
                    item[QStringLiteral("key")] = desc.key;
                    item[QStringLiteral("displayName")] = desc.displayName;
                    item[QStringLiteral("type")] = desc.type;

                    const QVariant currentValue = m_subSubValues.value(desc.key, desc.defaultValue);
                    item[QStringLiteral("value")] = currentValue;

                    if (desc.type == QStringLiteral("float") || desc.type == QStringLiteral("vector3d")) {
                        item[QStringLiteral("min")] = desc.min;
                        item[QStringLiteral("max")] = desc.max;
                        item[QStringLiteral("step")] = desc.step;
                        item[QStringLiteral("decimals")] = desc.decimals;
                    }
                    if (desc.type == QStringLiteral("enum")) {
                        item[QStringLiteral("enumOptions")] = QVariant(desc.enumOptions);
                    }

                    result.append(item);
                }
                return result;
            }
        }
        return {};
    }


    //  默认值初始化辅助


    void EditorParameterManager::_initDefaults()
    {
        m_values.clear();
        const auto& schemas = currentSchemasRef(
            m_currentCategory, m_shapeSchemas, m_fieldSchemas, m_constraintSchemas);
        for (const auto& schema : schemas) {
            if (schema.typeId == m_currentTypeId) {
                for (const auto& desc : schema.parameters) {
                    m_values[desc.key] = desc.defaultValue;
                }
                break;
            }
        }
    }

    void EditorParameterManager::_initSubDefaults()
    {
        m_subValues.clear();
        if (m_currentSubCategory == 0)
            return;
        const auto& schemas = schemasForSubCategory(m_currentSubCategory);
        for (const auto& schema : schemas) {
            if (schema.typeId == m_subTypeId) {
                for (const auto& desc : schema.parameters) {
                    m_subValues[desc.key] = desc.defaultValue;
                }
                break;
            }
        }
    }

    void EditorParameterManager::_initSubSubDefaults()
    {
        m_subSubValues.clear();
        if (m_subSubCategory == 0)
            return;
        const auto& schemas = schemasForSubCategory(m_subSubCategory);
        for (const auto& schema : schemas) {
            if (schema.typeId == m_subSubTypeId) {
                for (const auto& desc : schema.parameters) {
                    m_subSubValues[desc.key] = desc.defaultValue;
                }
                break;
            }
        }
    }


    //  公开的默认值重置


    void EditorParameterManager::resetToDefaults()
    {
        _initDefaults();
        _initSubDefaults();
        _initSubSubDefaults();
    }


    //  信号发射辅助（内联在 set 方法中调用）


    void EditorParameterManager::emitHasSubTypes()
    {
        emit hasSubTypesChanged();
    }

    void EditorParameterManager::emitSubTypesChanged()
    {
        emit subTypesChanged();
    }

    void EditorParameterManager::emitSubParametersChanged()
    {
        emit subParametersChanged();
    }

    void EditorParameterManager::emitHasSubSubTypes()
    {
        emit hasSubSubTypesChanged();
    }


    //  序列化辅助


    QVariantMap EditorParameterManager::collectAllValues() const
    {
        QVariantMap result = m_values;

        // 添加子类型参数（前缀 "sub_"）
        for (auto it = m_subValues.cbegin(); it != m_subValues.cend(); ++it) {
            result[QStringLiteral("sub_") + it.key()] = it.value();
        }

        // 添加子子类型参数（前缀 "subsub_"）
        for (auto it = m_subSubValues.cbegin(); it != m_subSubValues.cend(); ++it) {
            result[QStringLiteral("subsub_") + it.key()] = it.value();
        }

        return result;
    }

    void EditorParameterManager::loadFromValues(const QVariantMap& values)
    {
        m_values.clear();
        m_subValues.clear();
        m_subSubValues.clear();

        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            const QString& key = it.key();
            if (key.startsWith(QStringLiteral("subsub_"))) {
                m_subSubValues[key.mid(7)] = it.value();
            }
            else if (key.startsWith(QStringLiteral("sub_"))) {
                m_subValues[key.mid(4)] = it.value();
            }
            else {
                m_values[key] = it.value();
            }
        }

        emit currentParametersChanged();
        if (hasSubTypes()) {
            emit subParametersChanged();
        }
        if (hasSubSubTypes()) {
            emit subSubParametersChanged();
        }
    }
} // namespace egret
