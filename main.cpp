#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QImage>
#include <QTimer>
#include <QUrl>
#include "AppContext.h"
#include "Graph/SldView.h"

namespace {

// Renders one view to a PNG and exits.
//
// The window cannot be captured from outside on this setup: the scene graph
// runs through RHI/OpenGL, and X11's GetImage on a GL surface returns black
// even though the compositor shows the window. QQuickWindow::grabWindow() does
// the readback inside the process, which is the only route that works headless.
int grabToFile(QQuickWindow* window, const QString& outPath) {
    QImage img = window->grabWindow();
    if (img.isNull()) {
        qWarning("StationViz: grabWindow() returned a null image");
        return 2;
    }
    if (!img.save(outPath)) {
        qWarning("StationViz: could not write %s", qPrintable(outPath));
        return 2;
    }
    qInfo("StationViz: wrote %s (%dx%d)", qPrintable(outPath), img.width(), img.height());
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("StationViz");
    QCoreApplication::setApplicationName("StationVizApp");
    QQuickStyle::setStyle("Basic");   // the UI draws its own chrome

    qmlRegisterType<SldView>("StationViz", 1, 0, "SldView");

    static AppContext appContext;
    qmlRegisterSingletonInstance("StationViz", 1, 0, "App", &appContext);

    appContext.uiStore()->loadSettings();

    // ---- arguments ----
    QString scdFile;
    QString shotPath;
    QString tabId;
    int shotW = 1600, shotH = 1000;

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--screenshot" && i + 1 < argc) {
            shotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (a == "--tab" && i + 1 < argc) {
            tabId = QString::fromLocal8Bit(argv[++i]);
        } else if (a == "--size" && i + 1 < argc) {
            const QString s = QString::fromLocal8Bit(argv[++i]);
            const QStringList parts = s.split(QLatin1Char('x'));
            if (parts.size() == 2) {
                shotW = parts[0].toInt();
                shotH = parts[1].toInt();
            }
        } else if (!a.startsWith(QLatin1String("--"))) {
            scdFile = a;
        }
    }

    QQmlApplicationEngine engine;
    Q_INIT_RESOURCE(qml);
    engine.addImportPath("qrc:/");
    engine.load(QUrl("qrc:/App.qml"));
    if (engine.rootObjects().isEmpty()) return -1;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        qWarning("StationViz: root object is not a window");
        return -1;
    }

    if (!scdFile.isEmpty()) {
        if (!appContext.openSclFile(scdFile))
            qWarning("StationViz: could not open %s", qPrintable(scdFile));
    }
    if (!tabId.isEmpty()) appContext.uiStore()->setViewMode(tabId);

    if (!shotPath.isEmpty()) {
        window->resize(shotW, shotH);
        window->show();

        // Wait for the first real frame, then one more so layout and the async
        // SVG atlas have settled, then grab.
        auto* frames = new int(0);
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, shotPath, frames]{
            if (++(*frames) < 3) return;
            QTimer::singleShot(150, window, [window, shotPath, frames]{
                const int rc = grabToFile(window, shotPath);
                window->close();
                QCoreApplication::exit(rc);
                delete frames;
            });
        });
        // If nothing ever renders, do not hang forever.
        QTimer::singleShot(15000, window, [window, shotPath]{
            grabToFile(window, shotPath);
            window->close();
            QCoreApplication::exit(2);
        });
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        appContext.uiStore()->saveSettings();
    });

    return app.exec();
}