/**
 * @file test_editor_state.cpp
 * @brief 验证编辑器场景往返、对象识别和撤销重做。
 */
#include "editor_manager.h"
#include "clipboard_helper.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace egret;

/** @brief 在 Release 构建中也保留断言。 */
static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

/** @brief 验证旧场景加载、新摩擦参数保存、同名修改和变换历史。 */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        EditorManager editor;
        const QString source = QStringLiteral(R"({"models":[{"id":"1","name":"相同","category":0,"type":1,"mass":1,"static_friction":0.9,"dynamic_friction":0.4,"materials":{"baseColor":"#ff123456"}},{"id":"2","name":"相同","category":0,"type":1,"mass":1}]})");
        require(editor.fromJsonString(source), "场景加载失败");
        auto* first = editor.getModel("1");
        auto* second = editor.getModel("2");
        require(first && second && first->staticFriction() == 0.9 && second->staticFriction() == 0.5, "摩擦参数或旧场景默认值错误");
        first->setStaticFriction(1.2);
        const auto saved = QJsonDocument::fromJson(editor.toJsonString(false).toUtf8()).object();
        require(std::abs(saved["models"].toArray()[0].toObject()["static_friction"].toDouble() - 1.2) < 1e-12, "旧字段覆盖了新的摩擦系数");
        require(editor.setModelTransform("2", QVector3D(3, 4, 5), QQuaternion()), "变换更新失败");
        require(editor.undo(), "变换撤销失败");
        require(editor.getModelAtIndex(0)->id() == "1" && editor.getModelAtIndex(1)->id() == "2", "撤销改变了对象顺序");
        require(editor.getModel("2")->pos() == QVector3D(), "撤销未恢复变换");
        require(editor.redo() && editor.getModel("2")->pos() == QVector3D(3, 4, 5), "重做未恢复变换");
        const QString edit = QStringLiteral(R"({"entity_id":"2","entity_name":"重命名","entity_type":1,"category":0,"entity_mass":1,"restitution":1,"static_friction":0.6,"dynamic_friction":0.2,"position":[0,0,0],"scale":[1,1,1],"rotation":[1,0,0,0],"box_size":[1,1,1],"material_base_color":"#abcdef"})");
        require(editor.modifyModelByJsonString(edit), "按 ID 修改失败");
        require(editor.getModel("1")->name() == QStringLiteral("相同") && editor.getModel("2")->name() == QStringLiteral("重命名"), "修改了错误的同名对象");
        require(editor.undo() && editor.getModel("2")->name() == QStringLiteral("相同"), "重命名无法撤销");
        QString changeType = edit;
        changeType.replace(QStringLiteral("\"entity_type\":1"), QStringLiteral("\"entity_type\":8"));
        require(editor.modifyModelByJsonString(changeType) && editor.getModel("2")->type()==8, "修改模式没有更新对象类型");
        EditorManager constraintsEditor;
        const QString constraintScene = QStringLiteral(R"({"models":[{"id":"1","category":0,"type":8,"mass":1,"pos":[0,0,0]},{"id":"2","category":0,"type":8,"mass":1,"pos":[3,4,0]},{"id":"3","category":2,"type":1,"entity_ids":["1","2"],"constraint_data":{"length":5}},{"id":"4","category":2,"type":3,"entity_ids":["1","2"],"constraint_data":{"length":5}},{"id":"5","category":2,"type":2,"entity_ids":["2"],"anchor_pos":{"x":0,"y":0,"z":0},"constraint_data":{"length":5}},{"id":"6","category":2,"type":5,"entity_ids":["1","2"],"constraint_data":{"restLength":5}}]})");
        require(constraintsEditor.fromJsonString(constraintScene), "约束测试场景加载失败");
        require(constraintsEditor.setModelTransform("2", QVector3D(6, 8, 0), QQuaternion()), "移动实体失败");
        for (const auto* id : {"3", "4", "5"})
            require(std::abs(constraintsEditor.getModel(id)->payload()["constraint_data"].toObject()["length"].toDouble() - 10) < 1e-6, "关联约束长度未同步");
        require(constraintsEditor.getModel("6")->payload()["constraint_data"].toObject()["restLength"].toDouble() == 5, "弹簧自然长度被改变");
        require(constraintsEditor.undo(), "约束变换撤销失败");
        require(constraintsEditor.getModel("4")->payload()["constraint_data"].toObject()["length"].toDouble() == 5, "撤销未恢复约束长度");
        require(constraintsEditor.redo(), "约束变换重做失败");
        require(constraintsEditor.getModel("4")->payload()["constraint_data"].toObject()["length"].toDouble() == 10, "重做未恢复约束长度");
        constraintsEditor.getModel("1")->setPos(QVector3D(6, 0, 0));
        require(std::abs(constraintsEditor.getModel("3")->payload()["constraint_data"].toObject()["length"].toDouble() - 8) < 1e-6, "直接移动未同步约束长度");
        QTemporaryDir documentDir(QDir::currentPath() + QStringLiteral("/editor-test-XXXXXX"));
        require(documentDir.isValid(), "临时目录创建失败");
        EditorManager documentsEditor;
        require(documentsEditor.documentNeedsSaveAs(), "新场景应要求另存为");
        require(documentsEditor.fromJsonString(source), "文档场景初始化失败");
        const QString documentPath = documentDir.filePath("scene.epscene");
        require(documentsEditor.saveDocument(documentPath), "文档首次保存失败");
        require(!documentsEditor.documents()[0].toMap()["dirty"].toBool(), "保存后文档仍标记为修改");
        require(documentsEditor.setModelTransform("2", QVector3D(3, 4, 5), QQuaternion()), "文档修改失败");
        require(documentsEditor.documents()[0].toMap()["dirty"].toBool(), "修改后文档未标记为修改");
        documentsEditor.newDocument();
        require(documentsEditor.count() == 0 && documentsEditor.documents().size() == 2, "新建标签没有隔离场景");
        require(documentsEditor.switchDocument(0) && documentsEditor.getModel("2")->pos() == QVector3D(3, 4, 5), "切换标签丢失场景");
        require(documentsEditor.undo() && documentsEditor.getModel("2")->pos() == QVector3D(), "切换标签丢失撤销历史");
        require(!documentsEditor.documents()[0].toMap()["dirty"].toBool(), "撤销到保存状态后仍标记为修改");
        require(QFile::remove(documentPath), "测试文件删除失败");
        require(documentsEditor.documentNeedsSaveAs() && !documentsEditor.saveDocument(), "源文件消失后未要求选择路径");
        auto* sharedEditor = EditorManager::instance();
        const QString copyScene = QStringLiteral(R"({"models":[{"id":"101","name":"甲","category":0,"type":1,"mass":1},{"id":"102","name":"乙","category":0,"type":1,"mass":1},{"id":"103","name":"绳","category":2,"type":1,"entity_ids":["101","102"],"constraint_data":{"length":1}}]})");
        require(sharedEditor->fromJsonString(copyScene), "复制测试场景加载失败");
        auto* clipboard = ClipboardHelper::instance();
        require(clipboard->copyEntities({"101", "102", "103"}), "复制失败");
        const QStringList pasted = clipboard->pasteEntities();
        require(pasted.size()==3, "粘贴对象数量错误");
        const QJsonArray targets = sharedEditor->getModel(pasted[2])->payload()["entity_ids"].toArray();
        require(targets.size()==2 && targets[0].toString()==pasted[0] && targets[1].toString()==pasted[1], "粘贴约束没有指向复制的对象");
        require(sharedEditor->undo() && sharedEditor->count()==3, "多对象粘贴没有作为一次操作撤销");
        std::cout << "编辑器回归通过\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
