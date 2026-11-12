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
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include "async_logger.h"
#include "unique_id_generator.h"
#include "os/windows/windows_sound.h"
#include "serialize/transform_transformer.h"
#include "serialize/scene_settings.h"
#include "serialize/epscene_archive.h"
#include "physics/field/field_base.h"
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace egret
{
    EditorManager::EditorManager(QObject* parent) : QObject(parent)
    {
        m_sceneSettings=normalizeSceneSettings({});
        m_documents.append(Document{});
        m_documents[0].savedContent = documentContent();
        connect(this, &EditorManager::modelListChanged, this, [this] { if (!m_switchingDocument) emit documentsChanged(); });
        connect(this, &EditorManager::modelUpdated, this, [this](const QString&) { if (!m_switchingDocument) emit documentsChanged(); });
    }

    QByteArray EditorManager::documentContent() const
    {
        QJsonArray models;
        for (const auto& model : m_modelList) models.append(model->toJson());
        return QJsonDocument(QJsonObject{{"models",models},{"scene_settings",m_sceneSettings}}).toJson(QJsonDocument::Compact);
    }

    QVariantList EditorManager::documents() const
    {
        QVariantList result;
        for (int i = 0; i < m_documents.size(); ++i) {
            const auto& document = m_documents[i];
            QJsonArray models;
            if (i != m_currentDocument) for (const auto& model : document.models) models.append(model->toJson());
            const QByteArray content = i == m_currentDocument ? documentContent() : QJsonDocument(QJsonObject{{"models",models},{"scene_settings",document.settings}}).toJson(QJsonDocument::Compact);
            result.append(QVariantMap{{"name", document.path.isEmpty() ? QStringLiteral("untitled.epscenex") : QFileInfo(document.path).fileName()},
                                      {"path", document.path}, {"dirty", content != document.savedContent}});
        }
        return result;
    }

    void EditorManager::storeDocument()
    {
        auto& document = m_documents[m_currentDocument];
        document.models = m_modelList;
        document.settings=m_sceneSettings;
        document.undo = m_undoStack;
        document.redo = m_redoStack;
    }

    void EditorManager::restoreDocument(int index)
    {
        m_switchingDocument = true;
        m_currentDocument = index;
        const auto& document = m_documents[index];
        // 保留上一批实体，直到视图收到列表切换通知并解绑旧委托。
        const auto previousModels = m_modelList;
        m_modelList = document.models;
        m_sceneSettings=normalizeSceneSettings(document.settings);
        m_undoStack = document.undo;
        m_redoStack = document.redo;
        emit historyChanged();
        m_models.clear();
        for (const auto& model : m_modelList) m_models.insert(model->id(), model);
        emitModelListChanged();
        m_switchingDocument = false;
        emit sceneSettingsChanged();
        emit sceneLoaded();
        emit documentsChanged();
    }

    void EditorManager::newDocument()
    {
        storeDocument();
        Document document;
        document.settings=normalizeSceneSettings({});
        document.savedContent = QJsonDocument(QJsonObject{{"models",QJsonArray{}},{"scene_settings",document.settings}}).toJson(QJsonDocument::Compact);
        m_documents.append(document);
        restoreDocument(m_documents.size() - 1);
    }

    QUuid EditorManager::currentDocumentId() const
    {
        return m_documents[m_currentDocument].id;
    }

    QJsonObject EditorManager::agentSceneSnapshot() const
    {
        auto snapshot = QJsonDocument::fromJson(documentContent()).object();
        snapshot["version"] = "1.0";
        snapshot["modelCount"] = m_modelList.size();
        return snapshot;
    }

    /** @brief 校验模型返回的有限标量及其取值范围。 */
    static bool agentNumberInRange(const QJsonValue& value, double minimum, double maximum)
    {
        return value.isDouble() && std::isfinite(value.toDouble()) &&
            value.toDouble() >= minimum && value.toDouble() <= maximum;
    }

    /** @brief 校验向量长度与每个分量的取值范围。 */
    static bool agentVectorInRange(const QJsonArray& values, qsizetype size, double minimum, double maximum)
    {
        return values.size() == size && std::all_of(values.begin(), values.end(), [=](const QJsonValue& value) {
            return agentNumberInRange(value, minimum, maximum);
        });
    }

    /** @brief 仅允许 Agent 修改既有实体的已验证领域属性。 */
    static bool validAgentEdit(const QJsonObject& scene, const QJsonObject& base)
    {
        const auto oldModels = base.value("models").toArray();
        const auto newModels = scene.value("models").toArray();
        if (oldModels.size() != newModels.size()) return false;
        auto allowedModels = oldModels;
        for (qsizetype i = 0; i < oldModels.size(); ++i) {
            auto oldModel = oldModels.at(i).toObject();
            const auto model = newModels.at(i).toObject();
            const auto category = oldModel.value("category").toInt(-1);
            if (model.value("name").isString() && model.value("name").toString().size() <= 256)
                oldModel["name"] = model.value("name");
            if (category == 0) {
                if (oldModel.value("type").toInt() == static_cast<int>(ShapeType::Box)) {
                    const auto oldSize = oldModel.contains("box_size") ? oldModel.value("box_size").toArray() : QJsonArray{1, 1, 1};
                    const auto size = model.value("box_size").toArray();
                    if (oldSize.size() == 3 && size.size() == 3) {
                        for (qsizetype axis = 0; axis < size.size(); ++axis) {
                            if (size.at(axis) == oldSize.at(axis)) continue;
                            const double length = size.at(axis).toDouble();
                            if (!size.at(axis).isDouble() || !std::isfinite(length) || length < 0.001 || length > 100000)
                                return false;
                        }
                        oldModel["box_size"] = size;
                    }
                }
                for (const auto* key : {"pos", "initial_velocity", "scale"}) {
                    const bool scale = QLatin1String(key) == "scale";
                    const auto value = model.value(key);
                    if (agentVectorInRange(value.toArray(), 3, scale ? 0.001 : -1e12, scale ? 100000.0 : 1e12))
                        oldModel[key] = value;
                }
                const auto rotation = model.value("rotation").toArray();
                if (agentVectorInRange(rotation, 4, -1.0, 1.0)) {
                    double normSquared = 0;
                    for (const auto& component : rotation)
                        normSquared += component.toDouble() * component.toDouble();
                    if (std::abs(normSquared - 1.0) <= 1e-4) oldModel["rotation"] = rotation;
                }
                for (const auto* key : {"static_friction", "dynamic_friction", "restitution"}) {
                    const auto value = model.value(key);
                    if (agentNumberInRange(value, 0, QLatin1String(key) == "restitution" ? 1.0 : 10.0))
                        oldModel[key] = value;
                }
                if (oldModel.value("type").toInt() == static_cast<int>(ShapeType::Sphere) &&
                    agentNumberInRange(model.value("radius"), 0.001, 100000.0))
                    oldModel["radius"] = model.value("radius");

                const auto mass = model.value("mass");
                if (mass.isDouble() && std::isfinite(mass.toDouble()) && mass.toDouble() >= 0)
                    oldModel["mass"] = mass;
                auto material = oldModel.value("materials").toObject();
                const auto color = model.value("materials").toObject().value("baseColor");
                if (color.isString() && QColor::isValidColorName(color.toString())) {
                    material["baseColor"] = color;
                    oldModel["materials"] = material;
                }
            }
            if (category == 1 && oldModel.value("type").toInt() == static_cast<int>(FieldType::Gravity)) {
                auto field = oldModel.value("field_data").toObject();
                const auto gravity = model.value("field_data").toObject().value("gravity").toArray();
                if (gravity.size() == 3 && std::all_of(gravity.begin(), gravity.end(), [](const QJsonValue& component) {
                    return component.isDouble() && std::abs(component.toDouble()) <= 100000.0;
                })) {
                    field["gravity"] = gravity;
                    oldModel["field_data"] = field;
                }
            }
            allowedModels[i] = oldModel;
        }
        auto allowed = base;
        allowed["models"] = allowedModels;
        return allowed == scene;
    }

    bool EditorManager::applyAgentScene(const QJsonObject& scene, const QUuid& target, const QJsonObject& base, QString& error)
    {
        const bool create = target.isNull();
        if (!create && (currentDocumentId() != target || agentSceneSnapshot() != base)) {
            error = QStringLiteral("请求期间当前标签或场景内容已变化，请重新提交。");
            return false;
        }
        if (scene.value("version").toString() != "1.0" || !scene.value("models").isArray()) {
            error = QStringLiteral("场景版本或实体列表无效。");
            return false;
        }
        if (!create && !validAgentEdit(scene, base)) {
            error = QStringLiteral("变更包含未授权字段、实体类型不匹配或数值超出允许范围。旋转须为单位四元数，缩放和尺寸须为正，摩擦系数为 0～10，恢复系数为 0～1。");
            return false;
        }
        const auto programs = scene.value("scene_settings").toObject().value("programs");
        if (create && !programs.isUndefined() && programs != QJsonArray{} &&
            programs != normalizeSceneSettings({}).value("programs")) {
            error = QStringLiteral("新场景不能附带 Agent 生成的可执行控制器，请在控制器编辑器中单独审阅。");
            return false;
        }
        try {
            if (scene.contains("scene_settings") && !scene.value("scene_settings").isObject())
                throw std::invalid_argument("场景设置必须为对象");
            parseSceneSettings(scene.value("scene_settings").toObject());
        } catch (const std::exception& exception) {
            error = QString::fromUtf8(exception.what());
            return false;
        }

        const auto models = scene.value("models").toArray();
        QSet<QString> ids;
        QList<QSharedPointer<ModelItemData>> staged;
        for (const auto& entry : models) {
            const auto object = entry.toObject();
            if (create) {
                const auto category = object.value("category").toInt(-1);
                const auto type = object.value("type").toInt(-1);
                const auto asset = object.value("source").toString();
                static const QSet<QString> primitives{"", "#Cube", "#Sphere", "#Cylinder", "#Cone", "#Rectangle"};
                if (category < 0 || category > 2 ||
                    (category == 1 && type == static_cast<int>(FieldType::VectorForce)) ||
                    !primitives.contains(asset)) {
                    error = QStringLiteral("生成场景只允许内置几何、普通力场和约束。");
                    return false;
                }
            }
            auto model = QSharedPointer<ModelItemData>::create();
            if (!entry.isObject() || !model->fromJson(object) || model->id().isEmpty() || ids.contains(model->id())) {
                error = QStringLiteral("实体 %1 无法加载，或缺少唯一 ID。").arg(staged.size() + 1);
                return false;
            }
            ids.insert(model->id());
            staged.append(model);
        }

        if (create) {
            storeDocument();
            Document document;
            document.models = staged;
            document.settings = normalizeSceneSettings(scene.value("scene_settings").toObject());
            m_documents.append(document);
        } else {
            if (scene == base) return true;
            saveState();
            storeDocument();
            auto& document = m_documents[m_currentDocument];
            document.models = staged;
            document.settings = normalizeSceneSettings(scene.value("scene_settings").toObject());
        }
        for (const auto& model : staged) {
            QQmlEngine::setObjectOwnership(model.data(), QQmlEngine::CppOwnership);
            QQmlEngine::setObjectOwnership(model->materials(), QQmlEngine::CppOwnership);
            watchModelPosition(model.data());
        }
        restoreDocument(create ? int(m_documents.size()) - 1 : m_currentDocument);
        return true;
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
        QJsonObject scene;
        QString error;
        if (!EpsceneArchive::read(localPath, scene, error)) {
            emit operationCompleted(false, error);
            return false;
        }
        const QByteArray data = QJsonDocument(scene).toJson(QJsonDocument::Compact);
        newDocument();
        m_switchingDocument = true;
        const bool ok = fromJsonString(QString::fromUtf8(data));
        m_undoStack.clear();
        m_redoStack.clear();
        emit historyChanged();
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
            document.settings = normalizeSceneSettings({});
            document.savedContent = QJsonDocument(QJsonObject{{"models",QJsonArray{}},{"scene_settings",document.settings}}).toJson(QJsonDocument::Compact);
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

    /** @brief 校验根级设置后一次性写入历史与当前场景。 */
    bool EditorManager::setSceneSettings(const QVariantMap& settings)
    {
        const auto object=normalizeSceneSettings(QJsonObject::fromVariantMap(settings));
        try {parseSceneSettings(object);} catch(const std::exception& error) {
            emit warning(QStringLiteral("场景设置无效"),QString::fromUtf8(error.what()));return false;
        }
        if(object==m_sceneSettings) return true;
        saveState();m_sceneSettings=object;emit sceneSettingsChanged();emit documentsChanged();return true;
    }

    void EditorManager::watchModelPosition(ModelItemData* model)
    {
        connect(model, &ModelItemData::posChanged, this, [this, model] {
            if (model->category() == 0 || model->category() == 1)
                updateConstraintLengths(model->id());
        });
    }

    /** @brief 对外始终提供含默认源码的设置。 */
    QVariantMap EditorManager::sceneSettings()const{return normalizeSceneSettings(m_sceneSettings).toVariantMap();}

    /** @brief 后台检查捕获的程序列表，诊断不随编辑器切换而覆盖源码。 */
    void EditorManager::validateControlPrograms(const QVariantList& programs)
    {
        if(m_controlValidationRunning)return;
        std::vector<control::ControlProgramSpec> specs;
        try{specs=control::parseProgramSpecs(QJsonDocument(QJsonArray::fromVariantList(programs)).toJson(QJsonDocument::Compact).toStdString());
            for(const auto& spec:specs)if(spec.enabled)for(const auto& [alias,id]:spec.bindings){auto model=m_models.value(QString::number(id));if(!model||model->category()!=0)throw std::runtime_error("绑定物体已删除或类型不受支持");}
        }catch(const std::exception& error){m_controlDiagnostics=QString::fromUtf8(error.what());emit controlDiagnosticsChanged();return;}
        m_controlValidationRunning=true;m_controlDiagnostics=QStringLiteral("正在检查当前源码快照……");emit controlDiagnosticsChanged();
        auto* watcher=new QFutureWatcher<QString>(this);
        connect(watcher,&QFutureWatcher<QString>::finished,this,[this,watcher]{m_controlDiagnostics=watcher->result();m_controlValidationRunning=false;watcher->deleteLater();emit controlDiagnosticsChanged();});
        watcher->setFuture(QtConcurrent::run([specs=std::move(specs)]{
            try{unsigned index=1;for(const auto& spec:specs){if(spec.enabled)control::prepareProgram(spec,control::defaultCompilerOptions(),index);++index;}return QStringLiteral("检查通过：已完成各后端源码检查与独立实例准备。正式求解仍会重新检查源码版本。");}
            catch(const std::exception& error){return QString::fromUtf8(error.what());}
        }));
    }

    void EditorManager::updateConstraintLengths(const QString& entityId)
    {
        for (const auto& constraint : m_modelList) {
            if (constraint->category() != 2 || (constraint->type() != 1 && constraint->type() != 3)) continue;
            QJsonObject payload = constraint->payload();
            const QJsonArray ids = payload.value("entity_ids").toArray();
            bool involved = false;
            for (const auto& id : ids) if (id.toString() == entityId) involved = true;
            if (!involved || ids.isEmpty()) continue;
            const auto* first = getModel(ids[0].toString());
            if (!first) continue;
            QVector3D other;
            if (ids.size() == 1) {
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

    // ========================================================================
    //  预设场景
    // ========================================================================

    namespace
    {
        /**
         * @brief   内置预设场景的元信息。
         * @details id 同时是资源文件名（:/presets/<id>.epscene）；菜单按本数组顺序显示。
         */
        struct PresetMeta
        {
            const char* id;          ///< 预设标识
            const char* name;        ///< 显示名称
            const char* category;    ///< 分类（场 / 地面 / 组合场景）
            const char* description; ///< 说明文字
        };

        /** @brief 内置预设场景清单。 */
        const PresetMeta kPresetMetas[] = {
            {"standard_gravity", "标准重力场", "场", "竖直向下 10 m/s² 的匀强重力场，通常只需添加一次"},
            {"large_ground_plane", "大型地平面", "地面", "10000 × 10000 × 10 的静态地面，顶面位于 z = 0"},
            {"standard_test_scene", "标准测试场景", "组合场景", "重力场 + 大型地平面 + 两个自由落体球"},
            {"friction_ground", "摩擦地面", "地面", "400 × 400 × 2 的静态地面，静摩擦 0.8 / 动摩擦 0.6"},
            {"pendulum", "单摆", "组合场景", "重力场 + 悬点 + 连接线组成的单摆"},
            {"spring_oscillator", "弹簧振子", "组合场景", "重力场 + 固定悬点 + 弹簧组成的阻尼振子"},
            {"free_fall_spheres", "自由落体球", "组合场景", "重力场 + 1 / 10 / 100 kg 三个小球，验证重力加速度与质量无关"},
        };

        /**
         * @brief       按标识查找预设元信息。
         * @param presetId 预设标识。
         * @return      指向元信息的指针，未找到时返回 nullptr。
         */
        const PresetMeta* findPresetMeta(const QString& presetId)
        {
            for (const PresetMeta& meta : kPresetMetas) {
                if (presetId == QString::fromUtf8(meta.id))
                    return &meta;
            }
            return nullptr;
        }

        /**
         * @brief       生成不与场景中已有名称冲突的实体名。
         * @param usedNames 场景中已占用的名称集合，生成成功后会追加新名称。
         * @param baseName  预设中的原始实体名。
         * @return      可用的实体名称（重名时追加 -2、-3 …… 后缀）。
         */
        QString makeUniqueEntityName(QStringList& usedNames, const QString& baseName)
        {
            if (baseName.isEmpty())
                return baseName;
            if (!usedNames.contains(baseName)) {
                usedNames.append(baseName);
                return baseName;
            }
            for (int suffix = 2; suffix < 10000; ++suffix) {
                const QString candidate = QStringLiteral("%1-%2").arg(baseName).arg(suffix);
                if (!usedNames.contains(candidate)) {
                    usedNames.append(candidate);
                    return candidate;
                }
            }
            return baseName;
        }

        /**
         * @brief       按映射表重写 JSON 数组中的实体 ID 引用。
         * @param ids    待重写的实体 ID 数组。
         * @param idMap  旧 ID → 新 ID 的映射表；未命中的引用保持原值。
         */
        void remapEntityIdArray(QJsonArray& ids, const QHash<QString, QString>& idMap)
        {
            for (int index = 0; index < ids.size(); ++index) {
                const auto mapped = idMap.constFind(ids.at(index).toString());
                if (mapped != idMap.cend())
                    ids[index] = mapped.value();
            }
        }
    }

    QVariantList EditorManager::availablePresets() const
    {
        QVariantList presets;
        for (const PresetMeta& meta : kPresetMetas) {
            QVariantMap item;
            item[QStringLiteral("id")] = QString::fromUtf8(meta.id);
            item[QStringLiteral("name")] = QString::fromUtf8(meta.name);
            item[QStringLiteral("category")] = QString::fromUtf8(meta.category);
            item[QStringLiteral("description")] = QString::fromUtf8(meta.description);
            presets.append(item);
        }
        return presets;
    }

    bool EditorManager::addPreset(const QString& presetId)
    {
        const PresetMeta* meta = findPresetMeta(presetId);
        if (meta == nullptr) {
            emit error(QStringLiteral("预设场景"), QStringLiteral("未知的预设标识：%1").arg(presetId));
            return false;
        }

        // ---- 1. 读取并解析预设资源文件 ----
        const QString resourcePath = QStringLiteral(":/presets/%1.epscene").arg(presetId);
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit error(QStringLiteral("预设场景"),
                       QStringLiteral("无法读取预设资源：%1").arg(resourcePath));
            return false;
        }
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();
        if (parseError.error != QJsonParseError::NoError) {
            emit error(QStringLiteral("预设场景"),
                       QStringLiteral("预设文件格式错误（第 %1 字节）：%2")
                           .arg(parseError.offset)
                           .arg(parseError.errorString()));
            return false;
        }
        const QJsonArray sourceModels = document.object().value(QStringLiteral("models")).toArray();
        if (sourceModels.isEmpty()) {
            emit error(QStringLiteral("预设场景"), QStringLiteral("预设文件中不包含任何实体。"));
            return false;
        }

        // ---- 2. 为预设中的每个实体分配全新的十进制数字 ID ----
        // 实体 ID 必须是纯十进制数字字符串（场景设置/控制规则校验依赖 toULongLong），
        // 因此这里使用唯一 ID 生成器，并同时避开当前场景与同批次已分配的 ID。
        QHash<QString, QString> idMap;
        QStringList usedIds = m_models.keys();
        for (int index = 0; index < sourceModels.size(); ++index) {
            const QString sourceId = sourceModels.at(index).toObject().value(QStringLiteral("id")).toString();
            if (!sourceId.isEmpty() && idMap.contains(sourceId))
                continue; // 预设内部重复 ID 只映射一次
            QString newId;
            do {
                newId = QString::number(UniqueIDGenerator::instance().generate());
            } while (usedIds.contains(newId));
            usedIds.append(newId);
            // 没有 ID 的实体同样分配新 ID，用批次序号作为内部映射键
            idMap.insert(sourceId.isEmpty() ? QStringLiteral("#%1").arg(index) : sourceId, newId);
        }

        // ---- 3. 重建实体，并重映射预设内部的实体引用 ----
        QStringList usedNames;
        usedNames.reserve(m_modelList.size());
        bool gravityAlreadyExists = false;
        for (const QSharedPointer<ModelItemData>& model : m_modelList) {
            usedNames.append(model->name());
            if (model->category() == 1 && model->type() == 1)
                gravityAlreadyExists = true;
        }

        QList<ModelItemData*> models;
        models.reserve(sourceModels.size());
        bool presetHasGravityField = false;
        for (int index = 0; index < sourceModels.size(); ++index) {
            const QJsonObject source = sourceModels.at(index).toObject();
            QJsonObject json = source;
            const QString sourceId = source.value(QStringLiteral("id")).toString();
            const QString mapKey = sourceId.isEmpty() ? QStringLiteral("#%1").arg(index) : sourceId;
            json[QStringLiteral("id")] = idMap.value(mapKey);

            // 连接线/连接杆/弹簧/导轨等约束对实体的引用
            if (json.value(QStringLiteral("entity_ids")).isArray()) {
                QJsonArray entityIds = json.value(QStringLiteral("entity_ids")).toArray();
                remapEntityIdArray(entityIds, idMap);
                json[QStringLiteral("entity_ids")] = entityIds;
            }
            // 空气阻力等约束的子实体覆写引用
            if (json.value(QStringLiteral("constraint_data")).isObject()) {
                QJsonObject constraintData = json.value(QStringLiteral("constraint_data")).toObject();
                if (constraintData.value(QStringLiteral("entity_overrides")).isArray()) {
                    QJsonArray overrides = constraintData.value(QStringLiteral("entity_overrides")).toArray();
                    for (int overrideIndex = 0; overrideIndex < overrides.size(); ++overrideIndex) {
                        QJsonObject override = overrides.at(overrideIndex).toObject();
                        const QString overrideId = override.value(QStringLiteral("entity_id")).toString();
                        if (!overrideId.isEmpty() && idMap.contains(overrideId))
                            override[QStringLiteral("entity_id")] = idMap.value(overrideId);
                        overrides[overrideIndex] = override;
                    }
                    constraintData[QStringLiteral("entity_overrides")] = overrides;
                }
                json[QStringLiteral("constraint_data")] = constraintData;
            }

            auto* model = new ModelItemData();
            if (!model->fromJson(json)) {
                delete model;
                continue;
            }
            model->setName(makeUniqueEntityName(usedNames, source.value(QStringLiteral("name")).toString()));
            if (model->category() == 1 && model->type() == 1)
                presetHasGravityField = true;
            models.append(model);
        }

        if (models.isEmpty()) {
            emit error(QStringLiteral("预设场景"), QStringLiteral("预设中没有可添加的实体。"));
            return false;
        }

        // ---- 4. 一次性提交：addModels() 内部只调用一次 saveState()，整体可一次撤销 ----
        addModels(models);

        // ---- 5. 结果反馈 ----
        emit information(QStringLiteral("预设场景"),
                         QStringLiteral("已添加预设「%1」，共 %2 个实体。")
                             .arg(QString::fromUtf8(meta->name))
                             .arg(models.size()));
        if (presetHasGravityField && gravityAlreadyExists) {
            emit warning(QStringLiteral("预设场景"),
                         QStringLiteral("当前场景已包含重力场，重复叠加会让重力效果累计，请确认是否需要重复添加。"));
        }
        return true;
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
        auto payload = data.m_payload;
        // 属性面板尚未提供这两个字段时，保留场景文件中已有的动力学配置。
        for (const auto key : {"rotation_enabled", "constant_force"})
            if (!payload.contains(key) && model->payload().contains(key))
                payload[key] = model->payload().value(key);
        model->setPayload(payload);

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
        root["scene_settings"]=m_sceneSettings;

        QString error;
        if (QFileInfo(filePath).suffix().compare(QStringLiteral("epscene"), Qt::CaseInsensitive) == 0) {
            QSaveFile file(filePath);
            const QByteArray data = QJsonDocument(root).toJson();
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
                emit operationCompleted(false, file.errorString());
                return false;
            }
            emit operationCompleted(true, QStringLiteral("场景已保存为 V1.0 JSON"));
            return true;
        }
        if (!EpsceneArchive::write(filePath, root, error)) {
            emit operationCompleted(false, error);
            return false;
        }
        emit operationCompleted(true, QStringLiteral("场景已保存为 V2.0 归档"));
        return true;
    }

    bool EditorManager::loadFromJson(const QString& filePath)
    {
        QJsonObject scene;
        QString error;
        if (!EpsceneArchive::read(filePath, scene, error)) {
            emit operationCompleted(false, error);
            return false;
        }
        return fromJsonString(QString::fromUtf8(QJsonDocument(scene).toJson()));
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
        root["scene_settings"]=m_sceneSettings;

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

        try {
            if(root.contains("scene_settings") && !root["scene_settings"].isObject()) throw std::invalid_argument("场景设置必须为对象");
            parseSceneSettings(root["scene_settings"].toObject());
        } catch(const std::exception& error) {emit warning(QStringLiteral("场景设置无效"),QString::fromUtf8(error.what()));return false;}
        saveState();
        clearAllNoSave();
        m_sceneSettings=normalizeSceneSettings(root["scene_settings"].toObject());
        emit sceneSettingsChanged();

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
        emit sceneLoaded();
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
        m_undoStack.append({currentState,m_sceneSettings});
        m_redoStack.clear();
        emit historyChanged();
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
        m_redoStack.append({currentState,m_sceneSettings});

        // 恢复上一个状态
        const auto prevState = m_undoStack.takeLast();

        // 用内部清空方法（不触发 saveState），避免破坏 redo 栈
        clearAllNoSave();

        m_sceneSettings=prevState.settings;
        emit sceneSettingsChanged();
        for (const QSharedPointer<ModelItemData>& model : prevState.models) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModelNoSave(copy)) {
                delete copy;
            }
        }
        emitModelListChanged();

        emit operationCompleted(true, "Undo successful");
        emit historyChanged();
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
        m_undoStack.append({currentState,m_sceneSettings});

        const auto nextState = m_redoStack.takeLast();

        // 用内部清空方法（不触发 saveState）
        clearAllNoSave();

        m_sceneSettings=nextState.settings;
        emit sceneSettingsChanged();
        for (const QSharedPointer<ModelItemData>& model : nextState.models) {
            ModelItemData* copy = ModelItemData::createCopy(*model);
            if (!addModelNoSave(copy)) {
                delete copy;
            }
        }
        emitModelListChanged();

        emit operationCompleted(true, "Redo successful");
        emit historyChanged();
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
