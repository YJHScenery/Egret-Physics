/**
 * @file        main.cpp
 * @brief       Egret Physics 物理引擎主程序入口文件。
 * @details     负责初始化 Qt QML 环境、日志系统、Windows 配置，
 *              并启动主窗口。是整个应用程序的入口点。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Main
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>
#include <QQmlContext>

#include <QIcon>

#include "view_model/simulator/scene_manager.h"
#include "basic_utils.h"
#include "async_logger.h"
#include "qt_component_helper.h"
#include "os/windows/windows_config.h"
#include "resource_helper.h"
#include "editor/editor_manager.h"
#include "editor/parameter_manager.h"
#include "serialize/model_item_data.h"
#include <QQuickStyle>
#include <QStandardPaths>

#include "egret_version.h"
#include "enum_handler.h"
#include "file_handler.h"
#include "qdebug_handler.h"
#include "spring_geometry.h"
#include "view/geometry/bezier_curve_geometry.h"
#include "curve_geometry.h"

#include "view/qt_media_helper.h"
#include "view_model/editor/clipboard_helper.h"

#include "command_line.h"
#include "simulation_export_controller.h"


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 命令行分支：无参数时进入图形界面；其余模式在创建 QML 引擎之前处理完毕。
    const egret::command::Request command = egret::command::parse(app.arguments());
    if (command.mode != egret::command::Mode::Gui) {
        return egret::command::execute(command);
    }

    QQuickStyle::setStyle("Fusion");

    std::function<void()> exitReady{
        []()
        {
            LOG_INFO_LITERAL("准备退出");
        }
    };

    QObject::connect(&app, &QGuiApplication::aboutToQuit, exitReady);

    setTerminalUtf8();

    CustomMessageHandler::setOutputTarget("./logs/qdebug_output.log");
    CustomMessageHandler::install();

    egret::AsyncLogger& logger{egret::AsyncLogger::instance()};
    logger.setLogFilePath("./logs/async_logger.log");
    logger.setMaxFileSize(20 * 1024 * 1024); // 20 MB
    logger.setMaxBackupFiles(10);
    logger.setOutputToConsole(true);
    logger.setOutputToFile(true);


    app.setApplicationName(EGRET_PHYSICS_APPLICATION_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationDisplayName(EGRET_PHYSICS_APPLICATION_DISPLAY_NAME); // NOLINT(*-static-accessed-through-instance)
    app.setApplicationVersion(egret::EGRET_VERSION_STRING.data()); // NOLINT(*-static-accessed-through-instance)
    app.setWindowIcon(QIcon(":/app_icon/assets/favicon/favicon_1024.png")); // NOLINT(*-static-accessed-through-instance)


    LOG_INFO_LITERAL("程序启动");

    egret::EnumHandler::registerEnums();
    qmlRegisterType<egret::ModelItemData>("EditorManager", 1, 0, "ModelItem");
    qmlRegisterType<egret::MaterialData>("EditorManager", 1, 0, "MaterialData");
    qmlRegisterSingletonInstance<egret::EditorManager>("EditorManager", 1, 0, "EditorManager",
                                                       egret::EditorManager::instance());

    qmlRegisterSingletonInstance<egret::EditorParameterManager>(
        "EditorManager", 1, 0, "EditorParameterManager", egret::EditorParameterManager::instance());
    qmlRegisterSingletonInstance<egret::ClipboardHelper>("ClipboardManager", 1, 0, "ClipboardManager",
                                                         egret::ClipboardHelper::instance());
    qmlRegisterSingletonInstance<
        egret::EnumHandler>("EnumHandler", 1, 0, "EnumHandler", egret::EnumHandler::instance());

    qmlRegisterType<egret::SpringGeometry>("EgretPhysics", 1, 0, "SpringGeometry");
    qmlRegisterType<egret::BezierCurveGeometry>("EgretPhysics", 1, 0, "BezierCurveGeometry");
    qmlRegisterType<egret::CurveGeometry>("EgretPhysics", 1, 0, "CurveGeometry");

    egret::SceneManagerViewModel sceneController;
    egret::SimulationExportController simulationExporter;
    egret::QtComponentHelper qtHelper;
    egret::ResourceHelper resourceHelper;
    egret::FileHandler fileHandler;
    egret::QtMediaHelper qtMediaHelper;

    QQmlApplicationEngine engine;
    // egret::EditorManager::instance()->addModels(egret::createDefaultModels());
    // egret::EditorManager::instance()->addModels(egret::createDefaultModels());

    engine.rootContext()->setContextProperty("sceneController", &sceneController);
    engine.rootContext()->setContextProperty("simulationExporter", &simulationExporter);
    engine.rootContext()->setContextProperty("qtHelper", &qtHelper);
    engine.rootContext()->setContextProperty("resourceHelper", &resourceHelper);
    engine.rootContext()->setContextProperty("fileHandler", &fileHandler);
    engine.rootContext()->setContextProperty("documentsFolder",
                                             QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    engine.rootContext()->setContextProperty("mediaHelper", &qtMediaHelper);


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
