/**
 * @file        model_item_data.cpp
 * @brief       模型数据项实现文件，定义编辑器和序列化使用的模型数据结构。
 * @details     实现 MaterialData 和 ModelItemData 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "model_item_data.h"
#include <QUuid>
#include <QJsonDocument>
#include <QQmlEngine>

#include "logger.h"
#include "resource_helper.h"
#include "shape_base.h"

namespace egret
{
    ModelItemDataField parseModelItemDataFromQMLJson(const QString& qmlJson)
    {
        qDebug() << qmlJson;
        ModelItemDataField data;
        data.m_materials = std::make_shared<MaterialData>();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(qmlJson.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "JSON解析失败:" << parseError.errorString();
            return {};
        }

        if (!doc.isObject()) {
            qDebug() << "JSON不是对象格式";
            return {};
        }

        QJsonObject obj = doc.object();
        auto getString = [&](const QString& key) -> QString
        {
            return obj[key].toString();
        };

        auto getDouble = [&](const QString& key) -> double
        {
            return obj[key].toDouble();
        };

        auto getVector3D = [&](const QString& key) -> QVector3D
        {
            QJsonArray arr = obj[key].toArray();
            if (arr.size() >= 3) {
                return {
                    static_cast<float>(arr[0].toDouble()),
                    static_cast<float>(arr[1].toDouble()),
                    static_cast<float>(arr[2].toDouble())
                };
            }
            return {};
        };

        auto getQuaternion = [&](const QString& key) -> QQuaternion
        {
            QJsonArray arr = obj[key].toArray();
            if (arr.size() >= 4) {
                return {
                    static_cast<float>(arr[0].toDouble()), // w
                    static_cast<float>(arr[1].toDouble()), // x
                    static_cast<float>(arr[2].toDouble()), // y
                    static_cast<float>(arr[3].toDouble()) // z
                };
            }
            return {};
        };

        // 3. 读取所有字段到临时变量
        data.m_name = getString("entity_name");

        data.m_type = obj["entity_type"].toInteger();

        if (obj.contains("category")) {
            data.m_category = obj["category"].toInt();
        }

        data.m_mass = getDouble("entity_mass");

        data.m_loadTime = getDouble("load_time");

        data.m_restitution = getDouble("restitution");

        switch (data.m_type) {
        case static_cast<std::uint32_t>(ShapeType::Box): {
            data.m_boxSize = getVector3D("box_size");
            break;
        }
        case static_cast<std::uint32_t>(ShapeType::Cylinder):
            [[fallthrough]];
        case static_cast<std::uint32_t>(ShapeType::CylindricalShell): {
            data.m_radius = getDouble("circle_radius");
            data.m_height = getDouble("height");
            break;
        }
        case static_cast<std::uint32_t>(ShapeType::Disk):
            [[fallthrough]];
        case static_cast<std::uint32_t>(ShapeType::Ring): {
            data.m_radius = getDouble("circle_radius");
            break;
        }
        case static_cast<std::uint32_t>(ShapeType::Rod): {
            data.m_length = getDouble("length");
            break;
        }
        case static_cast<std::uint32_t>(ShapeType::Sphere):
            [[fallthrough]];
        case static_cast<std::uint32_t>(ShapeType::SphericalShell): {
            data.m_radius = getDouble("circle_radius");
            break;
        }
        default:
            break;
        }

        data.m_pos = getVector3D("position");
        data.m_scale = getVector3D("scale");

        qDebug() << data.m_scale;

        data.m_rotation = getQuaternion("rotation");

        if (data.m_rotation.length() != 0) {
            data.m_rotation.normalize();
        }
        else {
            LOG_WARN_LITERAL("Invalid Rotation Info (Cannot be 0)");
            data.m_rotation = QQuaternion{1.0, 0.0, 0.0, 0.0};
        }

        data.m_initialVelo = getVector3D("initial_velocity");
        data.m_initialAnguVelo = getVector3D("initial_angular_velocity");

        data.m_materials->setBaseColor(getString("material_base_color"));

        data.m_materials->setMetalness(getDouble("material_metalness"));
        data.m_materials->setRoughness(getDouble("material_roughness"));

        // 提取 field_data / constraint_data 到 payload
        if (obj.contains("field_data") && obj["field_data"].isObject()) {
            data.m_payload["field_data"] = obj["field_data"].toObject();
        }
        if (obj.contains("constraint_data") && obj["constraint_data"].isObject()) {
            data.m_payload["constraint_data"] = obj["constraint_data"].toObject();
        }

        return data;
    }

    // MaterialData 实现
    MaterialData::MaterialData(QObject* parent) : QObject(parent)
    {
    }

    QColor MaterialData::baseColor() const { return m_baseColor; }
    qreal MaterialData::metalness() const { return m_metalness; }
    qreal MaterialData::roughness() const { return m_roughness; }
    QString MaterialData::alphaMode() const { return m_alphaMode; }

    void MaterialData::setBaseColor(const QColor& baseColor)
    {
        if (m_baseColor != baseColor) {
            m_baseColor = baseColor;
            emit baseColorChanged();
        }
    }

    void MaterialData::setMetalness(qreal metalness)
    {
        if (!qFuzzyCompare(m_metalness, metalness)) {
            m_metalness = metalness;
            emit metalnessChanged();
        }
    }

    void MaterialData::setRoughness(qreal roughness)
    {
        if (!qFuzzyCompare(m_roughness, roughness)) {
            m_roughness = roughness;
            emit roughnessChanged();
        }
    }

    void MaterialData::setAlphaMode(const QString& alphaMode)
    {
        if (m_alphaMode != alphaMode) {
            m_alphaMode = alphaMode;
            emit alphaModeChanged();
        }
    }

    QJsonObject MaterialData::toJson() const
    {
        QJsonObject obj;
        obj["baseColor"] = m_baseColor.name(QColor::HexArgb);
        obj["metalness"] = m_metalness;
        obj["roughness"] = m_roughness;
        obj["alphaMode"] = m_alphaMode;
        return obj;
    }

    bool MaterialData::fromJson(const QJsonObject& json)
    {
        if (json.contains("baseColor"))
            setBaseColor(QColor(json["baseColor"].toString()));
        if (json.contains("metalness"))
            setMetalness(json["metalness"].toDouble());
        if (json.contains("roughness"))
            setRoughness(json["roughness"].toDouble());
        if (json.contains("alphaMode"))
            setAlphaMode(json["alphaMode"].toString());
        return true;
    }

    // ModelItemData 实现
    ModelItemData::ModelItemData(QObject* parent)
        : QObject(parent)
    {
        m_data.m_materials = std::make_shared<MaterialData>(nullptr);
        // 防止 QML 通过 materials 属性接管 MaterialData 所有权
        QQmlEngine::setObjectOwnership(m_data.m_materials.get(), QQmlEngine::CppOwnership);
        m_data.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        m_data.m_scale = QVector3D(1.0f, 1.0f, 1.0f);
        m_data.m_rotation = QQuaternion();
    }

    double ModelItemData::mass() const { return m_data.m_mass; }

    double ModelItemData::loadTime() const { return m_data.m_loadTime; }

    QString ModelItemData::id() const { return m_data.m_id; }
    QString ModelItemData::name() const { return m_data.m_name; }
    QString ModelItemData::source() const { return m_data.m_source; }

    std::uint32_t ModelItemData::type() const { return m_data.m_type; }

    double ModelItemData::restitution() const { return m_data.m_restitution; }

    QVector3D ModelItemData::pos() const { return m_data.m_pos; }
    QVector3D ModelItemData::scale() const { return m_data.m_scale; }
    QQuaternion ModelItemData::rotation() const { return m_data.m_rotation; }

    QVector3D ModelItemData::initialVelo() const { return m_data.m_initialVelo; }

    QVector3D ModelItemData::initialAnguVelo() const { return m_data.m_initialAnguVelo; }

    MaterialData* ModelItemData::materials() const { return m_data.m_materials.get(); }

    void ModelItemData::setMass(double mass)
    {
        if (m_data.m_mass != mass) {
            m_data.m_mass = mass;
            emit massChanged();
        }
    }

    void ModelItemData::setLoadTime(double loadTime)
    {
        if (m_data.m_loadTime != loadTime) {
            m_data.m_loadTime = loadTime;
            emit loadTimeChanged();
        }
    }

    void ModelItemData::setId(const QString& id)
    {
        if (m_data.m_id != id) {
            m_data.m_id = id;
            emit idChanged();
        }
    }

    void ModelItemData::setName(const QString& name)
    {
        if (m_data.m_name != name) {
            m_data.m_name = name;
            emit nameChanged();
        }
    }

    void ModelItemData::setType(std::uint32_t type)
    {
        if (m_data.m_type != type) {
            m_data.m_type = type;
            matchSource();
            emit typeChanged();
        }
    }

    void ModelItemData::setType(ShapeType type)
    {
        setType(static_cast<std::uint32_t>(type));
    }

    void ModelItemData::setSource(const QString& source)
    {
        if (m_data.m_source != source) {
            m_data.m_source = source;
            emit sourceChanged();
        }
    }

    void ModelItemData::setPos(const QVector3D& pos)
    {
        if (m_data.m_pos != pos) {
            m_data.m_pos = pos;
            emit posChanged();
        }
    }

    void ModelItemData::setScale(const QVector3D& scale)
    {
        if (m_data.m_scale != scale) {
            m_data.m_scale = scale;
            emit scaleChanged();
        }
    }

    void ModelItemData::setRotation(const QQuaternion& rotation)
    {
        if (m_data.m_rotation != rotation) {
            m_data.m_rotation = rotation;
            emit rotationChanged();
        }
    }

    void ModelItemData::setInitialVelo(const QVector3D& initialVelo)
    {
        if (m_data.m_initialVelo != initialVelo) {
            m_data.m_initialVelo = initialVelo;
            emit initialAnguVeloChanged();
        }
    }

    void ModelItemData::setInitialAnguVelo(const QVector3D& initialAnguVelo)
    {
        if (m_data.m_initialAnguVelo != initialAnguVelo) {
            m_data.m_initialAnguVelo = initialAnguVelo;
            emit initialAnguVeloChanged();
        }
    }

    void ModelItemData::setRestitution(double restitution)
    {
        if (m_data.m_restitution != restitution) {
            m_data.m_restitution = restitution;
            emit restitutionChanged();
        }
    }

    QVector3D ModelItemData::boxSize() const
    {
        return m_data.m_boxSize.value_or(QVector3D(1.0, 1.0, 1.0));
    }

    void ModelItemData::setBoxSize(const QVector3D& boxSize)
    {
        if (!m_data.m_boxSize.has_value() || !qFuzzyCompare(m_data.m_boxSize.value(), boxSize)) {
            m_data.m_boxSize = boxSize;
            emit boxSizeChanged();
        }
    }

    double ModelItemData::radius() const
    {
        return m_data.m_radius.value_or(0.0);
    }

    void ModelItemData::setRadius(double radius)
    {
        if (!m_data.m_radius.has_value() || !qFuzzyCompare(m_data.m_radius.value(), radius)) {
            m_data.m_radius = radius;
            emit radiusChanged();
        }
    }

    double ModelItemData::height() const
    {
        return m_data.m_height.value_or(0.0);
    }

    void ModelItemData::setHeight(double height)
    {
        if (!m_data.m_height.has_value() || !qFuzzyCompare(m_data.m_height.value(), height)) {
            m_data.m_height = height;
            emit heightChanged();
        }
    }

    double ModelItemData::length() const
    {
        return m_data.m_length.value_or(0.0);
    }

    void ModelItemData::setLength(double length)
    {
        if (!m_data.m_length.has_value() || !qFuzzyCompare(m_data.m_length.value(), length)) {
            m_data.m_length = length;
            emit lengthChanged();
        }
    }

    void ModelItemData::matchSource()
    {
        const QString source{ResourceHelper::getSourceByShape(m_data.m_type)};
        if (!source.isEmpty()) {
            if (m_data.m_source != source) {
                m_data.m_source = source;
                emit sourceChanged();
            }
        }
        else {
            LOG_FATAL_LITERAL("Known Shape");
        }
    }

    QJsonObject ModelItemData::toJson() const
    {
        QJsonObject obj;
        // 公共字段
        obj["category"] = m_data.m_category;
        obj["id"] = m_data.m_id;
        obj["name"] = m_data.m_name;
        obj["load_time"] = m_data.m_loadTime;
        obj["type"] = QJsonValue{static_cast<std::int64_t>(m_data.m_type)};
        obj["pos"] = QJsonArray{m_data.m_pos.x(), m_data.m_pos.y(), m_data.m_pos.z()};

        if (m_data.m_category == 0) {
            // ============ Shape ============
            obj["mass"] = m_data.m_mass;
            obj["restitution"] = m_data.m_restitution;
            obj["source"] = m_data.m_source;
            obj["scale"] = QJsonArray{m_data.m_scale.x(), m_data.m_scale.y(), m_data.m_scale.z()};
            obj["rotation"] = QJsonArray{
                m_data.m_rotation.scalar(), m_data.m_rotation.x(), m_data.m_rotation.y(), m_data.m_rotation.z()
            };
            obj["initial_velocity"] = QJsonArray{
                m_data.m_initialVelo.x(), m_data.m_initialVelo.y(), m_data.m_initialVelo.z()
            };
            obj["initial_angular_velocity"] = QJsonArray{
                m_data.m_initialAnguVelo.x(), m_data.m_initialAnguVelo.y(), m_data.m_initialAnguVelo.z()
            };
            if (m_data.m_boxSize.has_value())
                obj["box_size"] = QJsonArray{m_data.m_boxSize->x(), m_data.m_boxSize->y(), m_data.m_boxSize->z()};
            if (m_data.m_radius.has_value())
                obj["radius"] = m_data.m_radius.value();
            if (m_data.m_height.has_value())
                obj["height"] = m_data.m_height.value();
            if (m_data.m_length.has_value())
                obj["length"] = m_data.m_length.value();
            obj["materials"] = m_data.m_materials->toJson();
        }

        // 透传 payload（field_data / constraint_data）
        for (auto it = m_data.m_payload.begin(); it != m_data.m_payload.end(); ++it) {
            obj[it.key()] = it.value();
        }

        return obj;
    }

    bool ModelItemData::fromJson(const QJsonObject& json)
    {
        // 公共字段
        if (json.contains("category")) {
            m_data.m_category = json["category"].toInt();
        }
        if (json.contains("load_time")) {
            setLoadTime(json["load_time"].toDouble());
        }
        if (json.contains("type")) {
            setType(static_cast<std::uint32_t>(json["type"].toInteger()));
        }
        if (json.contains("id")) {
            setId(json["id"].toString());
        }
        if (json.contains("name")) {
            setName(json["name"].toString());
        }
        if (json.contains("pos") && json["pos"].isArray()) {
            QJsonArray posArr = json["pos"].toArray();
            if (posArr.size() >= 3) {
                setPos(QVector3D(static_cast<float>(posArr[0].toDouble()), static_cast<float>(posArr[1].toDouble()),
                                 static_cast<float>(posArr[2].toDouble())));
            }
        }

        if (m_data.m_category == 0) {
            // ============ Shape ============
            if (json.contains("mass"))
                setMass(json["mass"].toDouble());
            if (json.contains("restitution"))
                setRestitution(json["restitution"].toDouble());
            if (json.contains("source"))
                setSource(json["source"].toString());
            if (json.contains("scale") && json["scale"].isArray()) {
                QJsonArray arr = json["scale"].toArray();
                if (arr.size() >= 3)
                    setScale(QVector3D(static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()), static_cast<float>(arr[2].toDouble())));
            }
            if (json.contains("rotation") && json["rotation"].isArray()) {
                QJsonArray arr = json["rotation"].toArray();
                if (arr.size() >= 4)
                    setRotation(QQuaternion(static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()), static_cast<float>(arr[2].toDouble()), static_cast<float>(arr[3].toDouble())));
            }
            if (json.contains("initial_velocity") && json["initial_velocity"].isArray()) {
                QJsonArray arr = json["initial_velocity"].toArray();
                if (arr.size() >= 3)
                    setInitialVelo(QVector3D(static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()), static_cast<float>(arr[2].toDouble())));
            }
            if (json.contains("initial_angular_velocity") && json["initial_angular_velocity"].isArray()) {
                QJsonArray arr = json["initial_angular_velocity"].toArray();
                if (arr.size() >= 3)
                    setInitialAnguVelo(QVector3D(static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()), static_cast<float>(arr[2].toDouble())));
            }
            if (json.contains("materials") && json["materials"].isObject())
                m_data.m_materials->fromJson(json["materials"].toObject());
            if (json.contains("box_size") && json["box_size"].isArray()) {
                QJsonArray arr = json["box_size"].toArray();
                if (arr.size() >= 3) {
                    m_data.m_boxSize = QVector3D(static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()), static_cast<float>(arr[2].toDouble()));
                    emit boxSizeChanged();
                }
            }
            if (json.contains("radius")) {
                m_data.m_radius = json["radius"].toDouble();
                emit radiusChanged();
            } else if (json.contains("circle_radius")) {
                m_data.m_radius = json["circle_radius"].toDouble();
                emit radiusChanged();
            }
            if (json.contains("height")) {
                m_data.m_height = json["height"].toDouble();
                emit heightChanged();
            }
            if (json.contains("length")) {
                m_data.m_length = json["length"].toDouble();
                emit lengthChanged();
            }
        }

        // 恢复 payload（非标准字段存入 payload）
        m_data.m_payload = {};
        for (auto it = json.begin(); it != json.end(); ++it) {
            const QString &k = it.key();
            if (k == "mass" || k == "load_time" || k == "restitution"
                || k == "id" || k == "name" || k == "source"
                || k == "type" || k == "category" || k == "box_size" || k == "circle_radius"
                || k == "radius" || k == "height" || k == "length"
                || k == "pos" || k == "scale" || k == "rotation"
                || k == "initial_velocity" || k == "initial_angular_velocity"
                || k == "materials") {
                continue;
            }
            m_data.m_payload[k] = it.value();
        }

        return true;
    }

    QSharedPointer<ModelItemData> ModelItemData::clone() const
    {
        return QSharedPointer<ModelItemData>(createCopy(*this));
    }

    ModelItemData* ModelItemData::createCopy(const ModelItemData& source, QObject* parent)
    {
        auto* copy = new ModelItemData(parent);
        copy->setMass(source.mass());
        copy->setLoadTime(source.loadTime());
        copy->setId(source.id());
        copy->setName(source.name());
        copy->setSource(source.source());
        copy->setPos(source.pos());
        copy->setScale(source.scale());
        copy->setRotation(source.rotation());
        copy->setInitialVelo(source.initialVelo());
        copy->setInitialAnguVelo(source.initialAnguVelo());

        MaterialData* sourceMaterials = source.materials();
        if (sourceMaterials) {
            MaterialData* targetMaterials = copy->materials();
            targetMaterials->setBaseColor(sourceMaterials->baseColor());
            targetMaterials->setMetalness(sourceMaterials->metalness());
            targetMaterials->setRoughness(sourceMaterials->roughness());
            targetMaterials->setAlphaMode(sourceMaterials->alphaMode());
        }

        copy->m_data.m_payload = source.m_data.m_payload;
        copy->m_data.m_category = source.m_data.m_category;

        return copy;
    }
} // egret
