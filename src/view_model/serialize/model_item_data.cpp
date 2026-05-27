//
// Created by jehor on 2026/5/25.
//

#include "model_item_data.h"
#include <QUuid>
#include <QJsonDocument>

namespace egret
{
    const QMap<QString, QString> ModelItemData::StaticGeneralTypeSourceMap = {
        {"Standard Box", "#Cube"},
        {"Standard Cylinder", "#Cylinder"},
        {"Standard Cylindrical Shell", "qrc:/model_3d/assets/model_3d/cylinder_side/cylinder_side.mesh"},
        {"Standard Disk", "#Cylinder"},
        {"Standard Rod", "#Cylinder"},
        {"Standard Sphere", "#Sphere"},
        {"Standard Spherical Shell", "#Sphere"},

        {"标准盒体", "#Cube"},
        {"标准圆柱体", "#Cylinder"},
        {"标准圆柱面", "qrc:/model_3d/assets/model_3d/cylinder_side/cylinder_side.mesh"},
        {"标准圆盘", "#Cylinder"},
        {"标准圆环", "#Cylinder"},
        {"标准细杆", "#Cylinder"},
        {"标准球体", "#Sphere"},
        {"标准球壳", "#Sphere"}
    };

    const QString ModelItemData::StaticBasicRingSourceStr = "qrc:/model_3d/assets/model_3d/torus/mesh/torus_R%1.mesh";


    ModelItemDataField parseModelItemDataFromQMLJson(const QString& qmlJson)
    {
        qDebug() << qmlJson;
        ModelItemDataField data;
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
                return QVector3D(
                    arr[0].toDouble(),
                    arr[1].toDouble(),
                    arr[2].toDouble()
                );
            }
            return QVector3D();
        };

        auto getQuaternion = [&](const QString& key) -> QQuaternion
        {
            QJsonArray arr = obj[key].toArray();
            if (arr.size() >= 4) {
                return QQuaternion(
                    arr[0].toDouble(), // w
                    arr[1].toDouble(), // x
                    arr[2].toDouble(), // y
                    arr[3].toDouble() // z
                );
            }
            return QQuaternion();
        };

        // 3. 读取所有字段到临时变量
        data.m_name = getString("entity_name");
        data.m_type = getString("entity_type");
        data.m_mass = getDouble("entity_mass");
        data.m_loadTime = getDouble("load_time");
        data.m_restitution = getDouble("restitution");

        data.m_pos = getVector3D("position");
        data.m_scale = getVector3D("scale");
        data.m_rotation = getQuaternion("rotation");
        data.m_initialVelo = getVector3D("initial_velocity");
        data.m_initialAnguVelo = getVector3D("initial_angular_velocity");

        data.m_baseColor = getString("material_base_color");

        data.m_metalness = getDouble("material_metalness");
        data.m_roughness = getDouble("material_roughness");

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
          , m_materials(new MaterialData(this))
          , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
        m_scale = QVector3D(1.0f, 1.0f, 1.0f);
        m_rotation = QQuaternion();
    }

    double ModelItemData::mass() const { return m_mass; }

    double ModelItemData::loadTime() const { return m_loadTime; }

    QString ModelItemData::id() const { return m_id; }
    QString ModelItemData::name() const { return m_name; }
    QString ModelItemData::source() const { return m_source; }

    QString ModelItemData::type() const { return m_type; }

    double ModelItemData::restitution() const { return m_restitution; }

    QVector3D ModelItemData::pos() const { return m_pos; }
    QVector3D ModelItemData::scale() const { return m_scale; }
    QQuaternion ModelItemData::rotation() const { return m_rotation; }

    QVector3D ModelItemData::initialVelo() const { return m_initialVelo; }

    QVector3D ModelItemData::initialAnguVelo() const { return m_initialAnguVelo; }

    MaterialData* ModelItemData::materials() const { return m_materials; }

    void ModelItemData::setMass(double mass)
    {
        if (m_mass != mass) {
            m_mass = mass;
            emit massChanged();
        }
    }

    void ModelItemData::setLoadTime(double loadTime)
    {
        if (m_loadTime != loadTime) {
            m_loadTime = loadTime;
            emit loadTimeChanged();
        }
    }

    void ModelItemData::setId(const QString& id)
    {
        if (m_id != id) {
            m_id = id;
            emit idChanged();
        }
    }

    void ModelItemData::setName(const QString& name)
    {
        if (m_name != name) {
            m_name = name;
            emit nameChanged();
        }
    }

    void ModelItemData::setType(const QString& type)
    {
        if (m_type != type) {
            m_type = type;
            matchSource();
            emit typeChanged();
        }
    }

    void ModelItemData::setSource(const QString& source)
    {
        if (m_source != source) {
            m_source = source;
            emit sourceChanged();
        }
    }

    void ModelItemData::setPos(const QVector3D& pos)
    {
        if (m_pos != pos) {
            m_pos = pos;
            emit posChanged();
        }
    }

    void ModelItemData::setScale(const QVector3D& scale)
    {
        if (m_scale != scale) {
            m_scale = scale;
            emit scaleChanged();
        }
    }

    void ModelItemData::setRotation(const QQuaternion& rotation)
    {
        if (m_rotation != rotation) {
            m_rotation = rotation;
            emit rotationChanged();
        }
    }

    void ModelItemData::setInitialVelo(const QVector3D& initialVelo)
    {
        if (m_initialVelo != initialVelo) {
            m_initialVelo = initialVelo;
            emit initialAnguVeloChanged();
        }
    }

    void ModelItemData::setInitialAnguVelo(const QVector3D& initialAnguVelo)
    {
        if (m_initialAnguVelo != initialAnguVelo) {
            m_initialAnguVelo = initialAnguVelo;
            emit initialAnguVeloChanged();
        }
    }

    void ModelItemData::setRestitution(double restitution)
    {
        if (m_restitution != restitution) {
            m_restitution = restitution;
            emit restitutionChanged();
        }
    }

    void ModelItemData::matchSource()
    {
        if (StaticGeneralTypeSourceMap.contains(m_type)) {
            const QString source = StaticGeneralTypeSourceMap.value(m_type);
            if (m_source != source) {
                m_source = source;
                emit sourceChanged();
            }
        }
        else if (m_type == "Standard Ring") {
            const QString source = StaticBasicRingSourceStr.arg("1");
            if (m_source != source) {
                m_source = source;
                emit sourceChanged();
            }
        }
    }

    QJsonObject ModelItemData::toJson() const
    {
        QJsonObject obj;
        obj["mass"] = m_mass;
        obj["load_time"] = m_loadTime;
        obj["id"] = m_id;
        obj["name"] = m_name;
        obj["source"] = m_source;
        obj["pos"] = QJsonArray{m_pos.x(), m_pos.y(), m_pos.z()};
        obj["scale"] = QJsonArray{m_scale.x(), m_scale.y(), m_scale.z()};
        obj["rotation"] = QJsonArray{m_rotation.scalar(), m_rotation.x(), m_rotation.y(), m_rotation.z()};
        obj["initial_velocity"] = QJsonArray{m_initialVelo.x(), m_initialVelo.y(), m_initialVelo.z()};
        obj["initial_angular_velocity"] = QJsonArray{
            m_initialAnguVelo.x(), m_initialAnguVelo.y(), m_initialAnguVelo.z()
        };
        obj["materials"] = m_materials->toJson();
        return obj;
    }

    bool ModelItemData::fromJson(const QJsonObject& json)
    {
        if (json.contains("mass")) { setMass(json["mass"].toDouble()); }
        if (json.contains("load_time")) { setLoadTime(json["load_time"].toDouble()); }
        if (json.contains("id")) { setId(json["id"].toString()); }
        if (json.contains("name")) { setName(json["name"].toString()); }
        if (json.contains("source")) { setSource(json["source"].toString()); }

        if (json.contains("pos") && json["pos"].isArray()) {
            QJsonArray posArr = json["pos"].toArray();
            if (posArr.size() >= 3) {
                setPos(QVector3D(static_cast<float>(posArr[0].toDouble()), static_cast<float>(posArr[1].toDouble()),
                                 static_cast<float>(posArr[2].toDouble())));
            }
        }

        if (json.contains("scale") && json["scale"].isArray()) {
            QJsonArray scaleArr = json["scale"].toArray();
            if (scaleArr.size() >= 3) {
                setScale(QVector3D(static_cast<float>(scaleArr[0].toDouble()),
                                   static_cast<float>(scaleArr[1].toDouble()),
                                   static_cast<float>(scaleArr[2].toDouble())));
            }
        }

        if (json.contains("rotation") && json["rotation"].isArray()) {
            QJsonArray rotArr = json["rotation"].toArray();
            if (rotArr.size() >= 4) {
                setRotation(QQuaternion(static_cast<float>(rotArr[0].toDouble()),
                                        static_cast<float>(rotArr[1].toDouble()),
                                        static_cast<float>(rotArr[2].toDouble()),
                                        static_cast<float>(rotArr[3].toDouble())));
            }
        }

        if (json.contains("initial_velocity") && json["initial_velocity"].isArray()) {
            QJsonArray initialVelArr = json["initial_velocity"].toArray();
            if (initialVelArr.size() >= 3) {
                setInitialVelo(QVector3D(static_cast<float>(initialVelArr[0].toDouble()),
                                         static_cast<float>(initialVelArr[1].toDouble()),
                                         static_cast<float>(initialVelArr[2].toDouble())));
            }
        }

        if (json.contains("initial_angular_velocity") && json["initial_angular_velocity"].isArray()) {
            QJsonArray initialAngularVelArr = json["initial_angular_velocity"].toArray();
            if (initialAngularVelArr.size() >= 3) {
                setInitialAnguVelo(QVector3D(static_cast<float>(initialAngularVelArr[0].toDouble()),
                                             static_cast<float>(initialAngularVelArr[1].toDouble()),
                                             static_cast<float>(initialAngularVelArr[2].toDouble())));
            }
        }
        if (json.contains("materials") && json["materials"].isObject()) {
            m_materials->fromJson(json["materials"].toObject());
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

        return copy;
    }
} // egret
