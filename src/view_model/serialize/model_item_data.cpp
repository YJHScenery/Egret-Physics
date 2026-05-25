//
// Created by jehor on 2026/5/25.
//

#include "model_item_data.h"

namespace egret
{
#include <QUuid>
#include <QJsonDocument>

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

    QString ModelItemData::id() const { return m_id; }
    QString ModelItemData::name() const { return m_name; }
    QString ModelItemData::source() const { return m_source; }
    QColor ModelItemData::color() const { return m_color; }
    QVector3D ModelItemData::pos() const { return m_pos; }
    QVector3D ModelItemData::scale() const { return m_scale; }
    QQuaternion ModelItemData::rotation() const { return m_rotation; }
    MaterialData* ModelItemData::materials() const { return m_materials; }

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

    void ModelItemData::setSource(const QString& source)
    {
        if (m_source != source) {
            m_source = source;
            emit sourceChanged();
        }
    }

    void ModelItemData::setColor(const QColor& color)
    {
        if (m_color != color) {
            m_color = color;
            emit colorChanged();
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

    QJsonObject ModelItemData::toJson() const
    {
        QJsonObject obj;
        obj["id"] = m_id;
        obj["name"] = m_name;
        obj["source"] = m_source;
        obj["color"] = m_color.name(QColor::HexArgb);
        obj["pos"] = QJsonArray{m_pos.x(), m_pos.y(), m_pos.z()};
        obj["scale"] = QJsonArray{m_scale.x(), m_scale.y(), m_scale.z()};
        obj["rotation"] = QJsonArray{m_rotation.scalar(), m_rotation.x(), m_rotation.y(), m_rotation.z()};
        obj["materials"] = m_materials->toJson();
        return obj;
    }

    bool ModelItemData::fromJson(const QJsonObject& json)
    {
        if (json.contains("id")) setId(json["id"].toString());
        if (json.contains("name")) setName(json["name"].toString());
        if (json.contains("source")) setSource(json["source"].toString());
        if (json.contains("color")) setColor(QColor(json["color"].toString()));

        if (json.contains("pos") && json["pos"].isArray()) {
            QJsonArray posArr = json["pos"].toArray();
            if (posArr.size() >= 3)
                setPos(QVector3D(posArr[0].toDouble(), posArr[1].toDouble(), posArr[2].toDouble()));
        }

        if (json.contains("scale") && json["scale"].isArray()) {
            QJsonArray scaleArr = json["scale"].toArray();
            if (scaleArr.size() >= 3)
                setScale(QVector3D(scaleArr[0].toDouble(), scaleArr[1].toDouble(), scaleArr[2].toDouble()));
        }

        if (json.contains("rotation") && json["rotation"].isArray()) {
            QJsonArray rotArr = json["rotation"].toArray();
            if (rotArr.size() >= 4)
                setRotation(QQuaternion(rotArr[0].toDouble(), rotArr[1].toDouble(),
                                        rotArr[2].toDouble(), rotArr[3].toDouble()));
        }

        if (json.contains("materials") && json["materials"].isObject())
            m_materials->fromJson(json["materials"].toObject());

        return true;
    }

    ModelItemData* ModelItemData::clone() const
    {
        ModelItemData* newModel = new ModelItemData();
        newModel->fromJson(this->toJson());
        return newModel;
    }
} // egret
