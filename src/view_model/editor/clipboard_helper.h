/**
 * @file        clipboard_helper.h
 * @brief       编辑器剪贴板辅助类头文件，提供实体复制/粘贴/剪切功能。
 * @details     定义 ClipboardHelper 类，以 JSON 格式暂存复制的实体数据，
 *              支持粘贴时对位置进行累加偏移，避免实体重叠。
 *              采用单例模式，注册为 QML 单例供 EditorUnit 直接调用。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-11
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_CLIPBOARD_HELPER_H
#define EGRET_PHYSICS_CLIPBOARD_HELPER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector3D>
#include <QStringList>

namespace egret
{

    /**
     * @brief       编辑器剪贴板辅助类，提供实体的 JSON 格式复制/粘贴/剪切功能。
     * @details     ClipboardHelper 继承自 QObject，采用单例模式。
     *              内部以 QJsonArray 存储复制实体的完整 JSON 表示（ModelItemData::toJson() 格式）。
     *              粘贴时自动为位置累加偏移量，确保连续粘贴的实体不重叠。
     *              作为 QML 单例注册，EditorUnit 可直接通过 ClipboardManager 调用。
     *
     * @invariant   m_clipboard 为空时 hasContent == false
     * @invariant   m_pasteCount 仅在 copyEntities/cutEntities 时归零
     * @remark      ClipboardHelper 是单例，全局唯一实例
     * @see         EditorManager, ModelItemData
     */
    class ClipboardHelper : public QObject
    {
        Q_OBJECT
        /** 剪贴板中是否有待粘贴的内容 */
        Q_PROPERTY(bool hasContent READ hasContent NOTIFY contentChanged)
        /** 剪贴板中暂存的实体数量 */
        Q_PROPERTY(int contentCount READ contentCount NOTIFY contentChanged)

    public:
        /**
         * @brief       获取 ClipboardHelper 单例。
         * @return      ClipboardHelper 全局唯一实例指针。
         */
        static ClipboardHelper *instance();

        /**
         * @brief       复制指定 ID 的实体到剪贴板。
         * @details     对每个实体的位置应用 m_baseOffset 偏移后存入内部 QJsonArray。
         *              重置粘贴计数器。
         *              支持 Shape、Field、Constraint 全类别实体复制。
         * @param ids   要复制的实体 ID 列表。
         * @return      成功复制至少一个实体时返回 true。
         */
        Q_INVOKABLE bool copyEntities(const QStringList &ids);

        /**
         * @brief       剪切指定 ID 的实体（复制后从 EditorManager 中删除原实体）。
         * @param ids   要剪切的实体 ID 列表。
         * @return      成功剪切至少一个实体时返回 true。
         */
        Q_INVOKABLE bool cutEntities(const QStringList &ids);

        /**
         * @brief       将剪贴板中的实体粘贴到场景中。
         * @details     对每个实体的位置在已偏移的基础上再累加 m_pasteCount * m_baseOffset，
         *              为新实体生成全新 ID，通过 EditorManager::addModel 添加到场景。
         *              每次调用后 m_pasteCount 自增，支持连续粘贴产生渐进偏移。
         * @return      新创建的实体 ID 列表。
         */
        Q_INVOKABLE QStringList pasteEntities();

        /**
         * @brief       清空剪贴板内容并重置粘贴计数器。
         */
        Q_INVOKABLE void clear();

        /**
         * @brief       剪贴板是否包含有效内容。
         * @return      m_clipboard 非空时返回 true。
         */
        [[nodiscard]] bool hasContent() const;

        /**
         * @brief       剪贴板中暂存的实体数量。
         * @return      m_clipboard 数组的大小。
         */
        [[nodiscard]] int contentCount() const;

    signals:
        /**
         * @brief       剪贴板内容变化信号。
         * @details     在 copyEntities、cutEntities、clear、pasteEntities 后触发。
         */
        void contentChanged();

        /**
         * @brief       操作完成信号。
         * @param success   操作是否成功。
         * @param message   操作结果描述信息。
         */
        void operationCompleted(bool success, const QString &message);

    private:
        /**
         * @brief       私有构造函数（单例模式）。
         * @param parent 父对象指针。
         */
        explicit ClipboardHelper(QObject *parent = nullptr);

        /** 内部剪贴板存储，每个元素为一个实体的完整 JSON */
        QJsonArray m_clipboard;

        /** 基础位置偏移量，默认 (10, 10, 10) */
        QVector3D m_baseOffset{10.0f, 10.0f, 10.0f};

        /** 粘贴计数器，每次 pasteEntities 后自增 */
        int m_pasteCount{0};

        QMap<QString, int> m_nameCount{};
    };

} // egret

#endif // EGRET_PHYSICS_CLIPBOARD_HELPER_H
