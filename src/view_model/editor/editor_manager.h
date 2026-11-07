/**
 * @file        editor_manager.h
 * @brief       编辑器模型管理器头文件，提供编辑器层的数据模型管理功能。
 * @details     定义 ModelManager 类，负责编辑器中的模型数据管理，包括模型的创建、删除和序列化等。
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
#include <QSharedPointer>
#include "physical_entity.h"

#include "serialize/model_item_data.h"

namespace egret
{

    /**
     * @brief       编辑器模型管理器类，负责编辑器中的模型数据管理。
     * @details     ModelManager 继承自 QObject，是编辑器中的模型数据管理器。
     *              负责编辑器中的模型数据管理，包括模型的创建、删除和序列化等。
     *              采用单例模式，全局唯一实例。
     *              支持 QML 属性绑定和信号通知。
     *              提供 JSON 序列化和反序列化功能。
     *              支持批量操作和查找筛选功能。
     *
     * @invariant   count 属性表示模型数量
     * @invariant   modelIds 属性表示模型ID列表
     * @remark      ModelManager 是单例，全局唯一实例
     * @see         ModelItemData, MaterialData, QObject
     */
    class EditorManager : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QVariantList documents READ documents NOTIFY documentsChanged)
        Q_PROPERTY(int currentDocument READ currentDocument NOTIFY documentsChanged)
        Q_PROPERTY(int count READ count NOTIFY countChanged)
        Q_PROPERTY(QStringList modelIds READ modelIds NOTIFY modelListChanged)
        Q_PROPERTY(QVariantMap sceneSettings READ sceneSettings NOTIFY sceneSettingsChanged)
        Q_PROPERTY(QString controlDiagnostics READ controlDiagnostics NOTIFY controlDiagnosticsChanged)
        Q_PROPERTY(bool controlValidationRunning READ controlValidationRunning NOTIFY controlDiagnosticsChanged)
        Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
        Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

    public:
        explicit EditorManager(QObject *parent = nullptr);
        /** @brief 返回当前标签的显示设置与控制规则。 */
        QVariantMap sceneSettings() const;
        /** @brief 返回最近一次后台源码检查诊断。 */
        QString controlDiagnostics() const{return m_controlDiagnostics;}
        /** @brief 源码检查是否正在后台运行。 */
        bool controlValidationRunning() const{return m_controlValidationRunning;}
        /** @brief 捕获表单版本并在后台执行完整编译检查，不修改当前求解任务。 */
        Q_INVOKABLE void validateControlPrograms(const QVariantList& programs);
        /** @brief 校验并保存场景设置，作为一次可撤销操作。 */
        Q_INVOKABLE bool setSceneSettings(const QVariantMap& settings);

        // 单例访问
        static EditorManager *instance();

        /** @brief 返回编辑器标签列表。 */
        [[nodiscard]] QVariantList documents() const;
        /** @brief 返回当前标签索引。 */
        [[nodiscard]] int currentDocument() const { return m_currentDocument; }
        /** @brief 新建并切换到空场景。 */
        Q_INVOKABLE void newDocument();
        /** @brief 打开文件为新标签。 */
        Q_INVOKABLE bool openDocument(const QString &path);
        /** @brief 切换当前标签。 */
        Q_INVOKABLE bool switchDocument(int index);
        /** @brief 关闭指定标签，调用方负责处理未保存修改。 */
        Q_INVOKABLE bool closeDocument(int index);
        /** @brief 保存当前标签到指定路径；空路径使用原路径。 */
        Q_INVOKABLE bool saveDocument(const QString &path = {});
        /** @brief 判断当前文件是否需要通过对话框选择路径。 */
        Q_INVOKABLE bool documentNeedsSaveAs() const;

        // 基本操作
        Q_INVOKABLE bool addModel(ModelItemData *model);

        Q_INVOKABLE bool removeModel(const QString &id);

        /** @brief 批量删除对象，并作为一次操作记录到撤销历史。 */
        Q_INVOKABLE int removeModels(const QStringList &ids);

        Q_INVOKABLE ModelItemData *getModel(const QString &id);

        Q_INVOKABLE ModelItemData *getModelAtIndex(int index);

        Q_INVOKABLE bool updateModel(const QString &id, ModelItemData *model);

        /** @brief 更新物体变换并记录一次可撤销操作。 */
        Q_INVOKABLE bool setModelTransform(const QString &id, const QVector3D &position,
                                           const QQuaternion &rotation);

        /**
         * @brief 获取指定模型的透传负载数据（field_data / constraint_data）。
         * @param id 模型 ID。
         * @return payload 的 QVariantMap，若模型不存在则返回空 map。
         */
        Q_INVOKABLE QVariantMap getModelPayload(const QString &id) const;

        Q_INVOKABLE void clearAll();

        // 批量操作
        Q_INVOKABLE void addModels(const QList<ModelItemData *> &models);

        Q_INVOKABLE [[nodiscard]] QList<ModelItemData *> getAllModels() const;

        // 属性查询
        [[nodiscard]] qsizetype count() const;
        [[nodiscard]] QStringList modelIds() const;

        // JSON 序列化
        Q_INVOKABLE bool addModelByJsonString(const QString &jsonString);

        Q_INVOKABLE bool modifyModelByJsonString(const QString &jsonString);

        Q_INVOKABLE bool saveToJson(const QString &filePath);

        Q_INVOKABLE bool loadFromJson(const QString &filePath);

        Q_INVOKABLE QString toJsonString(bool pretty = true);

        Q_INVOKABLE bool fromJsonString(const QString &jsonString);

        // 导入导出
        Q_INVOKABLE bool exportToFile(const QString &filePath);

        Q_INVOKABLE bool importFromFile(const QString &filePath);

        // 查找和筛选
        Q_INVOKABLE [[nodiscard]] QList<ModelItemData *> findModelsByName(const QString &name);

        Q_INVOKABLE [[nodiscard]] QList<ModelItemData *> findModelsBySource(const QString &source);

        Q_INVOKABLE [[nodiscard]] QList<ModelItemData *> getModelsByCategory(int category) const;

        // 批量修改
        Q_INVOKABLE void setModelsPosition(const QVector3D &position, const QStringList &ids = QStringList());

        Q_INVOKABLE void setModelsScale(const QVector3D &scale, const QStringList &ids = QStringList());

        Q_INVOKABLE void setModelsMaterialProperty(const QString &property, const QVariant &value,
                                                   const QStringList &ids = QStringList());

        Q_INVOKABLE QVariantMap setQuick3DRenderTransform(int index);

        // 撤销/重做（可选）
        Q_INVOKABLE void saveState();

        Q_INVOKABLE bool undo();

        Q_INVOKABLE bool redo();
        /** @brief 当前标签是否存在可撤销的操作。 */
        [[nodiscard]] bool canUndo() const { return !m_undoStack.isEmpty(); }
        /** @brief 当前标签是否存在可重做的操作。 */
        [[nodiscard]] bool canRedo() const { return !m_redoStack.isEmpty(); }

        // ---- 预设场景 ----

        /**
         * @brief       返回内置预设场景列表。
         * @details     列表项为 QVariantMap，包含 id（预设标识）、name（显示名称）、
         *              description（说明文字）、category（分类：场 / 地面 / 组合场景）。
         *              预设场景以 epscene(JSON) 形式保存在资源 :/presets/ 目录下。
         * @return      预设场景描述列表。
         */
        Q_INVOKABLE [[nodiscard]] QVariantList availablePresets() const;

        /**
         * @brief       把一个内置预设场景叠加到当前文档。
         * @details     读取资源 :/presets/<presetId>.epscene，逐个重建实体并追加到当前文档，
         *              不替换、不清空现有内容，因此可以与其它预设反复叠加。
         *              为避免 ID 冲突，实体的十进制数字 ID 会重新分配，并在预设内部
         *              重映射 entity_ids / constraint_data.entity_overrides 中的引用。
         *              全部实体作为一次可撤销操作记录。
         * @param presetId 预设标识（availablePresets() 返回的 id）。
         * @return      是否成功添加预设。
         */
        Q_INVOKABLE bool addPreset(const QString &presetId);

    signals:
        /** @brief 当前标签的撤销或重做状态变化。 */
        void historyChanged();
        /** @brief 后台检查状态或诊断内容更新。 */
        void controlDiagnosticsChanged();
        /** @brief 当前场景设置或标签已切换。 */
        void sceneSettingsChanged();
        /** @brief 场景文件加载或标签恢复完成，可据此自动取景。 */
        void sceneLoaded();
        /** @brief 标签状态发生变化。 */
        void documentsChanged();
        void countChanged();

        void modelListChanged();

        void modelAdded(const QString &id);

        void modelRemoved(const QString &id);

        void modelUpdated(const QString &id);

        void operationCompleted(bool success, const QString &message);

        void information(const QString& title, const QString &message);

        void warning(const QString& title, const QString &message);

        void error(const QString& title, const QString &message);

    private:
        /** @brief 场景历史同时保存对象和根级设置。 */
        struct HistoryState {
            QList<QSharedPointer<ModelItemData>> models;
            QJsonObject settings;
        };
        /** @brief 单个标签的场景及独立撤销历史。 */
        struct Document {
            QString path;
            QByteArray savedContent;
            QList<QSharedPointer<ModelItemData>> models;
            QJsonObject settings;
            QList<HistoryState> undo;
            QList<HistoryState> redo;
        };
        QList<Document> m_documents;
        int m_currentDocument{0};
        bool m_switchingDocument{false};
        /** @brief 获取稳定的场景内容，用于判断是否有未保存修改。 */
        QByteArray documentContent() const;
        /** @brief 保存当前标签的内存状态。 */
        void storeDocument();
        /** @brief 恢复指定标签的内存状态。 */
        void restoreDocument(int index);

        QHash<QString, QSharedPointer<ModelItemData>> m_models;

        QList<QSharedPointer<ModelItemData>> m_modelList;

        QJsonObject m_sceneSettings;
        QString m_controlDiagnostics;
        bool m_controlValidationRunning{};
        QList<HistoryState> m_undoStack;

        QList<HistoryState> m_redoStack;

        /** @brief 监听实体位置并同步其相关约束的几何长度。 */
        void watchModelPosition(ModelItemData *model);

        /** @brief 根据实体当前位置更新连接线和连接杆长度。 */
        void updateConstraintLengths(const QString &entityId);

        void emitModelListChanged();

        static bool validateModel(const ModelItemData *model);

        /** 内部清空（不触发 saveState），供 undo/redo 使用 */
        void clearAllNoSave();

        /** 内部添加（不触发 saveState），供 undo/redo 重建使用 */
        bool addModelNoSave(ModelItemData *model);

    };

    [[deprecated]] QList<ModelItemData *> createDefaultModels();

}

#endif // EGRET_PHYSICS_EDITOR_MANAGER_H
