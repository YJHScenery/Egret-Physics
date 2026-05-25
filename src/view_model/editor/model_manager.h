//
// Created by jehor on 2026/5/25.
//

#ifndef EGRET_PHYSICS_EDITOR_MANAGER_H
#define EGRET_PHYSICS_EDITOR_MANAGER_H
#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector3D>
#include <QQuaternion>
#include <QColor>
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QUuid>

#include "serialize/model_item_data.h"

namespace egret
{
    class ModelManager : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int count READ count NOTIFY countChanged)
        Q_PROPERTY(QStringList modelIds READ modelIds NOTIFY modelListChanged)

    public:
        explicit ModelManager(QObject* parent = nullptr);

        // 单例访问
        static ModelManager* instance();

        // 基本操作
        Q_INVOKABLE bool addModel(ModelItemData* model);
        Q_INVOKABLE bool removeModel(const QString& id);
        Q_INVOKABLE ModelItemData* getModel(const QString& id);
        Q_INVOKABLE ModelItemData* getModelAtIndex(int index);
        Q_INVOKABLE bool updateModel(const QString& id, ModelItemData* model);
        Q_INVOKABLE void clearAll();

        // 批量操作
        Q_INVOKABLE void addModels(const QList<ModelItemData*>& models);
        Q_INVOKABLE [[nodiscard]] QList<ModelItemData*> getAllModels() const;

        // 属性查询
        [[nodiscard]] qsizetype count() const;
        [[nodiscard]] QStringList modelIds() const;

        // JSON 序列化
        Q_INVOKABLE bool saveToJson(const QString& filePath);
        Q_INVOKABLE bool loadFromJson(const QString& filePath);
        Q_INVOKABLE QString toJsonString(bool pretty = true);
        Q_INVOKABLE bool fromJsonString(const QString& jsonString);

        // 导入导出
        Q_INVOKABLE bool exportToFile(const QString& filePath);
        Q_INVOKABLE bool importFromFile(const QString& filePath);

        // 查找和筛选
        Q_INVOKABLE QList<ModelItemData*> findModelsByName(const QString& name);
        Q_INVOKABLE QList<ModelItemData*> findModelsBySource(const QString& source);

        // 批量修改
        Q_INVOKABLE void setModelsPosition(const QVector3D& position, const QStringList& ids = QStringList());
        Q_INVOKABLE void setModelsScale(const QVector3D& scale, const QStringList& ids = QStringList());
        Q_INVOKABLE void setModelsMaterialProperty(const QString& property, const QVariant& value,
                                                   const QStringList& ids = QStringList());

        // 撤销/重做（可选）
        Q_INVOKABLE void saveState();
        Q_INVOKABLE bool undo();
        Q_INVOKABLE bool redo();

    signals:
        void countChanged();

        void modelListChanged();

        void modelAdded(const QString& id);

        void modelRemoved(const QString& id);

        void modelUpdated(const QString& id);

        void operationCompleted(bool success, const QString& message);

    private:
        QHash<QString, ModelItemData*> m_models;
        QList<ModelItemData*> m_modelList;
        QList<QHash<QString, ModelItemData*>> m_undoStack;
        QList<QHash<QString, ModelItemData*>> m_redoStack;

        void emitModelListChanged();
        static bool validateModel(const ModelItemData* model);
        static QString generateUniqueId();
    };


    QList<ModelItemData*> createDefaultModels();

}


#endif //EGRET_PHYSICS_EDITOR_MANAGER_H
