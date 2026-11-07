/** @file test_code_editor.cpp @brief 验证 C 编辑器实际 QML 加载、字体及高亮优先级。 */
#include "editor_manager.h"
#include "code_editor_highlighter.h"
#include "code_editor_settings.h"
#include <QFontDatabase>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QTextBlock>
#include <QTextLayout>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QImage>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <memory>
#include <stdexcept>

/** @brief 检查条件，失败时保留具体诊断。 */
static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

/** @brief 查找指定位置的词法前景色。 */
static QColor foregroundAt(const QTextBlock& block, int position) {
    for (const auto& range : block.layout()->formats())
        if (position >= range.start && position < range.start + range.length)
            return range.format.foreground().color();
    return {};
}

/** @brief 用光标标记验证真实脚本生成的文本替换和光标位置。 */
static void checkIndent(QJSValue function, QString input, QString expected) {
    const int cursor = input.indexOf('|');
    input.remove(cursor, 1);
    const int expectedCursor = expected.indexOf('|');
    expected.remove(expectedCursor, 1);
    const auto edit = function.call({input, cursor, cursor});
    check(!edit.isError(), "缩进脚本执行失败");
    if (edit.isNull()) {
        check(input == expected && cursor == expectedCursor, "不应对注释或字符串应用右花括号缩进");
        return;
    }
    const int start = edit.property("start").toInt();
    const int end = edit.property("end").toInt();
    input.replace(start, end - start, edit.property("text").toString());
    check(input == expected, "自动缩进生成的源码不符合预期");
    check(edit.property("cursor").toInt() == expectedCursor, "自动缩进后光标位置错误");
}

/** @brief 验证 C 代码块缩进、普通语句、成对括号及注释字符串隔离。 */
static void testCIndent() {
    QFile file(":/components/components/editor/CIndent.js");
    check(file.open(QIODevice::ReadOnly), "无法读取缩进脚本");
    QString script = QString::fromUtf8(file.readAll());
    script.replace(".pragma library", "");
    QJSEngine engine;
    check(!engine.evaluate(script).isError(), "缩进脚本语法错误");
    const auto newline = engine.globalObject().property("newline");
    const auto closing = engine.globalObject().property("closingBrace");
    checkIndent(newline, "void on_step(void)\n{|", "void on_step(void)\n{\n    |");
    checkIndent(newline, "    if (ready) {|}", "    if (ready) {\n        |\n    }");
    checkIndent(newline, "    value++;|", "    value++;\n    |");
    checkIndent(newline, "int value;|", "int value;\n|");
    checkIndent(newline, "    |", "    \n    |");
    checkIndent(newline, "    if (ready) { // 注释中的 }\n        run();|", "    if (ready) { // 注释中的 }\n        run();\n        |");
    checkIndent(newline, "    const char *s = \"{\";|", "    const char *s = \"{\";\n    |");
    checkIndent(newline, "    /* { */ run();|", "    /* { */ run();\n    |");
    checkIndent(newline, "    if (ready) { // 进入代码块|", "    if (ready) { // 进入代码块\n        |");
    checkIndent(newline, "#define OPEN {|", "#define OPEN {\n|");
    checkIndent(newline, "\tif (ready) {|", "\tif (ready) {\n        |");
    checkIndent(newline, "void f() {\n    run();|}", "void f() {\n    run();\n|}");
    checkIndent(closing, "void f() {\n    if (ready) {\n        |", "void f() {\n    if (ready) {\n    }|");
    checkIndent(closing, "void f() {\n    // }\n    |", "void f() {\n    // }\n}|");
    checkIndent(closing, "void f() {\n    /*\n    |", "void f() {\n    /*\n    |");
    checkIndent(closing, "const char *s = \"{\n    |", "const char *s = \"{\n    |");
    const QString selected = "    if (ready) {replace}";
    const int selectionStart = int(selected.indexOf("replace"));
    const auto edit = newline.call({selected, selectionStart, selectionStart + 7});
    check(edit.property("text").toString() == "\n        \n    ", "替换选中文本时丢失代码块缩进");
}

/** @brief 验证语法高亮和实际控制窗口在离屏环境中创建成功。 */
void testCodeEditor() {
    using namespace egret;
    testCIndent();
    check(QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf") >= 0, "内置字体无法加载");
    check(QFontDatabase::families().contains("JetBrains Mono"), "JetBrains Mono 字体族不可用");
    // 离屏平台不会自动枚举 Windows 字体，显式加载中文字体以验证预览文字。
    if (QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc") >= 0)
        QGuiApplication::setFont(QFont("Microsoft YaHei"));
    QTextDocument document;
    CodeEditorHighlighter highlighter;
    highlighter.setDocument(&document);
    document.setPlainText("int a = 1+2; /* if\nreturn */ const char* s = \"// if\";\non_step(); // 123");
    highlighter.rehighlight();
    auto block = document.firstBlock();
    check(foregroundAt(block, 0) == highlighter.typeColor(), "C 类型未高亮");
    check(foregroundAt(block, 8) == highlighter.numberColor() && foregroundAt(block, 10) == highlighter.numberColor(), "相邻数值高亮错误");
    check(foregroundAt(block, 9) != highlighter.numberColor(), "数值高亮吞掉运算符");
    block = block.next();
    check(foregroundAt(block, 0) == highlighter.commentColor(), "跨行注释状态丢失");
    check(foregroundAt(block, block.text().indexOf("const")) == highlighter.keywordColor(), "块注释结束后没有恢复代码高亮");
    check(foregroundAt(block, block.text().indexOf("//")) == highlighter.stringColor(), "字符串被误识别为注释");
    block = block.next();
    check(foregroundAt(block, 0) == highlighter.functionColor(), "控制 API 函数未高亮");
    check(foregroundAt(block, block.text().indexOf("123")) == highlighter.commentColor(), "注释内数字高亮优先级错误");
    qmlRegisterType<CodeEditorHighlighter>("EditorManager", 1, 0, "CodeEditorHighlighter");
    qmlRegisterSingletonInstance("EditorManager", 1, 0, "EditorManager", EditorManager::instance());
    qmlRegisterSingletonInstance("EditorManager", 1, 0, "CodeEditorSettings", CodeEditorSettings::instance());
    QQmlEngine engine;
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&warnings](const QList<QQmlError>& errors) {
        for (const auto& error : errors) warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls.Basic
        import "qrc:/components/components"
        import "qrc:/components/components/presets"
        ApplicationWindow {
            width: 1400; height: 1000; visible: true
            ControlCodeDialog {objectName: "dialog"}
            EgretPresetMenu {objectName: "presets"}
        }
    )", QUrl("qrc:/tests/editor_smoke.qml"));
    std::unique_ptr<QObject> window(component.create());
    if (!window) throw std::runtime_error(component.errorString().toStdString());
    QObject* dialog = window->findChild<QObject*>("dialog");
    check(dialog && QMetaObject::invokeMethod(dialog, "begin"), "控制窗口无法打开");
    const auto* editor = dialog->findChild<QObject*>("codeEditor");
    check(editor && editor->property("source").toString().contains("on_step"), "编辑器未显示当前程序");
    check(editor->property("fontFamily").toString() == CodeEditorSettings::instance()->fontFamily(), "编辑区未同步字体设置");
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
    auto* quickWindow = qobject_cast<QQuickWindow*>(window.get());
    check(quickWindow && quickWindow->grabWindow().save("code_editor_preview.png"), "无法渲染控制窗口预览");
    check(QMetaObject::invokeMethod(dialog, "close"), "无法关闭控制窗口");
    QObject* presets = window->findChild<QObject*>("presets");
    check(presets && QMetaObject::invokeMethod(presets, "openAt", Q_ARG(QVariant, 40), Q_ARG(QVariant, 40)), "预设菜单无法打开");
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
    check(presets->property("visible").toBool(), "预设菜单不可见");
    check(presets->property("width").toDouble() >= 350 && presets->property("height").toDouble() >= 400, "预设菜单没有正确计算尺寸");
    check(quickWindow->grabWindow().save("scene_presets_preview.png"), "无法渲染预设菜单预览");
    QCoreApplication::processEvents();
    if (!warnings.isEmpty()) throw std::runtime_error(warnings.join('\n').toStdString());
}
