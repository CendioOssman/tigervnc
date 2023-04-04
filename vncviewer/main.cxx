#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QTranslator>
#include <QMainWindow>
#include "eventfilter.h"
#include "config.h"
#include "viewerconfig.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "vnccanvas.h"
#include "vncrenderer.h"

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QTGLESSTREAM_DISPLAY")) {
        qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", QByteArray("213"));
        qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", QByteArray("120"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
#endif
    }

    QVNCApplication app(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "qvncviewer_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    ViewerConfig::initialize();
    AppManager::initialize();
    qmlRegisterType<VncCanvas>("Qt.TigerVNC", 1, 0, "VncCanvas");
    qmlRegisterType<VNCFramebuffer>("Qt.TigerVNC", 1, 0, "VNCFramebuffer");

    QQmlApplicationEngine engine;

    QQmlContext *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("PACKAGE_VERSION"), PACKAGE_VERSION);
    context->setContextProperty(QStringLiteral("BUILD_TIMESTAMP"), BUILD_TIMESTAMP);
    context->setContextProperty(QStringLiteral("COPYRIGHT_YEAR"), 2022);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    EventListener eventListener;
    QList<QObject*> qobjects = engine.rootObjects();
    qobjects.first()->installEventFilter(&eventListener);

    return app.exec();
}
