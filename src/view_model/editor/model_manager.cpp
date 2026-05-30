//
// Created by jehor on 2026/5/25.
//

#include "model_manager.h"
// ModelManager.cpp
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QUuid>
#include <QQmlEngine>
#include "logger.h"
#include "serialize/transform_transformer.h"


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

        model->setParent(nullptr);
        if (QQmlEngine::objectOwnership(model) == QQmlEngine::JavaScriptOwnership) {
            qWarning() << "Object is already owned by QML engine!";
            LOG_WARN_LITERAL("Object is already owned by QML engine!");
        }
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);

        QSharedPointer<ModelItemData> modelPtr(model);
        m_models[id] = modelPtr;
        m_modelList.append(modelPtr);
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

        QSharedPointer<ModelItemData> model = m_models.value(id);
        m_modelList.removeOne(model);
        m_models.remove(id);

        emitModelListChanged();
        emit modelRemoved(id);
        emit operationCompleted(true, "Model removed successfully");
        return true;
    }

    ModelItemData* ModelManager::getModel(const QString& id)
    {
        return m_models.value(id, QSharedPointer<ModelItemData>()).data();
    }

    ModelItemData* ModelManager::getModelAtIndex(int index)
    {
        if (index < 0 || index >= m_modelList.size())
            return nullptr;
        return m_modelList[index].data();
    }

    bool ModelManager::updateModel(const QString& id, ModelItemData* model)
    {
        if (!m_models.contains(id)) {
            emit operationCompleted(false, "Model not found");
            return false;
        }

        ModelItemData* oldModel = m_models[id].data();
        QJsonObject newData = model->toJson();
        oldModel->fromJson(newData);

        emit modelUpdated(id);
        emit operationCompleted(true, "Model updated successfully");
        return true;
    }

    void ModelManager::clearAll()
    {
        saveState();
        m_models.clear();
        m_modelList.clear();
        emitModelListChanged();
        emit operationCompleted(true, "All models cleared");
    }

    void ModelManager::addModels(const QList<ModelItemData*>& models)
    {
        saveState();
        for (ModelItemData* model : models) {
            if (!addModel(model)) {
                delete model;
            }
        }
    }

    QList<ModelItemData*> ModelManager::getAllModels() const
    {
        QList<ModelItemData*> results;
        results.reserve(m_modelList.size());
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            results.append(model.data());
        }
        return results;
    }

    qsizetype ModelManager::count() const
    {
        return m_modelList.size();
    }

    QStringList ModelManager::modelIds() const
    {
        return m_models.keys();
    }


    bool ModelManager::addModelByJsonString(const QString& jsonString)
    {
        const auto data = parseModelItemDataFromQMLJson(jsonString);
        // 4. 验证必要字段（根据你的需求调整）
        if (data.m_name.isEmpty()) {
            qDebug() << "entity_name cannot be empty";
            return false;
        }

        auto* newModel = new ModelItemData();
        newModel->setName(data.m_name);
        // newModel->setSource("#Cylinder");
        newModel->setType(data.m_type);
        newModel->setPos(data.m_pos);
        newModel->setScale(data.m_scale);

        qDebug() << "New Model: " << newModel->scale();
        newModel->setMass(data.m_mass);
        newModel->setLoadTime(data.m_loadTime);
        newModel->setRestitution(data.m_restitution);
        newModel->setRotation(data.m_rotation);
        newModel->setInitialVelo(data.m_initialVelo);
        newModel->setInitialAnguVelo(data.m_initialAnguVelo);

        if (data.m_boxSize.has_value()) {
            newModel->setBoxSize(data.m_boxSize.value());
        }
        if (data.m_radius.has_value()) {
            newModel->setRadius(data.m_radius.value());
        }
        if (data.m_height.has_value()) {
            newModel->setHeight(data.m_height.value());
        }
        if (data.m_length.has_value()) {
            newModel->setLength(data.m_length.value());
        }

        newModel->materials()->setBaseColor(data.m_baseColor);
        newModel->materials()->setMetalness(data.m_metalness);
        newModel->materials()->setRoughness(data.m_roughness);

        newModel->materials()->setAlphaMode("Opaque");

        qDebug() << __func__ << "Create";
        return addModel(newModel);
    }

    bool ModelManager::modifyModelByJsonString(const QString& jsonString)
    {
        const auto data = parseModelItemDataFromQMLJson(jsonString);
        if (data.m_name.isEmpty()) {
            qDebug() << "entity_name cannot be empty";
            return false;
        }

        const QList<ModelItemData*> models{findModelsByName(data.m_name)};
        if (models.isEmpty()) {
            qDebug() << "cannot find models";
            return false;
        }

        ModelItemData* model = models[0];
        qDebug() << "name: " << model->name();
        model->setMass(data.m_mass);
        model->setLoadTime(data.m_loadTime);
        model->setRestitution(data.m_restitution);
        model->setPos(data.m_pos);
        qDebug() << "new position" << model->pos();

        model->setScale(data.m_scale);
        model->setRotation(data.m_rotation);
        model->setInitialVelo(data.m_initialVelo);
        model->setInitialAnguVelo(data.m_initialAnguVelo);

        if (data.m_boxSize.has_value()) {
            model->setBoxSize(data.m_boxSize.value());
        }
        if (data.m_radius.has_value()) {
            model->setRadius(data.m_radius.value());
        }
        if (data.m_height.has_value()) {
            model->setHeight(data.m_height.value());
        }
        if (data.m_length.has_value()) {
            model->setLength(data.m_length.value());
        }
        model->materials()->setBaseColor(data.m_baseColor);
        model->materials()->setMetalness(data.m_metalness);
        model->materials()->setRoughness(data.m_roughness);


        emit modelListChanged();
        return true;
    }

    bool ModelManager::saveToJson(const QString& filePath)
    {
        QJsonArray modelsArray;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
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
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
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
                    if (!addModel(model)) {
                        delete model;
                    }
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
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            if (model->name().contains(name, Qt::CaseInsensitive)) {
                results.append(model.data());
            }
        }
        return results;
    }

    QList<ModelItemData*> ModelManager::findModelsBySource(const QString& source)
    {
        QList<ModelItemData*> results;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            if (model->source() == source) {
                results.append(model.data());
            }
        }
        return results;
    }

    void ModelManager::setModelsPosition(const QVector3D& position, const QStringList& ids)
    {
        saveState();
        QStringList targetIds = ids.isEmpty() ? m_models.keys() : ids;

        for (const QString& id : targetIds) {
            QSharedPointer<ModelItemData> model = m_models.value(id);
            if (model) {
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
            QSharedPointer<ModelItemData> model = m_models.value(id);
            if (model) {
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
            QSharedPointer<ModelItemData> model = m_models.value(id);
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

    QVariantMap ModelManager::setQuick3DRenderTransform(int index)
    {
        qDebug() << __func__ << index;
        const ModelItemData *model = getModelAtIndex(index);
        if (model == nullptr) {
            return {};
        }

        auto val = TransformTransformer::buildQuick3DRenderTransform(*model);
        qDebug() << __func__ << val;
        return val;
    }


    void ModelManager::saveState()
    {
        QHash<QString, QSharedPointer<ModelItemData>> currentState;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            currentState[model->id()] = QSharedPointer<ModelItemData>(model->clone());
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
        QHash<QString, QSharedPointer<ModelItemData>> currentState;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            currentState[model->id()] = QSharedPointer<ModelItemData>(model->clone());
        }
        m_redoStack.append(currentState);

        // 恢复上一个状态
        QHash<QString, QSharedPointer<ModelItemData>> prevState = m_undoStack.takeLast();
        clearAll();
        for (const QSharedPointer<ModelItemData>& model : prevState.values()) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModel(copy)) {
                delete copy;
            }
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

        QHash<QString, QSharedPointer<ModelItemData>> nextState = m_redoStack.takeLast();
        saveState(); // 保存当前状态到 undo
        clearAll();

        for (const QSharedPointer<ModelItemData>& model : nextState.values()) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModel(copy)) {
                delete copy;
            }
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
        cube->setType(ShapeType::Box);
        cube->setMass(0);
        cube->setBoxSize({600, 600, 10});
        cube->setPos(QVector3D(0, 0, -100));
        cube->materials()->setBaseColor(QColor("#4CA3FF"));
        cube->materials()->setMetalness(0.8);
        cube->materials()->setRoughness(0.3);
        cube->materials()->setAlphaMode("Opaque");
        models.append(cube);

        auto* cube2 = new ModelItemData();
        cube2->setName("Default Cube 2");
        cube2->setType(ShapeType::Box);
        cube2->setMass(100);
        cube2->setBoxSize({10, 10, 10});
        cube2->setPos(QVector3D(0, 100, 100));
        cube2->materials()->setBaseColor(QColor("#4CA3FF"));
        cube2->materials()->setMetalness(0.8);
        cube2->materials()->setRoughness(0.3);
        cube2->materials()->setAlphaMode("Opaque");
        models.append(cube2);

        auto* sphere = new ModelItemData();
        sphere->setName("Default Sphere");
        sphere->setType(ShapeType::Sphere);
        sphere->setMass(100);
        sphere->setRadius(10);
        sphere->setPos(QVector3D(0, 0, 100));
        sphere->materials()->setBaseColor(QColor("#4CA3FF"));
        sphere->materials()->setMetalness(0.8);
        sphere->materials()->setRoughness(0.3);
        sphere->materials()->setAlphaMode("Opaque");
        models.append(sphere);

        auto* sphere2 = new ModelItemData();
        sphere2->setName("Default Sphere");
        sphere2->setType(ShapeType::Sphere);
        sphere2->setMass(10);
        sphere2->setRadius(10);
        sphere2->setPos(QVector3D(0, 0, 300));
        sphere2->materials()->setBaseColor(QColor("#4CA3FF"));
        sphere2->materials()->setMetalness(0.8);
        sphere2->materials()->setRoughness(0.3);
        sphere2->materials()->setAlphaMode("Opaque");
        models.append(sphere2);

        return models;
    }
}
