/** @file scene_agent.h @brief 场景 Agent 的共享会话和设置。 */
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QUuid>
#include <QNetworkAccessManager>

class QNetworkReply;

namespace egret
{
    /** @brief 为各编辑侧栏提供同一个异步 Agent 会话。 */
    class SceneAgent final : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool busy READ busy NOTIFY changed)
        Q_PROPERTY(QString status MEMBER status NOTIFY changed)
        Q_PROPERTY(QString preview MEMBER preview NOTIFY changed)
        Q_PROPERTY(QString source MEMBER source NOTIFY changed)
        Q_PROPERTY(QString owner MEMBER owner NOTIFY changed)
        Q_PROPERTY(QString suffix MEMBER suffix NOTIFY changed)
    public:
        /** @brief 初始化会话。 */
        explicit SceneAgent(QObject* parent = nullptr);
        /** @brief 返回请求是否尚未完成。 */
        bool busy() const;
        /** @brief 读取非敏感设置和凭据状态。 */
        Q_INVOKABLE QVariantMap settings() const;
        /** @brief 保存设置，并按需替换或清除当前用户凭据。 */
        Q_INVOKABLE QString saveSettings(const QString& key, bool clearKey, int timeout, int tokens, const QUrl& skill);
        /** @brief 发起请求，可附带当前编辑器上下文。 */
        Q_INVOKABLE void start(int task, const QString& prompt, const QString& backend,
                               const QString& context, const QString& ownerId);
        /** @brief 取消正在进行的请求。 */
        Q_INVOKABLE void cancel();
        /** @brief 保存当前草稿至用户选择的位置。 */
        Q_INVOKABLE void saveDraft(const QUrl& output);
    signals:
        /** @brief 通知所有侧栏刷新会话状态。 */
        void changed();
        /** @brief 通知界面切换到刚更新的场景。 */
        void sceneApplied();
    private:
        /** @brief 接收并校验结构化草稿。 */
        void acceptDraft(int task, const QJsonObject& base, const QString& backend, const QJsonObject& result, const QUuid& target);
        QNetworkAccessManager network;
        QNetworkReply* reply = nullptr;
        QString status;
        QString preview;
        QString owner;

        QString suffix;
        QString source;

    };
}
