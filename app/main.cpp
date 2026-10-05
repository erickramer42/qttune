#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle> 

#include "qttunebridge.h"
#include "qttune/version.h" 

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("QtTune");
    app.setApplicationName("QtTune");
    app.setApplicationVersion(QStringLiteral(QTTUNE_VERSION_STRING));

    QQuickStyle::setStyle("Basic");

    QtTuneBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("QtTune", &bridge);
    engine.loadFromModule("QtTune", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical("=== QML FAILED TO LOAD ===");
        return -1;
    }

    return app.exec();
}
