#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <cstring>
#include <iostream>
#include <string>
#include <QQmlContext>

#include <QIcon>

// #include "shape_rod.h"
// #include "standard_collide_judge_group.h"
#include "view_model/qt/scene_manager.h"


int main(int argc, char* argv[]) {
    const QGuiApplication app(argc, argv);
    // egret::ShapeRod rod(2.0);
    // egret::ShapeSphere sphere(0.8);
    //
    // egret::Transform transRod;
    // egret::Transform transSphere;
    // transSphere.setTranslation(Eigen::Vector3d(0, 0, 0.6));
    //
    // {
    //     egret::ContactManifold manifold;
    //     bool result = collideRodSphere(rod, transRod, sphere, transSphere, manifold);
    //     std::cout << result << std::endl;
    // }

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
