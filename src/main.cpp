#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>
#include <cstring>
#include <iostream>
#include <string>
#include <QQmlContext>

#include <QIcon>

#include "scene_manager.h"
#include "basic_utils.h"
#include "logger.h"
#include "view/coordinate.h"

void setWindowsTerminalUtf8();

int main(int argc, char* argv[]) {

    // ReSharper disable once CppLocalVariableMayBeConst
    QGuiApplication app(argc, argv);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, []() {
           LOG_INFO_LITERAL("准备退出");
           // 执行清理工作
       });

    setWindowsTerminalUtf8();

    app.setApplicationName(EGRET_PHYSICS_APPLICATION_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationDisplayName(EGRET_PHYSICS_APPLICATION_DISPLAY_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationVersion(EGRET_PHYSICS_VERSION_STRING); // NOLINT(*-static-accessed-through-instance)
    app.setWindowIcon(QIcon(":/app_icon/assets/favicon/favicon_bg_radius.png")); // NOLINT(*-static-accessed-through-instance)

    egret::AsyncLogger& logger{egret::AsyncLogger::instance()};
    logger.setLogFilePath("./logs/egret_physics_debug.log");
    logger.setMaxFileSize(20 * 1024 * 1024);  // 20 MB
    logger.setMaxBackupFiles(10);
    logger.setOutputToConsole(true);
    logger.setOutputToFile(true);

    LOG_INFO_LITERAL("程序启动");
    // 新代码说明：将场景控制器注入 QML，作为 ModelView 层的统一入口。
    egret::SceneManagerViewModel sceneController;

    qmlRegisterType<CoordinateGeometry>("CustomGeometry", 1, 0, "SimpleGeometry");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("sceneController", &sceneController);
    // engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    const QUrl url(QStringLiteral("qrc:/main/main/MainWindow.qml"));
    constexpr int QML_LOAD_FAILED{-1};

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url, &app](QObject* obj, const QUrl& objUrl)
        {
            if (!obj && url == objUrl) {
                LOG_FATAL_LITERAL("QML 脚本加载失败，错误代码:");
                app.exit(QML_LOAD_FAILED); // NOLINT(*-static-accessed-through-instance)
            }
        },
        Qt::QueuedConnection);

    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        LOG_FATAL_LITERAL("QML 引擎根对象创建失败，错误代码: -1");
        return QML_LOAD_FAILED;
    }

    const int exitCode = app.exec(); // NOLINT(*-static-accessed-through-instance)

    if (exitCode != 0) {
        LOG_WARN_LITERAL("程序异常退出");
    }else {
        LOG_INFO_LITERAL("程序退出");
    }
    return exitCode;
}
