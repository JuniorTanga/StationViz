#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFileInfo>

#include "SldFacade.h"

int main(int argc, char *argv[]) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Exposer SldFacade au QML
    auto* facade = new SldFacade(&app);
    engine.rootContext()->setContextProperty("sldFacade", facade);
    facade->planJson();

    // Passer un chemin initial (optionnel) depuis la ligne de commande
    QString initialPath;
    if (argc > 1) {
        QFileInfo fi(QString::fromLocal8Bit(argv[1]));
        if (fi.exists()) initialPath = fi.absoluteFilePath();
    }
    engine.rootContext()->setContextProperty("initialSclPath", initialPath);

    // Charger Main.qml depuis le répertoire courant
    const QUrl url = QUrl::fromLocalFile(QStringLiteral("ui/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl) QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
