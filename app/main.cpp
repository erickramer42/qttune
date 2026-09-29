#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "qttunebridge.h"
#include "qttune/version.h" 

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("QtTune");
    app.setApplicationName("QtTune");
    app.setApplicationVersion(QStringLiteral(QTTUNE_VERSION_STRING));

    QtTuneBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Qtune", &bridge);
    engine.loadFromModule("QtTune", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical("=== QML FAILED TO LOAD ===");
        return -1;
    }

    return app.exec();
}
