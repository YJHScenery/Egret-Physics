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

    public:
        explicit EditorManager(QObject *parent = nullptr);

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

    signals:
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
        /** @brief 单个标签的场景及独立撤销历史。 */
        struct Document {
            QString path;
            QByteArray savedContent;
            QList<QSharedPointer<ModelItemData>> models;
            QList<QList<QSharedPointer<ModelItemData>>> undo;
            QList<QList<QSharedPointer<ModelItemData>>> redo;
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

        QList<QList<QSharedPointer<ModelItemData>>> m_undoStack;

        QList<QList<QSharedPointer<ModelItemData>>> m_redoStack;

        /** @brief 监听实体位置并同步其相关约束的几何长度。 */
        void watchModelPosition(ModelItemData *model);

        /** @brief 根据实体当前位置更新连接线、连接杆和单摆长度。 */
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
