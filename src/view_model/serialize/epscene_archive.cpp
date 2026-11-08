/** @file epscene_archive.cpp @brief 场景归档封装、身份校验及控制器源码映射。 */
#include "epscene_archive.h"

#include <QFile>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <array>

namespace egret
{
    namespace
    {
        const std::array<QString, 3> entityFiles = {
            QStringLiteral("entities/general.json"), QStringLiteral("entities/fields.json"),
            QStringLiteral("entities/constraints.json")};

        /** @brief 计算 ZIP 标准 CRC32，验证解压内容未损坏。 */
        quint32 zipChecksum(const QByteArray& bytes)
        {
            /** @brief 生成 CRC32 查找表。 */
            static const auto table = [] {
                std::array<quint32, 256> values{};
                for (quint32 i = 0; i < values.size(); ++i) {
                    quint32 value = i;
                    for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
                    values[i] = value;
                }
                return values;
            }();
            quint32 checksum = 0xffffffffu;
            for (unsigned char byte : bytes) checksum = table[(checksum ^ byte) & 0xff] ^ (checksum >> 8);
            return checksum ^ 0xffffffffu;
        }

        /** @brief 防止非法设置类型在转换时被静默清空。 */
        bool validSettings(const QJsonObject& root, QString& error)
        {
            if (root.contains("scene_settings") && !root.value("scene_settings").isObject()) {
                error = QStringLiteral("场景设置必须为对象"); return false;
            }
            const auto settings = root.value("scene_settings").toObject();
            if (settings.contains("programs") && !settings.value("programs").isArray()) {
                error = QStringLiteral("控制程序必须为数组"); return false;
            }
            for (const auto& program : settings.value("programs").toArray())
                if (!program.isObject()) { error = QStringLiteral("控制程序必须为对象"); return false; }
            return true;
        }

        /** @brief 验证归档路径只包含安全的相对路径段。 */
        bool safePath(const QString& path)
        {
            if (path.isEmpty() || path.startsWith('/') || path.contains('\\') || path.contains(':')) return false;
            const auto parts = path.split('/');
            for (qsizetype i = 0; i < parts.size(); ++i)
                if (parts[i] == "." || parts[i] == ".." || (parts[i].isEmpty() && i != parts.size() - 1)) return false;
            return true;
        }

        /** @brief 严格解析 JSON，避免把损坏数据作为空对象读取。 */
        bool parseJson(const QByteArray& bytes, QJsonDocument& document, QString& error)
        {
            QJsonParseError parseError;
            document = QJsonDocument::fromJson(bytes, &parseError);
            if (parseError.error == QJsonParseError::NoError && !document.isNull()) return true;
            error = QStringLiteral("场景 JSON 无效：%1").arg(parseError.errorString());
            return false;
        }
    }

    bool EpsceneArchive::read(const QString& path, QJsonObject& scene, QString& error)
    {
        error.clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
        QJsonObject root;
        if (!file.peek(4).startsWith("PK")) {
            QJsonDocument document;
            if (!parseJson(file.readAll(), document, error)) return false;
            if (!document.isObject()) { error = QStringLiteral("场景必须为 JSON 对象"); return false; }
            root = document.object();
            const QString version = root.value("version").toString("1.0");
            if (version != "1.0") { error = QStringLiteral("不支持的 JSON 场景版本：%1").arg(version); return false; }
        } else {
            QZipReader zip(&file);
            QSet<QString> names;
            qint64 totalSize = 0;
            const auto entries = zip.fileInfoList();
            for (const auto& entry : entries) {
                totalSize += entry.size;
                if (!safePath(entry.filePath) || entry.isSymLink || names.contains(entry.filePath) ||
                    entry.size < 0 || entry.size > 64 * 1024 * 1024 || totalSize > 256 * 1024 * 1024) {
                    error = QStringLiteral("归档包含重复、不安全或过大的条目"); return false;
                }
                if (entry.isFile) names.insert(entry.filePath);
            }
            /** @brief 读取并检查必需条目的内容和长度。 */
            const auto readEntry = [&](const QString& name, QByteArray& bytes) {
                if (!names.contains(name)) { error = QStringLiteral("归档缺少文件：%1").arg(name); return false; }
                bytes = zip.fileData(name);
                for (const auto& entry : entries)
                    if (entry.filePath == name && (bytes.size() != entry.size || zipChecksum(bytes) != entry.crc)) {
                        error = QStringLiteral("归档文件损坏：%1").arg(name); return false;
                    }
                if (zip.status() != QZipReader::NoError) { error = QStringLiteral("读取 ZIP 失败"); return false; }
                return true;
            };
            QByteArray bytes;
            QJsonDocument document;
            if (!readEntry("metadata.json", bytes) || !parseJson(bytes, document, error)) return false;
            const auto metadata = document.object();
            if (metadata.value("magic") != "EPSCENE" || metadata.value("version") != "2.0" ||
                !metadata.value("scene").isObject() || !metadata.value("entity_order").isArray()) {
                error = QStringLiteral("归档身份、版本或元数据无效"); return false;
            }
            root = metadata.value("scene").toObject();
            if (!validSettings(root, error)) return false;
            std::array<QJsonArray, 3> groups;
            for (int category = 0; category < 3; ++category) {
                if (!readEntry(entityFiles[category], bytes) || !parseJson(bytes, document, error)) return false;
                if (!document.isArray()) { error = QStringLiteral("实体文件必须为 JSON 数组"); return false; }
                groups[category] = document.array();
                for (const auto& value : groups[category]) {
                    if (!value.isObject() || value.toObject().value("category").toInt(0) != category) {
                        error = QStringLiteral("实体类别与文件不一致"); return false;
                    }
                }
            }
            std::array<qsizetype, 3> offsets{};
            QJsonArray models;
            for (const auto& value : metadata.value("entity_order").toArray()) {
                const int category = value.toInt(-1);
                if (category < 0 || category > 2 || value.toDouble(-1) != category || offsets[category] >= groups[category].size()) {
                    error = QStringLiteral("实体顺序索引无效"); return false;
                }
                models.append(groups[category][offsets[category]++]);
            }
            for (int i = 0; i < 3; ++i)
                if (offsets[i] != groups[i].size()) { error = QStringLiteral("实体顺序索引不完整"); return false; }
            root["models"] = models;
            auto settings = root.value("scene_settings").toObject();
            auto programs = settings.value("programs").toArray();
            for (qsizetype i = 0; i < programs.size(); ++i) {
                auto program = programs[i].toObject();
                const QString source = program.value("source").toString();
                if (!safePath(source) || !source.startsWith("control/") || !source.endsWith(program.value("backend").toString() == "lua" ? ".lua" : ".c")) {
                    error = QStringLiteral("控制器源码路径无效"); return false;
                }
                if (!readEntry(source, bytes)) return false;
                program["source"] = QString::fromUtf8(bytes);
                programs[i] = program;
            }
            if (settings.contains("programs")) settings["programs"] = programs;
            if (root.contains("scene_settings")) root["scene_settings"] = settings;
        }
        if (!root.value("models").isArray()) { error = QStringLiteral("场景缺少实体数组"); return false; }
        scene = root;
        return true;
    }

    bool EpsceneArchive::write(const QString& path, const QJsonObject& scene, QString& error)
    {
        error.clear();
        if (!scene.value("models").isArray()) { error = QStringLiteral("场景缺少实体数组"); return false; }
        if (!validSettings(scene, error)) return false;
        QByteArray archive;
        QBuffer buffer(&archive);
        if (!buffer.open(QIODevice::WriteOnly)) { error = buffer.errorString(); return false; }
        QZipWriter zip(&buffer);
        zip.addDirectory("entities");
        zip.addDirectory("control");
        zip.addDirectory("config");
        QJsonObject root = scene;
        std::array<QJsonArray, 3> groups;
        QJsonArray order;
        for (const auto& value : root.take("models").toArray()) {
            const int category = value.toObject().value("category").toInt(0);
            if (!value.isObject() || category < 0 || category > 2) {
                error = QStringLiteral("实体类别无效"); return false;
            }
            groups[category].append(value);
            order.append(category);
        }
        for (int i = 0; i < 3; ++i) zip.addFile(entityFiles[i], QJsonDocument(groups[i]).toJson());
        auto settings = root.value("scene_settings").toObject();
        auto programs = settings.value("programs").toArray();
        for (qsizetype i = 0; i < programs.size(); ++i) {
            auto program = programs[i].toObject();
            if (!program.value("source").isString()) {
                error = QStringLiteral("控制器源码必须为字符串"); return false;
            }
            const QString sourcePath = QStringLiteral("control/program_%1.%2").arg(i + 1).arg(program.value("backend").toString() == "lua" ? "lua" : "c");
            zip.addFile(sourcePath, program.value("source").toString().toUtf8());
            program["source"] = sourcePath;
            programs[i] = program;
        }
        if (settings.contains("programs")) settings["programs"] = programs;
        if (root.contains("scene_settings")) root["scene_settings"] = settings;
        const QJsonObject metadata{{"magic", "EPSCENE"}, {"version", "2.0"}, {"scene", root}, {"entity_order", order}};
        zip.addFile("metadata.json", QJsonDocument(metadata).toJson());
        zip.close();
        if (zip.status() != QZipWriter::NoError) {
            error = QStringLiteral("写入 ZIP 失败"); return false;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
        if (file.write(archive) != archive.size()) { error = file.errorString(); file.cancelWriting(); return false; }
        if (!file.commit()) { error = file.errorString(); return false; }
        return true;
    }
}
