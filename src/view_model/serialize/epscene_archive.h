/** @file epscene_archive.h @brief 场景 V1 JSON 与 V2 ZIP 文件的兼容读写接口。 */
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>

namespace egret
{
    /** @brief 在文件边界转换归档格式，内部仍使用包含源码的场景 JSON。 */
    class EpsceneArchive
    {
    public:
        /** @brief 自动识别 V1 JSON 或 V2 ZIP；失败时不改变输出场景。 */
        static bool read(const QString& path, QJsonObject& scene, QString& error);
        /** @brief 生成归档相对路径和文件内容组成的文件树。 */
        static QMap<QString, QByteArray> buildTree(const QJsonObject& scene, QString& error);
        /** @brief 将场景原子保存为 V2 ZIP，控制器源码单独存储。 */
        static bool write(const QString& path, const QJsonObject& scene, QString& error);
    };
}
