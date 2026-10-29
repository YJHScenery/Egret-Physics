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
#include <QUrl>
#include <QDir>

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
#include "analysis_session.h"
#include "view_model/video_export/encoder/video_export_controller.h"
#include "qcustomplot_item.h"


/** @brief 执行视频导出命令。 @param app 应用实例。 @param arguments 导出参数。 @return 进程退出码。 */
int runVideoExport(QApplication& app, const QStringList& arguments);

int main(int argc, char* argv[])
{
    if (argc > 1 && QByteArray(argv[1]) == "--export-video") {
        qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    }
    QApplication app(argc, argv);

    QStringList arguments = app.arguments();
    if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--export-video")) {
        arguments.removeAt(1);
        return runVideoExport(app, arguments);
    }

    // 命令行分支：无参数时进入图形界面；纯命令行模式在创建 QML 引擎之前处理完毕。
    // --play / --edit 是例外：前者要在图形界面里打开「播放器」工作区并自动播放，
    // 后者要在「场景编辑器」工作区载入场景，因此都不在这里提前退出，
    // 而是把文件路径带进下面的界面流程。
    const egret::command::Request command = egret::command::parse(app.arguments());
    if (command.mode != egret::command::Mode::Gui
        && command.mode != egret::command::Mode::Play
        && command.mode != egret::command::Mode::Edit) {
        return egret::command::execute(command);
    }

    // 双击 .epsx / .epscene 时资源管理器给出的工作目录不受控（可能是 %SystemRoot%\System32），
    // 而日志、示例网格等资源都按相对路径解析；把工作目录钉在可执行文件所在目录，
    // 让双击启动与「在部署目录运行」表现一致。
    if (command.mode == egret::command::Mode::Play || command.mode == egret::command::Mode::Edit) {
        QDir::setCurrent(QCoreApplication::applicationDirPath());
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
    // 数据分析工作区的图表项（QCustomPlot 桥接）
    qmlRegisterType<egret::analysis::QCustomPlotItem>("EgretCharts", 1, 0, "QCustomPlotItem");

    egret::SceneManagerViewModel sceneController;
    // 离线仿真工作区使用独立的空场景控制器：初始不加载任何场景，只显示坐标系与网格
    egret::SceneManagerViewModel offlineSceneController{nullptr, false};
    // 播放器工作区同样使用独立的空场景控制器：未打开仿真文件时不渲染任何实体
    egret::SceneManagerViewModel playerSceneController{nullptr, false};
    egret::SimulationExportController simulationExporter;
    egret::video::VideoExportController videoExporter;
    // 数据分析工作区：EPSX 读取、抽帧、导出与绘图数据都由该门面对象提供
    egret::analysis::AnalysisSession analysisSession;
    egret::QtComponentHelper qtHelper;
    egret::ResourceHelper resourceHelper;
    egret::FileHandler fileHandler;
    egret::QtMediaHelper qtMediaHelper;

    QQmlApplicationEngine engine;
    // egret::EditorManager::instance()->addModels(egret::createDefaultModels());
    // egret::EditorManager::instance()->addModels(egret::createDefaultModels());

    engine.rootContext()->setContextProperty("sceneController", &sceneController);
    engine.rootContext()->setContextProperty("offlineSceneController", &offlineSceneController);
    engine.rootContext()->setContextProperty("playerSceneController", &playerSceneController);
    engine.rootContext()->setContextProperty("simulationExporter", &simulationExporter);
    engine.rootContext()->setContextProperty("videoExporter", &videoExporter);
    engine.rootContext()->setContextProperty("analysisSession", &analysisSession);
    engine.rootContext()->setContextProperty("qtHelper", &qtHelper);
    engine.rootContext()->setContextProperty("resourceHelper", &resourceHelper);
    engine.rootContext()->setContextProperty("fileHandler", &fileHandler);
    // FileDialog 的 currentFolder 是 url 属性：必须传 file:// URL。
    // 传裸路径（"C:/Users/..."）会被解析成 scheme = "c"，Qt 会打印
    // QWindowsNativeFileDialogBase::shellItem : Unhandled scheme: "c"。
    engine.rootContext()->setContextProperty(
        "documentsFolder",
        QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
    engine.rootContext()->setContextProperty("mediaHelper", &qtMediaHelper);

    // 启动信息：--play <file> 让主窗口切到播放器工作区并自动播放；
    // --edit <file> 让主窗口切到场景编辑器工作区并载入场景。
    const bool startInPlayer = command.mode == egret::command::Mode::Play;
    const bool startInEditor = command.mode == egret::command::Mode::Edit;
    engine.rootContext()->setContextProperty(
        "startupWorkspaceRoute",
        startInPlayer ? QStringLiteral("player")
                      : (startInEditor ? QStringLiteral("solver") : QString{}));
    engine.rootContext()->setContextProperty(
        "startupSimulationFile",
        startInPlayer ? QDir::toNativeSeparators(command.playFile) : QString{});
    engine.rootContext()->setContextProperty(
        "startupSceneFile",
        startInEditor ? QDir::toNativeSeparators(command.editFile) : QString{});


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
