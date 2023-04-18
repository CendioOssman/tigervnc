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

//#define PRINT_SCREEN

#if defined(PRINT_SCREEN)
#include <QScreen>
#if defined(WIN32)
#include <windef.h>
#include <winuser.h>
#endif
#endif

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QTGLESSTREAM_DISPLAY")) {
        qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", QByteArray("213"));
        qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", QByteArray("120"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif
    }

    QVNCApplication app(argc, argv);

#if defined(PRINT_SCREEN)
    QScreen *primaryScreen = app.primaryScreen();
    QList<QScreen*> screens = app.screens();
    for (int i = 0; i < screens.size(); i++) {
      QScreen* screen = screens[i];
      qDebug() << "=================================";
      qDebug() << "index=" << i;
      qDebug() << "isPrimary=" << (screen == primaryScreen);
      qDebug() << "availableGeometory=" << screen->availableGeometry();
      qDebug() << "availableSize=" << screen->availableSize();
      qDebug() << "availableVirtualGeometry=" << screen->availableVirtualGeometry();
      qDebug() << "availableVirtualSize=" << screen->availableVirtualSize();
      qDebug() << "depth=" << screen->depth();
      qDebug() << "devicePixelRatio=" << screen->devicePixelRatio();
      qDebug() << "geometry=" << screen->geometry();
      qDebug() << "isLandscape=" << screen->isLandscape(Qt::PrimaryOrientation);
      qDebug() << "isPortrait=" << screen->isPortrait(Qt::PrimaryOrientation);
      qDebug() << "logicalDotsPerInch=" << screen->logicalDotsPerInch();
      qDebug() << "logicalDotsPerInchX=" << screen->logicalDotsPerInchX();
      qDebug() << "logicalDotsPerInchY=" << screen->logicalDotsPerInchY();
      qDebug() << "model=" << screen->model();
      qDebug() << "name=" << screen->name();
      qDebug() << "nativeOrientation=" << screen->nativeOrientation();
      qDebug() << "orientation=" << screen->orientation();
      qDebug() << "physicalDotsPerInch=" << screen->physicalDotsPerInch();
      qDebug() << "physicalDotsPerInchX=" << screen->physicalDotsPerInchX();
      qDebug() << "physicalDotsPerInchY=" << screen->physicalDotsPerInchY();
      qDebug() << "physicalSize=" << screen->physicalSize();
      qDebug() << "virtualGeometry=" << screen->virtualGeometry();
      qDebug() << "virtualSize=" << screen->virtualSize();
#if defined(WIN32)
      QRect sr = screen->geometry();
      HMONITOR hMon = MonitorFromPoint(POINT{sr.x(), sr.y()}, MONITOR_DEFAULTTONULL);
      if (hMon) {
        MONITORINFO info;
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMon, &info);
        qDebug() << "MONITORINFOEX.rcMonitor left=" << info.rcMonitor.left << ", top=" << info.rcMonitor.top << ", right=" << info.rcMonitor.right << ", bottom=" << info.rcMonitor.bottom;
        qDebug() << "MONITORINFOEX.rcWork    left=" << info.rcWork.left << ", top=" << info.rcWork.top << ", right=" << info.rcWork.right << ", bottom=" << info.rcWork.bottom;
        qDebug() << "MONITORINFOEX.dwFlags (primary?)=" << info.dwFlags;
      }
#endif
    }
#if defined(WIN32)
    for (int i = 0; i < 79; i++) {
      qDebug().nospace() << "GetSystemMetrics(" << i << ")=" << GetSystemMetrics(i);
    }
#endif
#endif

    ViewerConfig::initialize();
    AppManager::initialize();

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
