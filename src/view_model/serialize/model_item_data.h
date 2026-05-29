//
// Created by jehor on 2026/5/25.
//

#ifndef EGRET_PHYSICS_MATERIAL_DATA_H
#define EGRET_PHYSICS_MATERIAL_DATA_H
#include <QObject>
#include <QVector3D>
#include <QQuaternion>
#include <QColor>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace egret
{
    class MaterialData : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
        Q_PROPERTY(qreal metalness READ metalness WRITE setMetalness NOTIFY metalnessChanged)
        Q_PROPERTY(qreal roughness READ roughness WRITE setRoughness NOTIFY roughnessChanged)
        Q_PROPERTY(QString alphaMode READ alphaMode WRITE setAlphaMode NOTIFY alphaModeChanged)

    public:
        explicit MaterialData(QObject *parent = nullptr);

        // Getters/Setters
        [[nodiscard]] QColor baseColor() const;
        [[nodiscard]] qreal metalness() const;
        [[nodiscard]] qreal roughness() const;
        [[nodiscard]] QString alphaMode() const;

        void setBaseColor(const QColor &baseColor);
        void setMetalness(qreal metalness);
        void setRoughness(qreal roughness);
        void setAlphaMode(const QString &alphaMode);

        // 序列化
        [[nodiscard]] QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);

    signals:
        void baseColorChanged();
        void metalnessChanged();
        void roughnessChanged();
        void alphaModeChanged();

    private:
        QColor m_baseColor{};
        qreal m_metalness{};
        qreal m_roughness{};
        QString m_alphaMode{};
    };

    struct ModelItemDataField
    {
        double m_mass{};
        double m_loadTime{};
        double m_restitution{1.0};

        std::optional<QVector3D> m_boxSize;
        std::optional<double> m_radius;
        std::optional<double> m_height;
        std::optional<double> m_length;

        QString m_id{};
        QString m_name{};
        QString m_source{};
        std::uint32_t m_type{};
        QVector3D m_pos{};
        QVector3D m_scale{};
        QQuaternion m_rotation{};

        QVector3D m_initialVelo{};
        QVector3D m_initialAnguVelo{};
        QColor m_baseColor{};
        qreal m_metalness{};
        qreal m_roughness{};
        QString m_alphaMode{};
    };

    class ModelItemData : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(double mass READ mass WRITE setMass NOTIFY massChanged)
        Q_PROPERTY(double loadTime READ loadTime WRITE setLoadTime NOTIFY loadTimeChanged)
        Q_PROPERTY(QString id READ id WRITE setId NOTIFY idChanged)
        Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
        Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
        Q_PROPERTY(QVector3D pos READ pos WRITE setPos NOTIFY posChanged)
        Q_PROPERTY(QVector3D scale READ scale WRITE setScale NOTIFY scaleChanged)
        Q_PROPERTY(QQuaternion rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
        Q_PROPERTY(QVector3D initialVelo READ initialVelo NOTIFY initialVeloChanged)
        Q_PROPERTY(QVector3D initialAnguVelo READ initialAnguVelo NOTIFY initialAnguVeloChanged)
        Q_PROPERTY(MaterialData *materials READ materials CONSTANT)
        Q_PROPERTY(quint32 type READ type WRITE setType NOTIFY typeChanged)
        Q_PROPERTY(double restitution READ restitution WRITE setRestitution NOTIFY restitutionChanged)
        Q_PROPERTY(QVector3D boxSize READ boxSize WRITE setBoxSize NOTIFY boxSizeChanged)
        Q_PROPERTY(double radius READ radius WRITE setRadius NOTIFY radiusChanged)
        Q_PROPERTY(double height READ height WRITE setHeight NOTIFY heightChanged)
        Q_PROPERTY(double length READ length WRITE setLength NOTIFY lengthChanged)

    public:
        explicit ModelItemData(QObject *parent = nullptr);

        // Getters/Setters
        [[nodiscard]] double mass() const;
        [[nodiscard]] double loadTime() const;
        [[nodiscard]] QString id() const;
        [[nodiscard]] QString name() const;
        [[nodiscard]] QString source() const;
        [[nodiscard]] std::uint32_t type() const;
        [[nodiscard]] double restitution() const;
        [[nodiscard]] QVector3D pos() const;
        [[nodiscard]] QVector3D scale() const;
        [[nodiscard]] QQuaternion rotation() const;
        [[nodiscard]] QVector3D initialVelo() const;
        [[nodiscard]] QVector3D initialAnguVelo() const;
        [[nodiscard]] MaterialData *materials() const;
        [[nodiscard]] QVector3D boxSize() const;
        [[nodiscard]] double radius() const;
        [[nodiscard]] double height() const;
        [[nodiscard]] double length() const;

        void setMass(double mass);
        void setLoadTime(double loadTime);
        void setId(const QString &id);
        void setName(const QString &name);
        void setType(std::uint32_t type);
        void setSource(const QString &source);
        void setPos(const QVector3D &pos);
        void setScale(const QVector3D &scale);
        void setRotation(const QQuaternion &rotation);
        void setInitialVelo(const QVector3D &initialVelo);
        void setInitialAnguVelo(const QVector3D &initialAnguVelo);
        void setRestitution(double restitution);
        void setBoxSize(const QVector3D &boxSize);
        void setRadius(double radius);
        void setHeight(double height);
        void setLength(double length);

        // 序列化
        [[nodiscard]] QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);

        // 克隆
        [[nodiscard]] QSharedPointer<ModelItemData> clone() const;
        static ModelItemData *createCopy(const ModelItemData &source, QObject *parent = nullptr);

        static QMap<QString, std::uint32_t> ShowMatchesTypeIDMap;

    signals:
        void massChanged();
        void loadTimeChanged();
        void idChanged();
        void nameChanged();
        void typeChanged();
        void sourceChanged();
        void posChanged();
        void scaleChanged();
        void rotationChanged();
        void initialVeloChanged();
        void initialAnguVeloChanged();
        void restitutionChanged();
        void boxSizeChanged();
        void radiusChanged();
        void heightChanged();
        void lengthChanged();

    private:
        double m_mass{};
        double m_loadTime{};
        double m_restitution{1.0};
        QString m_id{};
        QString m_name{};
        QString m_source{};

        std::uint32_t m_type{};

        std::optional<QVector3D> m_boxSize;
        std::optional<double> m_radius;
        std::optional<double> m_height;
        std::optional<double> m_length;

        QVector3D m_pos{};
        QVector3D m_scale{1.0f, 1.0f, 1.0f};
        QQuaternion m_rotation{1.0f, 0, 0, 0};

        QVector3D m_initialVelo{};
        QVector3D m_initialAnguVelo{};

        MaterialData *m_materials{};

        // 原则上不允许外部修改 m_source。
        void matchSource();

    public:
        const static QMap<std::uint32_t, QString> StaticGeneralTypeSourceMap;
    };

    [[nodiscard]] ModelItemDataField parseModelItemDataFromQMLJson(const QString &qmlJson);
} // egret

#endif // EGRET_PHYSICS_MATERIAL_DATA_H
