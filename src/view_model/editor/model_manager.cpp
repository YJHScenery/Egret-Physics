//
// Created by jehor on 2026/5/25.
//

#include "model_manager.h"
// ModelManager.cpp
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QUuid>


namespace egret
{
    ModelManager::ModelManager(QObject* parent) : QObject(parent)
    {
    }

    ModelManager* ModelManager::instance()
    {
        static ModelManager manager;
        return &manager;
    }

    bool ModelManager::addModel(ModelItemData* model)
    {
        if (!validateModel(model)) return false;

        QString id = model->id().isEmpty() ? generateUniqueId() : model->id();
        model->setId(id);

        if (m_models.contains(id)) {
            emit operationCompleted(false, "Model with same ID already exists");
            return false;
        }

        m_models[id] = model;
        m_modelList.append(model);
        emitModelListChanged();
        emit modelAdded(id);
        emit operationCompleted(true, "Model added successfully");
        return true;
    }

    bool ModelManager::removeModel(const QString& id)
    {
        if (!m_models.contains(id)) {
            emit operationCompleted(false, "Model not found");
            return false;
        }

        ModelItemData* model = m_models[id];
        m_modelList.removeOne(model);
        m_models.remove(id);
        model->deleteLater();

        emitModelListChanged();
        emit modelRemoved(id);
        emit operationCompleted(true, "Model removed successfully");
        return true;
    }

    ModelItemData* ModelManager::getModel(const QString& id)
    {
        return m_models.value(id, nullptr);
    }

    ModelItemData* ModelManager::getModelAtIndex(int index)
    {
        if (index < 0 || index >= m_modelList.size())
            return nullptr;
        return m_modelList[index];
    }

    bool ModelManager::updateModel(const QString& id, ModelItemData* model)
    {
        if (!m_models.contains(id)) {
            emit operationCompleted(false, "Model not found");
            return false;
        }

        ModelItemData* oldModel = m_models[id];
        QJsonObject newData = model->toJson();
        oldModel->fromJson(newData);

        emit modelUpdated(id);
        emit operationCompleted(true, "Model updated successfully");
        return true;
    }

    void ModelManager::clearAll()
    {
        saveState();
        qDeleteAll(m_models);
        m_models.clear();
        m_modelList.clear();
        emitModelListChanged();
        emit operationCompleted(true, "All models cleared");
    }

    void ModelManager::addModels(const QList<ModelItemData*>& models)
    {
        saveState();
        for (ModelItemData* model : models) {
            addModel(model);
        }
    }

    QList<ModelItemData*> ModelManager::getAllModels() const
    {
        return m_modelList;
    }

    qsizetype ModelManager::count() const
    {
        return m_modelList.size();
    }

    QStringList ModelManager::modelIds() const
    {
        return m_models.keys();
    }

    bool ModelManager::saveToJson(const QString& filePath)
    {
        QJsonArray modelsArray;
        for (ModelItemData* model : m_modelList) {
            modelsArray.append(model->toJson());
        }

        QJsonObject root;
        root["version"] = "1.0";
        root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["modelCount"] = m_modelList.size();
        root["models"] = modelsArray;

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit operationCompleted(false, "Cannot open file for writing");
            return false;
        }

        QJsonDocument doc(root);
        file.write(doc.toJson());
        file.close();

        emit operationCompleted(true, "Models saved to JSON");
        return true;
    }

    bool ModelManager::loadFromJson(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit operationCompleted(false, "Cannot open file for reading");
            return false;
        }

        QByteArray data = file.readAll();
        file.close();

        return fromJsonString(QString::fromUtf8(data));
    }

    QString ModelManager::toJsonString(bool pretty)
    {
        QJsonArray modelsArray;
        for (ModelItemData* model : m_modelList) {
            modelsArray.append(model->toJson());
        }

        QJsonObject root;
        root["version"] = "1.0";
        root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["modelCount"] = m_modelList.size();
        root["models"] = modelsArray;

        QJsonDocument doc(root);
        return pretty ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
    }

    bool ModelManager::fromJsonString(const QString& jsonString)
    {
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (doc.isNull() || !doc.isObject()) {
            emit operationCompleted(false, "Invalid JSON format");
            return false;
        }

        QJsonObject root = doc.object();
        if (!root.contains("models") || !root["models"].isArray()) {
            emit operationCompleted(false, "Invalid models data structure");
            return false;
        }

        saveState();
        clearAll();

        QJsonArray modelsArray = root["models"].toArray();
        for (const QJsonValueRef& value : modelsArray) {
            if (value.isObject()) {
                auto* model = new ModelItemData(this);
                if (model->fromJson(value.toObject())) {
                    addModel(model);
                }
                else {
                    delete model;
                }
            }
        }

        emit operationCompleted(true, "Models loaded from JSON");
        return true;
    }

    bool ModelManager::exportToFile(const QString& filePath)
    {
        return saveToJson(filePath);
    }

    bool ModelManager::importFromFile(const QString& filePath)
    {
        return loadFromJson(filePath);
    }

    QList<ModelItemData*> ModelManager::findModelsByName(const QString& name)
    {
        QList<ModelItemData*> results;
        for (ModelItemData* model : m_modelList) {
            if (model->name().contains(name, Qt::CaseInsensitive)) {
                results.append(model);
            }
        }
        return results;
    }

    QList<ModelItemData*> ModelManager::findModelsBySource(const QString& source)
    {
        QList<ModelItemData*> results;
        for (ModelItemData* model : m_modelList) {
            if (model->source() == source) {
                results.append(model);
            }
        }
        return results;
    }

    void ModelManager::setModelsPosition(const QVector3D& position, const QStringList& ids)
    {
        saveState();
        QStringList targetIds = ids.isEmpty() ? m_models.keys() : ids;

        for (const QString& id : targetIds) {
            if (ModelItemData* model = m_models.value(id)) {
                model->setPos(position);
                emit modelUpdated(id);
            }
        }
        emit operationCompleted(true, QString("Updated position for %1 models").arg(targetIds.size()));
    }

    void ModelManager::setModelsScale(const QVector3D& scale, const QStringList& ids)
    {
        saveState();
        QStringList targetIds = ids.isEmpty() ? m_models.keys() : ids;

        for (const QString& id : targetIds) {
            if (ModelItemData* model = m_models.value(id)) {
                model->setScale(scale);
                emit modelUpdated(id);
            }
        }
        emit operationCompleted(true, QString("Updated scale for %1 models").arg(targetIds.size()));
    }

    void ModelManager::setModelsMaterialProperty(const QString& property, const QVariant& value, const QStringList& ids)
    {
        saveState();
        QStringList targetIds = ids.isEmpty() ? m_models.keys() : ids;

        for (const QString& id : targetIds) {
            ModelItemData* model = m_models.value(id);
            if (!model) continue;

            if (property == "baseColor" && value.canConvert<QColor>())
                model->materials()->setBaseColor(value.value<QColor>());
            else if (property == "metalness" && value.canConvert<double>())
                model->materials()->setMetalness(value.toDouble());
            else if (property == "roughness" && value.canConvert<double>())
                model->materials()->setRoughness(value.toDouble());
            else if (property == "alphaMode" && value.canConvert<QString>())
                model->materials()->setAlphaMode(value.toString());

            emit modelUpdated(id);
        }
        emit operationCompleted(true, QString("Updated material for %1 models").arg(targetIds.size()));
    }

    void ModelManager::saveState()
    {
        QHash<QString, ModelItemData*> currentState;
        for (ModelItemData* model : m_modelList) {
            currentState[model->id()] = model->clone();
        }
        m_undoStack.append(currentState);
        m_redoStack.clear();
    }

    bool ModelManager::undo()
    {
        if (m_undoStack.isEmpty()) {
            emit operationCompleted(false, "Nothing to undo");
            return false;
        }

        // 保存当前状态到 redo
        QHash<QString, ModelItemData*> currentState;
        for (ModelItemData* model : m_modelList) {
            currentState[model->id()] = model->clone();
        }
        m_redoStack.append(currentState);

        // 恢复上一个状态
        QHash<QString, ModelItemData*> prevState = m_undoStack.takeLast();
        clearAll();
        for (ModelItemData* model : prevState.values()) {
            addModel(model->clone());
        }

        emit operationCompleted(true, "Undo successful");
        return true;
    }

    bool ModelManager::redo()
    {
        if (m_redoStack.isEmpty()) {
            emit operationCompleted(false, "Nothing to redo");
            return false;
        }

        QHash<QString, ModelItemData*> nextState = m_redoStack.takeLast();
        saveState(); // 保存当前状态到 undo
        clearAll();

        for (ModelItemData* model : nextState.values()) {
            addModel(model->clone());
        }

        emit operationCompleted(true, "Redo successful");
        return true;
    }

    bool ModelManager::validateModel(const ModelItemData* model)
    {
        return model != nullptr;
    }

    QString ModelManager::generateUniqueId()
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }


    void ModelManager::emitModelListChanged()
    {
        emit modelListChanged();
        emit countChanged();
    }


    QList<ModelItemData*> createDefaultModels()
    {
        QList<ModelItemData*> models;

        // 1. Default Cube
        auto* cube = new ModelItemData();
        cube->setName("Default Cube");
        cube->setSource("#Cube");
        cube->setColor(QColor("#4CA3FF"));
        cube->setPos(QVector3D(80, 0, 0));
        cube->setScale(QVector3D(1.2f, 0.12f, 1.2f));
        cube->materials()->setBaseColor(QColor("#4CA3FF"));
        cube->materials()->setMetalness(0.8);
        cube->materials()->setRoughness(0.3);
        cube->materials()->setAlphaMode("Opaque");
        models.append(cube);

        // 2. Default Sphere
        auto* sphere = new ModelItemData();
        sphere->setName("Default Sphere");
        sphere->setSource("#Sphere");
        sphere->setColor(QColor("#4CA3FF"));
        sphere->setPos(QVector3D(0, 80, 0));
        sphere->materials()->setBaseColor(QColor("#4CA3FF"));
        sphere->materials()->setMetalness(0.8);
        sphere->materials()->setRoughness(0.3);
        models.append(sphere);

        // 3. Default Rectangle
        auto* rect = new ModelItemData();
        rect->setName("Default Rectangle");
        rect->setSource("#Rectangle");
        rect->setColor(QColor("#4CA3FF"));
        rect->setPos(QVector3D(140, 80, 0));
        rect->materials()->setBaseColor(QColor("#4CA3FF"));
        rect->materials()->setMetalness(0.8);
        rect->materials()->setRoughness(0.3);
        models.append(rect);

        // 4. Default Cylinder
        auto* cylinder = new ModelItemData();
        cylinder->setName("Default Cylinder");
        cylinder->setSource("#Cylinder");
        cylinder->setColor(QColor("#4CA3FF"));
        cylinder->setPos(QVector3D(-160, 80, 0));
        cylinder->materials()->setBaseColor(QColor("#4CA3FF"));
        cylinder->materials()->setMetalness(0.8);
        cylinder->materials()->setRoughness(0.3);
        models.append(cylinder);

        // 5. Torus
        auto* torus = new ModelItemData();
        torus->setName("Default Torus");
        torus->setSource("qrc:/model_3d/assets/model_3d/torus/mesh/torus_R1.mesh");
        torus->setColor(QColor("#4CA3FF"));
        torus->setPos(QVector3D(160, 180, 160));
        torus->setScale(QVector3D(100.0f, 100.0f, 100.0f));
        torus->materials()->setBaseColor(QColor("#c9dff6"));
        torus->materials()->setMetalness(0.2);
        torus->materials()->setRoughness(0.9);
        torus->materials()->setAlphaMode("Opaque");
        models.append(torus);

        auto* cylinder_side = new ModelItemData();
        cylinder_side->setName("Default Cylinder Side");
        cylinder_side->setSource("qrc:/model_3d/assets/model_3d/cylinder_side/cylinder_side.mesh");
        cylinder_side->setColor(QColor("#4CA3FF"));
        cylinder_side->setPos(QVector3D(-160, -180, -160));
        cylinder_side->setScale(QVector3D(100.0f, 100.0f, 100.0f));
        cylinder_side->materials()->setBaseColor(QColor("#c9dff6"));
        cylinder_side->materials()->setMetalness(0.2);
        cylinder_side->materials()->setRoughness(0.9);
        cylinder_side->materials()->setAlphaMode("Opaque");
        models.append(cylinder_side);


        auto* disk = new ModelItemData();
        disk->setName("Default Cylinder Side");
        disk->setSource("qrc:/model_3d/assets/model_3d/circle_disk/circle.mesh");
        disk->setColor(QColor("#4CA3FF"));
        disk->setPos(QVector3D(160, -180, -160));
        disk->setScale(QVector3D(100.0f, 100.0f, 100.0f));
        disk->materials()->setBaseColor(QColor("#c9dff6"));
        disk->materials()->setMetalness(0.2);
        disk->materials()->setRoughness(0.9);
        disk->materials()->setAlphaMode("Opaque");
        models.append(disk);

        return models;
    }
}
