#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QSettings>
#include <QResource>
#include "AppContext.h"
#include "Graph/SldView.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("StationViz");
    QCoreApplication::setApplicationName("StationVizApp");

    qmlRegisterType<SldView>("StationViz", 1, 0, "SldView");

    static AppContext appContext;
    qmlRegisterSingletonInstance("StationViz", 1, 0, "App", &appContext);



    // Charger l'état UI sauvegardé
    appContext.uiStore()->loadSettings();

    QQmlApplicationEngine engine;
    Q_INIT_RESOURCE(qml);
    engine.addImportPath("qrc:/");
    engine.load(QUrl("qrc:/App.qml"));
    if (engine.rootObjects().isEmpty()) return -1;

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        appContext.uiStore()->saveSettings();
    });

    return app.exec();
}
