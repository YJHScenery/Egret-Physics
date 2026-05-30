#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>
#include <QQmlContext>

#include <QIcon>

#include "view_model/simulator/scene_manager.h"
#include "basic_utils.h"
#include "logger.h"
#include "qt_component_helper.h"
#include "os/windows/windows_config.h"
#include "resource_helper.h"
#include "editor/model_manager.h"
#include "serialize/model_item_data.h"
#include <QQuickStyle>
#include <QStandardPaths>

#include "enum_handler.h"
#include "file_handler.h"

int main(int argc, char* argv[])
{
    // ReSharper disable once CppLocalVariableMayBeConst
    QApplication app(argc, argv);
    QQuickStyle::setStyle("Fusion");
    QObject::connect(&app, &QGuiApplication::aboutToQuit, []()
    {
        LOG_INFO_LITERAL("准备退出");
        // 执行清理工作
    });

    setWindowsTerminalUtf8();

    app.setApplicationName(EGRET_PHYSICS_APPLICATION_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationDisplayName(EGRET_PHYSICS_APPLICATION_DISPLAY_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationVersion(EGRET_PHYSICS_VERSION_STRING); // NOLINT(*-static-accessed-through-instance)
    app.setWindowIcon(QIcon(":/app_icon/assets/favicon/favicon_1024.png")); // NOLINT(*-static-accessed-through-instance)

    egret::AsyncLogger& logger{egret::AsyncLogger::instance()};
    logger.setLogFilePath("./logs/egret_physics_debug.log");
    logger.setMaxFileSize(20 * 1024 * 1024); // 20 MB
    logger.setMaxBackupFiles(10);
    logger.setOutputToConsole(true);
    logger.setOutputToFile(true);

    LOG_INFO_LITERAL("程序启动");

    egret::EnumHandler::registerEnums();
    qmlRegisterType<egret::ModelItemData>("ModelManager", 1, 0, "ModelItem");
    qmlRegisterType<egret::MaterialData>("ModelManager", 1, 0, "MaterialData");
    qmlRegisterSingletonInstance<egret::ModelManager>("ModelManager", 1, 0, "ModelManager",
                                                      egret::ModelManager::instance());
    qmlRegisterSingletonInstance<egret::EnumHandler>("EnumHandler", 1, 0, "EnumHandler", egret::EnumHandler::instance());
    
    // qmlRegisterType<egret::EnumHandler>("EnumHandler", 1, 0, "EnumHandler");

    egret::SceneManagerViewModel sceneController;
    egret::QtComponentHelper qtHelper;
    egret::ResourceHelper resourceHelper;
    egret::FileHandler fileHandler;
    // egret::EnumHandler enumHandler;

    // qmlRegisterType<egret::CoordinateGeometry>("CustomGeometry", 1, 0, "SimpleGeometry");

    QQmlApplicationEngine engine;
    egret::ModelManager::instance()->addModels(egret::createDefaultModels());

    engine.rootContext()->setContextProperty("sceneController", &sceneController);
    engine.rootContext()->setContextProperty("qtHelper", &qtHelper);
    engine.rootContext()->setContextProperty("resourceHelper", &resourceHelper);
    engine.rootContext()->setContextProperty("fileHandler", &fileHandler);
    engine.rootContext()->setContextProperty("documentsFolder",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    // engine.rootContext()->setContextProperty("enumHandler", &enumHandler);


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
                LOG_FATAL_LITERAL("QML 脚本加载失败，错误代码: -1");
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
    }
    else {
        LOG_INFO_LITERAL("程序退出");
    }
    return exitCode;
}
