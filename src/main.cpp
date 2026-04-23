#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <cstring>
#include <iostream>
#include <string>
#include <QQmlContext>

#include <QIcon>


int main(int argc, char* argv[]) {
    const QGuiApplication app(argc, argv);

    // QGuiApplication::setWindowIcon(QIcon(":/icon/logo.png"));


    // engine.rootContext()->setContextProperty("game2048", &game2048);
    QQmlApplicationEngine engine;
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
