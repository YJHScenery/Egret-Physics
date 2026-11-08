/** @file test_epscene_archive.cpp @brief 验证场景归档兼容性、分组、源码往返及损坏输入处理。 */
#include "view_model/serialize/epscene_archive.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <iostream>
#include <stdexcept>

/** @brief 在 Release 模式下也执行检查。 */
static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

/** @brief 执行格式边界及旧版兼容测试。 */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir(QDir::currentPath() + QStringLiteral("/epscene-test-XXXXXX"));
        require(dir.isValid(), "无法创建临时目录");
        const QString path = dir.filePath(QStringLiteral("中文场景.epscenex"));
        const QString source = QStringLiteral("/** @brief 中文控制器。 */\nvoid on_step(void) {\n}\n");
        const QJsonObject original{
            {"version", "1.0"}, {"custom", QJsonObject{{"future", true}}},
            {"models", QJsonArray{QJsonObject{{"id", "field"}, {"category", 1}},
                                  QJsonObject{{"id", "body"}, {"category", 0}},
                                  QJsonObject{{"id", "joint"}, {"category", 2}},
                                  QJsonObject{{"id", "legacy"}}}},
            {"scene_settings", QJsonObject{{"programs", QJsonArray{
                QJsonObject{{"source", source}, {"name", "程序一"}, {"enabled", false}},
                QJsonObject{{"source", "-- 中文 Lua\nfunction on_step() end\n"}, {"backend", "lua"}, {"name", "程序二"}}}},
                {"display", QJsonObject{{"reference_length", 0.01}}}}}};
        QString error;
        if (!egret::EpsceneArchive::write(path, original, error)) throw std::runtime_error(error.toStdString());
        QJsonObject loaded;
        require(egret::EpsceneArchive::read(path, loaded, error) && loaded == original, "V2 往返改变内容或实体顺序");
        require(!egret::EpsceneArchive::write(path, QJsonObject{{"models", QJsonArray{QJsonObject{{"category", 3}}}}}, error),
                "非法实体未被拒绝");
        require(egret::EpsceneArchive::read(path, loaded, error) && loaded == original, "失败保存破坏已有归档");
        QJsonObject metadata;
        {
            QZipReader zip(path);
            metadata = QJsonDocument::fromJson(zip.fileData("metadata.json")).object();
            require(metadata["magic"] == "EPSCENE" && metadata["version"] == "2.0", "归档身份无效");
            require(zip.fileData("control/program_1.c") == source.toUtf8(), "源码文件内容改变");
            require(metadata["scene"].toObject()["scene_settings"].toObject()["programs"].toArray()[0].toObject()["source"] == "control/program_1.c", "源码未替换为相对路径");
            require(QJsonDocument::fromJson(zip.fileData("entities/general.json")).array().size() == 2, "一般实体分组错误");
            require(QJsonDocument::fromJson(zip.fileData("entities/fields.json")).array().size() == 1, "场分组错误");
            require(QJsonDocument::fromJson(zip.fileData("entities/constraints.json")).array().size() == 1, "约束分组错误");
            require(metadata["scene"].toObject()["scene_settings"].toObject()["programs"].toArray()[1].toObject()["source"] == "control/program_2.lua", "Lua 源码扩展名错误");
            require(zip.fileData("control/program_2.lua").contains("on_step"), "Lua 源码未归档");
            bool config = false;
            for (const auto& entry : zip.fileInfoList()) if (entry.isDir && entry.filePath == "config") config = true;
            require(config, "预留配置目录缺失");
        }
        const QString legacyPath = dir.filePath("legacy.epscene");
        QFile legacy(legacyPath);
        require(legacy.open(QIODevice::WriteOnly), "旧版测试文件打开失败");
        legacy.write(QJsonDocument(original).toJson());
        legacy.close();
        require(egret::EpsceneArchive::read(legacyPath, loaded, error) && loaded == original, "V1 读取失败");
        auto unversioned = original;
        unversioned.remove("version");
        require(legacy.open(QIODevice::WriteOnly | QIODevice::Truncate), "无版本测试文件打开失败");
        legacy.write(QJsonDocument(unversioned).toJson());
        legacy.close();
        require(egret::EpsceneArchive::read(legacyPath, loaded, error) && loaded == unversioned, "无版本旧场景读取失败");
        require(egret::EpsceneArchive::write(path, QJsonObject{{"models", QJsonArray{}}}, error), "空场景保存失败");
        require(egret::EpsceneArchive::read(path, loaded, error) && loaded["models"].toArray().isEmpty(), "空场景读取失败");
        loaded = original;
        const QString badPath = dir.filePath("bad.epscenex");
        /** @brief 构造损坏归档，验证失败后不会覆盖调用方已有场景。 */
        const auto reject = [&](QJsonObject info, const QString& extra = QString()) {
            {
                QZipWriter zip(badPath);
                zip.addFile("metadata.json", QJsonDocument(info).toJson());
                for (const auto& name : {"general", "fields", "constraints"})
                    zip.addFile(QStringLiteral("entities/%1.json").arg(name), "[]");
                if (!extra.isEmpty()) zip.addFile(extra, "bad");
                zip.close();
            }
            require(!egret::EpsceneArchive::read(badPath, loaded, error) && !error.isEmpty() && loaded == original,
                    "损坏归档被接受或改变已有场景");
        };
        auto bad = metadata;
        bad["magic"] = "OTHER"; reject(bad);
        bad = metadata; bad["version"] = "3.0"; reject(bad);
        reject(metadata, "../outside.c");
        bad = metadata; bad["entity_order"] = QJsonArray{}; reject(bad);
        bad = metadata;
        auto scene = bad["scene"].toObject();
        auto settings = scene["scene_settings"].toObject();
        settings["programs"] = QJsonArray{QJsonObject{{"source", "control/../outside.c"}}};
        scene["scene_settings"] = settings; bad["scene"] = scene; bad["entity_order"] = QJsonArray{};
        reject(bad);
        {
            QZipWriter zip(badPath);
            zip.setCompressionPolicy(QZipWriter::NeverCompress);
            zip.addFile("metadata.json", QJsonDocument(metadata).toJson());
            zip.close();
        }
        QFile corrupt(badPath);
        require(corrupt.open(QIODevice::ReadWrite), "损坏校验测试文件打开失败");
        auto contents = corrupt.readAll();
        const auto magicOffset = contents.indexOf("EPSCENE");
        require(magicOffset >= 0, "校验测试缺少魔数");
        contents[magicOffset] = 'X';
        corrupt.seek(0);
        corrupt.write(contents);
        corrupt.close();
        require(!egret::EpsceneArchive::read(badPath, loaded, error) && error.contains(QStringLiteral("损坏")),
                "CRC 校验未检测内容修改");
        const auto demo = QStringLiteral(EGRET_SOURCE_DIR "/demo/programmable_body_control.epscene");
        require(egret::EpsceneArchive::read(demo, loaded, error), "真实 V1 控制器示例读取失败");
        require(egret::EpsceneArchive::write(path, loaded, error), "真实 V1 升级失败");
        QJsonObject upgraded;
        require(egret::EpsceneArchive::read(path, upgraded, error) && upgraded == loaded, "真实示例升级丢失数据");
        for (const auto& folder : {QStringLiteral("/demo"), QStringLiteral("/resources/presets")}) {
            const QDir scenes(QStringLiteral(EGRET_SOURCE_DIR) + folder);
            for (const auto& name : scenes.entryList({"*.epscene"}, QDir::Files)) {
                require(egret::EpsceneArchive::read(scenes.filePath(name), loaded, error), "旧场景读取失败");
                require(egret::EpsceneArchive::write(path, loaded, error), "旧场景升级失败");
                require(egret::EpsceneArchive::read(path, upgraded, error) && upgraded == loaded, "旧场景升级丢失数据");
            }
        }
        std::cout << "场景 V1/V2 兼容、分组、源码和非法归档测试通过\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
