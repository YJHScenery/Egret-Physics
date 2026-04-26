#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <cstring>
#include <iostream>
#include <string>
#include <QQmlContext>

#include <QIcon>

#include "view_model/qt/scene_manager.h"


int main(int argc, char* argv[]) {
    const QGuiApplication app(argc, argv);

    QGuiApplication::setWindowIcon(QIcon(":/app_icon/assets/favicon/favicon_bg_radius.png"));


    // 原代码：
    // engine.rootContext()->setContextProperty("game2048", &game2048);
    // 新代码说明：将场景控制器注入 QML，作为 ModelView 层的统一入口。
    egret::SceneManagerViewModel sceneController;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("sceneController", &sceneController);
    // engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    const QUrl url(QStringLiteral("qrc:/main/main/MainWindow.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject* obj, const QUrl& objUrl)
        {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return QGuiApplication::exec();
}
