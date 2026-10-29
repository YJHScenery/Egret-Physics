/**
 * @file        editor_manager.cpp
 * @brief       编辑器模型管理器实现文件，提供编辑器层的数据模型管理功能。
 * @details     实现 ModelManager 类的各项成员函数。
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
#include "editor_manager.h"
// EditorManager.cpp
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QJsonDocument>
#include <QDebug>
#include <QQmlEngine>
#include "async_logger.h"
#include "unique_id_generator.h"
#include "os/windows/windows_sound.h"
#include "serialize/transform_transformer.h"

namespace egret
{
    EditorManager::EditorManager(QObject* parent) : QObject(parent)
    {
        m_documents.append(Document{});
        m_documents[0].savedContent = documentContent();
        connect(this, &EditorManager::modelListChanged, this, [this] { if (!m_switchingDocument) emit documentsChanged(); });
        connect(this, &EditorManager::modelUpdated, this, [this](const QString&) { if (!m_switchingDocument) emit documentsChanged(); });
    }

    QByteArray EditorManager::documentContent() const
    {
        QJsonArray models;
        for (const auto& model : m_modelList) models.append(model->toJson());
        return QJsonDocument(models).toJson(QJsonDocument::Compact);
    }

    QVariantList EditorManager::documents() const
    {
        QVariantList result;
        for (int i = 0; i < m_documents.size(); ++i) {
            const auto& document = m_documents[i];
            QJsonArray models;
            if (i != m_currentDocument) for (const auto& model : document.models) models.append(model->toJson());
            const QByteArray content = i == m_currentDocument ? documentContent() : QJsonDocument(models).toJson(QJsonDocument::Compact);
            result.append(QVariantMap{{"name", document.path.isEmpty() ? QStringLiteral("untitled.epscene") : QFileInfo(document.path).fileName()},
                                      {"path", document.path}, {"dirty", content != document.savedContent}});
        }
        return result;
    }

    void EditorManager::storeDocument()
    {
        auto& document = m_documents[m_currentDocument];
        document.models = m_modelList;
        document.undo = m_undoStack;
        document.redo = m_redoStack;
    }

    void EditorManager::restoreDocument(int index)
    {
        m_switchingDocument = true;
        m_currentDocument = index;
        const auto& document = m_documents[index];
        m_modelList = document.models;
        m_undoStack = document.undo;
        m_redoStack = document.redo;
        m_models.clear();
        for (const auto& model : m_modelList) m_models.insert(model->id(), model);
        emitModelListChanged();
        m_switchingDocument = false;
        emit documentsChanged();
    }

    void EditorManager::newDocument()
    {
        storeDocument();
        Document document;
        document.savedContent = QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact);
        m_documents.append(document);
        restoreDocument(m_documents.size() - 1);
    }

    bool EditorManager::switchDocument(int index)
    {
        if (index < 0 || index >= m_documents.size()) return false;
        if (index == m_currentDocument) return true;
        storeDocument();
        restoreDocument(index);
        return true;
    }

    bool EditorManager::openDocument(const QString& path)
    {
        const QString localPath = path.startsWith(QStringLiteral("file:")) ? QUrl(path).toLocalFile() : path;
        QFile file(localPath);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray data = file.readAll();
        const QJsonDocument parsed = QJsonDocument::fromJson(data);
        if (!parsed.isObject() || !parsed.object().value("models").isArray()) return false;
        newDocument();
        m_switchingDocument = true;
        const bool ok = fromJsonString(QString::fromUtf8(data));
        m_undoStack.clear();
        m_redoStack.clear();
        m_switchingDocument = false;
        if (!ok) { closeDocument(m_currentDocument); return false; }
        m_documents[m_currentDocument].path = QFileInfo(localPath).absoluteFilePath();
        m_documents[m_currentDocument].savedContent = documentContent();
        emit documentsChanged();
        return true;
    }

    bool EditorManager::documentNeedsSaveAs() const
    {
        const QString path = m_documents[m_currentDocument].path;
        return path.isEmpty() || !QFileInfo::exists(path);
    }

    bool EditorManager::saveDocument(const QString& path)
    {
        const QString target = path.isEmpty() ? m_documents[m_currentDocument].path :
            (path.startsWith(QStringLiteral("file:")) ? QUrl(path).toLocalFile() : path);
        if (target.isEmpty() || (path.isEmpty() && !QFileInfo::exists(target))) return false;
        if (!saveToJson(target)) return false;
        m_documents[m_currentDocument].path = QFileInfo(target).absoluteFilePath();
        m_documents[m_currentDocument].savedContent = documentContent();
        emit documentsChanged();
        return true;
    }

    bool EditorManager::closeDocument(int index)
    {
        if (index < 0 || index >= m_documents.size()) return false;
        storeDocument();
        const bool wasCurrent = index == m_currentDocument;
        m_documents.removeAt(index);
        if (m_documents.isEmpty()) {
            Document document;
            document.savedContent = QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact);
            m_documents.append(document);
        }
        if (wasCurrent) restoreDocument(qMin(index, int(m_documents.size()) - 1));
        else { if (index < m_currentDocument) --m_currentDocument; emit documentsChanged(); }
        return true;
    }

    EditorManager* EditorManager::instance()
    {
        static EditorManager manager;
        return &manager;
    }

    void EditorManager::watchModelPosition(ModelItemData* model)
    {
        connect(model, &ModelItemData::posChanged, this, [this, model] {
            if (model->category() == 0 || model->category() == 1)
                updateConstraintLengths(model->id());
        });
    }

    void EditorManager::updateConstraintLengths(const QString& entityId)
    {
        for (const auto& constraint : m_modelList) {
            if (constraint->category() != 2 || (constraint->type() != 1 && constraint->type() != 2 && constraint->type() != 3)) continue;
            QJsonObject payload = constraint->payload();
            const QJsonArray ids = payload.value("entity_ids").toArray();
            bool involved = false;
            for (const auto& id : ids) if (id.toString() == entityId) involved = true;
            if (!involved || ids.isEmpty()) continue;
            const auto* first = getModel(ids[0].toString());
            if (!first) continue;
            QVector3D other;
            if (constraint->type() == 2) {
                const QJsonObject anchor = payload.value("anchor_pos").toObject();
                if (anchor.isEmpty()) continue;
                other = QVector3D(anchor.value("x").toDouble(), anchor.value("y").toDouble(), anchor.value("z").toDouble());
            } else {
                if (ids.size() != 2) continue;
                const auto* second = getModel(ids[1].toString());
                if (!second) continue;
                other = second->pos();
            }
            QJsonObject data = payload.value("constraint_data").toObject();
            data["length"] = static_cast<double>((first->pos() - other).length());
            payload["constraint_data"] = data;
            constraint->setPayload(payload);
            emit modelUpdated(constraint->id());
        }
    }

    bool EditorManager::addModel(ModelItemData* model)
    {
        if (!validateModel(model))
            return false;

        saveState();

        QString id = model->id().isEmpty() ? QString::number(UniqueIDGenerator::instance().generate()) : model->id();
        model->setId(id);

        if (m_models.contains(id)) {
            emit operationCompleted(false, "Model with same ID already exists");
            return false;
        }

        model->setParent(nullptr);
        if (QQmlEngine::objectOwnership(model) == QQmlEngine::JavaScriptOwnership) {
            qWarning() << "Object is already owned by QML engine!";
            LOG_WARN("Object is already owned by QML engine!");
        }
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        // MaterialData 通过 shared_ptr 管理，也必须设为 CppOwnership 防止 QML 接管
        QQmlEngine::setObjectOwnership(model->materials(), QQmlEngine::CppOwnership);

        QSharedPointer<ModelItemData> modelPtr(model);
        m_models[id] = modelPtr;
        m_modelList.append(modelPtr);
        watchModelPosition(model);
        emitModelListChanged();
        emit modelAdded(id);
        emit operationCompleted(true, "Model added successfully");
        return true;
    }

    /**
     * @brief       内部添加方法，不触发 saveState（供 undo/redo 重建使用）。
     */
    bool EditorManager::addModelNoSave(ModelItemData* model)
    {
        if (!validateModel(model))
            return false;

        QString id = model->id().isEmpty() ? QString::number(UniqueIDGenerator::instance().generate()) : model->id();
        model->setId(id);

        if (m_models.contains(id)) {
            return false;
        }

        model->setParent(nullptr);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        QQmlEngine::setObjectOwnership(model->materials(), QQmlEngine::CppOwnership);

        QSharedPointer<ModelItemData> modelPtr(model);
        m_models[id] = modelPtr;
        m_modelList.append(modelPtr);
        watchModelPosition(model);
        return true;
    }

    bool EditorManager::removeModel(const QString& id)
    {
        if (!m_models.contains(id)) {
            emit operationCompleted(false, "Model not found");
            return false;
        }

        saveState();

        QSharedPointer<ModelItemData> model = m_models.value(id);
        m_modelList.removeOne(model);
        m_models.remove(id);

        emitModelListChanged();
        emit modelRemoved(id);
        emit operationCompleted(true, "Model removed successfully");
        return true;
    }

    ModelItemData* EditorManager::getModel(const QString& id)
    {
        return m_models.value(id, QSharedPointer<ModelItemData>()).data();
    }

    int EditorManager::removeModels(const QStringList& ids)
    {
        QStringList existing;
        for (const auto& id : ids) if (m_models.contains(id) && !existing.contains(id)) existing.append(id);
        if (existing.isEmpty()) return 0;
        saveState();
        for (const auto& id : existing) {
            const auto model = m_models.take(id);
            m_modelList.removeOne(model);
            emit modelRemoved(id);
        }
        emitModelListChanged();
        return existing.size();
    }

    ModelItemData* EditorManager::getModelAtIndex(int index)
    {
        if (index < 0 || index >= m_modelList.size())
            return nullptr;
        return m_modelList[index].data();
    }

    QVariantMap EditorManager::getModelPayload(const QString &id) const
    {
        const ModelItemData* model = const_cast<EditorManager*>(this)->getModel(id);
        if (!model) return {};
        return model->payload().toVariantMap();
    }

    bool EditorManager::updateModel(const QString& id, ModelItemData* model)
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

    bool EditorManager::setModelTransform(const QString& id, const QVector3D& position, const QQuaternion& rotation)
    {
        auto* model = getModel(id);
        if (!model) return false;
        if (model->pos() == position && model->rotation() == rotation) return true;
        saveState();
        model->setPos(position);
        model->setRotation(rotation);
        emit modelUpdated(id);
        return true;
    }

    void EditorManager::clearAll()
    {
        saveState();
        m_models.clear();
        m_modelList.clear();
        emitModelListChanged();
        emit operationCompleted(true, "All models cleared");
    }

    /**
     * @brief       内部清空方法，不触发 saveState（供 undo/redo 使用）。
     */
    void EditorManager::clearAllNoSave()
    {
        m_models.clear();
        m_modelList.clear();
        emitModelListChanged();
    }

    void EditorManager::addModels(const QList<ModelItemData*>& models)
    {
        saveState();
        for (ModelItemData* model : models) {
            if (!addModelNoSave(model)) {
                delete model;
            }
        }
        emitModelListChanged();
    }

    QList<ModelItemData*> EditorManager::getAllModels() const
    {
        QList<ModelItemData*> results;
        results.reserve(m_modelList.size());
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            results.append(model.data());
        }
        return results;
    }

    qsizetype EditorManager::count() const
    {
        return m_modelList.size();
    }

    QStringList EditorManager::modelIds() const
    {
        return m_models.keys();
    }

    bool EditorManager::addModelByJsonString(const QString& jsonString)
    {
        const auto data = parseModelItemDataFromQMLJson(jsonString);
        // 4. 验证必要字段（根据你的需求调整）
        if (data.m_name.isEmpty()) {
            emit warning("Model Manager", "Entity Name Cannot be Empty!");
            qDebug() << "entity_name cannot be empty";
            return false;
        }

        auto* newModel = new ModelItemData();
        newModel->setCategory(data.m_category);
        newModel->setName(data.m_name);
        // newModel->setSource("#Cylinder");
        newModel->setType(data.m_type);
        newModel->setPos(data.m_pos);
        newModel->setScale(data.m_scale);

        qDebug() << "New Model: " << newModel->scale();
        newModel->setMass(data.m_mass);
        newModel->setLoadTime(data.m_loadTime);
        newModel->setRestitution(data.m_restitution);
        newModel->setStaticFriction(data.m_staticFriction);
        newModel->setDynamicFriction(data.m_dynamicFriction);
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

        newModel->materials()->setBaseColor(data.m_materials->baseColor());
        newModel->materials()->setAlphaMode(data.m_materials->alphaMode());

        // 透传 field_data / constraint_data payload
        newModel->setPayload(data.m_payload);

        // qDebug() << __func__ << "Create";
        return addModel(newModel);
    }

    bool EditorManager::modifyModelByJsonString(const QString& jsonString)
    {
        const auto data = parseModelItemDataFromQMLJson(jsonString);
        if (data.m_name.isEmpty()) {
            qDebug() << "entity_name cannot be empty";
            return false;
        }

        ModelItemData* model = getModel(data.m_id);
        if (!model) {
            qDebug() << "cannot find model by id";
            return false;
        }
        qDebug() << "name: " << model->name();

        qDebug() << "type: " << model->type();

        saveState();

        model->setCategory(data.m_category);
        model->setName(data.m_name);
        model->setType(data.m_type);
        model->setMass(data.m_mass);
        model->setLoadTime(data.m_loadTime);
        model->setRestitution(data.m_restitution);
        model->setStaticFriction(data.m_staticFriction);
        model->setDynamicFriction(data.m_dynamicFriction);
        model->setPos(data.m_pos);
        qDebug() << "new position" << model->pos();
        qDebug() << "new size:" << data.m_boxSize;

        model->setScale(data.m_scale);
        model->setRotation(data.m_rotation);
        model->setInitialVelo(data.m_initialVelo);
        model->setInitialAnguVelo(data.m_initialAnguVelo);

        if (data.m_boxSize.has_value()) {
            model->setBoxSize(data.m_boxSize.value());
            qDebug() << "set new size" << model->boxSize();
        }
        if (data.m_radius.has_value()) {
            qDebug() << "set new radius" << data.m_radius.value();
            model->setRadius(data.m_radius.value());
        }
        if (data.m_height.has_value()) {
            model->setHeight(data.m_height.value());
        }
        if (data.m_length.has_value()) {
            model->setLength(data.m_length.value());
        }
        model->materials()->setBaseColor(data.m_materials->baseColor());

        // 更新透传负载（field_data / constraint_data / entity_ids / anchor_pos）
        model->setPayload(data.m_payload);

        emit modelListChanged();
        return true;
    }

    bool EditorManager::saveToJson(const QString& filePath)
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

        QSaveFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit operationCompleted(false, "Cannot open file for writing");
            return false;
        }

        const QByteArray data = QJsonDocument(root).toJson();
        if (file.write(data) != data.size() || !file.commit()) {
            emit operationCompleted(false, "Cannot write scene file");
            return false;
        }

        emit operationCompleted(true, "Models saved to JSON");
        return true;
    }

    bool EditorManager::loadFromJson(const QString& filePath)
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

    QString EditorManager::toJsonString(bool pretty)
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

    bool EditorManager::fromJsonString(const QString& jsonString)
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
        clearAllNoSave();

        QJsonArray modelsArray = root["models"].toArray();
        for (const QJsonValueRef& value : modelsArray) {
            if (value.isObject()) {
                auto* model = new ModelItemData(this);
                if (model->fromJson(value.toObject())) {
                    if (!addModelNoSave(model)) {
                        delete model;
                    }
                }
                else {
                    delete model;
                }
            }
        }
        emitModelListChanged();

        emit operationCompleted(true, "Models loaded from JSON");
        return true;
    }

    bool EditorManager::exportToFile(const QString& filePath)
    {
        return saveToJson(filePath);
    }

    bool EditorManager::importFromFile(const QString& filePath)
    {
        return loadFromJson(filePath);
    }

    QList<ModelItemData*> EditorManager::findModelsByName(const QString& name)
    {
        QList<ModelItemData*> results;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            if (model->name().contains(name, Qt::CaseInsensitive)) {
                results.append(model.data());
            }
        }
        return results;
    }

    QList<ModelItemData*> EditorManager::findModelsBySource(const QString& source)
    {
        QList<ModelItemData*> results;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            if (model->source() == source) {
                results.append(model.data());
            }
        }
        return results;
    }

    QList<ModelItemData*> EditorManager::getModelsByCategory(int category) const
    {
        QList<ModelItemData*> results;
        results.reserve(m_modelList.size());
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            if (model->category() == category) {
                results.append(model.data());
            }
        }
        return results;
    }

    void EditorManager::setModelsPosition(const QVector3D& position, const QStringList& ids)
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

    void EditorManager::setModelsScale(const QVector3D& scale, const QStringList& ids)
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

    void EditorManager::setModelsMaterialProperty(const QString& property, const QVariant& value, const QStringList& ids)
    {
        saveState();
        QStringList targetIds = ids.isEmpty() ? m_models.keys() : ids;

        for (const QString& id : targetIds) {
            QSharedPointer<ModelItemData> model = m_models.value(id);
            if (!model)
                continue;

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

    QVariantMap EditorManager::setQuick3DRenderTransform(int index)
    {
        // qDebug() << __func__ << index;
        const ModelItemData* model = getModelAtIndex(index);
        if (model == nullptr) {
            return {};
        }

        auto val = TransformTransformer::buildQuick3DRenderTransform(*model);
        return val;
    }

    void EditorManager::saveState()
    {
        QList<QSharedPointer<ModelItemData>> currentState;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            currentState.append(model->clone());
        }
        m_undoStack.append(currentState);
        m_redoStack.clear();
    }

    bool EditorManager::undo()
    {
        if (m_undoStack.isEmpty()) {
            emit operationCompleted(false, "Nothing to undo");
            return false;
        }

        // 保存当前状态到 redo（手动克隆，不用 saveState 因为 saveState 会清空 m_redoStack）
        QList<QSharedPointer<ModelItemData>> currentState;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            currentState.append(model->clone());
        }
        m_redoStack.append(currentState);

        // 恢复上一个状态
        const auto prevState = m_undoStack.takeLast();

        // 用内部清空方法（不触发 saveState），避免破坏 redo 栈
        clearAllNoSave();

        for (const QSharedPointer<ModelItemData>& model : prevState) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModelNoSave(copy)) {
                delete copy;
            }
        }
        emitModelListChanged();

        emit operationCompleted(true, "Undo successful");
        return true;
    }

    bool EditorManager::redo()
    {
        if (m_redoStack.isEmpty()) {
            emit operationCompleted(false, "Nothing to redo");
            return false;
        }

        // 保存当前状态到 undo（手动克隆）
        QList<QSharedPointer<ModelItemData>> currentState;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            currentState.append(model->clone());
        }
        m_undoStack.append(currentState);

        const auto nextState = m_redoStack.takeLast();

        // 用内部清空方法（不触发 saveState）
        clearAllNoSave();

        for (const QSharedPointer<ModelItemData>& model : nextState) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModelNoSave(copy)) {
                delete copy;
            }
        }
        emitModelListChanged();

        emit operationCompleted(true, "Redo successful");
        return true;
    }

    bool EditorManager::validateModel(const ModelItemData* model)
    {
        return model != nullptr;
    }


    void EditorManager::emitModelListChanged()
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
        sphere2->setName("Default Sphere 2");
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
